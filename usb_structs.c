/*
 * usb_structs.c
 *
 *  Created on: 31 dec. 2012
 *      Author: Frans-Willem
 */

#include "inc/hw_types.h"
#include "driverlib/usb.h"
#include "usblib/usblib.h"
#include "usblib/usb-ids.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdbulk.h"
#include "usb_structs.h"

const unsigned char g_pLangDescriptor[] =
{
    4,
    USB_DTYPE_STRING,
    USBShort(USB_LANG_EN_US)
};

const unsigned char g_pManufacturerString[] =
{
    (23 + 1) * 2,
    USB_DTYPE_STRING,
    'C',0,'a',0,'m',0,'b',0,'r',0,'i',0,'d',0,'g',0,'e',0,' ',0,'S',0,'i',0,'l',0,'i',0,'c',0,'o',0,'n',0,' ',0,'R',0,'a',0,'d',0,'i',0,'o',0
};
const unsigned char g_pProductString[] =
{
    (14 + 1) * 2,
    USB_DTYPE_STRING,
    'C',0,'S',0,'R',0,' ',0,'P',0,'r',0,'o',0,'g',0,'r',0,'a',0,'m',0,'m',0,'e',0,'r',0
};
const unsigned char g_pSerialNumberString[] =
{
    (8 + 1) * 2,
    USB_DTYPE_STRING,
    '1', 0, '2', 0, '3', 0, '4', 0, '5', 0, '6', 0, '7', 0, '8', 0
};
const unsigned char g_pDataInterfaceString[] =
{
    (19 + 1) * 2,
    USB_DTYPE_STRING,
    'B', 0, 'u', 0, 'l', 0, 'k', 0, ' ', 0, 'D', 0, 'a', 0, 't', 0,
    'a', 0, ' ', 0, 'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0,
    'a', 0, 'c', 0, 'e', 0
};
const unsigned char g_pConfigString[] =
{
    (23 + 1) * 2,
    USB_DTYPE_STRING,
    'B', 0, 'u', 0, 'l', 0, 'k', 0, ' ', 0, 'D', 0, 'a', 0, 't', 0,
    'a', 0, ' ', 0, 'C', 0, 'o', 0, 'n', 0, 'f', 0, 'i', 0, 'g', 0,
    'u', 0, 'r', 0, 'a', 0, 't', 0, 'i', 0, 'o', 0, 'n', 0
};
const unsigned char * const g_pStringDescriptors[] =
{
    g_pLangDescriptor,
    g_pManufacturerString,
    g_pProductString,
    g_pSerialNumberString,
    g_pDataInterfaceString,
    g_pConfigString
};

#define NUM_STRING_DESCRIPTORS (sizeof(g_pStringDescriptors) /                \
                                sizeof(unsigned char *))

//*****************************************************************************
//
// The bulk device initialization and customization structures. In this case,
// we are using USBBuffers between the bulk device class driver and the
// application code. The function pointers and callback data values are set
// to insert a buffer in each of the data channels, transmit and receive.
//
// With the buffer in place, the bulk channel callback is set to the relevant
// channel function and the callback data is set to point to the channel
// instance data. The buffer, in turn, has its callback set to the application
// function and the callback data set to our bulk instance structure.
//
//*****************************************************************************
tBulkInstance g_sBulkInstance;

const tUSBDBulkDevice g_sBulkDevice =
{
    0x0a12, //VID
    0x0042, //PID
    500,
    USB_CONF_ATTR_SELF_PWR,
    RxHandler,
    (void *)0,
    TxHandler,
    (void *)0,
    g_pStringDescriptors,
    NUM_STRING_DESCRIPTORS,
    &g_sBulkInstance
};
