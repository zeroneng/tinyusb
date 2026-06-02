/* jammate_tinyusb_audio.c — JamMate R6 stereo-mic UAC2 audio control + data plane
 *
 * R6 = R5f API-compatible, Windows-start-safe UAC2/no-feedback descriptor,
 *      but with all TinyUSB audio read/write calls moved out of the Daisy
 *      audio callback.
 *
 * Context contract:
 *   - AudioIF_PushSamples() and AudioIF_PopSamples() are ISR/audio-callback safe.
 *     They touch only local SPSC ring buffers and never call TinyUSB.
 *   - JamMate_TinyUSB_AudioTask() runs from the main loop after tud_task().
 *     It is the only place that calls tud_audio_n_read() / tud_audio_n_write().
 *
 * This version adds a conservative stereo-mic 47/48/49 packet guard:
 *   - sends at most one mic packet per JamMate_TinyUSB_AudioTask() pass
 *   - never emits an underfilled packet below 47 frames
 *   - chooses 47 / 48 / 49 stereo frames from mic FIFO depth thresholds
 */

#include "jammate_tinyusb_audio.h"

#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "tusb.h"
#include "usb_descriptors.h"

/* ── Streaming state ────────────────────────────────────────────────────── */
static volatile bool spk_streaming = false;
static volatile bool mic_streaming = false;

/* ── Diagnostics ────────────────────────────────────────────────────────── */
volatile uint32_t jammate_usb_rx_packets    = 0;
volatile uint32_t jammate_usb_tx_packets    = 0;
volatile uint32_t jammate_usb_set_itf_count = 0;
volatile uint32_t jammate_usb_rx_done_count = 0;
volatile uint32_t jammate_usb_tx_done_count = 0;
volatile uint32_t jammate_usb_tx_short      = 0;
volatile uint32_t jammate_usb_rx_short      = 0;
volatile uint32_t jammate_usb_fb_count      = 0;   /* API compat: no feedback EP in R6 */
volatile uint32_t jammate_usb_fb_value      = 0;   /* API compat */

static volatile uint32_t jammate_usb_spk_underflows = 0;
static volatile uint32_t jammate_usb_spk_overflows  = 0;
static volatile uint32_t jammate_usb_mic_overflows  = 0;
static volatile uint32_t jammate_usb_mic_underflows = 0;

/* ── Volume / mute / clock state ───────────────────────────────────────── */
static uint8_t  mute[3]   = {0, 0, 0};
static int16_t  volume[3] = {0, 0, 0};
static uint32_t current_sample_rate = JAMMATE_AUDIO_SAMPLE_RATE;

/* ── SPSC ring helpers ──────────────────────────────────────────────────── */
static inline uint32_t ring_next(uint32_t v, uint32_t cap)
{
  v++;
  return (v >= cap) ? 0u : v;
}

static inline uint32_t ring_count(uint32_t rd, uint32_t wr, uint32_t cap)
{
  return (wr >= rd) ? (wr - rd) : (cap - rd + wr);
}

/* ── Speaker FIFO: producer=main/task, consumer=audio ISR ──────────────── */
#define JAMMATE_SPK_FIFO_FRAMES      (JAMMATE_AUDIO_FRAMES_PER_MS * 32u)
#define JAMMATE_SPK_FIFO_CAP         (JAMMATE_SPK_FIFO_FRAMES + 1u)
#define JAMMATE_SPK_PREFILL_FRAMES   (JAMMATE_AUDIO_FRAMES_PER_MS * 16u)
#define JAMMATE_SPK_DRAIN_FRAMES     (JAMMATE_AUDIO_FRAMES_PER_MS *  4u)

static int16_t  spk_fifo[JAMMATE_SPK_FIFO_CAP * JAMMATE_AUDIO_SPK_CHANNELS];
static volatile uint32_t spk_fifo_rd = 0;
static volatile uint32_t spk_fifo_wr = 0;
static volatile bool     spk_fifo_primed = false;
static int16_t  spk_last_l = 0;
static int16_t  spk_last_r = 0;

