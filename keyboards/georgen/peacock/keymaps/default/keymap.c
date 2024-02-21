// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {[0] = LAYOUT(KC_BTN1, KC_BTN2, LT(1, KC_BTN3), KC_ENTER, KC_BACKSPACE),
                                                              [1] = LAYOUT(QK_BOOT, RGB_MOD, KC_TRNS, RGB_RMOD, EE_CLR),
                                                              [2] = LAYOUT(KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS),
                                                              [3] = LAYOUT(KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS)};
const uint8_t INDICATOR_LED = 5;

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = { ENCODER_CCW_CW(KC_LEFT, KC_RIGHT),  ENCODER_CCW_CW(KC_UP, KC_DOWN)  },
    [1] = { ENCODER_CCW_CW(RGB_HUD, RGB_HUI),   ENCODER_CCW_CW(RGB_SAD, RGB_SAI)  },
    [2] = { ENCODER_CCW_CW(RGB_VAD, RGB_VAI),   ENCODER_CCW_CW(RGB_SPD, RGB_SPI)  },
    [3] = { ENCODER_CCW_CW(RGB_RMOD, RGB_MOD),  ENCODER_CCW_CW(KC_RIGHT, KC_LEFT) },
};
#endif

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    switch(get_highest_layer(layer_state|default_layer_state)) {
        case 3:
            rgb_matrix_set_color(INDICATOR_LED, RGB_BLUE);
            break;
        case 2:
            rgb_matrix_set_color(INDICATOR_LED, RGB_RED);
            break;
        case 1:
            rgb_matrix_set_color(INDICATOR_LED, RGB_GREEN);
            break;
        default:
            rgb_matrix_set_color(INDICATOR_LED, RGB_OFF);
            break;
    }
    return false;
}

bool shutdown_kb(bool jump_to_bootloader) {
    if (!shutdown_user(jump_to_bootloader)) {
        return false;
    }
    void rgb_matrix_update_pwm_buffers(void);
    if (jump_to_bootloader) {
        rgb_matrix_set_color_all(RGB_RED);
    }
    else {
        rgb_matrix_set_color_all(RGB_OFF);
    }
    rgb_matrix_update_pwm_buffers();
    return true;
}
