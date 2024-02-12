// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
#    include "i2c_master.h"
#    include "drivers/sensors/azoteq_iqs5xx.h"
#    ifndef DIGITIZER_HAS_STYLUS
#        define DIGITIZER_HAS_STYLUS false
#    endif
#    ifndef DIGITIZER_FINGER_COUNT
#        define DIGITIZER_FINGER_COUNT 5
#    endif
#    define DIGITIZER_WIDTH_MM      AZOTEQ_IQS5XX_WIDTH_MM
#    define DIGITIZER_HEIGHT_MM     AZOTEQ_IQS5XX_HEIGHT_MM
#    define DIGITIZER_RESOLUTION_X  AZOTEQ_IQS5XX_RESOLUTION_X
#    define DIGITIZER_RESOLUTION_Y  AZOTEQ_IQS5XX_RESOLUTION_Y
#    define DIGITIZER_TOUCH_PAD
#elif defined(DIGITIZER_DRIVER_maxtouch)
#    include "drivers/sensors/maxtouch.h"
#    ifndef DIGITIZER_HAS_STYLUS
#        define DIGITIZER_HAS_STYLUS false
#    endif
#    ifndef DIGITIZER_FINGER_COUNT
#        define DIGITIZER_FINGER_COUNT 5
#    endif
#    define DIGITIZER_WIDTH_MM      MXT_SENSOR_WIDTH_MM
#    define DIGITIZER_HEIGHT_MM     MXT_SENSOR_HEIGHT_MM
    // TODO: Magic numbers
#    define DIGITIZER_RESOLUTION_X  3685
#    define DIGITIZER_RESOLUTION_Y  2150
#    define DIGITIZER_TOUCH_PAD
#else
#    ifndef DIGITIZER_HAS_STYLUS
#        define DIGITIZER_HAS_STYLUS true
#    endif
#    ifndef DIGITIZER_FINGER_COUNT
#        define DIGITIZER_FINGER_COUNT 0
#    endif
#    define DIGITIZER_DRIVER_none
#endif
