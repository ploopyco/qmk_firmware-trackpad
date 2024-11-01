// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
#    include "i2c_master.h"
#    undef POINTING_DEVICE_ENABLE
#    include "drivers/sensors/azoteq_iqs5xx.h"
#    ifndef DIGITIZER_CONTACT_COUNT
#        define DIGITIZER_CONTACT_COUNT 5
#    endif
#    define DIGITIZER_WIDTH_MM AZOTEQ_IQS5XX_WIDTH_MM
#    define DIGITIZER_HEIGHT_MM AZOTEQ_IQS5XX_HEIGHT_MM
#    define DIGITIZER_RESOLUTION_X AZOTEQ_IQS5XX_RESOLUTION_X
#    define DIGITIZER_RESOLUTION_Y AZOTEQ_IQS5XX_RESOLUTION_Y
#    define DIGITIZER_TOUCH_PAD
#elif defined(DIGITIZER_DRIVER_maxtouch)
#    include "drivers/sensors/procyon.h"
#    ifndef DIGITIZER_CONTACT_COUNT
#        define DIGITIZER_CONTACT_COUNT 5
#    endif
#    ifdef MXT_SWITCH_XY
#        define DIGITIZER_WIDTH_MM MXT_SENSOR_HEIGHT_MM
#        define DIGITIZER_HEIGHT_MM MXT_SENSOR_WIDTH_MM
#    else
#        define DIGITIZER_WIDTH_MM MXT_SENSOR_WIDTH_MM
#        define DIGITIZER_HEIGHT_MM MXT_SENSOR_HEIGHT_MM
#    endif
#    ifndef DIGITIZER_RESOLUTION_X
#        define DIGITIZER_RESOLUTION_X (DIGITIZER_WIDTH_MM * 50)

#    endif
#    ifndef DIGITIZER_RESOLUTION_Y
#         define DIGITIZER_RESOLUTION_Y (DIGITIZER_HEIGHT_MM * 50)
#    endif
#    define DIGITIZER_TOUCH_PAD
#else
// No driver
#    ifndef DIGITIZER_WIDTH_MM
#        define DIGITIZER_WIDTH_MM 100
#    endif
#    ifndef DIGITIZER_HEIGHT_MM
#        define DIGITIZER_HEIGHT_MM 100
#    endif

#    ifndef DIGITIZER_RESOLUTION_X
#        define DIGITIZER_RESOLUTION_X 1920
#    endif
#    ifndef DIGITIZER_RESOLUTION_Y
#        define DIGITIZER_RESOLUTION_Y 1080
#    endif

#    ifndef DIGITIZER_HAS_STYLUS
#        define DIGITIZER_HAS_STYLUS true
#    endif
#    ifndef DIGITIZER_CONTACT_COUNT
#        define DIGITIZER_CONTACT_COUNT 1
#    endif
#endif