static void spk_fifo_reset(void)
{
  spk_fifo_rd = 0;
  spk_fifo_wr = 0;
  spk_fifo_primed = false;
  spk_last_l = 0;
  spk_last_r = 0;
}

static uint32_t spk_fifo_count(void)
{
  return ring_count(spk_fifo_rd, spk_fifo_wr, JAMMATE_SPK_FIFO_CAP);
}

static bool spk_fifo_push_frame(int16_t l, int16_t r)
{
  uint32_t wr = spk_fifo_wr;
  uint32_t next = ring_next(wr, JAMMATE_SPK_FIFO_CAP);

  if (next == spk_fifo_rd) {
    jammate_usb_spk_overflows++;
    return false; /* drop newest: keeps ISR side lock-free */
  }

  spk_fifo[2u * wr + 0u] = l;
  spk_fifo[2u * wr + 1u] = r;
  spk_fifo_wr = next;
  return true;
}

static bool spk_fifo_pop_frame(int16_t *l, int16_t *r)
{
  uint32_t rd = spk_fifo_rd;

  if (rd == spk_fifo_wr) return false;

  *l = spk_fifo[2u * rd + 0u];
  *r = spk_fifo[2u * rd + 1u];
  spk_fifo_rd = ring_next(rd, JAMMATE_SPK_FIFO_CAP);

  spk_last_l = *l;
  spk_last_r = *r;
  return true;
}

/* ── Stereo mic FIFO: producer=audio ISR, consumer=main/task ─────────────── */
#define JAMMATE_MIC_FIFO_FRAMES      (JAMMATE_AUDIO_FRAMES_PER_MS * 32u)
#define JAMMATE_MIC_FIFO_CAP         (JAMMATE_MIC_FIFO_FRAMES + 1u)

/* Stereo mic IN packet guard:
 *   47 stereo frames = 188 bytes
 *   48 stereo frames = 192 bytes
 *   49 stereo frames = 196 bytes
 *
 * Keep descriptor/config EP_IN max at 196 bytes for stereo 16-bit.
 */
#define MIC_PACKET_MIN_FRAMES        47u
#define MIC_PACKET_NOM_FRAMES        48u
#define MIC_PACKET_MAX_FRAMES        49u

/* Target FIFO depth around 8 ms. Use hysteresis around that target.
 * Low depth: send 47 frames to let FIFO refill.
 * High depth: send 49 frames to drain extra.
 */
#define MIC_FIFO_LOW_FRAMES          (JAMMATE_AUDIO_FRAMES_PER_MS * 6u)   /* 288 frames / 6 ms */
#define MIC_FIFO_HIGH_FRAMES         (JAMMATE_AUDIO_FRAMES_PER_MS * 10u)  /* 480 frames / 10 ms */

static int16_t  mic_fifo[JAMMATE_MIC_FIFO_CAP * JAMMATE_AUDIO_MIC_CHANNELS];
static volatile uint32_t mic_fifo_rd = 0; /* frame index */
static volatile uint32_t mic_fifo_wr = 0; /* frame index */

static void mic_fifo_reset(void)
{
  mic_fifo_rd = 0;
  mic_fifo_wr = 0;
}

static uint32_t mic_fifo_count(void)
{
  return ring_count(mic_fifo_rd, mic_fifo_wr, JAMMATE_MIC_FIFO_CAP);
}

static bool mic_fifo_push_frame(int16_t l, int16_t r)
{
  uint32_t wr = mic_fifo_wr;
  uint32_t next = ring_next(wr, JAMMATE_MIC_FIFO_CAP);

  if (next == mic_fifo_rd) {
    jammate_usb_mic_overflows++;
    jammate_usb_tx_short++;
    return false;
  }

#if JAMMATE_AUDIO_MIC_CHANNELS == 2
  mic_fifo[JAMMATE_AUDIO_MIC_CHANNELS * wr + 0u] = l;
  mic_fifo[JAMMATE_AUDIO_MIC_CHANNELS * wr + 1u] = r;
#else
  mic_fifo[JAMMATE_AUDIO_MIC_CHANNELS * wr + 0u] = (int16_t)(((int32_t)l + (int32_t)r) / 2);
#endif

  mic_fifo_wr = next;
  return true;
}

