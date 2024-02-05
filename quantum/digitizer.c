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
    digitizer_t (*get_report)(digitizer_t digitizer_report);
} digitizer_driver_t;

#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
    // TODO
#elif defined(DIGITIZER_DRIVER_maxtouch)
    extern void pointing_device_driver_init(void);
    extern digitizer_t digitizer_driver_get_report(digitizer_t digitizer_report);

    const digitizer_driver_t digitizer_driver = {
        .init = pointing_device_driver_init,
        .get_report = digitizer_driver_get_report
    };
#else
    const digitizer_driver_t digitizer_driver = {};
#endif

static digitizer_t digitizer_state = {};
static bool dirty = false;


#if DIGITIZER_HAS_STYLUS
void digitizer_flush(void) {
    if (dirty) {
        digitizer_report_t report = { .stylus = digitizer_state.stylus };
        host_digitizer_send(&report);
        dirty = false;
    }
}

void digitizer_in_range_on(void) {
    digitizer_state.stylus.in_range = true;
    dirty    = true;
    digitizer_flush();
}

void digitizer_in_range_off(void) {
    digitizer_state.stylus.in_range = false;
    dirty    = true;
    digitizer_flush();
}

void digitizer_tip_switch_on(void) {
    digitizer_state.stylus.tip   = true;
    dirty = true;
    digitizer_flush();
}

void digitizer_tip_switch_off(void) {
    digitizer_state.stylus.tip   = false;
    dirty = true;
    digitizer_flush();
}

void digitizer_barrel_switch_on(void) {
    digitizer_state.stylus.barrel = true;
    dirty  = true;
    digitizer_flush();
}

void digitizer_barrel_switch_off(void) {
    digitizer_state.stylus.barrel = false;
    dirty  = true;
    digitizer_flush();
}

void digitizer_set_position(float x, float y) {
    digitizer_state.stylus.x    = x;
    digitizer_state.stylus.y    = y;
    dirty       = true;
    digitizer_flush();
}
#endif

static bool has_digitizer_report_changed(digitizer_t *new_report, digitizer_t *old_report) {
    return memcmp(new_report, old_report, sizeof(digitizer_t)) != 0;
}

/**
 * @brief Gets the current digitizer report used by the digitizer task
 *
 * @return report_mouse_t
 */
digitizer_t digitizer_get_report(void) {
    return digitizer_state;
}

/**
 * @brief Sets digitizer report used by the digitier task
 *
 * @param[in] mouse_report
 */
void digitizer_set_report(digitizer_t digitizer_report) {
    dirty |= has_digitizer_report_changed(&digitizer_state, &digitizer_report);
    memcpy(&digitizer_state, &digitizer_report, sizeof(report_digitizer_t));
}

void digitizer_init(void) {
#if DIGITIZER_FINGER_COUNT > 0
    // Set unique contact_ids for each finger
    for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
        digitizer_state.fingers[i].contact_id  = i;
    }
#endif
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
            digitizer_t new_state = digitizer_driver.get_report(digitizer_state);
            report_digitizer_t report = { .fingers = {}, .contact_count = 0, .scan_time = new_state.scan_time, .button1 = new_state.button1, .button2 = new_state.button2, .button3 = new_state.button3 };
            int skip_count = 0;
            for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
                const bool contact = new_state.fingers[i].tip || (digitizer_state.fingers[i].tip != new_state.fingers[i].tip);
                if (contact) {
                    memcpy(&report.fingers[report.contact_count], &new_state.fingers[i], sizeof(digitizer_finger_report_t));
                    report.contact_count ++;
                }
                else {
                    report.fingers[DIGITIZER_FINGER_COUNT - skip_count - 1].contact_id = i;
                    skip_count ++;
                }
            }
            report.contact_count = DIGITIZER_FINGER_COUNT;
            host_digitizer_send(&report);
            digitizer_state = new_state;
        }
    }

    return false;
}