// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MXT_X_PITCH 38
#define MXT_Y_PITCH 38

#ifndef MXT_GAIN
#    define MXT_GAIN 8
#endif
#ifndef MXT_TOUCH_THRESHOLD
#    define MXT_TOUCH_THRESHOLD 15
#endif
#ifndef MXT_TOUCH_HYST
#    define MXT_TOUCH_HYST 10
#endif
#ifndef MXT_INTERNAL_TOUCH_HYST
#    define MXT_INTERNAL_TOUCH_HYST 10
#endif
#ifndef MXT_INTERNAL_TOUCH_THRESHOLD
#    define MXT_INTERNAL_TOUCH_THRESHOLD 5
#endif

#ifdef PROCYON_50_42
#    define MXT_SENSOR_WIDTH_MM 50
#    define MXT_SENSOR_HEIGHT_MM 42
#    define MXT_MATRIX_X_SIZE 12
#    define MXT_MATRIX_Y_SIZE 10
#endif
