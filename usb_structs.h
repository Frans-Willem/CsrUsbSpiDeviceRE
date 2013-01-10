/*
 * usb_structs.h
 *
 *  Created on: 31 dec. 2012
 *      Author: Frans-Willem
 */

#ifndef USB_STRUCTS_H_
#define USB_STRUCTS_H_

#define BULK_BUFFER_SIZE 256

extern unsigned long RxHandler(void *pvCBData, unsigned long ulEvent,
                               unsigned long ulMsgValue, void *pvMsgData);
extern unsigned long TxHandler(void *pvlCBData, unsigned long ulEvent,
                               unsigned long ulMsgValue, void *pvMsgData);

extern const tUSBDBulkDevice g_sBulkDevice;
extern unsigned char g_pucUSBTxBuffer[];
extern unsigned char g_pucUSBRxBuffer[];


#endif /* USB_STRUCTS_H_ */
