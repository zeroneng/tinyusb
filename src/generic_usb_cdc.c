/* generic_usb_cdc.c — Generic USB CDC ACM wrapper
 *
 * 512-byte software RX ring:
 *   tud_cdc_rx_cb() pushes bytes immediately on TinyUSB task callback.
 *   GenericUSB_CDC_Read() drains the ring from any context.
 *   GenericUSB_CDCTask() only flushes the TX FIFO.
 */

#include "generic_usb_cdc.h"

#include <stdio.h>
#include <string.h>
#include "tusb.h"

#define GENERIC_USB_CDC_RX_RING_SIZE  512u

static uint8_t           rx_ring[GENERIC_USB_CDC_RX_RING_SIZE];
static volatile uint32_t rx_rd      = 0;
static volatile uint32_t rx_wr      = 0;
static volatile uint32_t rx_count   = 0;
static volatile uint32_t rx_bytes   = 0;
static volatile uint32_t tx_bytes   = 0;
static volatile uint32_t rx_dropped = 0;

static void rx_ring_reset(void)
{
    rx_rd = rx_wr = rx_count = 0;
}

static void rx_ring_push(uint8_t b)
{
    if (rx_count >= GENERIC_USB_CDC_RX_RING_SIZE) {
        rx_dropped++;
        return;
    }
    rx_ring[rx_wr] = b;
    rx_wr    = (rx_wr + 1u) % GENERIC_USB_CDC_RX_RING_SIZE;
    rx_count++;
    rx_bytes++;
}

/* ── Public init / task ─────────────────────────────────────────────────── */
void GenericUSB_CDCInit(void)
{
    rx_ring_reset();
    rx_bytes = tx_bytes = rx_dropped = 0;
}

void GenericUSB_CDCTask(void)
{
    if (tud_cdc_connected()) tud_cdc_write_flush();
}

/* ── Public API ─────────────────────────────────────────────────────────── */
bool GenericUSB_CDC_Connected(void) { return tud_cdc_connected(); }
uint32_t GenericUSB_CDC_Available(void) { return rx_count; }

uint32_t GenericUSB_CDC_Read(uint8_t *dst, uint32_t max_len)
{
    if (!dst || !max_len) return 0;
    uint32_t n = 0;
    while (n < max_len && rx_count > 0) {
        dst[n++] = rx_ring[rx_rd];
        rx_rd    = (rx_rd + 1u) % GENERIC_USB_CDC_RX_RING_SIZE;
        rx_count--;
    }
    return n;
}

uint32_t GenericUSB_CDC_Write(const uint8_t *src, uint32_t len)
{
    if (!src || !len || !tud_cdc_connected()) return 0;
    uint32_t written  = tud_cdc_write(src, len);
    tx_bytes         += written;
    return written;
}

uint32_t GenericUSB_CDC_PrintFloat(const char *label, float value)
{
    char buf[96];

    if(label && label[0])
    {
        int n = snprintf(buf, sizeof(buf), "%s%.6f", label, (double)value);
        if(n <= 0) return 0;

        if((uint32_t)n >= sizeof(buf))
            n = (int)sizeof(buf) - 1;

        return GenericUSB_CDC_Write((const uint8_t *)buf, (uint32_t)n);
    }
    else
    {
        int n = snprintf(buf, sizeof(buf), "%.6f", (double)value);
        if(n <= 0) return 0;

        if((uint32_t)n >= sizeof(buf))
            n = (int)sizeof(buf) - 1;

        return GenericUSB_CDC_Write((const uint8_t *)buf, (uint32_t)n);
    }
}

uint32_t GenericUSB_CDC_PrintFloatLn(const char *label, float value)
{
    char buf[100];

    if(label && label[0])
    {
        int n = snprintf(buf, sizeof(buf), "%s%.6f\r\n", label, (double)value);
        if(n <= 0) return 0;

        if((uint32_t)n >= sizeof(buf))
            n = (int)sizeof(buf) - 1;

        return GenericUSB_CDC_Write((const uint8_t *)buf, (uint32_t)n);
    }
    else
    {
        int n = snprintf(buf, sizeof(buf), "%.6f\r\n", (double)value);
        if(n <= 0) return 0;

        if((uint32_t)n >= sizeof(buf))
            n = (int)sizeof(buf) - 1;

        return GenericUSB_CDC_Write((const uint8_t *)buf, (uint32_t)n);
    }
}

uint32_t GenericUSB_CDC_WriteString(const char *s)
{
    if (!s) return 0;
    return GenericUSB_CDC_Write((uint8_t const *)s, (uint32_t)strlen(s));
}

void     GenericUSB_CDC_Flush(void)            { tud_cdc_write_flush(); }
uint32_t GenericUSB_CDC_RxBytes(void)          { return rx_bytes;   }
uint32_t GenericUSB_CDC_TxBytes(void)          { return tx_bytes;   }
uint32_t GenericUSB_CDC_DroppedRxBytes(void)   { return rx_dropped; }

/* ── TinyUSB CDC callbacks ──────────────────────────────────────────────── */
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf; (void)rts;
    if (dtr) {
        GenericUSB_CDC_WriteString("Generic USB UAC2 CDC ready\r\n");
        GenericUSB_CDC_Flush();
    }
}

void tud_cdc_rx_cb(uint8_t itf)
{
    uint8_t buf[64];
    while (tud_cdc_n_available(itf) > 0) {
        uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));
        for (uint32_t i = 0; i < count; i++) rx_ring_push(buf[i]);
    }
}
