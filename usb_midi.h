#ifndef GENERIC_USB_MIDI_H_
#define GENERIC_USB_MIDI_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void GenericUSB_MIDIInit(void);
void GenericUSB_MIDITask(void);
uint32_t GenericUSB_MIDI_RxPackets(void);
uint32_t GenericUSB_MIDI_TxPackets(void);
uint32_t GenericUSB_MIDI_DroppedPackets(void);

#ifdef __cplusplus
}
#endif

#endif /* GENERIC_USB_MIDI_H_ */
