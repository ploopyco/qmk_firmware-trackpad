// Copyright 2023 Dasky (@daskygit)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "azoteq_iqs5xx.h"
#include "pointing_device_internal.h"
#include "pointing_device.h"
#include "debug.h"
#include <stddef.h>
#define AZOTEQ_IQS5XX_ADDRESS 0x74
#define AZOTEQ_IQS5XX_TIMEOUT_MS 10

#define AZOTEQ_IQS5XX_REG_PRODUCT_NUMBER 0x0000
#define AZOTEQ_IQS5XX_REG_PROJECT_NUMBER 0x0002
#define AZOTEQ_IQS5XX_REG_MAJOR_VERSION 0x0004
#define AZOTEQ_IQS5XX_REG_MINOR_VERSION 0x0005
#define AZOTEQ_IQS5XX_REG_BOOTLOADER_STATUS 0x0006
#define AZOTEQ_IQS5XX_REG_MAX_TOUCH_COL_ROW 0x000B
#define AZOTEQ_IQS5XX_REG_PREVIOUS_CYCLE_TIME 0x000C
#define AZOTEQ_IQS5XX_REG_GESTURE_EVENTS_O 0x000D
#define AZOTEQ_IQS5XX_REG_GESTURE_EVENTS_1 0x000E
#define AZOTEQ_IQS5XX_REG_SYSTEM_INFO_0 0x000F
#define AZOTEQ_IQS5XX_REG_SYSTEM_INFO_1 0x0010

#define AZOTEQ_IQS5XX_REG_NUMBER_OF_FINGERS 0x0011
#define AZOTEQ_IQS5XX_REG_RELATIVE_X 0x0012
#define AZOTEQ_IQS5XX_REG_RELATIVE_Y 0x0014

// Finger 1
#define AZOTEQ_IQS5XX_REG_ABSOLUTE_X 0x0016
#define AZOTEQ_IQS5XX_REG_ABSOLUTE_Y 0x0018
#define AZOTEQ_IQS5XX_REG_TOUCH_STRENGTH 0x001A
#define AZOTEQ_IQS5XX_REG_TOUCH_AREA 0x001C
// Finger 2
#define AZOTEQ_IQS5XX_REG_ABSOLUTE_X_2 0x001D
// Finger 3
#define AZOTEQ_IQS5XX_REG_ABSOLUTE_X_3 0x0024
// Finger 4
#define AZOTEQ_IQS5XX_REG_ABSOLUTE_X_4 0x002B
// Finger 5
#define AZOTEQ_IQS5XX_REG_ABSOLUTE_X_5 0x0032

#define AZOTEQ_IQS5XX_REG_REPORT_RATE_ACTIVE 0x057A
#define AZOTEQ_IQS5XX_REG_REPORT_RATE_IDLE_TOUCH 0x057C
#define AZOTEQ_IQS5XX_REG_REPORT_RATE_IDLE 0x057E
#define AZOTEQ_IQS5XX_REG_REPORT_RATE_LP1 0x0580
#define AZOTEQ_IQS5XX_REG_REPORT_RATE_LP2 0x0582

#define AZOTEQ_IQS5XX_REG_SYSTEM_CONFIG_1 0x058F

#define AZOTEQ_IQS5XX_REG_SINGLE_FINGER_GESTURES 0x06B7
#define AZOTEQ_IQS5XX_REG_MULTI_FINGER_GESTURES 0x06B8

#define AZOTEQ_IQS5XX_REG_ 0x000

#define AZOTEQ_IQS5XX_REG_END_COMMS 0xEEEE

const pointing_device_driver_t     azoteq_iqs5xx_driver_default     = {.init = azoteq_iqs5xx_init, .get_report = azoteq_iqs5xx_get_report, .set_cpi = NULL, .get_cpi = NULL};
const pointing_device_i2c_config_t azoteq_iqs5xx_i2c_config_default = {.address = AZOTEQ_IQS5XX_ADDRESS, .timeout = AZOTEQ_IQS5XX_TIMEOUT_MS};

i2c_status_t azoteq_iqs5xx_get_report_data(const pointing_device_i2c_config_t *i2c_config, azoteq_iqs5xx_report_data_t *report_data) {
        i2c_status_t status = i2c_readReg16(i2c_config->address << 1, AZOTEQ_IQS5XX_REG_NUMBER_OF_FINGERS, (uint8_t *)report_data, sizeof(azoteq_iqs5xx_report_data_t), i2c_config->timeout);
    azoteq_iqs5xx_end_session(i2c_config);
    return status;
}

i2c_status_t azoteq_iqs5xx_get_base_data(const pointing_device_i2c_config_t *i2c_config, azoteq_iqs5xx_base_data_t *base_data) {
    i2c_status_t status = i2c_readReg16(i2c_config->address << 1, AZOTEQ_IQS5XX_REG_PREVIOUS_CYCLE_TIME, (uint8_t *)base_data, sizeof(azoteq_iqs5xx_base_data_t), i2c_config->address);
    azoteq_iqs5xx_end_session(i2c_config);
    return status;
}

i2c_status_t azoteq_iqs5xx_end_session(const pointing_device_i2c_config_t *i2c_config) {
    const uint8_t END_BYTE   = 1; // any data
    i2c_status_t  i2c_status = i2c_writeReg16(i2c_config->address << 1, AZOTEQ_IQS5XX_REG_END_COMMS, &END_BYTE, sizeof(END_BYTE), i2c_config->timeout);
    i2c_stop();
    return i2c_status;
}

