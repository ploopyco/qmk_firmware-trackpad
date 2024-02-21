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
#include "keyboard.h"
#ifdef MOUSEKEY_ENABLE
#    include "mousekey.h"
#endif

#ifdef DIGITIZER_MOTION_PIN
#    undef DIGITIZER_TASK_THROTTLE_MS
#endif

#if defined(DIGITIZER_LEFT) || defined(DIGITIZER_RIGHT)
#    ifndef SPLIT_DIGITIZER_ENABLE
#        error "Using DIGITIZER_LEFT or DIGITIZER_RIGHT, then SPLIT_DIGITIZER_ENABLE is required but has not been defined"
#    endif
#endif

typedef struct {
    void (*init)(void);
    digitizer_t (*get_report)(digitizer_t digitizer_report);
} digitizer_driver_t;

#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
#include "drivers/sensors/azoteq_iqs5xx.h"
#include "wait.h"

static i2c_status_t azoteq_iqs5xx_init_status = 1;
    void azoteq_iqs5xx_init(void) {
        i2c_init();
        azoteq_iqs5xx_wake();
        azoteq_iqs5xx_reset_suspend(true, false, true);
        wait_ms(100);
        azoteq_iqs5xx_wake();
        if (azoteq_iqs5xx_get_product() != AZOTEQ_IQS5XX_UNKNOWN) {
            azoteq_iqs5xx_setup_resolution();
            azoteq_iqs5xx_init_status = azoteq_iqs5xx_set_report_rate(AZOTEQ_IQS5XX_REPORT_RATE, AZOTEQ_IQS5XX_ACTIVE, false);
            azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_event_mode(false, false);
            azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_reati(true, false);
    #    if defined(AZOTEQ_IQS5XX_ROTATION_90)
            azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_xy_config(false, true, true, true, false);
    #    elif defined(AZOTEQ_IQS5XX_ROTATION_180)
            azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_xy_config(true, true, false, true, false);
    #    elif defined(AZOTEQ_IQS5XX_ROTATION_270)
            azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_xy_config(true, false, true, true, false);
    #    else
            azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_xy_config(false, false, false, true, false);
    #    endif
            azoteq_iqs5xx_init_status |= azoteq_iqs5xx_set_gesture_config(true);
            wait_ms(AZOTEQ_IQS5XX_REPORT_RATE + 1);
        }
    };
    extern digitizer_t digitizer_driver_get_report(digitizer_t digitizer_report);

    const digitizer_driver_t digitizer_driver = {
        .init = azoteq_iqs5xx_init,
        .get_report = digitizer_driver_get_report
    };
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

#if defined(SPLIT_DIGITIZER_ENABLE)

#    if defined(DIGITIZER_LEFT)
#        define DIGITIZER_THIS_SIDE is_keyboard_left()
#    elif defined(DIGITIZER_RIGHT)
#        define DIGITIZER_THIS_SIDE !is_keyboard_left()
#    endif

digitizer_t shared_digitizer_report = {};

/**
 * @brief Sets the shared digitizer report used by digitizer device task
 *
 * NOTE : Only available when using SPLIT_DIGITIZER_ENABLE
 *
 * @param[in] report digitizer_t
 */
void digitizer_set_shared_report(digitizer_t report) {
    shared_digitizer_report = report;
}
#endif     // defined(SPLIT_DIGITIZER_ENABLE)

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
    int cmp = 0;
    if (new_report != NULL && old_report != NULL) {
#if DIGITIZER_STYLUS
        cmp |= memcmp(&(new_report->stylus), &(old_report->stylus), sizeof(digitizer_stylus_report_t));
#endif
#if DIGITIZER_FINGER_COUNT > 0
        cmp |= memcmp(new_report->fingers, old_report->fingers, sizeof(digitizer_finger_report_t) * DIGITIZER_FINGER_COUNT);
#endif
    }
    return cmp != 0;
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
#if DIGITIZER_STYLUS
    memcpy(&digitizer_state.stylus, &digitizer_report.stylus, sizeof(digitizer_stylus_report_t));
#endif
#if DIGITIZER_FINGER_COUNT > 0
    memcpy(digitizer_state.fingers, digitizer_report.fingers, sizeof(digitizer_finger_report_t) * DIGITIZER_FINGER_COUNT);
#endif
}

void digitizer_init(void) {
#if defined(SPLIT_POINTING_ENABLE)
    if (!(POINTING_DEVICE_THIS_SIDE))
        return;
#endif
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
    bool updated_report = false;
    static int last_contacts = 0;
    report_digitizer_t report = { .fingers = {}, .contact_count = 0, .scan_time = 0, .button1 = digitizer_state.button1, .button2 = digitizer_state.button2, .button3 = digitizer_state.button3 };
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

#if defined(SPLIT_DIGITIZER_ENABLE)
#    if defined(DIGITIZER_LEFT) || defined(DIGITIZER_RIGHT)
            digitizer_t new_state = DIGITIZER_THIS_SIDE ? digitizer_driver.get_report(digitizer_state) : shared_digitizer_report;
#    else
#        error "You need to define the side(s) the digitizer is on. DIGITIZER_LEFT / DIGITIZER_RIGHT"
#    endif
#else
            digitizer_t new_state = digitizer_driver.get_report(digitizer_state);
#endif
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
            digitizer_state = new_state;
            updated_report = true;

#if DIGITIZER_FINGER_COUNT > 0
            uint32_t scan_time = 0;

            // Reset the scan_time after a period of inactivity - for now any time when there are no contacts
            // is treated as a period of inactivity - but we should consider waiting a little longer.
            if (last_contacts == 0 && report.contact_count) {
                scan_time = timer_read32();
            }
            last_contacts = report.contact_count;

            // Microsoft require we report in 100us ticks. TODO: Move.
            uint32_t scan = timer_elapsed32(scan_time);
            report.scan_time = scan * 10;
#endif
        }
    }

#ifdef MOUSEKEY_ENABLE
    const report_mouse_t mousekey_report = mousekey_get_report();
    const bool button1 = !!(mousekey_report.buttons & 0x1);
    const bool button2 = !!(mousekey_report.buttons & 0x2);
    const bool button3 = !!(mousekey_report.buttons & 0x4);
    bool button_state_changed = false;

    if (digitizer_state.button1 != button1) {
        digitizer_state.button1 = report.button1 = button1;
        button_state_changed = true;
    }
    if (digitizer_state.button2 != button2) {
        digitizer_state.button2 = report.button2 = button2;
        button_state_changed = true;
    }
    if (digitizer_state.button3 != button3) {
        digitizer_state.button3 = report.button3 = button3;
        button_state_changed = true;
    }

    // Always send some sort of finger state along with the changed buttons
    if (!updated_report && button_state_changed) {
        memcpy(report.fingers, digitizer_state.fingers, sizeof(digitizer_finger_report_t) * DIGITIZER_FINGER_COUNT);
        report.contact_count = last_contacts;
    }
#endif

    if (updated_report || button_state_changed) {
        host_digitizer_send(&report);
    }

    return false;
}