static uint32_t mic_fifo_pop_frames(int16_t *dst, uint32_t max_frames)
{
  uint32_t n = 0;

  while (n < max_frames) {
    uint32_t rd = mic_fifo_rd;
    if (rd == mic_fifo_wr) break;

#if JAMMATE_AUDIO_MIC_CHANNELS == 2
    dst[JAMMATE_AUDIO_MIC_CHANNELS * n + 0u] = mic_fifo[JAMMATE_AUDIO_MIC_CHANNELS * rd + 0u];
    dst[JAMMATE_AUDIO_MIC_CHANNELS * n + 1u] = mic_fifo[JAMMATE_AUDIO_MIC_CHANNELS * rd + 1u];
#else
    dst[JAMMATE_AUDIO_MIC_CHANNELS * n + 0u] = mic_fifo[JAMMATE_AUDIO_MIC_CHANNELS * rd + 0u];
#endif

    mic_fifo_rd = ring_next(rd, JAMMATE_MIC_FIFO_CAP);
    n++;
  }

  return n;
}

/* ── TinyUSB task-side drains/flushes ───────────────────────────────────── */
static void spk_drain_tinyusb(void)
{
  if (!tud_mounted() || !tud_audio_n_mounted(0) || !spk_streaming) return;

  static int16_t tmp[JAMMATE_SPK_DRAIN_FRAMES * JAMMATE_AUDIO_SPK_CHANNELS];

  for (uint32_t pass = 0; pass < 8u; pass++) {
    uint16_t got = tud_audio_n_read(0, tmp, (uint16_t)sizeof(tmp));
    if (got == 0) break;

    uint32_t frames = (uint32_t)got /
      (JAMMATE_AUDIO_SPK_CHANNELS * sizeof(int16_t));

    for (uint32_t i = 0; i < frames; i++) {
      (void)spk_fifo_push_frame(tmp[2u * i + 0u], tmp[2u * i + 1u]);
    }

    jammate_usb_rx_packets++;
    if (got < (uint16_t)sizeof(tmp)) break;
  }
}

static void mic_flush_tinyusb(void)
{
  if (!tud_mounted() || !tud_audio_n_mounted(0) || !mic_streaming) {
    return;
  }

  /* Sized for worst-case 49-frame packet. */
  static int16_t tmp[MIC_PACKET_MAX_FRAMES * JAMMATE_AUDIO_MIC_CHANNELS];

  uint32_t depth = mic_fifo_count();

  /* Do not send arbitrary short packets. Wait until we can send at least
   * the intended low-rate 47-frame packet.
   */
  if (depth < MIC_PACKET_MIN_FRAMES) {
    jammate_usb_mic_underflows++;
    return;
  }

  uint32_t frames = MIC_PACKET_NOM_FRAMES;

  if (depth > MIC_FIFO_HIGH_FRAMES) {
    frames = MIC_PACKET_MAX_FRAMES;
  } else if (depth < MIC_FIFO_LOW_FRAMES) {
    frames = MIC_PACKET_MIN_FRAMES;
  }

  /* This should only matter immediately after a race with the ISR. Keep packet
   * size in the legal 47/48/49 zone whenever possible.
   */
  if (depth < frames) {
    frames = depth;
  }

  uint32_t n = mic_fifo_pop_frames(tmp, frames);
  if (n < MIC_PACKET_MIN_FRAMES) {
    /* Should be rare due to the depth check; do not emit a too-short packet. */
    jammate_usb_mic_underflows++;
    return;
  }

  uint16_t bytes = (uint16_t)(n * JAMMATE_AUDIO_MIC_CHANNELS * sizeof(int16_t));

  uint16_t written = tud_audio_n_write(0, tmp, bytes);

  if (written == bytes) {
    jammate_usb_tx_packets++;
  } else {
    jammate_usb_tx_short++;
  }
}

