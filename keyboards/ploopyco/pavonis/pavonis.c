// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

bool shutdown_kb(bool jump_to_bootloader) {
    if (!shutdown_user(jump_to_bootloader)) {
        return false;
    }

    if (jump_to_bootloader) {
        // Buzz to indicate the reboot to bootloader gesture was detected
        gpio_set_pin_output_push_pull(HAPTICS_PIN);
        gpio_write_pin_high(HAPTICS_PIN);
        wait_ms(50);
        gpio_write_pin_low(HAPTICS_PIN);
    }

    return true;
}
