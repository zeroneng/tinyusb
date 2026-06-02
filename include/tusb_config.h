#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT        0
#endif
#ifndef BOARD_DEVICE_RHPORT_NUM
#define BOARD_DEVICE_RHPORT_NUM BOARD_TUD_RHPORT
#endif
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED     OPT_MODE_FULL_SPEED
#endif
#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU            OPT_MCU_STM32H7
#endif
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS             OPT_OS_NONE
#endif
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG          0
#endif

#define CFG_TUD_ENABLED         1
#define CFG_TUD_MAX_SPEED       OPT_MODE_FULL_SPEED
#define CFG_TUD_VBUS_DETECT_HW  0

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN      __attribute__((aligned(4)))
#endif
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  64
#endif
#ifndef CFG_TUD_ENDPOINT0_BUFSIZE
#define CFG_TUD_ENDPOINT0_BUFSIZE CFG_TUD_ENDPOINT0_SIZE
#endif

/* Class enables */
#define CFG_TUD_CDC     1
#define CFG_TUD_MSC     0
#define CFG_TUD_HID     1
#define CFG_TUD_MIDI    0
#define CFG_TUD_AUDIO   1
#define CFG_TUD_VENDOR  0

/* ── JamMate R6s: UAC2 + CDC ACM, Daisy Seed / STM32H7 full-speed ─────────
 *
 *  Speaker OUT : stereo s16, 48 kHz, ADAPTIVE isochronous (no feedback EP)
 *  Mic    IN   : stereo s16, 48 kHz, ASYNCHRONOUS isochronous
 *  CDC         : standard ACM serial (debug / control)
 *
 *  R6 changes vs R5f
 *  ------------------
 *  FIX-1  All TinyUSB audio read/write calls moved out of the Daisy audio
 *         callback. AudioIF_PushSamples/PopSamples now use local FIFOs;
 *         JamMate_TinyUSB_AudioTask performs tud_audio_n_read/write in
 *         main-loop context only.
 *
 *  FIX-2  EP_OUT_SW_BUF reduced from 48x to 8x EP size.
 *         The 48x buffer was masking FIX-1. With ADAPTIVE sync the host
 *         self-paces; 8x (~8 ms) is sufficient.
 *
 *  FIX-3  PREFILL raised from 3 ms to 16 ms to absorb tud_task() jitter
 *         without triggering re-prime clicks.
 *
 *  CLEAN  Dead tud_audio_feedback_params_cb() removed (FEEDBACK_EP=0).
 *
 *  Everything else (descriptors, EP map, CDC, API, volume/mute, clock
 *  entity, all callback signatures) is identical to R5f.
 * ────────────────────────────────────────────────────────────────────────*/

#define CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP               0
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS                  1
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE            48000

/* TX = device -> host (microphone) */
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX              2
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX      2
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_TX              16
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX \
        CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX \
        CFG_TUD_AUDIO_FUNC_1_RESOLUTION_TX

/* RX = host -> device (speaker) */
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX              2
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX      2
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX              16
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX \
        CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX \
        CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX

#define CFG_TUD_AUDIO_ENABLE_EP_OUT                     1
#define CFG_TUD_AUDIO_ENABLE_EP_IN                      1
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                0   /* ADAPTIVE — no feedback EP */
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION 0
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL                1

/* 192 B = 48 frames x 2 ch x 2 B/sample at 48 kHz */
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT        192u
/* 196 B = 49 stereo frames x 2 ch x 2 B/sample (+1 headroom packet) */
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN         196u

#define CFG_TUD_AUDIO_FUNC_1_EP_SZ_IN    CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN
#define CFG_TUD_AUDIO_FUNC_1_EP_SZ_OUT   CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT
#define CFG_TUD_AUDIO_EP_SZ_IN           CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN
#define CFG_TUD_AUDIO_EP_SZ_OUT          CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT
#define CFG_TUD_AUDIO_EPSIZE_IN          CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN
#define CFG_TUD_AUDIO_EPSIZE_OUT         CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX  CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT

/* FIX-2: 8x instead of R5f's 48x */
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ  (12u * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ  (8u * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT)

#define CFG_TUD_AUDIO_TX_FIFO_SIZE   CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ
#define CFG_TUD_AUDIO_RX_FIFO_SIZE   CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ
#define CFG_TUD_AUDIO_TX_FIFO_COUNT  1
#define CFG_TUD_AUDIO_RX_FIFO_COUNT  1

/* CDC */
#define CFG_TUD_CDC_RX_BUFSIZE  256
#define CFG_TUD_CDC_TX_BUFSIZE  256
#define CFG_TUD_CDC_RX_EPSIZE    64
#define CFG_TUD_CDC_TX_EPSIZE    64

/* HID */
#define CFG_TUD_HID_EP_BUFSIZE   64

#ifdef __cplusplus
}
#endif
#endif /* TUSB_CONFIG_H_ */
