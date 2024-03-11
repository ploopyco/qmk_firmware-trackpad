// Copyright 2023 Stefan Kerkmann (@KarlK90)
// SPDX-License-Identifier: GPL-3.0-or-later

#include <ch.h>
#include <hal.h>

#include "usb_main.h"
#include "usb_driver.h"
#include "usb_endpoints.h"
#include "report.h"

// Allocate buffers for reports
static report_keyboard_t keyboard_report = {
#if defined(KEYBOARD_SHARED_EP)
    .report_id = REPORT_ID_KEYBOARD
#endif
};
#if defined(MOUSE_ENABLE)
static report_mouse_t mouse_report = {
#if defined(MOUSE_SHARED_EP)
    .report_id = REPORT_ID_MOUSE
#endif
};
#endif
#if defined(EXTRAKEY_ENABLE)
static report_extra_t system_report = {
    .report_id = REPORT_ID_SYSTEM
};
static report_extra_t consumer_report = {
    .report_id = REPORT_ID_CONSUMER
};
#endif
#if defined(PROGRAMMABLE_BUTTON_ENABLE)
static report_programmable_button_t programmable_button_report = {
    .report_id = REPORT_ID_PROGRAMMABLE_BUTTON
};
#endif
#if defined(NKRO_ENABLE)
static report_nkro_t nkro_report = {
    .report_id = REPORT_ID_NKRO
};
#endif
#if defined(JOYSTICK_ENABLE)
static report_joystick_t joystick_report = {
#if defined(JOYSTICK_SHARED_EP)
    .report_id = REPORT_ID_JOYSTICK
#endif
};
#endif
#if defined(DIGITIZER_ENABLE)
static report_digitizer_t digitizer_report = {
#if defined(DIGITIZER_SHARED_EP)
    .report_id = REPORT_ID_MOUSE
#endif
};
#endif
#if defined(CONSOLE_ENABLE)
static uint8_t console_report[CONSOLE_EPSIZE];
#endif

#if defined(RAW_ENABLE)
static uint8_t raw_report[RAW_EPSIZE];
#endif


usb_endpoint_in_t usb_endpoints_in[USB_ENDPOINT_IN_COUNT] = {
// clang-format off
#if defined(SHARED_EP_ENABLE)
    [USB_ENDPOINT_IN_SHARED] = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_INTR, SHARED_EPSIZE, SHARED_IN_EPNUM, SHARED_IN_CAPACITY, NULL,
    QMK_USB_REPORT_STORAGE(
        // TODO: Seems wrong to replace the shared EP get_report fn with a digitizer one..
        &usb_digitizer_get_report,
        &usb_shared_set_report,
        &usb_shared_reset_report,
        &usb_shared_get_idle_rate,
        &usb_shared_set_idle_rate,
        &usb_shared_idle_timer_elapsed,
        (REPORT_ID_COUNT + 1),
#if defined(KEYBOARD_SHARED_EP)
        QMK_USB_REPORT_STROAGE_ENTRY(REPORT_ID_KEYBOARD, keyboard_report),
#endif
#if defined(MOUSE_SHARED_EP)
        QMK_USB_REPORT_STROAGE_ENTRY(REPORT_ID_MOUSE, mouse_report),
#endif
#if defined(EXTRAKEY_ENABLE)
        QMK_USB_REPORT_STROAGE_ENTRY(REPORT_ID_SYSTEM, system_report),
        QMK_USB_REPORT_STROAGE_ENTRY(REPORT_ID_CONSUMER, consumer_report),
#endif
#if defined(PROGRAMMABLE_BUTTON_ENABLE)
        QMK_USB_REPORT_STROAGE_ENTRY(REPORT_ID_PROGRAMMABLE_BUTTON, programmable_button_report),
#endif
#if defined(NKRO_ENABLE)
        QMK_USB_REPORT_STROAGE_ENTRY(REPORT_ID_NKRO, nkro_report),
#endif
#if defined(JOYSTICK_SHARED_EP)
        QMK_USB_REPORT_STROAGE_ENTRY(REPORT_ID_JOYSTICK, joystick_report),
#endif
#if defined(DIGITIZER_SHARED_EP)
        QMK_USB_REPORT_STROAGE_ENTRY(REPORT_ID_DIGITIZER, digitizer_report),
#endif
        )
    ),
