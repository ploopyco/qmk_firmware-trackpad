#include "bela_trill.h"
#include "print.h"

// TODO: Lots of address options
#ifndef BELA_TRILL_ADDRESS
#    define BELA_TRILL_ADDRESS (0x40 << 1)
#endif
#ifndef BELA_TRILL_TIMEOUT_MS
#    define BELA_TRILL_TIMEOUT_MS 10
#endif

// Registers
#define BELA_TRILL_CMD1_REG     0x00
#define BELA_TRILL_CMD_ARG0_REG 0x01
#define BELA_TRILL_CMD_ARG1_REG 0x02
#define BELA_TRILL_STATUS_REG   0x03
#define BELA_TRILL_PAYLOAD_REG  0x04

#define GET_LE(A) ((A##_h << 8) | A##_l)
#define CONSTRAIN_HID_XY(amt) ((amt) < XY_REPORT_MIN ? XY_REPORT_MIN : ((amt) > XY_REPORT_MAX ? XY_REPORT_MAX : (amt)))

typedef struct PACKED {
    uint8_t location_h;
    uint8_t location_l;
    uint8_t size_h;
    uint8_t size_l;
} touch_data;


// Applicable to the Hex and Square sensors
typedef struct PACKED {
    uint8_t status;
    touch_data vertical[4];
    touch_data horizontal[4]; 
} payload_2d_centroid;

void bela_trill_init(void) {
    i2c_init();
}

report_mouse_t bela_trill_get_report(report_mouse_t mouse_report) {
    payload_2d_centroid payload = { 0 };
    i2c_status_t status = i2c_read_register(BELA_TRILL_ADDRESS, BELA_TRILL_STATUS_REG, (uint8_t *)&payload, sizeof(payload_2d_centroid), BELA_TRILL_TIMEOUT_MS);
    if (status == I2C_STATUS_SUCCESS) {
        int touch_count = 0;
        for (; touch_count < 4; touch_count++) {
            if (GET_LE(payload.horizontal[0].location) == 0xffff) break;
        }
        static bool contact = false;
        if (touch_count) {
            uprintf("S: %d, %dx%d %dx%d\n", payload.status, GET_LE(payload.horizontal[0].location), GET_LE(payload.vertical[0].location), GET_LE(payload.horizontal[0].size), GET_LE(payload.vertical[0].size));
            static uint16_t last_x = 0;
            static uint16_t last_y = 0;
            const uint16_t x = GET_LE(payload.horizontal[0].location);
            const uint16_t y = GET_LE(payload.vertical[0].location);

            if (contact) {
                mouse_report.x = CONSTRAIN_HID_XY(x - last_x);
                mouse_report.y = CONSTRAIN_HID_XY(last_y - y);
            }
            last_x = x;
            last_y = y;
            contact = true;
        }
        else {
            contact = false;
        }
    }
    return mouse_report;
}
