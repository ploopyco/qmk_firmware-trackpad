#include "bela_trill.h"
#include "print.h"
#include "timer.h"

#ifndef BELA_TRILL_ADDRESS
#    define BELA_TRILL_ADDRESS (0x40 << 1)
#endif
#ifndef BELA_TRILL_TIMEOUT_MS
#    define BELA_TRILL_TIMEOUT_MS 10
#endif
#ifndef BELA_TRILL_SCROLL_SCALE_FACTOR
#    define BELA_TRILL_SCROLL_SCALE_FACTOR 20
#endif
#ifndef BELA_TRILL_TAP_TIME
#    define BELA_TRILL_TAP_TIME 200
#endif
#ifndef BELA_TRILL_SIZE_THRESHOLD
#    define BELA_TRILL_SIZE_THRESHOLD 1000
#endif
#ifndef BELA_TRILL_MINIMUM_SIZE
#    define BELA_TRILL_MINIMUM_SIZE 0
#endif
#ifndef BELA_TRILL_NOISE_THRESHOLD
#    define BELA_TRILL_NOISE_THRESHOLD 40
#endif
// Registers
#define BELA_TRILL_CMD1_REG     0x00
#define BELA_TRILL_CMD_ARG0_REG 0x01
#define BELA_TRILL_CMD_ARG1_REG 0x02
#define BELA_TRILL_STATUS_REG   0x03
#define BELA_TRILL_PAYLOAD_REG  0x04

// Status bits
#define BELA_TRILL_STATUS_FRAMEID_MASK   0x3F
#define BELA_TRILL_STATUS_ACTIVITY_MASK   0x40
#define BELA_TRILL_STATUS_RESET_MASK   0x80

// Commands
#define BELA_TRILL_MODE_CMD                                 0x01
#define BELA_TRILL_SCAN_SETTINGS_CMD                        0x02
#define BELA_TRILL_PRESCALER_CMD                            0x03
#define BELA_TRILL_NOISE_THRESHOLD_CMD                      0x04
#define BELA_TRILL_IDAC_CMD                                 0x05
#define BELA_TRILL_UPDATE_BASELINE_CMD                      0x06
#define BELA_TRILL_MINIMUM_SIZE_CMD                         0x07
#define BELA_TRILL_ADJACENT_CENTROID_NOISE_THRESHOLD_CMD    0x08
#define BELA_TRILL_EVENT_MODE_CMD                           0x09
#define BELA_TRILL_CHANNEL_MASK_LOW_CMD                     0x0A
#define BELA_TRILL_CHANNEL_MASK_HIGH_CMD                    0x0B
#define BELA_TRILL_RESET_CMD                                0x0C
#define BELA_TRILL_FORMAT_CMD                               0x0D
#define BELA_TRILL_AUTO_SCAN_TIMER_CMD                      0x0E
#define BELA_TRILL_SCAN_TRIGGER_CMD                         0x0F
#define BELA_TRILL_IDENTIFY_CMD                             0xFF

#define BELA_NO_TOUCH                                       0xFFFF

#define GET_LE(A) (((A).high << 8) | (A).low)
#define CONSTRAIN_HID(amt) ((amt) < INT8_MIN ? INT8_MIN : ((amt) > INT8_MAX ? INT8_MAX : (amt)))
#define CONSTRAIN_HID_XY(amt) ((amt) < XY_REPORT_MIN ? XY_REPORT_MIN : ((amt) > XY_REPORT_MAX ? XY_REPORT_MAX : (amt)))

typedef struct PACKED {
    uint8_t high;
    uint8_t low;
} short_be;


// Applicable to the Hex and Square sensors
typedef struct PACKED {
    uint8_t status;
    short_be vertical_location[4];
    short_be vertical_size[4];
    short_be horizontal_location[4];
    short_be horizontal_size[4];
} payload_2d_centroid;

i2c_status_t bela_send_cmd(uint8_t cmd, uint8_t arg1, uint8_t arg2) {
    uint8_t payload[] = { cmd, arg1, arg2 };
    return i2c_write_register(BELA_TRILL_ADDRESS, BELA_TRILL_CMD1_REG, (uint8_t *)&payload, sizeof(payload), BELA_TRILL_TIMEOUT_MS);
}

void bela_trill_init(void) {
    i2c_init();

    // Reset the board
    bela_send_cmd(BELA_TRILL_RESET_CMD, 0, 0);
    bela_send_cmd(BELA_TRILL_UPDATE_BASELINE_CMD, 0, 0);
    bela_send_cmd(BELA_TRILL_MINIMUM_SIZE_CMD, BELA_TRILL_MINIMUM_SIZE >> 8, BELA_TRILL_MINIMUM_SIZE & 0xFF);
    bela_send_cmd(BELA_TRILL_NOISE_THRESHOLD_CMD, BELA_TRILL_NOISE_THRESHOLD, 0);
    //bela_send_cmd(BELA_TRILL_AUTO_SCAN_TIMER_CMD, 32, 1); // Scan every millisecond
    //bela_send_cmd(BELA_TRILL_SCAN_TRIGGER_CMD, 2, 0);

}

