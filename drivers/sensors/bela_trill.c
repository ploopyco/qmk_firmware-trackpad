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

// Registers
#define BELA_TRILL_CMD1_REG     0x00
#define BELA_TRILL_CMD_ARG0_REG 0x01
#define BELA_TRILL_CMD_ARG1_REG 0x02
#define BELA_TRILL_STATUS_REG   0x03
#define BELA_TRILL_PAYLOAD_REG  0x04

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

void bela_trill_init(void) {
    i2c_init();

    // Reset the board
    uint8_t cmd[] = { BELA_TRILL_RESET_CMD };
    i2c_write_register(BELA_TRILL_ADDRESS, BELA_TRILL_CMD1_REG, (uint8_t *)&cmd, sizeof(cmd), BELA_TRILL_TIMEOUT_MS);
}

report_mouse_t bela_trill_get_report(report_mouse_t mouse_report) {
    payload_2d_centroid payload = { 0 };
    i2c_status_t status = i2c_read_register(BELA_TRILL_ADDRESS, BELA_TRILL_STATUS_REG, (uint8_t *)&payload, sizeof(payload_2d_centroid), BELA_TRILL_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        uint8_t h_touch_count = 0;
        uint8_t v_touch_count = 0;

        for (; h_touch_count < 4; h_touch_count++) {
            if (GET_LE(payload.horizontal_location[h_touch_count]) == 0xffff) break;
        }
        for (; v_touch_count < 4; v_touch_count++) {
            if (GET_LE(payload.vertical_location[v_touch_count]) == 0xffff) break;
        }
        const uint8_t touch_count = MAX(h_touch_count, v_touch_count);

        static bool contact = false;
        static uint32_t timer = 0;

        if (touch_count) {
            static uint16_t last_x = 0;
            static uint16_t last_y = 0;
            static int16_t scroll_h_carry = 0;
            static int16_t scroll_v_carry = 0;
            const uint16_t x = GET_LE(payload.horizontal_location[0]);
            const uint16_t y = GET_LE(payload.vertical_location[0]);

            // Dont generate an event if we are not already in contact. We cannot
            // compute a relative movement in this case.
            if (contact) {
                if (touch_count == 1) {
                    mouse_report.x = CONSTRAIN_HID_XY(x - last_x);
                    mouse_report.y = CONSTRAIN_HID_XY(last_y - y);
                } else if (touch_count > 1) {
                    int16_t h = x - last_x + scroll_h_carry;
                    int16_t v = last_y - y + scroll_v_carry;
                    scroll_h_carry = h % BELA_TRILL_SCROLL_SCALE_FACTOR;
                    scroll_v_carry = v % BELA_TRILL_SCROLL_SCALE_FACTOR; 
                    mouse_report.h = CONSTRAIN_HID(h/BELA_TRILL_SCROLL_SCALE_FACTOR);
                    mouse_report.v = CONSTRAIN_HID(v/BELA_TRILL_SCROLL_SCALE_FACTOR);
                }
            }
            else {
                timer = timer_read32();
                mouse_report.buttons = 0;
            }
            last_x = x;
            last_y = y;
            contact = true;
        }
        else {
            if (timer_elapsed32(timer) < BELA_TRILL_TAP_TIME) {
                mouse_report.buttons |= 0x1;
            }
            contact = false;
        }
    }
    return mouse_report;
}
