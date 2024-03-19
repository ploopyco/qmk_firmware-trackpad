// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "i2c_master.h"
#include "maxtouch.h"

#include "digitizer.h"
#include "digitizer_driver.h"

#define DIVIDE_UNSIGNED_ROUND(numerator, denominator) (((numerator) + ((denominator) / 2)) / (denominator))
#define CPI_TO_SAMPLES(cpi, dist_in_mm) (DIVIDE_UNSIGNED_ROUND((cpi) * (dist_in_mm) * 10, 254))
#define SWAP_BYTES(a) ((a << 8) | (a >> 8))

// By default we assume all available X and Y pins are in use, but a designer
// may decide to leave some pins unconnected, so the size can be overridden here.
#ifndef MXT_MATRIX_X_SIZE
#   define MXT_MATRIX_X_SIZE information.matrix_x_size
#endif

#ifndef MXT_MATRIX_Y_SIZE
#   define MXT_MATRIX_Y_SIZE information.matrix_y_size
#endif

#ifndef MXT_SCROLL_DIVISOR
#   define MXT_SCROLL_DIVISOR 4
#endif


// We detect a tap gesture if an UP event occurs within MXT_TAP_TIME
// milliseconds of the DOWN event.
#ifndef MXT_TAP_TIME
#   define MXT_TAP_TIME 100
#endif

// We detect a tap and hold gesture if a finger does not move
// further than MXT_TAP_AND_HOLD_DISTANCE within MXT_TAP_AND_HOLD_TIME
// milliseconds of being put down on the sensor.
#ifndef MXT_TAP_AND_HOLD_TIME
#   define MXT_TAP_AND_HOLD_TIME 200
#endif
#ifndef MXT_TAP_AND_HOLD_DISTANCE
#   define MXT_TAP_AND_HOLD_DISTANCE 5
#endif

#ifndef MXT_CPI
    #define MXT_CPI 600
#endif

#ifndef MXT_RECALIBRATE_AFTER
    // Steps of 200ms, 25 = 5 seconds
    #define MXT_RECALIBRATE_AFTER 25
#endif

#ifndef MXT_SURFACE_TYPE
    #define MXT_SURFACE_TYPE VINYL
#endif

#define VINYL 1
#define ACRYLIC 2

#if MXT_SURFACE_TYPE==VINYL
    #ifndef MXT_TOUCH_THRESHOLD
        #define MXT_TOUCH_THRESHOLD 18
    #endif

    #ifndef MXT_GAIN
        #define MXT_GAIN 4
    #endif
#elif MXT_SURFACE_TYPE==ACRYLIC
    #ifndef MXT_TOUCH_THRESHOLD
        #define MXT_TOUCH_THRESHOLD 12
    #endif

    #ifndef MXT_GAIN
        #define MXT_GAIN 5
    #endif
#else
    #error "Unknown surface type."
#endif

#ifndef MXT_DX_GAIN
    #define MXT_DX_GAIN 255
#endif

// Data from the object table. Registers are not at fixed addresses, they may vary between firmware
// versions. Instead must read the addresses from the object table.
static uint16_t t2_encryption_status_address                        = 0;
static uint16_t t5_message_processor_address                        = 0;
static uint16_t t5_max_message_size                                 = 0;
static uint16_t t6_command_processor_address                        = 0;
static uint16_t t7_powerconfig_address                              = 0;
static uint16_t t8_acquisitionconfig_address                        = 0;
static uint16_t t44_message_count_address                           = 0;
static uint16_t t46_cte_config_address                              = 0;
static uint16_t t100_multiple_touch_touchscreen_address             = 0;

// The object table also contains report_ids. These are used to identify which object generated a
// message. Again we must lookup these values rather than using hard coded values.
// Most messages are ignored, we basically just want the messages from the t100 object for now.
static uint16_t t100_first_report_id                                = 0;
static uint16_t t100_second_report_id                               = 0;
static uint16_t t100_subsequent_report_ids[DIGITIZER_FINGER_COUNT]  = {};
static uint16_t t100_num_reports                                    = 0;