report_mouse_t bela_trill_get_report(report_mouse_t mouse_report) {
    payload_2d_centroid payload = { 0 };
    i2c_status_t status = i2c_read_register(BELA_TRILL_ADDRESS, BELA_TRILL_STATUS_REG, (uint8_t *)&payload, sizeof(payload_2d_centroid), BELA_TRILL_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        uint8_t h_touch_count = 0;
        uint8_t v_touch_count = 0;
        const uint8_t frame_id = payload.status & BELA_TRILL_STATUS_FRAMEID_MASK;
        static uint8_t last_frame_id = 0;
        const bool activity = (payload.status & BELA_TRILL_STATUS_ACTIVITY_MASK) == BELA_TRILL_STATUS_ACTIVITY_MASK;

        if (frame_id == last_frame_id) {
            // No new data
            return mouse_report;
        }

        if (activity) {
            for (; h_touch_count < 4; h_touch_count++) {
                if (GET_LE(payload.horizontal_location[h_touch_count]) == BELA_NO_TOUCH) break;
            }
            for (; v_touch_count < 4; v_touch_count++) {
                if (GET_LE(payload.vertical_location[v_touch_count]) == BELA_NO_TOUCH) break;
            }
        }

        // The two axes are independent sensors. If only one axis registers a touch, we cant do much with it so treat
        // it as no touches. Otherwise, count up how many fingers are present.
        const uint8_t touch_count = (h_touch_count && v_touch_count) ? MAX(h_touch_count, v_touch_count) : 0;

        static bool contact = false;
        static uint16_t last_x = BELA_NO_TOUCH;
        static uint16_t last_y = BELA_NO_TOUCH;
        static uint16_t last_size_x = 0;
        static uint16_t last_size_y = 0;

        static uint32_t timer = 0;
        last_frame_id = frame_id;

        if (touch_count) {
            const uint16_t x = GET_LE(payload.horizontal_location[0]);
            const uint16_t y = GET_LE(payload.vertical_location[0]);
            const uint16_t size_x = GET_LE(payload.horizontal_size[0]);
            const uint16_t size_y = GET_LE(payload.vertical_size[0]);

            // When a touch grows or shrinks (touchdown or lift off), it can look like movement. So filter out
            // small touches, or touches where the size is changing too much.
            const bool stable_size = (size_x + size_y) > BELA_TRILL_SIZE_THRESHOLD &&
                                     (size_x * 100 < last_size_x * 125) && (size_x * 100 > last_size_x * 75) &&
                                     (size_y * 100 < last_size_y * 125) && (size_y * 100 > last_size_y * 75);
            last_size_x = size_x;
            last_size_y = size_y;

            // Dont generate an event if we are not already in contact. We cannot
            // compute a relative movement in this case.
            if (contact) {
                if (stable_size)
                {
                    if (touch_count == 1) {
                        if (x != BELA_NO_TOUCH && last_x != BELA_NO_TOUCH && y != BELA_NO_TOUCH && last_y != BELA_NO_TOUCH) {
                            mouse_report.x = CONSTRAIN_HID_XY(x - last_x);
                            mouse_report.y = CONSTRAIN_HID_XY(last_y - y);
                            //uprintf("%02x %d %d: %dx%d %d (%d) (%d)\n", payload.status, contact, touch_count, mouse_report.x, mouse_report.y, mouse_report.buttons, GET_LE(payload.horizontal_size[0]), GET_LE(payload.vertical_size[0]));
                        }
                    } else if (touch_count > 1) {
                        if (x != BELA_NO_TOUCH && last_x != BELA_NO_TOUCH && y != BELA_NO_TOUCH && last_y != BELA_NO_TOUCH) {
                            static int16_t scroll_h_carry = 0;
                            static int16_t scroll_v_carry = 0;
                            int16_t h = x - last_x + scroll_h_carry;
                            int16_t v = last_y - y + scroll_v_carry;

                            scroll_h_carry = h % BELA_TRILL_SCROLL_SCALE_FACTOR;
                            scroll_v_carry = v % BELA_TRILL_SCROLL_SCALE_FACTOR;

                            mouse_report.h = CONSTRAIN_HID(h/BELA_TRILL_SCROLL_SCALE_FACTOR);
                            mouse_report.v = CONSTRAIN_HID(v/BELA_TRILL_SCROLL_SCALE_FACTOR);
                        }
                    }

                    last_x = x;
                    last_y = y;
                }
            }
            else {
                if (timer_elapsed32(timer) < BELA_TRILL_TAP_TIME) {
                    // Tap hold gesture
                    mouse_report.buttons |= 0x1;
                }
                else {
                    // Release any taps.
                    mouse_report.buttons = 0x0;
                }
                timer = timer_read32();
                last_x = BELA_NO_TOUCH;
                last_y = BELA_NO_TOUCH;
            }
            contact = true;
        }
        else {
            if (contact && timer_elapsed32(timer) < BELA_TRILL_TAP_TIME) {
                // Tap gesture
                mouse_report.buttons |= 0x1;
                // Reset the timer - if another tap occurs we go to tap hold
                timer = timer_read32();
            }
            else {
                if (timer_elapsed32(timer) > BELA_TRILL_TAP_TIME) {
                    // Release any taps.
                    mouse_report.buttons = 0x0;
                }
            }

            last_x = BELA_NO_TOUCH;
            last_y = BELA_NO_TOUCH;
            contact = false;
        }
    }
    return mouse_report;
}
