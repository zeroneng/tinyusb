#ifndef GENERIC_USB_PORT_H_
#define GENERIC_USB_PORT_H_

#ifdef __cplusplus
extern "C" {
#endif

void GenericUSB_Init(void);
void GenericUSB_Task(void);
void GenericUSB_OTG_HS_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* GENERIC_USB_PORT_H_ */
