// Copyright 2024 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

enum peacock_keycodes {
    LMB_TOGGLE = QK_USER_0
};

const uint16_t PROGMEM etch_a_sketch_combo[] = { KC_LGUI, KC_ENTER, COMBO_END };
combo_t key_combos[] = {
    COMBO(etch_a_sketch_combo, TG(2))
};

#ifdef PEACOCK_ENCODER_BODGE
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {[0] = LAYOUT(KC_BTN1, KC_BTN2, LT(1, KC_BTN3), KC_LGUI, KC_ENTER),
                                                              [1] = LAYOUT(QK_BOOT, RGB_MOD, KC_TRNS, RGB_RMOD, EE_CLR),
                                                              [2] = LAYOUT(LMB_TOGGLE, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS),
                                                              [3] = LAYOUT(KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS)};
#else
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {[0] = LAYOUT(KC_MUTE, KC_BTN1, KC_BTN2, LT(1, KC_BTN3), KC_LGUI, KC_ENTER, C(KC_C)),
                                                              [1] = LAYOUT(KC_TRNS, QK_BOOT, RGB_MOD, KC_TRNS, RGB_RMOD, EE_CLR, KC_TRNS),
                                                              [2] = LAYOUT(KC_TRNS, LMB_TOGGLE, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS),
                                                              [3] = LAYOUT(KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS)};
#endif

const uint8_t INDICATOR_LED = 5;
static bool lmb_held = false;

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = { ENCODER_CCW_CW(KC_LEFT, KC_RIGHT),  ENCODER_CCW_CW(KC_UP, KC_DOWN)  },
    [1] = { ENCODER_CCW_CW(RGB_HUD, RGB_HUI),   ENCODER_CCW_CW(RGB_SAD, RGB_SAI)  },
    [2] = { ENCODER_CCW_CW(KC_MS_LEFT, KC_MS_RIGHT),   ENCODER_CCW_CW(KC_MS_UP, KC_MS_DOWN)  },
    [3] = { ENCODER_CCW_CW(RGB_RMOD, RGB_MOD),  ENCODER_CCW_CW(KC_RIGHT, KC_LEFT) },
};
#endif

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    switch(get_highest_layer(layer_state|default_layer_state)) {
        case 3:
            rgb_matrix_set_color(INDICATOR_LED, RGB_BLUE);
            lmb_held = false;
            break;
        case 2:
            rgb_matrix_set_color(INDICATOR_LED, RGB_RED);
            break;
        case 1:
            rgb_matrix_set_color(INDICATOR_LED, RGB_GREEN);
            lmb_held = false;
            break;
        default:
            rgb_matrix_set_color(INDICATOR_LED, RGB_OFF);
            lmb_held = false;
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

bool process_record_user(uint16_t keycode, keyrecord_t* record) {
    switch (keycode) {
        case LMB_TOGGLE:
            if (record->event.pressed) {
                lmb_held = !lmb_held;
            }
            if (lmb_held) {
                mousekey_on(KC_MS_BTN1);
            }
            else {
                mousekey_off(KC_MS_BTN1);
            }
            break;
    }
    return true;
}
