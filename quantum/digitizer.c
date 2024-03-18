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
#include <stdlib.h>
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

#ifndef DIGITIZER_MOUSE_TAP_TIME
#    define DIGITIZER_MOUSE_TAP_TIME 200
#endif

#ifndef DIGITIZER_MOUSE_TAP_HOLD_TIME
#    define DIGITIZER_MOUSE_TAP_HOLD_TIME 300
#endif

#ifndef DIGITIZER_MOUSE_TAP_DISTANCE
#    define DIGITIZER_MOUSE_TAP_DISTANCE 15
#endif

#ifndef DIGITIZER_SCROLL_DIVISOR
#    define DIGITIZER_SCROLL_DIVISOR 4
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

bool digitizer_send_mouse_reports = true;
static report_mouse_t mouse_report = {};

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
    extern void maxtouch_init(void);
    extern digitizer_t maxtouch_get_report(digitizer_t digitizer_report);

    const digitizer_driver_t digitizer_driver = {
        .init = maxtouch_init,
        .get_report = maxtouch_get_report
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
 * @return digitizer_t
 */
digitizer_t digitizer_get_report(void) {
    return digitizer_state;
}

/**
 * @brief Gets the current digitizer mouse report, the pointing device feature will send this is we
 * nave fallen back to mouse mode.
 *
 * @return report_mouse_t
 */
report_mouse_t digitizer_get_mouse_report(report_mouse_t _mouse_report) {
    report_mouse_t report = mouse_report;
    // Retain the button state, but drop any motion.
    memset(&mouse_report, 0, sizeof(report_mouse_t));
    mouse_report.buttons = report.buttons;
    return report;
}

/**
 * @brief Sets digitizer report used by the digitier task
 *
 * @param[in] digitizer_report
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
#if defined(SPLIT_DIGITIZER_ENABLE)
    if (!(DIGITIZER_THIS_SIDE))
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

typedef enum {
    NO_GESTURE,
    POSSIBLE_TAP,
    HOLD,
    RIGHT_CLICK
} gesture_state;


static gesture_state gesture = NO_GESTURE;
static int tap_time = 0;

static bool update_gesture_state(void) {
    if (digitizer_send_mouse_reports) {
        if (gesture == POSSIBLE_TAP) {
            const uint32_t duration = timer_elapsed32(tap_time);
            if (duration >= DIGITIZER_MOUSE_TAP_HOLD_TIME) {
                gesture = NO_GESTURE;
                return true;
            }
        }
        if (gesture == RIGHT_CLICK) {
            gesture = NO_GESTURE;
            return true;
        }
    }
    return false;
}

// We can fallback to reporting as a mouse for hosts which do not implement trackpad support
static void update_mouse_report(report_digitizer_t* report) {
    static report_digitizer_t last_report = {};

    // Some state held to perform basic gesture detection
    static int contact_start_time = 0;
    static int contact_start_x = 0;
    static int contact_start_y = 0;
    static uint8_t max_contacts = 0;

    memset(&mouse_report, 0, sizeof(report_mouse_t));
    int contacts = 0;
    int last_contacts = 0;

    for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
        if (report->fingers[i].tip) {
            contacts ++;
        }
        if (last_report.fingers[i].tip) {
            last_contacts ++;
        }
    }

    if (last_contacts == 0) {
        max_contacts = 0;

        if (contacts > 0) {
            contact_start_time = timer_read32();
            contact_start_x = report->fingers[0].x;
            contact_start_y = report->fingers[0].y;
        }

        if (gesture == POSSIBLE_TAP) {
            gesture = HOLD;
        }
    }
    else
    {
        max_contacts = MAX(contacts, max_contacts);
        switch (contacts) {
            case 0: {
                // Treat short contacts with little travel as a tap
                const uint32_t duration = timer_elapsed32(contact_start_time);
                const uint32_t distance_x = abs(report->fingers[0].x - contact_start_x);
                const uint32_t distance_y = abs(report->fingers[0].y - contact_start_y);

                if (gesture == HOLD) {
                    gesture = NO_GESTURE;
                }

                if (duration < DIGITIZER_MOUSE_TAP_TIME) {
                    // If we tapped quickly, without moving far, send a tap
                    if (max_contacts == 2) {
                        // Right click
			            gesture = RIGHT_CLICK;
                        tap_time = timer_read32();
                    }
                    else if (distance_x < DIGITIZER_MOUSE_TAP_DISTANCE && distance_y < DIGITIZER_MOUSE_TAP_DISTANCE) {
                        // Left click
                        gesture = POSSIBLE_TAP;
                        mouse_report.buttons |= 0x1;
                        tap_time = timer_read32();
                    }
                }
                break;
            }
            case 1:
                if (report->fingers[0].tip && last_report.fingers[0].tip) {
                    mouse_report.x = report->fingers[0].x - last_report.fingers[0].x;
                    mouse_report.y = report->fingers[0].y - last_report.fingers[0].y;
                }
                break;
            case 2:
                // Scrolling is too fast, so divide the h/v values.
                if (report->fingers[0].tip && last_report.fingers[0].tip) {
                    static int carry_h  = 0;
                    static int carry_v  = 0;
                    const int h         = report->fingers[0].x - last_report.fingers[0].x + carry_h;
                    const int v         = report->fingers[0].y - last_report.fingers[0].y + carry_v;

                    carry_h             = h % DIGITIZER_SCROLL_DIVISOR;
                    carry_v             = v % DIGITIZER_SCROLL_DIVISOR;

                    mouse_report.h      = h / DIGITIZER_SCROLL_DIVISOR;
                    mouse_report.v      = v / DIGITIZER_SCROLL_DIVISOR;
                }
                break;
            default:
                break;
                // Do nothing
        }
    }
    if (report->button1 || (max_contacts == 1 && (gesture == HOLD || gesture == POSSIBLE_TAP))) {
        mouse_report.buttons |= 0x1;
    }
    if (report->button2 || gesture == RIGHT_CLICK) {
        mouse_report.buttons |= 0x2;
    }
    if (report->button3) {
        mouse_report.buttons |= 0x4;
    }

    last_report = *report;
}

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
        const bool process_one_more_event = update_gesture_state();
        if (process_one_more_event || digitizer_motion_detected())
#else
	update_gesture_state();
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
            int contacts = 0;
            for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
                const bool contact = new_state.fingers[i].tip || (digitizer_state.fingers[i].tip != new_state.fingers[i].tip);
                // 'contacts' is the number of current contacts wheras 'report->contact_count' also counts fingers which have
                // been removed from the sensor since the last report.
                if (new_state.fingers[i].tip) {
                    contacts++;
                }
                if (contact) {
                    memcpy(&report.fingers[report.contact_count], &new_state.fingers[i], sizeof(digitizer_finger_report_t));
                    report.contact_count ++;
                }
                else {
                    report.fingers[DIGITIZER_FINGER_COUNT - skip_count - 1].contact_id = i;
                    skip_count ++;
                }
            }
            digitizer_state = new_state;
            updated_report = true;

#if DIGITIZER_FINGER_COUNT > 0
            uint32_t scan_time = 0;

            // Reset the scan_time after a period of inactivity (1000ms with no contacts)
            static uint32_t inactivity_timer = 0;
            if (last_contacts == 0 && contacts && timer_elapsed32(inactivity_timer) > 1000) {
                scan_time = timer_read32();
            }
            inactivity_timer = timer_read32();
            last_contacts = contacts;

            // Microsoft require we report in 100us ticks. TODO: Move.
            uint32_t scan = timer_elapsed32(scan_time);
            report.scan_time = scan * 10;
#endif
        }
    }

#if defined(MOUSEKEY_ENABLE) && !defined(POINTING_DEVICE_ENABLE)
    // Pointing device has a more fully featured mousekeys implementation,
    // so we prefer it if pointing device is enabled.
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

    updated_report |= button_state_changed;
#endif

    if (updated_report) {
        if (digitizer_send_mouse_reports) {
            update_mouse_report(&report);
#if !defined(POINTING_DEVICE_ENABLE)
            host_mouse_send(&mouse_report);
#endif
        }
        else {
            host_digitizer_send(&report);
        }
    }

    return updated_report;
}
