#include <string.h>
#include "tusb.h"
#include "generic_usb_hid.h"
#include "global.h"
#include "usb_descriptors.h"

static tusb_desc_device_t const desc_device =
{
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,

  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor           = USB_DEVICE_VID,
  .idProduct          = USB_DEVICE_PID,
  .bcdDevice          = USB_DEVICE_BCD,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *)&desc_device;
}

enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_AUDIO,
  STRID_MIC,
  STRID_CDC,
  STRID_MIDI,
  STRID_HID,
};

#define EPNUM_AUDIO_OUT   0x01u
#define EPNUM_AUDIO_IN    0x82u
#define EPNUM_CDC_NOTIF   0x83u
#define EPNUM_CDC_OUT     0x04u
#define EPNUM_CDC_IN      0x84u
#define EPNUM_MIDI_OUT    0x06u
#define EPNUM_MIDI_IN     0x86u
#define EPNUM_HID_IN      0x85u

static uint8_t const desc_hid_report[] =
{
  HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
  HID_USAGE(HID_USAGE_DESKTOP_KEYBOARD),
  HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),
    HID_USAGE_MIN(224),
    HID_USAGE_MAX(231),
    HID_LOGICAL_MIN(0),
    HID_LOGICAL_MAX(1),
    HID_REPORT_SIZE(1),
    HID_REPORT_COUNT(8),
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

    HID_USAGE_PAGE(HID_USAGE_PAGE_LED),
    HID_USAGE_MIN(1),
    HID_USAGE_MAX(5),
    HID_REPORT_SIZE(1),
    HID_REPORT_COUNT(5),
    HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
    HID_REPORT_SIZE(3),
    HID_REPORT_COUNT(1),
    HID_OUTPUT(HID_CONSTANT),

    HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),
    HID_USAGE_MIN(0),
    HID_USAGE_MAX(103),
    HID_REPORT_SIZE(1),
    HID_REPORT_COUNT(104),
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
  HID_COLLECTION_END
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void)instance;
  return desc_hid_report;
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_AUDIO20_GENERIC_HEADSET_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t desc_configuration[] =
{
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, 0, 0x00, 100),

  TUD_AUDIO20_GENERIC_HEADSET_DESCRIPTOR(
    STRID_AUDIO,
    EPNUM_AUDIO_OUT,
    EPNUM_AUDIO_IN),

  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, CFG_TUD_CDC_RX_EPSIZE),

  TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, STRID_MIDI, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, CFG_TUD_MIDI_EP_BUFSIZE),

  TUD_HID_DESCRIPTOR(ITF_NUM_HID, STRID_HID, HID_ITF_PROTOCOL_NONE,
                     sizeof(desc_hid_report), EPNUM_HID_IN,
                     CFG_TUD_HID_EP_BUFSIZE, 10)
};


uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void)index;

  uint16_t const total_len = (uint16_t)sizeof(desc_configuration);
  desc_configuration[2] = (uint8_t)(total_len & 0xffu);
  desc_configuration[3] = (uint8_t)((total_len >> 8) & 0xffu);

  return desc_configuration;
}

static char const *string_desc_arr[] =
{
  (const char[]) { 0x09, 0x04 },
  USB_STRING_MANUFACTURER,
  USB_STRING_PRODUCT,
  USB_STRING_SERIAL,
  USB_STRING_AUDIO_SPEAKER,
  USB_STRING_AUDIO_MICROPHONE,
  USB_STRING_CDC,
  USB_STRING_MIDI,
  USB_STRING_HID
};

static uint16_t desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void)langid;
  size_t chr_count;

  if (index == STRID_LANGID) {
    memcpy(&desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else {
    if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;

    const char *str = string_desc_arr[index];
    chr_count = strlen(str);
    if (chr_count > 32) chr_count = 32;

    for (size_t i = 0; i < chr_count; i++) desc_str[1 + i] = (uint8_t)str[i];
  }

  desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2u * chr_count + 2u));
  return desc_str;
}
