#ifndef GENERIC_USB_AUDIO_H_
#define GENERIC_USB_AUDIO_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Audio constants ────────────────────────────────────────────────────── */
#define GENERIC_USB_AUDIO_SAMPLE_RATE     48000u
#define GENERIC_USB_AUDIO_SPK_CHANNELS        2u
#define GENERIC_USB_AUDIO_MIC_CHANNELS        2u
#define GENERIC_USB_AUDIO_BYTES_PER_SAMPLE    2u
#define GENERIC_USB_AUDIO_FRAMES_PER_MS      48u   /* at 48 kHz */

/* ── Init / Task ────────────────────────────────────────────────────────── */
void     GenericUSB_AudioInit(void);

/* Call from main loop alongside tud_task().
 * Performs all TinyUSB audio read/write work here (task context ONLY).
 * Never call from DMA audio callback or any ISR.                          */
void     GenericUSB_AudioTask(void);

/* ── Audio interface ────────────────────────────────────────────────────── */
/* Call ONLY from Daisy DMA audio callback (ISR context).
 *
 * AudioIF_PushSamples:
 *   Push stereo interleaved s16 into the local stereo mic FIFO.
 *   stereo_samples[2*i+0]=L, stereo_samples[2*i+1]=R.
 *   TinyUSB write happens later
 *   in GenericUSB_AudioTask().
 *
 * AudioIF_PopSamples:
 *   Pop stereo interleaved s16 from speaker safety FIFO.
 *   Returns frames filled from USB data (0 = silent / priming).
 *   Repeats last-good sample on underflow (no hard click).
 *   Does NOT call any TinyUSB API — USB I/O is task-only.     */
void     AudioIF_PushSamples(const int16_t *stereo_samples, uint32_t num_frames);
uint32_t AudioIF_PopSamples (      int16_t *stereo_samples, uint32_t num_frames);

/* ── Diagnostics (read-only, any context) ───────────────────────────────── */
uint32_t GenericUSB_Audio_RxPackets(void);
uint32_t GenericUSB_Audio_TxPackets(void);
uint32_t GenericUSB_Audio_SetItfCount(void);
uint32_t GenericUSB_Audio_RxDoneCount(void);
uint32_t GenericUSB_Audio_TxDoneCount(void);
uint32_t GenericUSB_Audio_TxShort(void);
uint32_t GenericUSB_Audio_RxShort(void);
uint32_t GenericUSB_Audio_SpkUnderflows(void);
uint32_t GenericUSB_Audio_SpkOverflows(void);
uint32_t GenericUSB_Audio_SpkFifoFrames(void);
uint32_t GenericUSB_Audio_FbCount(void);
uint32_t GenericUSB_Audio_FbValue(void);

#ifdef __cplusplus
}
#endif
#endif /* GENERIC_USB_AUDIO_H_ */
