#include "jammate_tinyusb_hid.h"

#include <string.h>
#include "stm32h7xx_hal.h"
#include "tusb.h"

static uint32_t hid_in_reports = 0;
static uint32_t hid_out_reports = 0;
static uint32_t last_report_ms = 0;
static uint8_t last_out_report[JAMMATE_HID_REPORT_SIZE];

void JamMate_TinyUSB_HIDInit(void)
{
    hid_in_reports = 0;
    hid_out_reports = 0;
    last_report_ms = 0;
    memset(last_out_report, 0, sizeof(last_out_report));
}

void JamMate_TinyUSB_HIDTask(void)
{
    uint32_t now = HAL_GetTick();

    if (!tud_hid_ready()) return;
    if ((now - last_report_ms) < 100u) return;

    uint8_t report[JAMMATE_HID_REPORT_SIZE] = {0};
    report[0] = 'J';
    report[1] = 'M';
    report[2] = 'H';
    report[3] = 'I';
    report[4] = (uint8_t)(hid_in_reports & 0xffu);
    report[5] = (uint8_t)((hid_in_reports >> 8) & 0xffu);
    report[6] = (uint8_t)(now & 0xffu);
    report[7] = (uint8_t)((now >> 8) & 0xffu);
    report[8] = (uint8_t)(hid_out_reports & 0xffu);
    report[9] = (uint8_t)((hid_out_reports >> 8) & 0xffu);

    if (tud_hid_report(0, report, sizeof(report))) {
        hid_in_reports++;
        last_report_ms = now;
    }
}

uint32_t JamMate_HID_InputReports(void)
{
    return hid_in_reports;
}

uint32_t JamMate_HID_OutputReports(void)
{
    return hid_out_reports;
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

    uint16_t const len = (reqlen < JAMMATE_HID_REPORT_SIZE)
                       ? reqlen
                       : JAMMATE_HID_REPORT_SIZE;
    memset(buffer, 0, len);
    if (len >= 4u) {
        buffer[0] = 'J';
        buffer[1] = 'M';
        buffer[2] = 'H';
        buffer[3] = 'G';
    }
    return len;
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

    uint16_t const len = (bufsize < JAMMATE_HID_REPORT_SIZE)
                       ? bufsize
                       : JAMMATE_HID_REPORT_SIZE;
    memset(last_out_report, 0, sizeof(last_out_report));
    memcpy(last_out_report, buffer, len);
    hid_out_reports++;
}
