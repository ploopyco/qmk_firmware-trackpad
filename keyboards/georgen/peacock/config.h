// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Oh no! I wired up the wrong I2C pins.
// We will sacrifice the encoder buttons.
#define I2C_DRIVER I2CD0
#define I2C1_SDA_PIN GP16
#define I2C1_SCL_PIN GP21

#define DIGITIZER_INVERT_X
#define DIGITIZER_MOTION_PIN GP28
#define DIGITIZER_MOTION_PIN_ACTIVE_LOW yes
