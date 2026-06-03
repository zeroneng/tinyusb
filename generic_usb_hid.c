#include "generic_usb_hid.h"

#include <string.h>
#include "tusb.h"
#include "global.h"
#include "generic_usb_port.h"

#if USB_ENABLE_HID

static uint32_t hid_in_reports = 0;
static uint32_t hid_key_presses = 0;
static uint32_t last_key_ms = 0;
static bool key_is_down = false;

void GenericUSB_HIDInit(void)
{
    hid_in_reports = 0;
    hid_key_presses = 0;
    last_key_ms = 0;
    key_is_down = false;
}

void GenericUSB_HIDTask(void)
{
#if DEBUG_TEST_HID
    uint32_t now = GenericUSB_NowMs();

    if (!tud_hid_ready()) return;

    if (!key_is_down) {
        if ((now - last_key_ms) < 1000u) return;

        uint8_t report[GENERIC_USB_HID_REPORT_SIZE] = {0};
        report[1u + (HID_KEY_A / 8u)] = (uint8_t)(1u << (HID_KEY_A % 8u));

        if (tud_hid_report(0, report, sizeof(report))) {
            hid_in_reports++;
            hid_key_presses++;
            key_is_down = true;
            last_key_ms = now;
        }
    } else if ((now - last_key_ms) >= 40u) {
        uint8_t report[GENERIC_USB_HID_REPORT_SIZE] = {0};

        if (tud_hid_report(0, report, sizeof(report))) {
            hid_in_reports++;
            key_is_down = false;
        }
    }
#else
    if (key_is_down && tud_hid_ready()) {
        uint8_t report[GENERIC_USB_HID_REPORT_SIZE] = {0};

        if (tud_hid_report(0, report, sizeof(report))) {
            hid_in_reports++;
            key_is_down = false;
        }
    }
#endif
}

uint32_t GenericUSB_HID_InputReports(void)
{
    return hid_in_reports;
}

uint32_t GenericUSB_HID_KeyPresses(void)
{
    return hid_key_presses;
}

uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer,
                               uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;

    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer,
                           uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;

    (void)buffer;
    (void)bufsize;
}

#else

void GenericUSB_HIDInit(void) {}
void GenericUSB_HIDTask(void) {}
uint32_t GenericUSB_HID_InputReports(void) { return 0; }
uint32_t GenericUSB_HID_KeyPresses(void) { return 0; }

#endif
