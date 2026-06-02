#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Interface numbers */
enum {
    ITF_NUM_AUDIO_CONTROL = 0,
    ITF_NUM_AUDIO_STREAMING_SPK,
    ITF_NUM_AUDIO_STREAMING_MIC,
    ITF_NUM_CDC,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

/* ── USB IDs ────────────────────────────────────────────────────────────── */
#define JAMMATE_USB_VID  0x0483u
#define JAMMATE_USB_PID  0x5787u

/* ── Audio entity IDs ───────────────────────────────────────────────────── */
#define UAC2_ENTITY_SPK_INPUT_TERMINAL   0x01u
#define UAC2_ENTITY_SPK_FEATURE_UNIT     0x02u
#define UAC2_ENTITY_SPK_OUTPUT_TERMINAL  0x03u
#define UAC2_ENTITY_CLOCK                0x04u
#define UAC2_ENTITY_MIC_INPUT_TERMINAL   0x11u
#define UAC2_ENTITY_MIC_OUTPUT_TERMINAL  0x13u

/* NON_PREDEFINED: maximum compatibility with older TinyUSB headers and
 * Windows usbaudio2.sys. A bad channel bitmap is a common Code 10 cause. */
#ifndef JAMMATE_UAC2_CHANNEL_CONFIG_STEREO
#define JAMMATE_UAC2_CHANNEL_CONFIG_STEREO  AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED
#endif

/* ── Descriptor byte-length ─────────────────────────────────────────────── */
#define TUD_AUDIO20_JAMMATE_HEADSET_DESC_LEN  (   \
    TUD_AUDIO20_DESC_IAD_LEN                      \
  + TUD_AUDIO20_DESC_STD_AC_LEN                   \
  + TUD_AUDIO20_DESC_CS_AC_LEN                    \
  + TUD_AUDIO20_DESC_CLK_SRC_LEN                  \
  + TUD_AUDIO20_DESC_INPUT_TERM_LEN               \
  + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2)          \
  + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN              \
  + TUD_AUDIO20_DESC_INPUT_TERM_LEN               \
  + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN              \
  + TUD_AUDIO20_DESC_STD_AS_LEN                   \
  + TUD_AUDIO20_DESC_STD_AS_LEN                   \
  + TUD_AUDIO20_DESC_CS_AS_INT_LEN                \
  + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN            \
  + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN            \
  + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN             \
  + TUD_AUDIO20_DESC_STD_AS_LEN                   \
  + TUD_AUDIO20_DESC_STD_AS_LEN                   \
  + TUD_AUDIO20_DESC_CS_AS_INT_LEN                \
  + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN            \
  + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN            \
  + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)

/* ── Audio descriptor macro ─────────────────────────────────────────────── */
/*  Speaker OUT : ADAPTIVE isochronous  — identical to R5f
 *  Mic    IN   : stereo ASYNCHRONOUS isochronous              */