i2c_status_t azoteq_iqs5xx_get_report_rate(const pointing_device_i2c_config_t *i2c_config, azoteq_iqs5xx_report_rate_t *report_rate, azoteq_charging_modes_t mode) {
    if (mode > LP2) {
        pd_dprintf("IQS5XX - Invalid mode for get report rate.\n");
        return I2C_STATUS_ERROR;
    }
    uint16_t     selected_reg = AZOTEQ_IQS5XX_REG_REPORT_RATE_ACTIVE + (2 * mode);
    i2c_status_t status       = i2c_readReg16(i2c_config->address << 1, selected_reg, (uint8_t *)report_rate, sizeof(azoteq_iqs5xx_report_rate_t), i2c_config->timeout);
    azoteq_iqs5xx_end_session(i2c_config);
    return status;
}

i2c_status_t azoteq_iqs5xx_set_report_rate(const pointing_device_i2c_config_t *i2c_config, uint16_t report_rate_ms, azoteq_charging_modes_t mode) {
    if (mode > LP2) {
        pd_dprintf("IQS5XX - Invalid mode for set report rate.\n");
        return I2C_STATUS_ERROR;
    }
    uint16_t                    selected_reg = AZOTEQ_IQS5XX_REG_REPORT_RATE_ACTIVE + (2 * mode);
    azoteq_iqs5xx_report_rate_t report_rate  = {0};
    report_rate.h                            = (uint8_t)((report_rate_ms >> 8) & 0xFF);
    report_rate.l                            = (uint8_t)(report_rate_ms & 0xFF);
    i2c_status_t status                      = i2c_writeReg16(i2c_config->address << 1, selected_reg, (uint8_t *)&report_rate, sizeof(azoteq_iqs5xx_report_rate_t), i2c_config->timeout);
    azoteq_iqs5xx_end_session(i2c_config);
    return status;
}

void azoteq_iqs5xx_set_event_mode(const pointing_device_i2c_config_t *i2c_config, bool enabled) {
    azoteq_iqs5xx_system_config_1_t config = {0};
    i2c_status_t                    status = i2c_readReg16(i2c_config->address << 1, AZOTEQ_IQS5XX_REG_SYSTEM_CONFIG_1, (uint8_t *)&config, sizeof(azoteq_iqs5xx_system_config_1_t), i2c_config->timeout);
    if (status == I2C_STATUS_SUCCESS) {
        config.event_mode  = true;
        config.touch_event = true;
        config.tp_event    = true;
        status             = i2c_writeReg16(i2c_config->address << 1, AZOTEQ_IQS5XX_REG_SYSTEM_CONFIG_1, (uint8_t *)&config, sizeof(azoteq_iqs5xx_system_config_1_t), i2c_config->timeout);
    }
    azoteq_iqs5xx_end_session(i2c_config);
}

void azoteq_iqs5xx_gesture_config(const pointing_device_i2c_config_t *i2c_config) {
    azoteq_iqs5xx_gesture_config_t config = {0};
    i2c_status_t                 status = i2c_readReg16(i2c_config->address << 1, AZOTEQ_IQS5XX_REG_SINGLE_FINGER_GESTURES, (uint8_t *)&config, sizeof(azoteq_iqs5xx_gesture_config_t), i2c_config->timeout);
    if (status == I2C_STATUS_SUCCESS) {
        config.single_finger_gestures.single_tap    = true;
        config.multi_finger_gestures.two_finger_tap = true;
        config.multi_finger_gestures.scroll         = true;
        config.tap_time                             = 500;
        config.tap_distance                         = 100;
        config.scroll_initial_distance              = 5;
        status = i2c_writeReg16(i2c_config->address << 1, AZOTEQ_IQS5XX_REG_SINGLE_FINGER_GESTURES, (uint8_t *)&config, sizeof(azoteq_iqs5xx_gesture_config_t), i2c_config->timeout);
    }
    azoteq_iqs5xx_end_session(i2c_config);
}

void azoteq_iqs5xx_init(const void *i2c_config) {
    i2c_init();
    azoteq_iqs5xx_set_report_rate(i2c_config, 5, ACTIVE);
    azoteq_iqs5xx_set_event_mode(i2c_config, true);
    azoteq_iqs5xx_gesture_config(i2c_config);
};

report_mouse_t azoteq_iqs5xx_get_report(const void *i2c_config) {
    report_mouse_t              temp_report = {0};
    azoteq_iqs5xx_base_data_t   report_data = {0};
    i2c_status_t status = azoteq_iqs5xx_get_base_data(i2c_config, &report_data);

    if (status == I2C_STATUS_SUCCESS) {
        if (report_data.gesture_events_0.single_tap) {
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON1);;
        }
        if (report_data.gesture_events_1.two_finger_tap) {
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON2);;
        }
        if (report_data.gesture_events_1.scroll) {
#if defined(MOUSE_EXTENDED_REPORT)
            temp_report.v = (int16_t) AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(base_data.x.h, base_data.x.l);
#else
            temp_report.v = (int8_t) report_data.x.l;
#endif
        }
        else if (report_data.number_of_fingers != 0) {
#if defined(MOUSE_EXTENDED_REPORT)
            temp_report.x = (int16_t)AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(base_data.x.h, base_data.x.l);
            temp_report.y = (int16_t)AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(base_data.y.h, base_data.y.l);
#else
            temp_report.x = (int8_t)report_data.x.l;
            temp_report.y = (int8_t)report_data.y.l;
#endif
        }
    }

    return temp_report;
}
