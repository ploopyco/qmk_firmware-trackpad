/* Copyright 2021
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

#include "digitizer.h"
#include "debug.h"
#include "host.h"
#include "timer.h"
#include "gpio.h"

#ifdef DIGITIZER_MOTION_PIN
#    undef DIGITIZER_TASK_THROTTLE_MS
#endif


typedef struct {
    void (*init)(void);
    report_digitizer_t (*get_report)(report_digitizer_t digitizer_report);
} digitizer_driver_t;



#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
    // TODO
#elif defined(DIGITIZER_DRIVER_maxtouch)
    extern void pointing_device_driver_init(void);
    extern report_digitizer_t digitizer_driver_get_report(report_digitizer_t digitizer_report);

    const digitizer_driver_t digitizer_driver = {
        .init = pointing_device_driver_init,
        .get_report = digitizer_driver_get_report
    };
#else
    const digitizer_driver_t digitizer_driver = {};
#endif

typedef struct {
    report_digitizer_t  report;
    bool                dirty;
} digitizer_t;

digitizer_t digitizer_state = {
    .report   = {},
    .dirty    = false
};

void digitizer_flush(void) {
    if (digitizer_state.dirty) {
        host_digitizer_send(&digitizer_state.report);
        digitizer_state.dirty = false;
    }
}

#if DIGITIZER_HAS_STYLUS
void digitizer_in_range_on(void) {
    digitizer_state.report.in_range = true;
    digitizer_state.dirty    = true;
    digitizer_flush();
}

void digitizer_in_range_off(void) {
    digitizer_state.report.in_range = false;
    digitizer_state.dirty    = true;
    digitizer_flush();
}

void digitizer_tip_switch_on(void) {
    digitizer_state.report.tip   = true;
    digitizer_state.dirty = true;
    digitizer_flush();
}

void digitizer_tip_switch_off(void) {
    digitizer_state.report.tip   = false;
    digitizer_state.dirty = true;
    digitizer_flush();
}

void digitizer_barrel_switch_on(void) {
    digitizer_state.report.barrel = true;
    digitizer_state.dirty  = true;
    digitizer_flush();
}

void digitizer_barrel_switch_off(void) {
    digitizer_state.report.barrel = false;
    digitizer_state.dirty  = true;
    digitizer_flush();
}

void digitizer_set_position(float x, float y) {
    digitizer_state.report.x    = x;
    digitizer_state.report.y    = y;
    digitizer_state.dirty       = true;
    digitizer_flush();
}
#endif

/**
 * @brief Adjust digitizer report by any optional common digitizer configuration defines
 *
 * This applies rotation or inversion to the digitizer report as selected by the digitizer common configuration defines.
 *
 * @param report_digitizer_t[in] takes a report_digitizer_t to be adjusted
 * @return report_digitizer_t with adjusted values
 */
static report_digitizer_t digitizer_adjust_by_defines(report_digitizer_t digitizer_report) {
    // Support rotation of the sensor data
#if defined(DIGITIZER_ROTATION_90) || defined(DIGITIZER_ROTATION_180) || defined(DIGITIZER_ROTATION_270)
    const report_digitizer_t original_report = digitizer_report;
#    if defined(DIGITIZER_ROTATION_90)
#        if DIGITIZER_HAS_STYLUS
    digitizer_report.x = original_report.y;
    digitizer_report.y = - original_report.x;
#        endif
#        if DIGITIZER_FINGER_COUNT > 0
    for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
        digitizer_report.fingers[i].x = original_report.fingers[i].y;
        digitizer_report.fingers[i].y = - original_report.fingers[i].x;
    }
#        endif
#    elif defined(DIGITIZER_ROTATION_180)
#        if DIGITIZER_HAS_STYLUS
    digitizer_report.x = - original_report.x;
    digitizer_report.y = - original_report.y;
#        endif
#        if DIGITIZER_FINGER_COUNT > 0
    for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
        digitizer_report.fingers[i].x = - original_report.fingers[i].x;
        digitizer_report.fingers[i].y = - original_report.fingers[i].y;
    }
