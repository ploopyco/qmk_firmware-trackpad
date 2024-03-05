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

typedef struct {
 #if DIGITIZER_HAS_STYLUS
    digitizer_stylus_report_t stylus;
#endif
#if DIGITIZER_FINGER_COUNT > 0
    digitizer_finger_report_t fingers[DIGITIZER_FINGER_COUNT];
    uint8_t  button1 : 1;
    uint8_t  button2 : 1;
    uint8_t  button3 : 1;
#endif
} digitizer_t;

/**
 * \brief Gets the current digitizer state.
 *
 * \return The current digitizer state
 */
digitizer_t digitizer_get_report(void);

/**
 * \brief Sets the digitizer state, the new state will be sent when the digitizer task next runs.
 */
void digitizer_set_report(digitizer_t digitizer_report);

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
void     digitizer_set_shared_report(digitizer_t report);
#    if !defined(DIGITIZER_TASK_THROTTLE_MS)
#        define DIGITIZER_TASK_THROTTLE_MS 1
#    endif
#endif     // defined(SPLIT_DIGITIZER_ENABLE)

/** \} */
