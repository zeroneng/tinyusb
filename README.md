# Generic USB TinyUSB Daisy Seed

TinyUSB composite device for Daisy Seed on the external USB connector:

- UAC2 stereo audio in/out
- CDC ACM serial
- USB MIDI
- HID NKRO keyboard
- MSC SD card access

This project was built from code originally shared in the Daisy Seed Forum
thread [TinyUSB UAC2 2x2 + CDC serial I/O for the Daisy Seed](https://forum.electro-smith.com/t/tinyusb-uac2-2x2-cdc-serial-i-o-for-the-daisy-seed/9174).

## Current Status

The working build uses Daisy's external USB connector, which is the STM32H7
OTG HS peripheral running at full speed on PB14/PB15. TinyUSB owns that
peripheral directly.

The SD-backed MSC path uses libDaisy-facing SD access:

- `SdmmcHandler` for SD configuration
- libDaisy `BSP_SD_*` polling block APIs for raw sector reads/writes
- no app-level `HAL_SD_`, `hsd1`, or `SD_HandleTypeDef` calls

## Editable USB Identity

Change USB identity and feature flags in `global.h`:

```c
#define USB_DEVICE_VID 0x0483u
#define USB_DEVICE_PID 0x5787u
#define USB_DEVICE_BCD 0x020bu

#define USB_STRING_MANUFACTURER "Generic USB"
#define USB_STRING_PRODUCT      "Generic USB UAC2 CDC MIDI MSC NKRO"

#define USB_ENABLE_AUDIO 1
#define USB_ENABLE_CDC   1
#define USB_ENABLE_MIDI  1
#define USB_ENABLE_HID   1
#define USB_ENABLE_MSC   1
```

## Files To Copy Into Another Project

Copy these folders/files into the target project:

```text
usb_*.c
usb_*.cpp
usb_*.h
global.h
tusb_config.h
usb_descriptors.c
usb_descriptors.h
usbd_control.c
tinyusb/
```

At minimum, the TinyUSB integration depends on:

```text
global.h
tusb_config.h
usb_descriptors.h
usb_audio.h
usb_cdc.h
usb_hid.h
usb_midi.h
usb_msc.h
usb_port.h
usb_sd.h
usb_audio.c
usb_cdc.c
usb_hid.c
usb_midi.c
usb_msc.cpp
usb_port.c
usb_sd.cpp
usb_descriptors.c
usbd_control.c
tinyusb/
```

## Minimum App Changes

In the target app, include and initialize the wrapper:

```cpp
#include "daisy_seed.h"
#include "usb_port.h"

extern "C" {
#include "global.h"
#if USB_ENABLE_AUDIO
#include "usb_audio.h"
#endif
#include "usb_sd.h"
}
```

Initialize Daisy, start audio if audio is enabled, then start TinyUSB:

```cpp
hw.Init(true);

#if USB_ENABLE_AUDIO
hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
hw.SetAudioBlockSize(48);
hw.StartAudio(AudioCallback);
#endif

GenericUSB_Init();
GenericUSB_SDInit();
```

Run TinyUSB from the main loop:

```cpp
while(1)
{
    uint32_t now = daisy::System::GetNow();
    GenericUSB_SetNowMs(now);
    GenericUSB_Task();
    GenericUSB_SDTask();
}
```

For audio, copy the `AudioCallback` pattern from `main.cpp`. The important
rule is that the Daisy audio callback only pushes/pops samples through the
local audio FIFOs. TinyUSB audio reads/writes stay in `GenericUSB_AudioTask()`
from the main loop.

## Makefile Requirements

The target project must compile:

- the copied root-level `usb_*.c`, `usb_*.cpp`, and descriptor files
- TinyUSB core/device/class files under `tinyusb/src`
- TinyUSB DWC2 port files:
  - `portable/synopsys/dwc2/dwc2_common.c`
  - `portable/synopsys/dwc2/dcd_dwc2.c`
- libDaisy startup/linker support

Required include paths include:

```make
-I.
-I$(LIBDAISY_DIR)
-I$(LIBDAISY_DIR)/src
-I$(LIBDAISY_DIR)/src/sys
-I$(LIBDAISY_DIR)/Drivers/CMSIS/Include
-I$(LIBDAISY_DIR)/Drivers/CMSIS/Device/ST/STM32H7xx/Include
-I$(LIBDAISY_DIR)/Drivers/STM32H7xx_HAL_Driver/Inc
-I$(LIBDAISY_DIR)/Middlewares/Third_Party/FatFs/src
-I$(TINYUSB_DIR)/src
-I$(TINYUSB_DIR)/hw
-I$(TINYUSB_DIR)/src/class/audio
-I$(TINYUSB_DIR)/src/class/cdc
-I$(TINYUSB_DIR)/src/class/hid
-I$(TINYUSB_DIR)/src/class/midi
-I$(TINYUSB_DIR)/src/class/msc
-I$(TINYUSB_DIR)/src/portable/synopsys/dwc2
```

Required defines for the external Daisy USB connector:

```make
-DCFG_TUSB_MCU=OPT_MCU_STM32H7
-DBOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
-DBOARD_DEVICE_RHPORT_NUM=1
-DBOARD_TUD_RHPORT=1
```

`BOARD_TUD_RHPORT=1` is the key setting. `0` selects OTG FS/internal USB and
will not use the external Daisy USB connector.

## USB Peripheral Ownership

This project gives TinyUSB direct ownership of OTG HS:

- pins: PB14/PB15
- alternate function: `GPIO_AF12_OTG2_FS`
- IRQs:
  - `OTG_HS_IRQHandler`
  - `OTG_HS_EP1_IN_IRQHandler`
  - `OTG_HS_EP1_OUT_IRQHandler`
  - `OTG_HS_WKUP_IRQHandler`

Do not also start libDaisy's USB CDC stack on the same external USB
peripheral. If both stacks own OTG HS, interrupt and endpoint handling will
fight.

## libDaisy Weak IRQ Handler Note

In the working libDaisy tree, `src/hid/usb.cpp` has weak handlers for the
OTG FS interrupt names:

```cpp
__attribute__((weak)) void OTG_FS_EP1_OUT_IRQHandler(void);
__attribute__((weak)) void OTG_FS_EP1_IN_IRQHandler(void);
__attribute__((weak)) void OTG_FS_IRQHandler(void);
```

That weak-handler change is not required for this project as tested, because
this project uses OTG HS on the external Daisy USB connector and provides its
own non-weak OTG HS handlers in `usb_port.c`.

It may matter if you move TinyUSB to OTG FS/internal USB or if another project
needs to override libDaisy's default FS USB handlers. In that case, weak FS
handlers let your project provide its own `OTG_FS_*IRQHandler()` functions
without linker conflicts.

For the least-change copy into another Daisy Seed project, keep TinyUSB on
external USB / OTG HS with `BOARD_TUD_RHPORT=1` and copy the OTG HS handlers
from `usb_port.c`.

## MSC SD Notes

USB MSC exposes the SD card as raw blocks to the host. The host is responsible
for parsing MBR/GPT/FAT. This is why the original GPT-style RHYTHM card can
list cleanly over USB MSC even when firmware-side FatFS did not like the
partition layout.

The MSC descriptor endpoint max packet is `64` because this is a full-speed
bulk endpoint. The TinyUSB MSC transfer buffer remains `512` bytes so callbacks
can process normal SD sectors.

The MSC write path handles partial-sector writes with read/modify/write. That
is required because TinyUSB can request byte ranges while the SD card writes
whole sectors.

## Build And Flash

From this project:

```bash
PATH=/home/pi/Developer/gcc-arm-none-eabi-10-2020-q4-major/bin:$PATH make -j2
PATH=/home/pi/Developer/gcc-arm-none-eabi-10-2020-q4-major/bin:$PATH make program
```

Basic host checks:

```bash
lsusb
lsblk -o NAME,SIZE,FSTYPE,LABEL,MODEL,MOUNTPOINTS
ls -l /dev/serial/by-id/
aplay -l
arecord -l
amidi -l
```