/* ── Public init / task ─────────────────────────────────────────────────── */
void JamMate_TinyUSB_AudioInit(void)
{
  spk_streaming = false;
  mic_streaming = false;
  current_sample_rate = JAMMATE_AUDIO_SAMPLE_RATE;

  spk_fifo_reset();
  mic_fifo_reset();

  jammate_usb_rx_packets     = 0;
  jammate_usb_tx_packets     = 0;
  jammate_usb_set_itf_count  = 0;
  jammate_usb_rx_done_count  = 0;
  jammate_usb_tx_done_count  = 0;
  jammate_usb_tx_short       = 0;
  jammate_usb_rx_short       = 0;
  jammate_usb_fb_count       = 0;
  jammate_usb_fb_value       = (uint32_t)((JAMMATE_AUDIO_SAMPLE_RATE / 1000u) << 16u);
  jammate_usb_spk_underflows = 0;
  jammate_usb_spk_overflows  = 0;
  jammate_usb_mic_overflows  = 0;
  jammate_usb_mic_underflows = 0;
}

void JamMate_TinyUSB_AudioTask(void)
{
  spk_drain_tinyusb();
  mic_flush_tinyusb();
}

/* ── Audio-callback API: no TinyUSB calls here ──────────────────────────── */
void AudioIF_PushSamples(const int16_t *stereo_samples, uint32_t num_frames)
{
  if (!stereo_samples || num_frames == 0) return;
  if (!mic_streaming) return;

  for (uint32_t i = 0; i < num_frames; i++) {
    int16_t l = stereo_samples[2u * i + 0u];
    int16_t r = stereo_samples[2u * i + 1u];
    (void)mic_fifo_push_frame(l, r);
  }
}

uint32_t AudioIF_PopSamples(int16_t *stereo_samples, uint32_t num_frames)
{
  if (!stereo_samples || num_frames == 0) return 0;

  uint32_t want_bytes = num_frames * JAMMATE_AUDIO_SPK_CHANNELS * sizeof(int16_t);

  if (!spk_streaming) {
    memset(stereo_samples, 0, want_bytes);
    spk_fifo_reset();
    return 0;
  }

  if (!spk_fifo_primed) {
    if (spk_fifo_count() < JAMMATE_SPK_PREFILL_FRAMES) {
      memset(stereo_samples, 0, want_bytes);
      return 0;
    }
    spk_fifo_primed = true;
  }

  uint32_t frames_out = 0;

  for (uint32_t i = 0; i < num_frames; i++) {
    int16_t l, r;

    if (spk_fifo_pop_frame(&l, &r)) {
      stereo_samples[2u * i + 0u] = l;
      stereo_samples[2u * i + 1u] = r;
      frames_out++;
    } else {
      stereo_samples[2u * i + 0u] = spk_last_l;
      stereo_samples[2u * i + 1u] = spk_last_r;
      jammate_usb_rx_short++;
      jammate_usb_spk_underflows++;
      spk_fifo_primed = false;
    }
  }

  return frames_out;
}

/* ── TinyUSB audio callbacks ────────────────────────────────────────────── */
bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t n_bytes_received,
                           uint8_t func_id, uint8_t ep_out,
                           uint8_t cur_alt_setting)
{
  (void)rhport;
  (void)n_bytes_received;
  (void)func_id;
  (void)ep_out;
  (void)cur_alt_setting;

  jammate_usb_rx_done_count++;
  return true;
}

bool tud_audio_tx_done_isr(uint8_t rhport, uint16_t n_bytes_sent,
                           uint8_t func_id, uint8_t ep_in,
                           uint8_t cur_alt_setting)
{
  (void)rhport;
  (void)n_bytes_sent;
  (void)func_id;
  (void)ep_in;
  (void)cur_alt_setting;

  jammate_usb_tx_done_count++;
  return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport,
                                   tusb_control_request_t const *p_request)
{
  (void)rhport;

  uint8_t const itf = tu_u16_low(p_request->wIndex);
  uint8_t const alt = tu_u16_low(p_request->wValue);

  if (itf == ITF_NUM_AUDIO_STREAMING_SPK && alt == 0) {
    spk_streaming = false;
    spk_fifo_reset();
    tud_audio_n_clear_ep_out_ff(0);
  }

  if (itf == ITF_NUM_AUDIO_STREAMING_MIC && alt == 0) {
    mic_streaming = false;
    mic_fifo_reset();
    tud_audio_n_clear_ep_in_ff(0);
  }

  return true;
}

