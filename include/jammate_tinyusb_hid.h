#ifndef JAMMATE_TINYUSB_HID_H_
#define JAMMATE_TINYUSB_HID_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JAMMATE_HID_REPORT_SIZE 8u

void JamMate_TinyUSB_HIDInit(void);
void JamMate_TinyUSB_HIDTask(void);

uint32_t JamMate_HID_InputReports(void);
uint32_t JamMate_HID_KeyPresses(void);

#ifdef __cplusplus
}
#endif

#endif /* JAMMATE_TINYUSB_HID_H_ */
