#ifndef JAMMATE_TINYUSB_PORT_H_
#define JAMMATE_TINYUSB_PORT_H_

#ifdef __cplusplus
extern "C" {
#endif

void JamMate_TinyUSB_Init(void);
void JamMate_TinyUSB_Task(void);
void JamMate_TinyUSB_OTG_FS_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* JAMMATE_TINYUSB_PORT_H_ */
