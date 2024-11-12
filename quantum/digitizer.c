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
#include "action.h"
#if defined(POINTING_DEVICE_DRIVER_digitizer)
#    include "pointing_device.h"
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

#ifndef DIGITIZER_MOUSE_SWIPE_TIME
#    define DIGITIZER_MOUSE_SWIPE_TIME 500
#endif

#ifndef DIGITIZER_MOUSE_SWIPE_DISTANCE
#    define DIGITIZER_MOUSE_SWIPE_DISTANCE 300
#endif

#ifndef DIGITIZER_MOUSE_SWIPE_THRESHOLD
#    define DIGITIZER_MOUSE_SWIPE_THRESHOLD 100
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


#if defined(POINTING_DEVICE_DRIVER_digitizer)
bool                  digitizer_send_mouse_reports = true;
static report_mouse_t mouse_report                 = {};

static report_mouse_t digitizer_get_mouse_report(report_mouse_t _mouse_report);
static uint16_t digitizer_get_cpi(void);
static void digitizer_set_cpi(uint16_t cpi);

const pointing_device_driver_t digitizer_pointing_device_driver = {.init = NULL, .get_report = digitizer_get_mouse_report, .get_cpi = digitizer_get_cpi, .set_cpi = digitizer_set_cpi};
#endif

#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
#    include "drivers/sensors/azoteq_iqs5xx.h"
#    include "wait.h"

extern digitizer_t digitizer_driver_get_report(digitizer_t digitizer_report);

const digitizer_driver_t digitizer_driver = {.init = azoteq_iqs5xx_init, .get_report = digitizer_driver_get_report};
#elif defined(DIGITIZER_DRIVER_maxtouch)
extern void        maxtouch_init(void);
extern digitizer_t maxtouch_get_report(digitizer_t digitizer_report);

const digitizer_driver_t digitizer_driver = {.init = maxtouch_init, .get_report = maxtouch_get_report};
#else
const digitizer_driver_t digitizer_driver = {};
#endif

static digitizer_t digitizer_state = {};
static bool        dirty           = false;

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
#endif // defined(SPLIT_DIGITIZER_ENABLE)

static bool has_digitizer_state_changed(digitizer_t *tmp_state, digitizer_t *old_state) {
    const int cmp = memcmp(tmp_state, old_state, sizeof(digitizer_t));
    return cmp != 0;
}

/**
 * @brief Gets the current digitizer state used by the digitizer task
 *
 * @return digitizer_t
 */
digitizer_t digitizer_get_state(void) {
    return digitizer_state;
}

#if defined(POINTING_DEVICE_DRIVER_digitizer)
/**
 * @brief Gets the current digitizer mouse report, the pointing device feature will send this is we
 * nave fallen back to mouse mode.
 *
 * @return report_mouse_t
 */
static report_mouse_t digitizer_get_mouse_report(report_mouse_t _mouse_report) {
    report_mouse_t report = mouse_report;
    // Retain the button state, but drop any motion.
    memset(&mouse_report, 0, sizeof(report_mouse_t));
    mouse_report.buttons = report.buttons;
    return report;
}

static uint16_t mouse_cpi = 400;

static uint16_t digitizer_get_cpi(void) {
    return mouse_cpi;
}

static void digitizer_set_cpi(uint16_t cpi) {
    mouse_cpi = cpi;
}
#endif

/**
 * @brief Sets digitizer state used by the digitier task
 *
 * @param[in] new_digitizer_state
 */
void digitizer_set_state(digitizer_t new_digitizer_state) {
    dirty |= has_digitizer_state_changed(&digitizer_state, &new_digitizer_state);
    if (dirty) memcpy(&digitizer_state, &new_digitizer_state, sizeof(digitizer_t));
}

/**
 * @brief Keyboard level code pointing device initialisation
 *
 */
__attribute__((weak)) void digitizer_init_kb(void) {}

/**
 * @brief User level code pointing device initialisation
 *
 */
__attribute__((weak)) void digitizer_init_user(void) {}