#endif
// clang-format on

#if !defined(KEYBOARD_SHARED_EP)
    [USB_ENDPOINT_IN_KEYBOARD] = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_INTR, KEYBOARD_EPSIZE, KEYBOARD_IN_EPNUM, KEYBOARD_IN_CAPACITY, NULL, QMK_USB_REPORT_STORAGE_DEFAULT(keyboard_report)),
#endif

#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
    [USB_ENDPOINT_IN_MOUSE] = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_INTR, MOUSE_EPSIZE, MOUSE_IN_EPNUM, MOUSE_IN_CAPACITY, NULL, QMK_USB_REPORT_STORAGE_DEFAULT(mouse_report)),
#endif

#if defined(JOYSTICK_ENABLE) && !defined(JOYSTICK_SHARED_EP)
    [USB_ENDPOINT_IN_JOYSTICK] = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_INTR, JOYSTICK_EPSIZE, JOYSTICK_IN_EPNUM, JOYSTICK_IN_CAPACITY, QMK_USB_REPORT_STORAGE_DEFAULT(joystick_report)),
#endif

#if defined(DIGITIZER_ENABLE) && !defined(DIGITIZER_SHARED_EP)
    [USB_ENDPOINT_IN_DIGITIZER] = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_INTR, DIGITIZER_EPSIZE, DIGITIZER_IN_EPNUM, DIGITIZER_IN_CAPACITY, QMK_USB_REPORT_STORAGE_DEFAULT(digitizer_report)),
#endif

#if defined(CONSOLE_ENABLE)
#    if defined(USB_ENDPOINTS_ARE_REORDERABLE)
    [USB_ENDPOINT_IN_CONSOLE] = QMK_USB_ENDPOINT_IN_SHARED(USB_EP_MODE_TYPE_INTR, CONSOLE_EPSIZE, CONSOLE_IN_EPNUM, CONSOLE_IN_CAPACITY, NULL, QMK_USB_REPORT_STORAGE_DEFAULT(console_report[0])),
#    else
    [USB_ENDPOINT_IN_CONSOLE]  = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_INTR, CONSOLE_EPSIZE, CONSOLE_IN_EPNUM, CONSOLE_IN_CAPACITY, NULL, QMK_USB_REPORT_STORAGE_DEFAULT(console_report[0])),
#    endif
#endif

#if defined(RAW_ENABLE)
#    if defined(USB_ENDPOINTS_ARE_REORDERABLE)
    [USB_ENDPOINT_IN_RAW] = QMK_USB_ENDPOINT_IN_SHARED(USB_EP_MODE_TYPE_INTR, RAW_EPSIZE, RAW_IN_EPNUM, RAW_IN_CAPACITY, NULL, QMK_USB_REPORT_STORAGE_DEFAULT(raw_report[0])),
#    else
    [USB_ENDPOINT_IN_RAW]      = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_INTR, RAW_EPSIZE, RAW_IN_EPNUM, RAW_IN_CAPACITY, NULL, QMK_USB_REPORT_STORAGE_DEFAULT(raw_report[0])),
#    endif
#endif

#if defined(MIDI_ENABLE)
#    if defined(USB_ENDPOINTS_ARE_REORDERABLE)
    [USB_ENDPOINT_IN_MIDI] = QMK_USB_ENDPOINT_IN_SHARED(USB_EP_MODE_TYPE_BULK, MIDI_STREAM_EPSIZE, MIDI_STREAM_IN_EPNUM, MIDI_STREAM_IN_CAPACITY, NULL, NULL),
