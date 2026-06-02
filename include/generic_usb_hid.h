#ifndef GENERIC_USB_HID_H_
#define GENERIC_USB_HID_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GENERIC_USB_HID_NKRO_BYTES 13u
#define GENERIC_USB_HID_REPORT_SIZE (1u + GENERIC_USB_HID_NKRO_BYTES)

void GenericUSB_HIDInit(void);
void GenericUSB_HIDTask(void);

uint32_t GenericUSB_HID_InputReports(void);
uint32_t GenericUSB_HID_KeyPresses(void);

#ifdef __cplusplus
}
#endif

#endif /* GENERIC_USB_HID_H_ */