/**
 * @brief Weak function allowing for user level digitizer state modification
 *
 * Takes digitizer_t struct allowing modification at user level then returns digitizer_t.
 *
 * @param[in] digitizer_state digitizer_t
 * @return digitizer_t
 */
__attribute__((weak)) digitizer_t digitizer_task_user(digitizer_t digitizer_state) {
    return digitizer_state;
}

/**
 * @brief Weak function allowing for keyboard level digitizer state modification
 *
 * Takes digitizer_t struct allowing modification at keyboard level then returns digitizer_t.
 *
 * @param[in] digitizer_state digitizer_t
 * @return digitizer_t
 */
__attribute__((weak)) digitizer_t digitizer_task_kb(digitizer_t digitizer_state) {
    return digitizer_task_user(digitizer_state);
}

void digitizer_init(void) {
#if defined(SPLIT_DIGITIZER_ENABLE)
    if (!(DIGITIZER_THIS_SIDE)) return;
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

    digitizer_init_kb();
    digitizer_init_user();
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

#if defined(POINTING_DEVICE_DRIVER_digitizer)
typedef enum { NO_GESTURE, POSSIBLE_TAP, HOLD, RIGHT_CLICK, MIDDLE_CLICK, SWIPE } gesture_state;

static gesture_state gesture  = NO_GESTURE;
static int           tap_time = 0;

static bool update_gesture_state(void) {
    if (digitizer_send_mouse_reports) {
        if (gesture == POSSIBLE_TAP) {
            const uint32_t duration = timer_elapsed32(tap_time);
            if (duration >= DIGITIZER_MOUSE_TAP_HOLD_TIME) {
                gesture = NO_GESTURE;
                return true;
            }
        }
        if (gesture == RIGHT_CLICK || gesture == MIDDLE_CLICK) {
            gesture = NO_GESTURE;
            return true;
        }
    }
    return false;
}

// We can fallback to reporting as a mouse for hosts which do not implement trackpad support
static void update_mouse_report(report_digitizer_t *report) {
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;
    static bool last_tip = 0;

    // Some state held to perform basic gesture detection
    static int     contact_start_time = 0;
    static int     contact_start_x    = 0;
    static int     contact_start_y    = 0;
    static uint8_t max_contacts       = 0;

    memset(&mouse_report, 0, sizeof(report_mouse_t));
    int contacts      = 0;
    int last_contacts = 0;

    for (int i = 0; i < DIGITIZER_CONTACT_COUNT; i++) {
        if (report->fingers[i].tip) {
            contacts++;
        }
        if (last_tip) {
            last_contacts++;
        }
    }

    const uint16_t x = report->fingers[0].x * (DIGITIZER_RESOLUTION_X/DIGITIZER_WIDTH_MM) / mouse_cpi;
    const uint16_t y = report->fingers[0].y * (DIGITIZER_RESOLUTION_Y/DIGITIZER_HEIGHT_MM)  / mouse_cpi;

    if (last_contacts == 0) {
        if (contacts > 0) {
            contact_start_time = timer_read32();
            contact_start_x    = x;
            contact_start_y    = y;
        }

        if (contacts == 1 && gesture == POSSIBLE_TAP) {
            gesture = HOLD;
        }

        if (gesture == SWIPE) {
            gesture = NO_GESTURE;
        }
        max_contacts = contacts;
    } else {
        max_contacts = MAX(contacts, max_contacts);

        const uint32_t duration   = timer_elapsed32(contact_start_time);

        const int32_t  distance_x = x - contact_start_x;
        const int32_t  distance_y = y - contact_start_y;

        switch (contacts) {
            case 0: {
                if (gesture == HOLD) {
                    gesture = NO_GESTURE;
                }

                // Treat short contacts with little travel as a tap
                if (duration < DIGITIZER_MOUSE_TAP_TIME) {
                    // If we tapped quickly, without moving far, send a tap
                    if (max_contacts == 2) {
                        gesture  = RIGHT_CLICK;
                        tap_time = timer_read32();
                    } else if (max_contacts == 3) {
                        gesture  = MIDDLE_CLICK;
                        tap_time = timer_read32();
                    } else if (abs(distance_x) < DIGITIZER_MOUSE_TAP_DISTANCE && abs(distance_y) < DIGITIZER_MOUSE_TAP_DISTANCE) {
                        // Left click
                        gesture  = POSSIBLE_TAP;
                        tap_time = timer_read32();
                    }
                }
                break;
            }
            case 1:
                if (report->fingers[0].tip && last_tip) {
                    mouse_report.x = x - last_x;
                    mouse_report.y = y - last_y;
                }
                break;
            case 2:
                // Scrolling is too fast, so divide the h/v values.
                if (report->fingers[0].tip && last_tip) {
                    static int carry_h = 0;
                    static int carry_v = 0;
                    const int  h       = x - last_x + carry_h;
                    const int  v       = y - last_y + carry_v;

                    carry_h = h % DIGITIZER_SCROLL_DIVISOR;
                    carry_v = v % DIGITIZER_SCROLL_DIVISOR;

                    mouse_report.h = h / DIGITIZER_SCROLL_DIVISOR;
                    mouse_report.v = v / DIGITIZER_SCROLL_DIVISOR;
                }
                break;
            case 3:
                if (gesture != SWIPE && duration < DIGITIZER_MOUSE_SWIPE_TIME) {
                    if (distance_x > 0 && distance_x > DIGITIZER_MOUSE_SWIPE_DISTANCE && abs(distance_y) < DIGITIZER_MOUSE_SWIPE_THRESHOLD) {
                        // Swipe right
                        mouse_report.buttons |= 0x10;
                        gesture = SWIPE;
                    }
                    if (distance_x < 0 && distance_x < -DIGITIZER_MOUSE_SWIPE_DISTANCE && abs(distance_y) < DIGITIZER_MOUSE_SWIPE_THRESHOLD) {
                        // Swipe left
                        mouse_report.buttons |= 0x8;
                        gesture = SWIPE;
                    }
                    if (distance_y < 0 && distance_y < -DIGITIZER_MOUSE_SWIPE_DISTANCE && abs(distance_x) < DIGITIZER_MOUSE_SWIPE_THRESHOLD) {
                        // Swipe up
                        tap_code(KC_LEFT_GUI);
                        gesture = SWIPE;
                    }
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
    if (report->button3 || gesture == MIDDLE_CLICK) {
        mouse_report.buttons |= 0x4;
    }

    last_x = x;
    last_y = y;
    last_tip = report->fingers[0].tip;
}
#endif

bool digitizer_task(void) {
    static int         last_contacts = 0;
    report_digitizer_t report        = {.fingers = {}, .contact_count = 0, .scan_time = 0, .button1 = digitizer_state.button1, .button2 = digitizer_state.button2, .button3 = digitizer_state.button3};
#if defined(DIGITIZER_HAS_STYLUS)
    report_digitizer_stylus_t stylus_report  = {};
    bool                      updated_stylus = false;
#endif
    int  contacts             = 0;
    bool gesture_changed      = false;
    bool button_state_changed = false;

#if DIGITIZER_TASK_THROTTLE_MS
    static uint32_t last_exec = 0;
    if (timer_elapsed32(last_exec) < DIGITIZER_TASK_THROTTLE_MS) {
        return false;
    }
    last_exec = timer_read32();
#endif
#if defined(POINTING_DEVICE_DRIVER_digitizer)
    gesture_changed = update_gesture_state();
#endif

#if defined(DIGITIZER_MOTION_PIN)
    if (gesture_changed || digitizer_motion_detected())
#endif
    {
#if defined(SPLIT_DIGITIZER_ENABLE)
#    if defined(DIGITIZER_LEFT) || defined(DIGITIZER_RIGHT)
        digitizer_t driver_state = DIGITIZER_THIS_SIDE ? (digitizer_driver.get_report ? digitizer_driver.get_report(digitizer_state) : digitizer_state) : shared_digitizer_report;
#    else
#        error "You need to define the side(s) the digitizer is on. DIGITIZER_LEFT / DIGITIZER_RIGHT"
#    endif
#else
        digitizer_t driver_state = digitizer_driver.get_report ? digitizer_driver.get_report(digitizer_state) : digitizer_state;
#endif
        // Handle user modification of stylus state. We explicity do not store the user modified
        // state so we do not pass them back state that they have previously transformed.
        digitizer_t tmp_state = digitizer_task_kb(driver_state);

        if (digitizer_state.buttons != tmp_state.buttons) {
            button_state_changed = true;
        }

        int skip_count = 0;
        for (int i = 0; i < DIGITIZER_CONTACT_COUNT; i++) {
            // If this is a finger which is down, or it was on the last scan (but now it is up)..
            const bool    finger_contact = (tmp_state.contacts[i].type == FINGER && tmp_state.contacts[i].tip) ||
                                            (digitizer_state.contacts[i].type == FINGER && digitizer_state.contacts[i].tip);
            const uint8_t finger_index   = finger_contact ? report.contact_count : DIGITIZER_CONTACT_COUNT - skip_count - 1;

            if (tmp_state.contacts[i].type != UNKNOWN) {
                // 'contacts' is the number of current contacts wheras 'report->contact_count' also counts fingers which have
                // been removed from the sensor since the last report.
                contacts++;
            }
            if (finger_contact) {
                report.fingers[finger_index].tip = tmp_state.contacts[i].tip;
                report.contact_count++;
            } else {
                skip_count++;
                report.fingers[finger_index].tip = false;
            }
            report.fingers[finger_index].contact_id = i;
            report.fingers[finger_index].x          = tmp_state.contacts[i].x;
            report.fingers[finger_index].y          = tmp_state.contacts[i].y;
            report.fingers[finger_index].confidence = tmp_state.contacts[i].confidence;
#ifdef DIGITIZER_HAS_STYLUS
            if (tmp_state.contacts[i].type == STYLUS) {
                updated_stylus         = true;
                stylus_report.x        = tmp_state.contacts[i].x;
                stylus_report.y        = tmp_state.contacts[i].y;
                stylus_report.tip      = tmp_state.contacts[i].tip;
                stylus_report.in_range = tmp_state.contacts[i].in_range;
            } else if (digitizer_state.contacts[i].type == STYLUS) {
                // Drop the tip, then drop out of range next scan
                updated_stylus         = true;
                stylus_report.x        = digitizer_state.contacts[i].x;
                stylus_report.y        = digitizer_state.contacts[i].y;
                stylus_report.in_range = false;
                stylus_report.tip      = false;
            }
#endif
        }
        digitizer_state = driver_state;
    }

#if DIGITIZER_CONTACT_COUNT > 0
    static uint32_t scan_time = 0;

    // Reset the scan_time after a period of inactivity (1000ms with no contacts)
    static uint32_t inactivity_timer = 0;
    if (last_contacts == 0 && contacts && timer_elapsed32(inactivity_timer) > 1000) {
        scan_time = timer_read32();
    }
    inactivity_timer = timer_read32();
    last_contacts    = contacts;

    // Microsoft require we report in 100us ticks. TODO: Move.
    uint32_t scan    = timer_elapsed32(scan_time);
    report.scan_time = scan * 10;
#endif

#ifdef DIGITIZER_HAS_STYLUS
    if (updated_stylus) {
        host_digitizer_stylus_send(&stylus_report);
    }
#endif
    if (report.contact_count || button_state_changed || gesture_changed) {
#if defined(POINTING_DEVICE_DRIVER_digitizer)
        if (digitizer_send_mouse_reports) {
            update_mouse_report(&report);
#    if !defined(POINTING_DEVICE_ENABLE)
            host_mouse_send(&mouse_report);
#    endif
        }
        else 
#endif
        {
            host_digitizer_send(&report);
        }
    }

#ifdef DIGITIZER_HAS_STYLUS
    return report.contact_count > 0 || button_state_changed || updated_stylus;
#else
    return report.contact_count > 0 || button_state_changed;
#endif
}
