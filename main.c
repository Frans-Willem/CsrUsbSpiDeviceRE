#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "driverlib/rom.h"
#include "usblib/usblib.h"
#include "usblib/usb-ids.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdbulk.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "usb_structs.h"
#include "csrspi.h"

#define SYSCTL_PERIPH_LEDS	SYSCTL_PERIPH_GPIOF
#define GPIO_PORT_LEDS_BASE	GPIO_PORTF_BASE
#define GPIO_PIN_LED_R		GPIO_PIN_1
#define GPIO_PIN_LED_G		GPIO_PIN_3
#define GPIO_PIN_LED_B		GPIO_PIN_2

#define BUFFER_SIZE	1024

typedef unsigned int size_t;

unsigned char pReceiveBuffer[BUFFER_SIZE];
volatile size_t nReceiveLength;
volatile int bReceiving = 1;

unsigned char pTransmitBuffer[BUFFER_SIZE];
volatile size_t nTransmitLength;
volatile size_t nTransmitOffset;
volatile int bTransmitting = 0;

#define MODE_SPI	0
#define MODE_JTAG	0xFFFF;
unsigned short g_nMode = MODE_SPI;

#define CMD_READ		0x0100
#define CMD_WRITE		0x0200
#define CMD_SETSPEED	0x0300
#define CMD_GETSTOPPED	0x0400
#define CMD_GETSPEED	0x0500
#define CMD_UPDATE		0x0600
#define CMD_GETSERIAL	0x0700
#define CMD_GETVERSION	0x0800
#define CMD_SETMODE		0x0900
#define CMD_SETBITS		0x0F00
#define CMD_BCCMDINIT	0x4000
#define CMD_BCCMD		0x4100

void WriteWord(unsigned char *szOffset, unsigned short n) {
	szOffset[1] = n & 0xFF;
	szOffset[0] = (n >> 8) & 0xFF;
}

unsigned short ReadWord(unsigned char *szOffset) {
	return (szOffset[0] << 8) | szOffset[1];
}

void TransmitWord(unsigned short n) {
	if (nTransmitLength + 2 < BUFFER_SIZE) {
		WriteWord(&pTransmitBuffer[nTransmitLength], n);
		nTransmitLength += 2;
	}
}

void TransmitDWord(unsigned int n) {
	TransmitWord((n >> 16) & 0xFFFF);
	TransmitWord(n & 0xFFFF);
}

int CmdRead(unsigned short nAddress, unsigned short nLength);
int CmdWrite(unsigned short nAddress, unsigned short nLength, unsigned char *pData);
int CmdSetSpeed(unsigned short nSpeed);
int CmdGetStopped();
int CmdGetSpeed();
int CmdUpdate();
int CmdGetSerial();
int CmdGetVersion();
int CmdSetMode(unsigned short nMode);
int CmdSetBits(unsigned short nWhich, unsigned short nValue);
void CmdBcCmdInit(unsigned short nA, unsigned short nB);
int CmdBcCmd(unsigned short nLength, unsigned char *pData);

void TransmitCallback() {
	size_t nPacket;
	if (bTransmitting) {
		nPacket = nTransmitLength - nTransmitOffset;
		if (nPacket > 64)
			nPacket = 64;
		USBDBulkPacketWrite((void *)&g_sBulkDevice, &pTransmitBuffer[nTransmitOffset], nPacket, true);
		nTransmitOffset += nPacket;
		if (nPacket < 64)
			bTransmitting = 0;
	}
}
void StartTransmit() {
	nTransmitOffset = 0;
	bTransmitting = 1;
	if (USBDBulkTxPacketAvailable((void *)&g_sBulkDevice))
		TransmitCallback();
}
void WaitTransmit() {
	while (bTransmitting);
	nTransmitLength = nTransmitOffset = 0;
}

/*
 * main.c
 */