bool tud_audio_set_itf_cb(uint8_t rhport,
                          tusb_control_request_t const *p_request)
{
  (void)rhport;

  uint8_t const itf = tu_u16_low(p_request->wIndex);
  uint8_t const alt = tu_u16_low(p_request->wValue);

  jammate_usb_set_itf_count++;

  if (itf == ITF_NUM_AUDIO_STREAMING_SPK) {
    spk_streaming = (alt != 0);
    spk_fifo_reset();
  }

  if (itf == ITF_NUM_AUDIO_STREAMING_MIC) {
    mic_streaming = (alt != 0);
    mic_fifo_reset();
  }

  return true;
}

/* ── UAC2 clock + feature-unit control ──────────────────────────────────── */
static bool uac2_clock_get_request(uint8_t rhport, audio20_control_request_t const *req)
{
  if (req->bEntityID != UAC2_ENTITY_CLOCK) return false;

  if (req->bControlSelector == AUDIO20_CS_CTRL_SAM_FREQ) {
    if (req->bRequest == AUDIO20_CS_REQ_CUR) {
      audio20_control_cur_4_t cur = { (int32_t)tu_htole32((int32_t)current_sample_rate) };
      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)req,
                                                        &cur, sizeof(cur));
    }

    if (req->bRequest == AUDIO20_CS_REQ_RANGE) {
      audio20_control_range_4_n_t(1) range = { .wNumSubRanges = tu_htole16(1) };
      range.subrange[0].bMin = (int32_t)tu_htole32(JAMMATE_AUDIO_SAMPLE_RATE);
      range.subrange[0].bMax = (int32_t)tu_htole32(JAMMATE_AUDIO_SAMPLE_RATE);
      range.subrange[0].bRes = 0;

      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)req,
                                                        &range, sizeof(range));
    }
  }

  if (req->bControlSelector == AUDIO20_CS_CTRL_CLK_VALID &&
      req->bRequest == AUDIO20_CS_REQ_CUR) {
    audio20_control_cur_1_t valid = { .bCur = 1 };
    return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)req,
                                                      &valid, sizeof(valid));
  }

  return false;
}

static bool uac2_clock_set_request(uint8_t rhport,
                                   audio20_control_request_t const *req,
                                   uint8_t const *buf)
{
  (void)rhport;

  if (req->bEntityID != UAC2_ENTITY_CLOCK) return false;
  if (req->bControlSelector != AUDIO20_CS_CTRL_SAM_FREQ) return false;
  if (req->bRequest != AUDIO20_CS_REQ_CUR) return false;
  if (req->wLength != sizeof(audio20_control_cur_4_t)) return false;

  uint32_t req_rate = (uint32_t)((audio20_control_cur_4_t const *)buf)->bCur;

  if (req_rate != JAMMATE_AUDIO_SAMPLE_RATE) return false;

  current_sample_rate = req_rate;
  return true;
}

static bool uac2_feature_get_request(uint8_t rhport, audio20_control_request_t const *req)
{
  if (req->bEntityID != UAC2_ENTITY_SPK_FEATURE_UNIT) return false;
  if (req->bChannelNumber > 2) return false;

  if (req->bControlSelector == AUDIO20_FU_CTRL_MUTE &&
      req->bRequest == AUDIO20_CS_REQ_CUR) {
    audio20_control_cur_1_t m = { .bCur = mute[req->bChannelNumber] };
    return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)req,
                                                      &m, sizeof(m));
  }

  if (req->bControlSelector == AUDIO20_FU_CTRL_VOLUME) {
    if (req->bRequest == AUDIO20_CS_REQ_CUR) {
      audio20_control_cur_2_t v = { .bCur = tu_htole16(volume[req->bChannelNumber]) };
      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)req,
                                                        &v, sizeof(v));
    }

    if (req->bRequest == AUDIO20_CS_REQ_RANGE) {
      audio20_control_range_2_n_t(1) range = { .wNumSubRanges = tu_htole16(1) };
      range.subrange[0].bMin = tu_htole16((int16_t)(-90 * 256));
      range.subrange[0].bMax = tu_htole16(0);
      range.subrange[0].bRes = tu_htole16(256);

      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)req,
                                                        &range, sizeof(range));
    }
  }

  return false;
}

