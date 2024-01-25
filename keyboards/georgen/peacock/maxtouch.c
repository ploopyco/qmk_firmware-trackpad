// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "i2c_master.h"
#include "maxtouch.h"
#include "pointing_device.h"

#define DIVIDE_UNSIGNED_ROUND(numerator, denominator) (((numerator) + ((denominator) / 2)) / (denominator))
#define CPI_TO_SAMPLES(cpi, dist_in_mm) (DIVIDE_UNSIGNED_ROUND((cpi) * (dist_in_mm) * 10, 254))
#define SAMPLES_TO_CPI(samples, dist_in_mm) (DIVIDE_UNSIGNED_ROUND((samples) * 254, (dist_in_mm) * 10))
#define SWAP_BYTES(a) ((a << 8) | (a >> 8))

// TODO: These are peacock specific, they are used for handling CPI calculations. Is there a better default?
#ifndef MXT_SENSOR_WIDTH
#   define MXT_SENSOR_WIDTH 156
#endif

#ifndef MXT_SENSOR_HEIGHT
#   define MXT_SENSOR_HEIGHT 91
#endif

#ifndef MAX_TOUCH_REPORTS
#   define MAX_TOUCH_REPORTS 4 // Can be up to 10
#endif

#ifndef MXT_SCROLL_DIVISOR
#   define MXT_SCROLL_DIVISOR 4
#endif

// Data from the object table. Registers are not at fixed addresses, they may vary between firmware
// versions. Instead must read the addresses from the object table.
static uint16_t t2_encryption_status_address                    = 0;
static uint16_t t5_message_processor_address                    = 0;
static uint16_t t5_max_message_size                             = 0;
static uint16_t t6_command_processor_address                    = 0;
static uint16_t t7_powerconfig_address                          = 0;
static uint16_t t8_acquisitionconfig_address                    = 0;
static uint16_t t44_message_count_address                       = 0;
static uint16_t t46_cte_config_address                          = 0;
static uint16_t t100_multiple_touch_touchscreen_address         = 0;

// The object table also contains report_ids. These are used to identify which object generated a
// message. Again we must lookup these values rather than using hard coded values.
// Most messages are ignored, we basically just want the messages from the t100 object for now.
static uint16_t t100_first_report_id                            = 0;
static uint16_t t100_second_report_id                           = 0;
static uint16_t t100_subsequent_report_ids[MAX_TOUCH_REPORTS]   = {};
static uint16_t t100_num_reports                                = 0;

// Current driver state state
static uint16_t cpi                                             = 300;

// If true, we generate one more call to pointing_device_driver_get_report
// after the motion pin goes low. This enables us to clear button state
// in the case of a tap.
static bool extra_event                                         = false;    

