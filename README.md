# Generic USB TinyUSB Daisy Seed

This project was built from code originally shared on the Daisy Seed Forum. The app has since been adapted into a TinyUSB composite device for Daisy Seed with UAC2 audio, CDC serial, and NKRO HID keyboard support.

Editable USB identity values live in `include/global.h`, including the current product ID:

```c
#define USB_DEVICE_PID 0x5787u
```
