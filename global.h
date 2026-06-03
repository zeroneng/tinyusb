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

#ifndef USB_ENABLE_AUDIO
#define USB_ENABLE_AUDIO 1
#endif
#ifndef USB_ENABLE_CDC
#define USB_ENABLE_CDC   1
#endif
#ifndef USB_ENABLE_MIDI
#define USB_ENABLE_MIDI  1
#endif
#ifndef USB_ENABLE_HID
#define USB_ENABLE_HID   1
#endif
#ifndef USB_ENABLE_MSC
#define USB_ENABLE_MSC   1
#endif

#ifndef DEBUG_TEST_CDC
#define DEBUG_TEST_CDC  0
#endif
#ifndef DEBUG_TEST_HID
#define DEBUG_TEST_HID  0
#endif
#ifndef DEBUG_TEST_MIDI
#define DEBUG_TEST_MIDI 0
#endif
#ifndef DEBUG_TEST_SD
#define DEBUG_TEST_SD   0
#endif

#endif /* GLOBAL_H_ */