void pointing_device_driver_init(void) {
    mxt_information_block information = {0};

    i2c_init();
    i2c_status_t status = i2c_readReg16(MXT336UD_ADDRESS, MXT_REG_INFORMATION_BLOCK, (uint8_t *)&information,
        sizeof(mxt_information_block), MXT_I2C_TIMEOUT_MS);

    // First read the object table to lookup addresses and report_ids of the various objects
    if (status == I2C_STATUS_SUCCESS) {
        // I2C found device family: 166 with 34 objects
        dprintf("Found MXT %d:%d, fw %d.%d with %d objects. Matrix size %dx%d\n", information.family_id, information.variant_id,
            information.version, information.build, information.num_objects, information.matrix_x_size, information.matrix_y_size);
        int report_id = 1;
        uint16_t object_table_element_address = sizeof(mxt_information_block);
        for (int i = 0; i < information.num_objects; i++) {
            mxt_object_table_element object = {};
            i2c_status_t status = i2c_readReg16(MXT336UD_ADDRESS, SWAP_BYTES(object_table_element_address),
                (uint8_t *)&object, sizeof(mxt_object_table_element), MXT_I2C_TIMEOUT_MS);
            if (status == I2C_STATUS_SUCCESS) {
                // Store addresses in network byte order
                const uint16_t address = object.position_ms_byte | (object.position_ls_byte << 8);
                switch (object.type) {
                    case 2:
                        t2_encryption_status_address            = address;
                        break;
                    case 5:
                        t5_message_processor_address            = address;
                        t5_max_message_size                     = object.size_minus_one - 1;
                        break;
                    case 6:
                        t6_command_processor_address            = address;
                        break;
                    case 7:
                        t7_powerconfig_address                  = address;
                        break;
                    case 8:
                        t8_acquisitionconfig_address            = address;
                        break;
                    case 44:
                        t44_message_count_address               = address;
                        break;
                    case 46:
                        t46_cte_config_address                  = address;
                        break;
                    case 100:
                        t100_multiple_touch_touchscreen_address = address;
                        t100_first_report_id                    = report_id;
                        t100_second_report_id                   = report_id + 1;
                        for (t100_num_reports = 0; t100_num_reports < MAX_TOUCH_REPORTS && t100_num_reports < object.report_ids_per_instance; t100_num_reports++) {
                            t100_subsequent_report_ids[t100_num_reports] = report_id + 2 + t100_num_reports;
                        }
                        break;
                }
                object_table_element_address += sizeof(mxt_object_table_element);
                report_id += object.report_ids_per_instance * (object.instances_minus_one + 1);
            } else {
                dprintf("Failed to read object table element. Status: %d\n", status);
            }
        }
    } else {
        dprintf("Failed to read object table. Status: %d\n", status);
    }

    // TODO Remove? Maybe not interesting unless for whatever reason encryption is enabled and we need to turn it off
    if (t2_encryption_status_address) {
        mxt_gen_encryptionstatus_t2 t2 = {};
        i2c_status_t status = i2c_readReg16(MXT336UD_ADDRESS, t2_encryption_status_address,
            (uint8_t *)&t2, sizeof(mxt_gen_encryptionstatus_t2), MXT_I2C_TIMEOUT_MS);
        if (status != I2C_STATUS_SUCCESS) {
            dprintf("Failed to read T2. Status: %02x %d\n", t2.status, t2.error);
        }
    }

    // Configure power saving features
    if (t7_powerconfig_address) {
        mxt_gen_powerconfig_t7 t7 = {};
        t7.idleacqint   = 32;   // The acquisition interval while in idle mode
        t7.actacqint    = 10;   // The acquisition interval while in active mode
        t7.actv2idelto  = 50;   // The timeout for transitioning from active to idle mode
        t7.cfg          = T7_CFG_ACTVPIPEEN | T7_CFG_IDLEPIPEEN;    // Enable pipelining in both active and idle mode

        i2c_writeReg16(MXT336UD_ADDRESS, t7_powerconfig_address, (uint8_t *)&t7, sizeof(mxt_gen_powerconfig_t7), MXT_I2C_TIMEOUT_MS);
    }

    // Configure capacitive acquision, currently we use all the default values but it feels like some of this stuff might be important.
    if (t8_acquisitionconfig_address) {
        mxt_gen_acquisitionconfig_t8 t8 = {};
        i2c_writeReg16(MXT336UD_ADDRESS, t8_acquisitionconfig_address, (uint8_t *)&t8, sizeof(mxt_gen_acquisitionconfig_t8), MXT_I2C_TIMEOUT_MS);
    }

    // Mutural Capacitive Touch Engine (CTE) configuration, currently we use all the default values but it feels like some of this stuff might be important.
    if (t46_cte_config_address) {
        mxt_spt_cteconfig_t46 t46 = {};
        i2c_writeReg16(MXT336UD_ADDRESS, t46_cte_config_address, (uint8_t *)&t46, sizeof(mxt_spt_cteconfig_t46), MXT_I2C_TIMEOUT_MS);
    }

    // Multiple touch touchscreen confguration - defines an area of the sensor to use as a trackpad/touchscreen. This object generates all our interesting report messages.
    if (t100_multiple_touch_touchscreen_address) {
        mxt_touch_multiscreen_t100 cfg      = {};
        i2c_status_t status                 = i2c_readReg16(MXT336UD_ADDRESS, t100_multiple_touch_touchscreen_address,
                                                (uint8_t *)&cfg, sizeof(mxt_touch_multiscreen_t100), MXT_I2C_TIMEOUT_MS);
        cfg.ctrl                            = T100_CTRL_RPTEN | T100_CTRL_ENABLE;  // Enable the t100 object, and enable message reporting for the t100 object.1`
        cfg.cfg1                            = T100_CFG_SWITCHXY; // Could also handle rotation, and axis inversion in hardware here
        cfg.scraux                          = 0x1;  // AUX data: Report the number of touch events
        cfg.numtch                          = MAX_TOUCH_REPORTS; // The number of touch reports we want to receive (upto 10)
        cfg.xsize                           = information.matrix_x_size;    // TODO: Make configurable as this depends on the sensor design.
        cfg.ysize                           = information.matrix_y_size;    // TODO: Make configurable as this depends on the sensor design.
        cfg.xpitch                          = 15;   // Pitch between X-Lines (5mm + 0.1mm * XPitch). TODO: Make configurable as this depends on the sensor design.
        cfg.ypitch                          = 15;   // Pitch between Y-Lines (5mm + 0.1mm * YPitch). TODO: Make configurable as this depends on the sensor design.
        cfg.gain                            = 1;    // Single transmit gain for mutual capacitance measurements
        cfg.dxgain                          = 255;  // Dual transmit gain for mutual capacitance measurements
        cfg.tchthr                          = 12;   // Touch threshold
        cfg.mrgthr                          = 5;    // Merge threshold
        cfg.mrghyst                         = 5;    // Merge threshold hysterisis
        cfg.xrange                          = CPI_TO_SAMPLES(cpi, MXT_SENSOR_HEIGHT);    // CPI handling, adjust the reported resolution
        cfg.yrange                          = CPI_TO_SAMPLES(cpi, MXT_SENSOR_WIDTH);     // CPI handling, adjust the reported resolution
    
        status                              = i2c_writeReg16(MXT336UD_ADDRESS, t100_multiple_touch_touchscreen_address,
                                                (uint8_t *)&cfg, sizeof(mxt_touch_multiscreen_t100), MXT_I2C_TIMEOUT_MS);
        if (status != I2C_STATUS_SUCCESS) {
            dprintf("T100 Configuration failed: %d\n", status);
        }
    }
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    // TODO: Refactor the gesture handling.
    static bool button_held = false;
    static int max_fingers = 0;
    static pointing_device_buttons_t held_button = POINTING_DEVICE_BUTTON1;
    bool seen_t100 = false;
    static int fingers = 0;

    if (t44_message_count_address) {
        mxt_message_count message_count = {};
        i2c_status_t status = i2c_readReg16(MXT336UD_ADDRESS, t44_message_count_address,
            (uint8_t *)&message_count, sizeof(mxt_message_count), MXT_I2C_TIMEOUT_MS);
        if (status == I2C_STATUS_SUCCESS) {
            for (int i = 0; i < message_count.count; i++) {
                mxt_message message = {};
                status              = i2c_readReg16(MXT336UD_ADDRESS, t5_message_processor_address,
                                        (uint8_t *)&message, sizeof(mxt_message), MXT_I2C_TIMEOUT_MS);

                if (message.report_id == t100_first_report_id) {
                    // Track the maximum number of fingers since the last DOWN event.
                    fingers     = message.data[1];
                    max_fingers = MAX(max_fingers, fingers);
                }
                else if (message.report_id == t100_subsequent_report_ids[0]) {
                    int event                   = (message.data[0] & 0xf);
                    bool down                   = (event == 0x4) || (event == 0x8) || (event == 0x9);
                    static int last_x           = 0;
                    static int last_y           = 0;
                    static uint32_t down_timer  = 0;
                    static int down_x           = 0;
                    static int down_y           = 0;
                    static int move             = false;
                    uint32_t elapsed            = timer_elapsed32(down_timer);
                    uint16_t x                  = message.data[1] | (message.data[2] << 8);
                    uint16_t y                  = message.data[3] | (message.data[4] << 8);

                    seen_t100                   = true;

                    // If this isnt a finger down event, update either our move or scroll
                    // potition, depending on the number of fingers on the screen.
                    if (!down) {
                        if (fingers == 2) {
                            // Scrolling is too fast, so divide the h/v values.
                            static int carry_h  = 0;
                            static int carry_v  = 0;

                            int h               = last_x - x + carry_h;
                            int v               = y - last_y + carry_v;

                            carry_h             = h % MXT_SCROLL_DIVISOR;
                            carry_v             = v % MXT_SCROLL_DIVISOR;
                            
                            mouse_report.h      = h/MXT_SCROLL_DIVISOR;
                            mouse_report.v      = v/MXT_SCROLL_DIVISOR;
                        } else {
                            mouse_report.x      = x - last_x;
                            mouse_report.y      = y - last_y;
                        }
                    }

                    // If a finger is held in the same spot for 500ms, send a button down.
                    if (!move && !button_held && (event == NO_EVENT || event == MOVE || event == UP)) {
                        if (elapsed > 500) {
                            if ((abs(down_x - x) < 8) && (abs(down_y - y) < 8)) {
                                held_button             = POINTING_DEVICE_BUTTON1 + max_fingers - 1;
                                mouse_report.buttons    = pointing_device_handle_buttons(mouse_report.buttons, true, held_button);
                                button_held             = true;
                            }
                            else {
                                move                    = true;
                            }
                        }
                    }

                    // Detect tap events by measuring the time beween finger down and up, and the distance traveled.
                    if (event == DOWN) {
                        down_timer  = timer_read32();
                        down_x      = x;
                        down_y      = y;
                        move        = false;
                        button_held = false;
                        max_fingers = fingers;
                    } 
                    else if (event == UP) {
                        if (button_held) {
                            mouse_report.buttons    = pointing_device_handle_buttons(mouse_report.buttons, false, held_button);
                            button_held             = false;
                        }
                        else if (elapsed < 150) {
                            held_button             = POINTING_DEVICE_BUTTON1 + max_fingers - 1;
                            mouse_report.buttons    = pointing_device_handle_buttons(mouse_report.buttons, true, held_button);
                            button_held             = true;
                            extra_event             = true; // So we can send the button up event.
                        }
                        move = false;
                    }
                    last_x = x;
                    last_y = y;
                } else {
                    uprintf("Unhandled ID: %d\n", message.report_id);
                }
            }
        }
    }

    // Special case, if no messages were recieved this is probably an extra_event we requested after sending a
    // button down for a tap. We need to send the button up now. 
    if (!seen_t100 && button_held) {
        mouse_report.buttons    = pointing_device_handle_buttons(mouse_report.buttons, false, held_button);
        button_held             = false;
    }

    return mouse_report;
}

