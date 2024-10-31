// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef PROCYON_50_42
#   ifndef MXT_SENSOR_WIDTH_MM
#       define MXT_SENSOR_WIDTH_MM 50
#   endif
#   ifndef MXT_SENSOR_HEIGHT_MM
#       define MXT_SENSOR_HEIGHT_MM 42
#   endif
#   ifndef MXT_MATRIX_X_SIZE
#       define MXT_MATRIX_X_SIZE 12
#   endif
#   ifndef MXT_MATRIX_Y_SIZE
#       define MXT_MATRIX_Y_SIZE 10
#   endif
#   ifndef MXT_GAIN
#       define MXT_GAIN 10
#   endif
#   ifndef MXT_TOUCH_THRESHOLD
#       define MXT_TOUCH_THRESHOLD 20
#   endif
#   ifndef MXT_TOUCH_HYST
#       define MXT_TOUCH_HYST 10
#   endif
#   ifndef MXT_INTERNAL_TOUCH_HYST
#       define MXT_INTERNAL_TOUCH_HYST 10
#   endif
#   ifndef MXT_INTERNAL_TOUCH_THRESHOLD
#       define MXT_INTERNAL_TOUCH_THRESHOLD 5
#   endif
#endif
