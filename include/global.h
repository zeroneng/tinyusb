#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USB_DEVICE_VID 0x0483u
#define USB_DEVICE_PID 0x5787u
#define USB_DEVICE_BCD 0x020bu

extern char const g_usb_string_manufacturer[];
extern char const g_usb_string_product[];
extern char const g_usb_string_serial[];
extern char const g_usb_string_audio_speaker[];
extern char const g_usb_string_audio_microphone[];
extern char const g_usb_string_cdc[];
extern char const g_usb_string_hid[];

#ifdef __cplusplus
}
#endif

#endif /* GLOBAL_H_ */
