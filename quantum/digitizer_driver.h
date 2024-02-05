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
#    define DIGITIZER_TOUCH_PAD
#elif defined(DIGITIZER_DRIVER_maxtouch)
#    ifndef DIGITIZER_HAS_STYLUS
#        define DIGITIZER_HAS_STYLUS false
#    endif
#    ifndef DIGITIZER_FINGER_COUNT
#        define DIGITIZER_FINGER_COUNT 5
#    endif
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