#        endif
#    elif defined(DIGITIZER_ROTATION_270)
#        if DIGITIZER_HAS_STYLUS
    digitizer_report.x = - original_report.y;
    digitizer_report.y = original_report.x;
#        endif
#        if DIGITIZER_FINGER_COUNT > 0
    for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
        digitizer_report.fingers[i].x = - original_report.fingers[i].y;
        digitizer_report.fingers[i].y = original_report.fingers[i].x;
    }
#        endif
#    else
#        error "How the heck did you get here?!"
#    endif
#endif
    // Support Inverting the X and Y Axises
#if defined(DIGITIZER_INVERT_X)
#    if DIGITIZER_HAS_STYLUS
    digitizer_report.x = - digitizer_report.x;
#    endif
#    if DIGITIZER_FINGER_COUNT > 0
    for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
        digitizer_report.fingers[i].x = - digitizer_report.fingers[i].x;
    }
#    endif
#endif
#if defined(DIGITIZER_INVERT_Y)
#    if DIGITIZER_HAS_STYLUS
    digitizer_report.y = - digitizer_report.y;
#    endif
#    if DIGITIZER_FINGER_COUNT > 0
    for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
        digitizer_report.fingers[i].y = - digitizer_report.fingers[i].y;
    }
#    endif
#endif
    return digitizer_report;
}

static bool has_digitizer_report_changed(report_digitizer_t *new_report, report_digitizer_t *old_report) {
    return memcmp(new_report, old_report, sizeof(report_digitizer_t)) != 0;
}

/**
 * @brief Gets current mouse report used by pointing device task
 *
 * @return report_mouse_t
 */
report_digitizer_t digitizer_get_report(void) {
    return digitizer_state.report;
}

/**
 * @brief Sets mouse report used be pointing device task
 *
 * @param[in] mouse_report
 */
void digitizer_set_report(report_digitizer_t digitizer_report) {
    digitizer_state.dirty = has_digitizer_report_changed(&digitizer_state.report, &digitizer_report);
    memcpy(&digitizer_state.report, &digitizer_report, sizeof(report_digitizer_t));
}

void digitizer_init(void) {
    // Set unique contact_ids for each finger
    for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
        digitizer_state.report.fingers[i].contact_id  = i;
    }
    if (digitizer_driver.init) {
        digitizer_driver.init();
    }
#ifdef DIGITIZER_MOTION_PIN
#    ifdef DIGITIZER_MOTION_PIN_ACTIVE_LOW
        setPinInputHigh(DIGITIZER_MOTION_PIN);
#    else
        setPinInput(DIGITIZER_MOTION_PIN);
#    endif
#endif
}

#ifdef DIGITIZER_MOTION_PIN
__attribute__((weak)) bool digitizer_motion_detected(void) { 
#    ifdef DIGITIZER_MOTION_PIN_ACTIVE_LOW
    return !readPin(DIGITIZER_MOTION_PIN);
#    else
    return readPin(DIGITIZER_MOTION_PIN);
#    endif
}
#endif

bool digitizer_task(void) {

#if DIGITIZER_TASK_THROTTLE_MS
    static uint32_t last_exec = 0;
    if (timer_elapsed32(last_exec) < DIGITIZER_TASK_THROTTLE_MS) {
        return false;
    }
    last_exec = timer_read32();
#endif
    if (digitizer_driver.get_report) {
#ifdef DIGITIZER_MOTION_PIN
        if (digitizer_motion_detected())
#endif
        {
            uprintf("Motion detected.. %lu %d\n", readPin(DIGITIZER_MOTION_PIN), DIGITIZER_MOTION_PIN);
            report_digitizer_t new_report = digitizer_driver.get_report(digitizer_state.report);
            if (has_digitizer_report_changed(&new_report, &digitizer_state.report)) {
                digitizer_state.report = new_report;
                digitizer_state.dirty = true;
            }
        }
    }
    if (digitizer_state.dirty) {
        report_digitizer_t transformed_report = digitizer_adjust_by_defines(digitizer_state.report);
        host_digitizer_send(&transformed_report);
        digitizer_state.dirty = false;
        return true;
    }
    return false;
}