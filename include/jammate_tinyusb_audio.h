#ifndef JAMMATE_TINYUSB_AUDIO_H_
#define JAMMATE_TINYUSB_AUDIO_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Audio constants ────────────────────────────────────────────────────── */
#define JAMMATE_AUDIO_SAMPLE_RATE     48000u
#define JAMMATE_AUDIO_SPK_CHANNELS        2u
#define JAMMATE_AUDIO_MIC_CHANNELS        2u
#define JAMMATE_AUDIO_BYTES_PER_SAMPLE    2u
#define JAMMATE_AUDIO_FRAMES_PER_MS      48u   /* at 48 kHz */

/* ── Init / Task ────────────────────────────────────────────────────────── */
void     JamMate_TinyUSB_AudioInit(void);

/* Call from main loop alongside tud_task().
 * R6: performs all TinyUSB audio read/write work here (task context ONLY).
 * Never call from DMA audio callback or any ISR.                          */
void     JamMate_TinyUSB_AudioTask(void);

/* ── Audio interface ────────────────────────────────────────────────────── */
/* Call ONLY from Daisy DMA audio callback (ISR context).
 *
 * AudioIF_PushSamples:
 *   Push stereo interleaved s16 into the local stereo mic FIFO.
 *   stereo_samples[2*i+0]=L, stereo_samples[2*i+1]=R.
 *   TinyUSB write happens later
 *   in JamMate_TinyUSB_AudioTask().
 *
 * AudioIF_PopSamples:
 *   Pop stereo interleaved s16 from speaker safety FIFO.
 *   Returns frames filled from USB data (0 = silent / priming).
 *   Repeats last-good sample on underflow (no hard click).
 *   R6: does NOT call any TinyUSB API — USB I/O is task-only.     */
void     AudioIF_PushSamples(const int16_t *stereo_samples, uint32_t num_frames);
uint32_t AudioIF_PopSamples (      int16_t *stereo_samples, uint32_t num_frames);

/* ── Diagnostics (read-only, any context) ───────────────────────────────── */
uint32_t JamMate_USB_RxPackets(void);
uint32_t JamMate_USB_TxPackets(void);
uint32_t JamMate_USB_SetItfCount(void);
uint32_t JamMate_USB_RxDoneCount(void);
uint32_t JamMate_USB_TxDoneCount(void);
uint32_t JamMate_USB_TxShort(void);
uint32_t JamMate_USB_RxShort(void);
uint32_t JamMate_USB_SpkUnderflows(void);
uint32_t JamMate_USB_SpkOverflows(void);
uint32_t JamMate_USB_SpkFifoFrames(void);
uint32_t JamMate_USB_FbCount(void);
uint32_t JamMate_USB_FbValue(void);

#ifdef __cplusplus
}
#endif
#endif /* JAMMATE_TINYUSB_AUDIO_H_ */