#define TUD_AUDIO20_JAMMATE_HEADSET_DESCRIPTOR(_stridx, _epout, _epin)        \
  TUD_AUDIO20_DESC_IAD(ITF_NUM_AUDIO_CONTROL, 3, 0x00),                       \
  TUD_AUDIO20_DESC_STD_AC(ITF_NUM_AUDIO_CONTROL, 0x00, _stridx),              \
  TUD_AUDIO20_DESC_CS_AC(0x0200, AUDIO20_FUNC_HEADSET,                        \
    (TUD_AUDIO20_DESC_CLK_SRC_LEN + TUD_AUDIO20_DESC_INPUT_TERM_LEN +         \
     TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2) + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN  \
   + TUD_AUDIO20_DESC_INPUT_TERM_LEN + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN),     \
    AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS),                                 \
  TUD_AUDIO20_DESC_CLK_SRC(UAC2_ENTITY_CLOCK, 3, 7, 0x00, 0x00),             \
  TUD_AUDIO20_DESC_INPUT_TERM(UAC2_ENTITY_SPK_INPUT_TERMINAL,                 \
    AUDIO_TERM_TYPE_USB_STREAMING, 0x00, UAC2_ENTITY_CLOCK,                   \
    0x02, JAMMATE_UAC2_CHANNEL_CONFIG_STEREO, 0x00, 0x0000, 0x00),            \
  TUD_AUDIO20_DESC_FEATURE_UNIT(UAC2_ENTITY_SPK_FEATURE_UNIT,                 \
    UAC2_ENTITY_SPK_INPUT_TERMINAL, 0x00,                                      \
    (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS) |                 \
    (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS),                \
    0x00000000u, 0x00000000u),                                                 \
  TUD_AUDIO20_DESC_OUTPUT_TERM(UAC2_ENTITY_SPK_OUTPUT_TERMINAL,               \
    AUDIO_TERM_TYPE_OUT_HEADPHONES, 0x00,                                      \
    UAC2_ENTITY_SPK_FEATURE_UNIT, UAC2_ENTITY_CLOCK, 0x0000, 0x00),           \
  TUD_AUDIO20_DESC_INPUT_TERM(UAC2_ENTITY_MIC_INPUT_TERMINAL,                 \
    AUDIO_TERM_TYPE_IN_GENERIC_MIC, 0x00, UAC2_ENTITY_CLOCK,                  \
    0x02, JAMMATE_UAC2_CHANNEL_CONFIG_STEREO, 0x00, 0x0000, 0x00),         \
  TUD_AUDIO20_DESC_OUTPUT_TERM(UAC2_ENTITY_MIC_OUTPUT_TERMINAL,               \
    AUDIO_TERM_TYPE_USB_STREAMING, 0x00,                                       \
    UAC2_ENTITY_MIC_INPUT_TERMINAL, UAC2_ENTITY_CLOCK, 0x0000, 0x00),         \
  TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING_SPK, 0x00, 0x00, 0x04), \
  TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING_SPK, 0x01, 0x01, 0x04), \
  TUD_AUDIO20_DESC_CS_AS_INT(UAC2_ENTITY_SPK_INPUT_TERMINAL,                  \
    AUDIO20_CTRL_NONE, AUDIO20_FORMAT_TYPE_I, AUDIO20_DATA_FORMAT_TYPE_I_PCM, \
    CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX,                                        \
    JAMMATE_UAC2_CHANNEL_CONFIG_STEREO, 0x00),                                 \
  TUD_AUDIO20_DESC_TYPE_I_FORMAT(                                              \
    CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX,                      \
    CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX),                              \
  TUD_AUDIO20_DESC_STD_AS_ISO_EP(_epout,                                       \
    (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS |                                \
              (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE |                              \
              (uint8_t)TUSB_ISO_EP_ATT_DATA),                                  \
    CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT, 0x01),                           \
  TUD_AUDIO20_DESC_CS_AS_ISO_EP(                                               \
    AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK,                          \
    AUDIO20_CTRL_NONE,                                                          \
    AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, 0x0001),              \
  TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING_MIC, 0x00, 0x00, 0x05), \
  TUD_AUDIO20_DESC_STD_AS_INT(ITF_NUM_AUDIO_STREAMING_MIC, 0x01, 0x01, 0x05), \
  TUD_AUDIO20_DESC_CS_AS_INT(UAC2_ENTITY_MIC_OUTPUT_TERMINAL,                 \
    AUDIO20_CTRL_NONE, AUDIO20_FORMAT_TYPE_I, AUDIO20_DATA_FORMAT_TYPE_I_PCM, \
    CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX,                                        \
    JAMMATE_UAC2_CHANNEL_CONFIG_STEREO, 0x00),                              \
  TUD_AUDIO20_DESC_TYPE_I_FORMAT(                                              \
    CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX,                      \
    CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX),                              \
  TUD_AUDIO20_DESC_STD_AS_ISO_EP(_epin,                                        \
    (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS |                                \
              (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS |                          \
              (uint8_t)TUSB_ISO_EP_ATT_DATA),                                  \
    CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN, 0x01),                            \
  TUD_AUDIO20_DESC_CS_AS_ISO_EP(                                               \
    AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK,                          \
    AUDIO20_CTRL_NONE,                                                          \
    AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, 0x0000)

#ifdef __cplusplus
}
#endif
#endif /* USB_DESCRIPTORS_H_ */