#    else
    [USB_ENDPOINT_IN_MIDI]     = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_BULK, MIDI_STREAM_EPSIZE, MIDI_STREAM_IN_EPNUM, MIDI_STREAM_IN_CAPACITY, NULL, NULL),
#    endif
#endif

#if defined(VIRTSER_ENABLE)
#    if defined(USB_ENDPOINTS_ARE_REORDERABLE)
    [USB_ENDPOINT_IN_CDC_DATA] = QMK_USB_ENDPOINT_IN_SHARED(USB_EP_MODE_TYPE_BULK, CDC_EPSIZE, CDC_IN_EPNUM, CDC_IN_CAPACITY, virtser_usb_request_cb, NULL),
#    else
    [USB_ENDPOINT_IN_CDC_DATA] = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_BULK, CDC_EPSIZE, CDC_IN_EPNUM, CDC_IN_CAPACITY, virtser_usb_request_cb, NULL),
#    endif
    [USB_ENDPOINT_IN_CDC_SIGNALING] = QMK_USB_ENDPOINT_IN(USB_EP_MODE_TYPE_INTR, CDC_NOTIFICATION_EPSIZE, CDC_NOTIFICATION_EPNUM, CDC_SIGNALING_DUMMY_CAPACITY, NULL, NULL),
#endif
};

usb_endpoint_in_lut_t usb_endpoint_interface_lut[TOTAL_INTERFACES] = {
#if !defined(KEYBOARD_SHARED_EP)
    [KEYBOARD_INTERFACE] = USB_ENDPOINT_IN_KEYBOARD,
#endif

#if defined(RAW_ENABLE)
    [RAW_INTERFACE] = USB_ENDPOINT_IN_RAW,
#endif

#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
    [MOUSE_INTERFACE] = USB_ENDPOINT_IN_MOUSE,
#endif

#if defined(SHARED_EP_ENABLE)
    [SHARED_INTERFACE] = USB_ENDPOINT_IN_SHARED,
#endif

#if defined(CONSOLE_ENABLE)
    [CONSOLE_INTERFACE] = USB_ENDPOINT_IN_CONSOLE,
#endif

#if defined(MIDI_ENABLE)
    [AS_INTERFACE] = USB_ENDPOINT_IN_MIDI,
#endif

#if defined(VIRTSER_ENABLE)
    [CCI_INTERFACE] = USB_ENDPOINT_IN_CDC_SIGNALING,
    [CDI_INTERFACE] = USB_ENDPOINT_IN_CDC_DATA,
#endif

#if defined(JOYSTICK_ENABLE) && !defined(JOYSTICK_SHARED_EP)
    [JOYSTICK_INTERFACE] = USB_ENDPOINT_IN_JOYSTICK,
#endif

#if defined(DIGITIZER_ENABLE) && !defined(DIGITIZER_SHARED_EP)
    [DIGITIZER_INTERFACE] = USB_ENDPOINT_IN_DIGITIZER,
#endif
};

usb_endpoint_out_t usb_endpoints_out[USB_ENDPOINT_OUT_COUNT] = {
#if defined(RAW_ENABLE)
    [USB_ENDPOINT_OUT_RAW] = QMK_USB_ENDPOINT_OUT(USB_EP_MODE_TYPE_INTR, RAW_EPSIZE, RAW_OUT_EPNUM, RAW_OUT_CAPACITY),
#endif

#if defined(MIDI_ENABLE)
    [USB_ENDPOINT_OUT_MIDI] = QMK_USB_ENDPOINT_OUT(USB_EP_MODE_TYPE_BULK, MIDI_STREAM_EPSIZE, MIDI_STREAM_OUT_EPNUM, MIDI_STREAM_OUT_CAPACITY),
#endif

#if defined(VIRTSER_ENABLE)
    [USB_ENDPOINT_OUT_CDC_DATA] = QMK_USB_ENDPOINT_OUT(USB_EP_MODE_TYPE_BULK, CDC_EPSIZE, CDC_OUT_EPNUM, CDC_OUT_CAPACITY),
#endif
};
