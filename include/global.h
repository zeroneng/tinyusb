#ifndef GLOBAL_H_
#define GLOBAL_H_

#define USB_DEVICE_VID 0x0483u
#define USB_DEVICE_PID 0x5787u
#define USB_DEVICE_BCD 0x020bu

#define USB_STRING_MANUFACTURER     "Generic USB"
#define USB_STRING_PRODUCT          "Generic USB UAC2 CDC MIDI MSC NKRO"
#define USB_STRING_SERIAL           "000001"
#define USB_STRING_AUDIO_SPEAKER    "Generic USB Speakers"
#define USB_STRING_AUDIO_MICROPHONE "Generic USB Stereo Microphone"
#define USB_STRING_CDC              "Generic USB Serial"
#define USB_STRING_MIDI             "Generic USB MIDI"
#define USB_STRING_HID              "Generic USB NKRO Keyboard"
#define USB_STRING_MSC              "Generic USB SD Card"

#define DEBUG_TEST_CDC  1
#define DEBUG_TEST_HID  1
#define DEBUG_TEST_MIDI 1
#define DEBUG_TEST_SD   0

#endif /* GLOBAL_H_ */