void maxtouch_init(void) {
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
                        for (t100_num_reports = 0; t100_num_reports < DIGITIZER_FINGER_COUNT && t100_num_reports < object.report_ids_per_instance; t100_num_reports++) {
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
        t8.tchautocal = MXT_RECALIBRATE_AFTER;
        t8.atchcalst = MXT_RECALIBRATE_AFTER;

        // Antitouch detection - reject palms etc..
        t8.atchcalsthr = 20;
        t8.atchfrccalthr = 50;
        t8.atchfrccalratio = 25;

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
        // TODO: Generic handling of rotation/inversion for absolute mode?
#ifdef DIGITIZER_INVERT_X
        cfg.cfg1                            = T100_CFG_SWITCHXY | T100_CFG_INVERTY; // Could also handle rotation, and axis inversion in hardware here
#else
        cfg.cfg1                            = T100_CFG_SWITCHXY; // Could also handle rotation, and axis inversion in hardware here
#endif
        cfg.scraux                          = 0x1;  // AUX data: Report the number of touch events
        cfg.numtch                          = DIGITIZER_FINGER_COUNT;   // The number of touch reports we want to receive (upto 10)
        cfg.xsize                           = MXT_MATRIX_X_SIZE;    // Make configurable as this depends on the sensor design.
        cfg.ysize                           = MXT_MATRIX_Y_SIZE;    // Make configurable as this depends on the sensor design.
        cfg.xpitch                          = (MXT_SENSOR_WIDTH_MM * 10 / MXT_MATRIX_X_SIZE) - 50;     // Pitch between X-Lines (5mm + 0.1mm * XPitch).
        cfg.ypitch                          = (MXT_SENSOR_HEIGHT_MM * 10 / MXT_MATRIX_Y_SIZE) - 50;    // Pitch between Y-Lines (5mm + 0.1mm * YPitch).
        cfg.gain                            = MXT_GAIN; // Single transmit gain for mutual capacitance measurements
        cfg.dxgain                          = MXT_DX_GAIN;  // Dual transmit gain for mutual capacitance measurements (255 = auto calibrate)
        cfg.tchthr                          = MXT_TOUCH_THRESHOLD;  // Touch threshold
        cfg.mrgthr                          = 2;    // Merge threshold
        cfg.mrghyst                         = 2;    // Merge threshold hysteresis
        cfg.movsmooth                       = 224;  // The amount of smoothing applied to movements, this tails off at higher speeds
        cfg.movfilter                       = 4 & 0xF;  // The lower 4 bits are the speed response value, higher values reduce lag, but also smoothing

        // These two fields implement a simple filter for reducing jitter, but large values cause the pointer to stick in place before moving.
        cfg.movhysti                        = 6;    // Initial movement hysteresis
        cfg.movhystn                        = 4;    // Next movement hysteresis

        cfg.tchdiup                         = 1;    // Up touch detection integration - the number of cycles before the sensor decides an up event has occurred
        cfg.tchdidown                       = 1;    // Down touch detection integration - the number of cycles before the sensor decides an down event has occurred

        cfg.xrange                          = CPI_TO_SAMPLES(MXT_CPI, MXT_SENSOR_HEIGHT_MM);    // CPI handling, adjust the reported resolution
        cfg.yrange                          = CPI_TO_SAMPLES(MXT_CPI, MXT_SENSOR_WIDTH_MM);     // CPI handling, adjust the reported resolution
    
        status                              = i2c_writeReg16(MXT336UD_ADDRESS, t100_multiple_touch_touchscreen_address,
                                                (uint8_t *)&cfg, sizeof(mxt_touch_multiscreen_t100), MXT_I2C_TIMEOUT_MS);
        if (status != I2C_STATUS_SUCCESS) {
            dprintf("T100 Configuration failed: %d\n", status);
        }
    }
}

digitizer_t maxtouch_get_report(digitizer_t digitizer_report) {
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
                    // Unused for now
                }
#if DIGITIZER_FINGER_COUNT > 0
                else if ((message.report_id >= t100_subsequent_report_ids[0]) &&
                         (message.report_id <= t100_subsequent_report_ids[t100_num_reports-1])) {
                    const uint8_t contact_id    = message.report_id - t100_subsequent_report_ids[0];
                    int event                   = (message.data[0] & 0xf);
                    uint16_t x                  = message.data[1] | (message.data[2] << 8);
                    uint16_t y                  = message.data[3] | (message.data[4] << 8);
                    if (event == DOWN) {
                        digitizer_report.fingers[contact_id].tip     = 1;
                    }
                    if (event == UP || event == UNSUP || event == DOWNUP) {
                        digitizer_report.fingers[contact_id].tip     = 0;
                    }
                    digitizer_report.fingers[contact_id].confidence  = !(event == SUP || event == DOWNSUP);
                    if (event != UP) {
                        digitizer_report.fingers[contact_id].x           = x;
                        digitizer_report.fingers[contact_id].y           = y;
                    }
                }
#endif
                else {
                    uprintf("Unhandled ID: %d (%d..%d) %d\n", message.report_id, t100_subsequent_report_ids[0], t100_subsequent_report_ids[t100_num_reports], t100_subsequent_report_ids[t100_num_reports-1]);
                }
            }
        }
    }

    return digitizer_report;
}
