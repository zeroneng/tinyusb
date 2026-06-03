#ifndef GENERIC_USB_CDC_H_
#define GENERIC_USB_CDC_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Init / Task ────────────────────────────────────────────────────────── */
void     GenericUSB_CDCInit(void);
void     GenericUSB_CDCTask(void);   /* call from main loop */

/* ── API ────────────────────────────────────────────────────────────────── */
bool     GenericUSB_CDC_Connected(void);
uint32_t GenericUSB_CDC_Available(void);
uint32_t GenericUSB_CDC_Read(uint8_t *dst, uint32_t max_len);
uint32_t GenericUSB_CDC_Write(const uint8_t *src, uint32_t len);
uint32_t GenericUSB_CDC_PrintFloat(const char *label, float value);
uint32_t GenericUSB_CDC_PrintFloatLn(const char *label, float value);
uint32_t GenericUSB_CDC_WriteString(const char *s);
void     GenericUSB_CDC_Flush(void);

/* ── Diagnostics ────────────────────────────────────────────────────────── */
uint32_t GenericUSB_CDC_RxBytes(void);
uint32_t GenericUSB_CDC_TxBytes(void);
uint32_t GenericUSB_CDC_DroppedRxBytes(void);

#ifdef __cplusplus
}
#endif
#endif /* GENERIC_USB_CDC_H_ */
