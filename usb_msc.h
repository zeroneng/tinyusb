#ifndef GENERIC_USB_MSC_H_
#define GENERIC_USB_MSC_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void GenericUSB_MSCInit(void);
void GenericUSB_MSCSetEnabled(bool enabled);
bool GenericUSB_MSCIsEnabled(void);

#ifdef __cplusplus
}
#endif

#endif /* GENERIC_USB_MSC_H_ */
