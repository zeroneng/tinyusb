#include "generic_usb_midi.h"

#include "tusb.h"

static uint32_t midi_rx_packets;
static uint32_t midi_tx_packets;
static uint32_t midi_dropped_packets;

void GenericUSB_MIDIInit(void)
{
  midi_rx_packets = 0;
  midi_tx_packets = 0;
  midi_dropped_packets = 0;
}

void GenericUSB_MIDITask(void)
{
  if (!tud_midi_mounted()) return;

  while (tud_midi_available() != 0u) {
    uint8_t packet[4];

    if (!tud_midi_packet_read(packet)) break;
    midi_rx_packets++;

    if (tud_midi_packet_write(packet)) {
      midi_tx_packets++;
    } else {
      midi_dropped_packets++;
    }
  }
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
