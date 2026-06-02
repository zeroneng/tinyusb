TARGET = bypass

LIBDAISY_DIR ?= ../../libDaisy
TINYUSB_DIR ?= Middlewares/tinyusb
BUILD_DIR = build
USE_FATFS = 0

CC = arm-none-eabi-gcc
CXX = arm-none-eabi-g++
AS = arm-none-eabi-gcc -x assembler-with-cpp
OBJCOPY = arm-none-eabi-objcopy

MCU = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
C_DEFS = \
-DCORE_CM7 \
-DSTM32H750xx \
-DSTM32H750IB \
-DARM_MATH_CM7 \
-DHSE_VALUE=16000000 \
-DUSE_HAL_DRIVER \
-DUSE_FULL_LL_DRIVER \
-DCFG_TUSB_MCU=OPT_MCU_STM32H7 \
-DBOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED \
-DBOARD_DEVICE_RHPORT_NUM=1 \
-DBOARD_TUD_RHPORT=1

ifeq ($(USE_FATFS),1)
C_DEFS += -DFILEIO_ENABLE_FATFS_READER
endif

C_INCLUDES = \
-I. \
-Iinclude \
-I$(LIBDAISY_DIR) \
-I$(LIBDAISY_DIR)/src \
-I$(LIBDAISY_DIR)/src/sys \
-I$(LIBDAISY_DIR)/src/usbd \
-I$(LIBDAISY_DIR)/src/usbh \
-I$(LIBDAISY_DIR)/Drivers/CMSIS/Include \
-I$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Include \
-I$(LIBDAISY_DIR)/Drivers/CMSIS/Device/ST/STM32H7xx/Include \
-I$(LIBDAISY_DIR)/Drivers/STM32H7xx_HAL_Driver/Inc \
-I$(LIBDAISY_DIR)/Middlewares/ST/STM32_USB_Device_Library/Core/Inc \
-I$(LIBDAISY_DIR)/Middlewares/ST/STM32_USB_Host_Library/Core/Inc \
-I$(LIBDAISY_DIR)/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Inc \
-I$(LIBDAISY_DIR)/Middlewares/Third_Party/FatFs/src \
-I$(LIBDAISY_DIR)/core \
-I$(TINYUSB_DIR)/src \
-I$(TINYUSB_DIR)/hw \
-I$(TINYUSB_DIR)/src/common \
-I$(TINYUSB_DIR)/src/device \
-I$(TINYUSB_DIR)/src/class/audio \
-I$(TINYUSB_DIR)/src/class/cdc \
-I$(TINYUSB_DIR)/src/class/hid \
-I$(TINYUSB_DIR)/src/class/msc \
-I$(TINYUSB_DIR)/src/class/midi \
-I$(TINYUSB_DIR)/src/portable/synopsys/dwc2

CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) -include stm32h7xx.h -Ofast -Wall -Wno-missing-attributes -Wno-stringop-overflow -Wno-unused-variable -Wno-unused-function -fdata-sections -ffunction-sections -ffp-contract=fast -g -ggdb -MMD -MP -std=gnu11
CPPFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -fno-unwind-tables -fshort-enums -Wno-register -std=gnu++20
LDFLAGS = $(MCU) --specs=nano.specs --specs=nosys.specs -u _printf_float -T$(LIBDAISY_DIR)/core/STM32H750IB_flash.lds -L$(LIBDAISY_DIR)/build -ldaisy -lc -lm -lnosys -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections -Wl,--print-memory-usage -Wl,--allow-multiple-definition

CPP_SOURCES = \
main.cpp \
src/generic_usb_sd.cpp
C_SOURCES = \
src/generic_usb_port.c \
src/generic_usb_audio.c \
src/generic_usb_cdc.c \
src/generic_usb_hid.c \
src/generic_usb_msc.c \
src/generic_usb_midi.c \
src/usb_descriptors.c \
src/usbd_control.c \
$(TINYUSB_DIR)/src/tusb.c \
$(TINYUSB_DIR)/src/common/tusb_fifo.c \
$(TINYUSB_DIR)/src/device/usbd.c \
$(TINYUSB_DIR)/src/class/audio/audio_device.c \
$(TINYUSB_DIR)/src/class/cdc/cdc_device.c \
$(TINYUSB_DIR)/src/class/hid/hid_device.c \
$(TINYUSB_DIR)/src/class/msc/msc_device.c \
$(TINYUSB_DIR)/src/class/midi/midi_device.c \
$(TINYUSB_DIR)/src/portable/synopsys/dwc2/dwc2_common.c \
$(TINYUSB_DIR)/src/portable/synopsys/dwc2/dcd_dwc2.c \
$(LIBDAISY_DIR)/core/startup_stm32h750xx.c

ifeq ($(USE_FATFS),1)
C_SOURCES += \
$(LIBDAISY_DIR)/Middlewares/Third_Party/FatFs/src/option/unicode.c
endif

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o))) $(addprefix $(BUILD_DIR)/,$(notdir $(CPP_SOURCES:.cpp=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.cpp $(sort $(dir $(CPP_SOURCES)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile | $(BUILD_DIR)
	$(CXX) -c $(CPPFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(OBJCOPY) -O binary -S $< $@

clean:
	rm -rf $(BUILD_DIR)

program: $(BUILD_DIR)/$(TARGET).elf
	openocd -s /usr/local/share/openocd/scripts -f interface/stlink.cfg -f target/stm32h7x.cfg -c "program ./build/$(TARGET).elf verify reset exit"

-include $(wildcard $(BUILD_DIR)/*.d)