static bool uac2_feature_set_request(uint8_t rhport,
                                     audio20_control_request_t const *req,
                                     uint8_t const *buf)
{
  (void)rhport;

  if (req->bEntityID != UAC2_ENTITY_SPK_FEATURE_UNIT) return false;
  if (req->bRequest != AUDIO20_CS_REQ_CUR) return false;
  if (req->bChannelNumber > 2) return false;

  if (req->bControlSelector == AUDIO20_FU_CTRL_MUTE) {
    if (req->wLength != sizeof(audio20_control_cur_1_t)) return false;
    mute[req->bChannelNumber] = ((audio20_control_cur_1_t const *)buf)->bCur;
    return true;
  }

  if (req->bControlSelector == AUDIO20_FU_CTRL_VOLUME) {
    if (req->wLength != sizeof(audio20_control_cur_2_t)) return false;
    volume[req->bChannelNumber] = (int16_t)((audio20_control_cur_2_t const *)buf)->bCur;
    return true;
  }

  return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
  audio20_control_request_t const *req = (audio20_control_request_t const *)p_request;

  if (req->bEntityID == UAC2_ENTITY_CLOCK) {
    return uac2_clock_get_request(rhport, req);
  }

  if (req->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT) {
    return uac2_feature_get_request(rhport, req);
  }

  return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport,
                                 tusb_control_request_t const *p_request,
                                 uint8_t *buf)
{
  audio20_control_request_t const *req = (audio20_control_request_t const *)p_request;

  if (req->bEntityID == UAC2_ENTITY_CLOCK) {
    return uac2_clock_set_request(rhport, req, buf);
  }

  if (req->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT) {
    return uac2_feature_set_request(rhport, req, buf);
  }

  return false;
}

bool tud_audio_set_req_itf_cb(uint8_t rhport,
                              tusb_control_request_t const *p_request,
                              uint8_t *pBuff)
{
  (void)rhport;
  (void)p_request;
  (void)pBuff;
  return false;
}

bool tud_audio_get_req_itf_cb(uint8_t rhport,
                              tusb_control_request_t const *p_request)
{
  (void)rhport;
  (void)p_request;
  return false;
}

/* ── Diagnostics accessors ──────────────────────────────────────────────── */
uint32_t JamMate_USB_RxPackets(void)      { return jammate_usb_rx_packets;     }
uint32_t JamMate_USB_TxPackets(void)      { return jammate_usb_tx_packets;     }
uint32_t JamMate_USB_SetItfCount(void)    { return jammate_usb_set_itf_count;  }
uint32_t JamMate_USB_RxDoneCount(void)    { return jammate_usb_rx_done_count;  }
uint32_t JamMate_USB_TxDoneCount(void)    { return jammate_usb_tx_done_count;  }
uint32_t JamMate_USB_TxShort(void)        { return jammate_usb_tx_short;       }
uint32_t JamMate_USB_RxShort(void)        { return jammate_usb_rx_short;       }

uint32_t JamMate_USB_SpkUnderflows(void)  { return jammate_usb_spk_underflows; }
uint32_t JamMate_USB_SpkOverflows(void)   { return jammate_usb_spk_overflows;  }
uint32_t JamMate_USB_SpkFifoFrames(void)  { return spk_fifo_count();           }

uint32_t JamMate_USB_MicOverflows(void)   { return jammate_usb_mic_overflows;  }
uint32_t JamMate_USB_MicUnderflows(void)  { return jammate_usb_mic_underflows; }
uint32_t JamMate_USB_MicFifoFrames(void)  { return mic_fifo_count();           }

uint32_t JamMate_USB_FbCount(void)        { return jammate_usb_fb_count;       }
uint32_t JamMate_USB_FbValue(void)        { return jammate_usb_fb_value;       }
