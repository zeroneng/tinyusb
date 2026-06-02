# Generic USB TinyUSB Daisy Seed

This project was built from code originally shared in the Daisy Seed Forum thread
[TinyUSB UAC2 2x2 + CDC serial I/O for the Daisy Seed](https://forum.electro-smith.com/t/tinyusb-uac2-2x2-cdc-serial-i-o-for-the-daisy-seed/9174).
The app has since been adapted into a TinyUSB composite device for Daisy Seed with UAC2 audio, CDC serial, and NKRO HID keyboard support.

Editable USB identity values live in `include/global.h`, including the current product ID:

```c
#define USB_DEVICE_PID 0x5787u
```