void main(void) {
	size_t nOffset;
	unsigned short nCommand, nArgs[4];

	//Set Clock at 80MHz
	ROM_SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
	//Enable UART on Port A
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	GPIOPinConfigure(GPIO_PA0_U0RX);
	GPIOPinConfigure(GPIO_PA1_U0TX);
	ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
	//Do some output!
	UARTStdioInit(0);
	UARTprintf("CSR USB-SPI Emulator running!\n");
	//Csr SPI init
	CsrInit();
	UARTprintf("CSR initialized\n");

	//Status LEDs Init
	SysCtlPeripheralEnable(SYSCTL_PERIPH_LEDS);
	GPIOPinTypeGPIOOutput(GPIO_PORT_LEDS_BASE, GPIO_PIN_LED_R|GPIO_PIN_LED_G|GPIO_PIN_LED_B);
	GPIOPinWrite(GPIO_PORT_LEDS_BASE, GPIO_PIN_LED_R|GPIO_PIN_LED_G|GPIO_PIN_LED_B, 0);
	UARTprintf("LEDs initialized\n");


	//USB init?
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	ROM_GPIOPinTypeUSBAnalog(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    USBStackModeSet(0, USB_MODE_FORCE_DEVICE, 0);
    USBDBulkInit(0, (tUSBDBulkDevice *)&g_sBulkDevice);
    UARTprintf("Waiting for host...\n");

	while (1) {
		while (bReceiving); //Wait until we're ready receiving data
		if (nReceiveLength == 0) {
			UARTprintf("Empty packet...\n");
		} else {
			WaitTransmit();
			if (pReceiveBuffer[0] != '\0') {
				UARTprintf("Invalid start byte\n");
			} else {
				nOffset = 1;
				while (nReceiveLength >= nOffset + 2) {
					nCommand = ReadWord(&pReceiveBuffer[nOffset]);
					nOffset += 2;
					switch (nCommand) {
					case CMD_READ:
						if (nReceiveLength < nOffset + 4) {
							UARTprintf("Too few arguments to read\n");
						} else {
							nArgs[0] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							nArgs[1] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							CmdRead(nArgs[0], nArgs[1]);
						}
						break;
					case CMD_WRITE:
						if (nReceiveLength < nOffset + 4) {
							UARTprintf("Too few arguments to write\n");
						} else {
							nArgs[0] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							nArgs[1] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							if (nReceiveLength < nOffset + (nArgs[1] * 2)) {
								UARTprintf("Too few arguments to write\n");
							} else {
								CmdWrite(nArgs[0], nArgs[1], &pReceiveBuffer[nOffset]);
								nOffset+=nArgs[1] * 2;
							}
						}
						break;
					case CMD_SETSPEED:
						if (nReceiveLength < nOffset + 2) {
							UARTprintf("Too few arguments to set speed\n");
						} else {
							nArgs[0] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							CmdSetSpeed(nArgs[0]);
						}
						break;
					case CMD_GETSTOPPED:
						CmdGetStopped();
						break;
					case CMD_GETSPEED:
						CmdGetSpeed();
						break;
					case CMD_UPDATE:
						CmdUpdate();
						break;
					case CMD_GETSERIAL:
						CmdGetSerial();
						break;
					case CMD_GETVERSION:
						CmdGetVersion();
						break;
					case CMD_SETMODE:
						if (nReceiveLength < nOffset + 2) {
							UARTprintf("Too few arguments to set mode\n");
						} else {
							nArgs[0] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							CmdSetMode(nArgs[0]);
						}
						break;
					case CMD_SETBITS:
						if (nReceiveLength < nOffset + 4) {
							UARTprintf("Too few arguments to set bits\n");
						} else {
							nArgs[0] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							nArgs[1] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							CmdSetBits(nArgs[0], nArgs[1]);
						}
						break;
					case CMD_BCCMDINIT:
						if (nReceiveLength < nOffset + 4) {
							UARTprintf("Too few arguments to init bccmd\n");
						} else {
							nArgs[0] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							nArgs[1] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							CmdBcCmdInit(nArgs[0], nArgs[1]);
						}
						break;
					case CMD_BCCMD:
						if (nReceiveLength < nOffset + 2) {
							UARTprintf("Too few arguments to bccmd\n");
						} else {
							nArgs[0] = ReadWord(&pReceiveBuffer[nOffset]);
							nOffset += 2;
							if (nReceiveLength < nOffset + (nArgs[0] * 2)) {
								UARTprintf("Too few arguments to bccmd\n");
							} else {
								CmdBcCmd(nArgs[0], &pReceiveBuffer[nOffset]);
								nOffset+=nArgs[0] * 2;
							}
						}
						break;
					default:
						UARTprintf("Unknown command: %04X", nCommand);
						nOffset = nReceiveLength;
						continue;
					}
					nOffset += 2; //Read the two inbetween bytes
				}
			}
			if (nTransmitLength)
				StartTransmit();
		}

		//Get ready for the next packet
		nReceiveLength = 0;
		bReceiving = 1;
	}
}

unsigned long
TxHandler(void *pvCBData, unsigned long ulEvent, unsigned long ulMsgValue,
          void *pvMsgData)
{
    if(ulEvent == USB_EVENT_TX_COMPLETE)
    {
    	TransmitCallback();
    }
    return(0);
}

unsigned short pCsrBuffer[1024];


int CmdRead(unsigned short nAddress, unsigned short nLength) {
	unsigned short *pCurrent;
	GPIOPinWrite(GPIO_PORT_LEDS_BASE, GPIO_PIN_LED_G, GPIO_PIN_LED_G);
	if (g_nMode == MODE_SPI && nLength < 1024 && CsrSpiRead(nAddress,nLength,pCsrBuffer)) {
		GPIOPinWrite(GPIO_PORT_LEDS_BASE, GPIO_PIN_LED_G, 0);
		TransmitWord(CMD_READ);
		TransmitWord(nAddress);
		TransmitWord(nLength);
		pCurrent = pCsrBuffer;
		while (nLength--) {
			TransmitWord(*(pCurrent++));
		}
	} else {
		GPIOPinWrite(GPIO_PORT_LEDS_BASE, GPIO_PIN_LED_G, 0);
		UARTprintf("Read Failed\n");
		TransmitWord(CMD_READ + 1);
		TransmitWord(nAddress);
		TransmitWord(nLength);
		while (nLength--)
			TransmitWord(0);
	}
	return 1;
}
int CmdWrite(unsigned short nAddress, unsigned short nLength, unsigned char *pData) {
	if (nLength > 1024 || g_nMode != MODE_SPI)
		return 0;
	unsigned short i;
	for (i = 0; i < nLength; i++) {
		pCsrBuffer[i] = ReadWord(&pData[i * 2]);
	}
	GPIOPinWrite(GPIO_PORT_LEDS_BASE, GPIO_PIN_LED_R, GPIO_PIN_LED_R);
	CsrSpiWrite(nAddress, nLength, pCsrBuffer);
	GPIOPinWrite(GPIO_PORT_LEDS_BASE, GPIO_PIN_LED_R, 0);
	return 1;
}
int CmdSetSpeed(unsigned short nSpeed) {
	g_nSpeed = nSpeed;
	return 1;
}
int CmdGetStopped() {
	TransmitWord(CMD_GETSTOPPED);
	TransmitWord(g_nMode != MODE_SPI || CsrSpiIsStopped()); //TODO
	return 1;
}
int CmdGetSpeed() {
	TransmitWord(CMD_GETSPEED);
	TransmitWord(g_nSpeed);
	return 1;
}
int CmdUpdate() {
	return 1;
}
int CmdGetSerial() {
	TransmitWord(CMD_GETSERIAL);
	TransmitDWord(31337);
	return 1;
}
int CmdGetVersion() {
	TransmitWord(CMD_GETVERSION);
	TransmitWord(0x119);
	return 1;
}
int CmdSetMode(unsigned short nMode) {
	g_nMode = nMode;
	return 1;
}
int CmdSetBits(unsigned short nWhich, unsigned short nValue) {
	if (nWhich) g_nWriteBits = nValue;
	else g_nReadBits = nValue;
	return 1;
}

void CmdBcCmdInit(unsigned short nA, unsigned short nB) {
	UARTprintf("BCCMD Init: %04X %04X\n", nA, nB);
	g_nBcA = nA;
	g_nBcB = nB;
}

int CmdBcCmd(unsigned short nLength, unsigned char *pData) {
	unsigned short i;
	//UARTprintf("BCCMD input:\n");
	for (i = 0; i < nLength; i++) {
		pCsrBuffer[i] = ReadWord(&pData[i*2]);
	}
	GPIOPinWrite(GPIO_PORT_LEDS_BASE, GPIO_PIN_LED_B, GPIO_PIN_LED_B);
	if (CsrSpiBcCmd(nLength, pCsrBuffer)) {
		TransmitWord(CMD_BCCMD);
	} else {
		TransmitWord(CMD_BCCMD + 1);
	}
	GPIOPinWrite(GPIO_PORT_LEDS_BASE, GPIO_PIN_LED_B, 0);
	TransmitWord(nLength);
	for (i=0; i<nLength; i++) {
		TransmitWord(pCsrBuffer[i]);
	}
	return 1;
}

unsigned long RxHandler(void *pvCBData, unsigned long ulEvent, unsigned long ulMsgValue, void *pvMsgData)
{
    switch(ulEvent)
    {
        case USB_EVENT_CONNECTED:
        {
        	if (bReceiving) {
        		nReceiveLength = 0;
        	}
            UARTprintf("Connected...\n");
            break;
        }
        case USB_EVENT_DISCONNECTED:
        {
        	UARTprintf("Disconnected...\n");
            break;
        }
        case USB_EVENT_RX_AVAILABLE:
        {
        	if (bReceiving) {
        		if (nReceiveLength + ulMsgValue > BUFFER_SIZE) {
        			UARTprintf("Buffer overflow\n");
        			return 0;
        		}
        		nReceiveLength += USBDBulkPacketRead((void *)&g_sBulkDevice, &pReceiveBuffer[nReceiveLength], ulMsgValue, true);
        		if (ulMsgValue < 64) {
        			bReceiving = 0;
        		}
        		return ulMsgValue;
        	}
        	return 0;
        }
        case USB_EVENT_SUSPEND:
        case USB_EVENT_RESUME:
        {
            break;
        }
        default:
        {
            break;
        }
    }

    return(0);
}
