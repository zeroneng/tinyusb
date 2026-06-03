#include "usb_midi.h"

#include <stdio.h>

#include "tusb.h"
#include "global.h"
#include "usb_cdc.h"
#include "usb_port.h"

#if USB_ENABLE_MIDI

static uint32_t midi_rx_packets;
static uint32_t midi_tx_packets;
static uint32_t midi_dropped_packets;
static uint32_t last_test_ms;
static uint32_t next_test_ms;
static bool test_note_is_on;

#if DEBUG_TEST_MIDI
static void midi_log_input_packet(uint8_t const packet[4])
{
  char line[40];
  int const len = snprintf(line,
                           sizeof(line),
                           "MIDI IN: %02X %02X %02X %02X\r\n",
                           packet[0],
                           packet[1],
                           packet[2],
                           packet[3]);

  if (len > 0) {
    GenericUSB_CDC_Write((uint8_t const *)line, (uint32_t)len);
    GenericUSB_CDC_Flush();
  }
}
#endif

void GenericUSB_MIDIInit(void)
{
  midi_rx_packets = 0;
  midi_tx_packets = 0;
  midi_dropped_packets = 0;
  last_test_ms = 0;
  next_test_ms = 1000u;
  test_note_is_on = false;
}

void GenericUSB_MIDITask(void)
{
  if (!tud_midi_mounted()) return;

  while (tud_midi_available() != 0u) {
    uint8_t packet[4];

    if (!tud_midi_packet_read(packet)) break;
    midi_rx_packets++;

#if DEBUG_TEST_MIDI
    midi_log_input_packet(packet);
#endif

    if (tud_midi_packet_write(packet)) {
      midi_tx_packets++;
    } else {
      midi_dropped_packets++;
    }
  }

#if DEBUG_TEST_MIDI
  uint32_t now = GenericUSB_NowMs();

  if (!test_note_is_on) {
    if ((int32_t)(now - next_test_ms) < 0) return;

    uint8_t const note_on[4] = { 0x09u, 0x90u, 60u, 64u };

    if (tud_midi_packet_write(note_on)) {
      midi_tx_packets++;
      test_note_is_on = true;
      last_test_ms = now;
    } else {
      midi_dropped_packets++;
    }
  } else if ((now - last_test_ms) >= 40u) {
    uint8_t const note_off[4] = { 0x08u, 0x80u, 60u, 0u };

    if (tud_midi_packet_write(note_off)) {
      midi_tx_packets++;
      test_note_is_on = false;
      last_test_ms = now;
      next_test_ms = now + 1000u;
    } else {
      midi_dropped_packets++;
    }
  }
#endif
}

uint32_t GenericUSB_MIDI_RxPackets(void)
{
  return midi_rx_packets;
}

uint32_t GenericUSB_MIDI_TxPackets(void)
{
  return midi_tx_packets;
}

uint32_t GenericUSB_MIDI_DroppedPackets(void)
{
  return midi_dropped_packets;
}

#else

void GenericUSB_MIDIInit(void) {}
void GenericUSB_MIDITask(void) {}
uint32_t GenericUSB_MIDI_RxPackets(void) { return 0; }
uint32_t GenericUSB_MIDI_TxPackets(void) { return 0; }
uint32_t GenericUSB_MIDI_DroppedPackets(void) { return 0; }

#endif
