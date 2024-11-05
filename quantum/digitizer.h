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

#pragma once

#include <stdbool.h>
#include "report.h"

/**
 * \file
 *
 * defgroup digitizer HID Digitizer
 * \{
 */
typedef enum { UNKNOWN, FINGER, STYLUS } digitizer_type_t;

typedef struct {
    digitizer_type_t type;
    uint8_t          tip : 1;
    uint8_t          in_range : 1;
    uint8_t          confidence : 1;
    uint16_t         x;
    uint16_t         y;
} digitizer_contact_t;

typedef struct {
    digitizer_contact_t contacts[DIGITIZER_CONTACT_COUNT];
    union {
        uint8_t buttons;
        struct {
            uint8_t button1 : 1;
            uint8_t button2 : 1;
            uint8_t button3 : 1;
        };
    };
} digitizer_t;

/**
 * \brief Gets the current digitizer state.
 *
 * \return The current digitizer state
 */
digitizer_t digitizer_get_state(void);

/**
 * \brief Sets the digitizer state, the new state will be sent when the digitizer task next runs.
 */
void digitizer_set_state(digitizer_t digitizer_state);

__attribute__((weak)) void digitizer_init_kb(void);

/**
 * @brief User level code pointing device initialisation
 *
 */
__attribute__((weak)) void digitizer_init_user(void);

/**
 * @brief Weak function allowing for user level digitizer state modification
 *
 * Takes digitizer_t struct allowing modification at user level then returns digitizer_t.
 *
 * @param[in] digitizer_state digitizer_t
 * @return digitizer_t
 */
__attribute__((weak)) digitizer_t digitizer_task_user(digitizer_t digitizer_state);

/**
 * @brief Weak function allowing for keyboard level digitizer state modification
 *
 * Takes digitizer_t struct allowing modification at keyboard level then returns digitizer_t.
 *
 * @param[in] digitizer_state digitizer_t
 * @return digitizer_t
 */
__attribute__((weak)) digitizer_t digitizer_task_kb(digitizer_t digitizer_state);

/**
 * \brief Initializes the digitizer feature.
 */
void digitizer_init(void);

/**
 * \brief Task processing routine for the digitizer feature. This function polls the digitizer hardware
 * and sends events to the host as required.
 *
 * \return true if a new event was sent
 */
bool digitizer_task(void);

#if defined(SPLIT_DIGITIZER_ENABLE)
/**
 * \brief Updates the digitizer report from the slave half.
 */
void digitizer_set_shared_report(digitizer_t report);
#    if !defined(DIGITIZER_TASK_THROTTLE_MS)
#        define DIGITIZER_TASK_THROTTLE_MS 1
#    endif
#endif // defined(SPLIT_DIGITIZER_ENABLE)

/** \} */
