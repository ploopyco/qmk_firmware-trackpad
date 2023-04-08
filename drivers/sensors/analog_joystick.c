/* Copyright 2020 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "analog_joystick.h"
#include "analog.h"
#include "gpio.h"
#include "wait.h"
#include "timer.h"
#include <stdlib.h>

int16_t xOrigin, yOrigin;

const pointing_device_driver_t analog_joystick_driver_default = {.init = analog_joystick_init, .get_report = analog_joystick_get_report, .get_cpi = NULL, .set_cpi = NULL};

#if defined(ANALOG_JOYSTICK_X_AXIS_PIN) & defined(ANALOG_JOYSTICK_Y_AXIS_PIN)
const analog_joystick_config_t analog_joystick_config_default = {.x = ANALOG_JOYSTICK_X_AXIS_PIN, .y = ANALOG_JOYSTICK_Y_AXIS_PIN, .axis_min = ANALOG_JOYSTICK_AXIS_MIN, .axis_max = ANALOG_JOYSTICK_AXIS_MAX, .speed_regulator = ANALOG_JOYSTICK_SPEED_REGULATOR, .speed_max = ANALOG_JOYSTICK_SPEED_MAX, .button = ANALOG_JOYSTICK_CLICK_PIN};
#endif

int16_t axisCoordinate(analog_joystick_config_t *analog_config, pin_t pin, uint16_t origin) {
    int8_t  direction;
    int16_t distanceFromOrigin;
    int16_t range;

    int16_t position = analogReadPin(pin);

    if (origin == position) {
        return 0;
    } else if (origin > position) {
        distanceFromOrigin = origin - position;
        range              = origin - analog_config->axis_min;
        direction          = -1;
    } else {
        distanceFromOrigin = position - origin;
        range              = analog_config->axis_max - origin;
        direction          = 1;
    }

    float   percent    = (float)distanceFromOrigin / range;
    int16_t coordinate = (int16_t)(percent * 100);
    if (coordinate < 0) {
        return 0;
    } else if (coordinate > 100) {
        return 100 * direction;
    } else {
        return coordinate * direction;
    }
}

int8_t axisToMouseComponent(analog_joystick_config_t *analog_config, pin_t pin, int16_t origin) {
    int16_t coordinate = axisCoordinate(analog_config, pin, origin);
    if (coordinate != 0) {
        float percent = (float)coordinate / 100;
        return percent * analog_config->speed_max * (abs(coordinate) / analog_config->speed_regulator);
    } else {
        return 0;
    }
}

report_analog_joystick_t analog_joystick_read(analog_joystick_config_t *analog_config) {
    report_analog_joystick_t report = {0};

    report.x = axisToMouseComponent(analog_config, analog_config->x, xOrigin);
    report.y = axisToMouseComponent(analog_config, analog_config->y, yOrigin);

    if (analog_config->button != NO_PIN) {
        report.button = !readPin(analog_config->button);
    }
    return report;
}

void analog_joystick_init(const void *config) {
    analog_joystick_config_t *analog_config = (analog_joystick_config_t *)config;
    if (analog_config->button != NO_PIN) {
        setPinInputHigh(analog_config->button);
    }
    // Account for drift
    xOrigin = analogReadPin(analog_config->x);
    yOrigin = analogReadPin(analog_config->y);
}

report_mouse_t analog_joystick_get_report(const void *config) {
    analog_joystick_config_t *analog_config = (analog_joystick_config_t *)config;
    report_analog_joystick_t  data          = analog_joystick_read(analog_config);

    report_mouse_t mouse_report = {0};

    pd_dprintf("Raw ] X: %d, Y: %d\n", data.x, data.y);

    mouse_report.x = data.x;
    mouse_report.y = data.y;

    mouse_report.buttons = pointing_device_handle_buttons(mouse_report.buttons, data.button, POINTING_DEVICE_BUTTON1);

    return mouse_report;
}