uint16_t pointing_device_driver_get_cpi(void) {
    return cpi;
}

void pointing_device_driver_set_cpi(uint16_t new_cpi) {
    cpi = new_cpi;
    if (t100_multiple_touch_touchscreen_address) {
        // TODO, We could probably just write 2 bytes.
        mxt_touch_multiscreen_t100 cfg  = {};
        i2c_status_t status             = i2c_readReg16(MXT336UD_ADDRESS, t100_multiple_touch_touchscreen_address, 
                                            (uint8_t *)&cfg, sizeof(mxt_touch_multiscreen_t100), MXT_I2C_TIMEOUT_MS);
        cfg.xrange                      = CPI_TO_SAMPLES(cpi, MXT_SENSOR_HEIGHT);
        cfg.yrange                      = CPI_TO_SAMPLES(cpi, MXT_SENSOR_WIDTH);
        status                          = i2c_writeReg16(MXT336UD_ADDRESS, t100_multiple_touch_touchscreen_address,
                                            (uint8_t *)&cfg, sizeof(mxt_touch_multiscreen_t100), MXT_I2C_TIMEOUT_MS);
        if (status != I2C_STATUS_SUCCESS) {
            dprintf("T100 Configuration failed: %d\n", status);
        }
    }
}

#ifdef POINTING_DEVICE_MOTION_PIN
// TODO: This is an extension to the standard pointing device feature. Essentially to detect a tap
// we measure the time between a DOWN and UP event, and if it is short we send a button press. But
// we also need to send a button up event, and if there is no motion, we dont get a chance to do it.
// Perhaps there is a better way of handling this?
bool pointing_device_driver_motion_detected(void) {
    bool result = extra_event || !readPin(POINTING_DEVICE_MOTION_PIN);
    extra_event = false;
    return result;
}
#endif
