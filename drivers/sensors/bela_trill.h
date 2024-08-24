// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "i2c_master.h"
#include "pointing_device.h"
#include "util.h"

void bela_trill_init(void);
report_mouse_t bela_trill_get_report(report_mouse_t mouse_report);