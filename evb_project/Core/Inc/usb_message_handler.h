#ifndef USB_MESSAGE_HANDLER_H
#define USB_MESSAGE_HANDLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void UsbMsg_Reset(void);
uint8_t UsbMsg_Enqueue(uint8_t event_idx, uint8_t cmd_id, const uint8_t *data, uint16_t len);
uint8_t UsbMsg_ProcessNext(void);
uint32_t UsbMsg_GetDroppedCount(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_MESSAGE_HANDLER_H */
