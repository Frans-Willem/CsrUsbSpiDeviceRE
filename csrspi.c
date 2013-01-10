/*
 * csrspi.c
 *
 *  Created on: 2 jan. 2013
 *      Author: Frans-Willem
 */
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include "driverlib/gpio.h"

#define PIN_CS			GPIO_PIN_4 //Equivalent of pin 0x1 in CSR code
#define PIN_MISO		GPIO_PIN_6 //Equivalent of pin 0x2 in CSR code
#define PIN_MOSI		GPIO_PIN_5 //Equivalent of pin 0x4 in CSR code
#define PIN_CLK			GPIO_PIN_7 //Equivalent of pin 0x8 in CSR code
#define PIN_UNK			GPIO_PIN_4 //Equivalent to pin 0x10 in CSR code
#define PIN_ALL_OUT		PIN_CS|PIN_MOSI|PIN_CLK|PIN_UNK
#define PIN_ALL_IN		PIN_MISO
#define PORT_BASE	GPIO_PORTC_BASE

#define SET_CS()	GPIOPinWrite(PORT_BASE, PIN_CS, PIN_CS)
#define CLR_CS()	GPIOPinWrite(PORT_BASE, PIN_CS, 0)
#define SET_CLK()	GPIOPinWrite(PORT_BASE, PIN_CLK, PIN_CLK)
#define	CLR_CLK()	GPIOPinWrite(PORT_BASE, PIN_CLK, 0)
#define SET_MOSI()	GPIOPinWrite(PORT_BASE, PIN_MOSI, PIN_MOSI)
#define CLR_MOSI()	GPIOPinWrite(PORT_BASE, PIN_MOSI, 0)
#define GET_MISO()	GPIOPinRead(PORT_BASE, PIN_MISO)


unsigned short g_nSpeed = 393;
unsigned short g_nReadBits = 0;
unsigned short g_nWriteBits = 0;
unsigned short g_nBcA = 0;
unsigned short g_nBcB = 0;
unsigned short g_nUseSpecialRead = 0;

void CsrInit() {
	//Initialize Port B
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    GPIOPinTypeGPIOOutput(PORT_BASE, PIN_ALL_OUT);
    GPIOPadConfigSet(PORT_BASE, PIN_MISO, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void CsrSpiDelay() {
	int kHz = 1000000 / (126 * g_nSpeed + 434);
	//Used to be 3000
	ROM_SysCtlDelay((SysCtlClockGet()/6000)/kHz);
}

//Equivalent of function at 022D
void CsrSpiStart() {
//022D
//Unsets all outputs pins, sets unk and cs.
	GPIOPinWrite(PORT_BASE, PIN_ALL_OUT, PIN_UNK | PIN_CS);
	CsrSpiDelay();
//0239
	GPIOPinWrite(PORT_BASE, PIN_ALL_OUT, PIN_UNK | PIN_CS | PIN_CLK);
	CsrSpiDelay();
//023C
	GPIOPinWrite(PORT_BASE, PIN_ALL_OUT, PIN_UNK | PIN_CS);
	CsrSpiDelay();
//023F
	GPIOPinWrite(PORT_BASE, PIN_ALL_OUT, PIN_UNK | PIN_CS | PIN_CLK);
	CsrSpiDelay();
//0242
	GPIOPinWrite(PORT_BASE, PIN_ALL_OUT, PIN_UNK | PIN_CS);
	CsrSpiDelay();
//0245
	GPIOPinWrite(PORT_BASE, PIN_ALL_OUT, PIN_UNK);
	CsrSpiDelay();
//0247
	return;
}

//Equivalent of function at 0248
void CsrSpiStop() {
	//Clears MOSI and CLK, sets CS and UNK
	GPIOPinWrite(PORT_BASE, PIN_ALL_OUT, PIN_UNK|PIN_CS);
}

//Equivalent of function at 0250
//R10 is number of bits
//R0 is what to send?
//Normally R7 and R8 hold the first and second delay function, but I opted for a bool instead
void CsrSpiSendBits(unsigned int nData, unsigned int nBits) {
	//CsrTransfer(nData, (1<<(nBits-1)));
	//return;
	unsigned char nClkLow = 0; //R2
	unsigned char nClkHigh = PIN_CLK; //R3
	nData <<= 24 - nBits; //Could've gone with 32, but hey, who cares
	while (nBits--) {
		GPIOPinWrite(PORT_BASE, PIN_CLK|PIN_MOSI|PIN_CS, nClkLow);
		if (nData & 0x800000) nClkHigh |= PIN_MOSI;
		nClkLow = nClkHigh - PIN_CLK;
		CsrSpiDelay();
		GPIOPinWrite(PORT_BASE, PIN_MOSI|PIN_CS, nClkHigh);
		GPIOPinWrite(PORT_BASE, PIN_CLK|PIN_MOSI|PIN_CS, nClkHigh);
		CsrSpiDelay();
		nClkHigh = PIN_CLK;
		nData <<= 1;
	}
}

//Equivalent of function at 0263
//nBits = R10?
unsigned int CsrSpiReadBits(unsigned int nBits) {
	//return CsrTransfer(0, (1<<(nBits-1)));
	unsigned int nRetval = 0;
	//CSR code disables interrupts here
	while (nBits--) {
		GPIOPinWrite(PORT_BASE, PIN_CLK, 0);
		CsrSpiDelay();
		nRetval <<= 1;
		GPIOPinWrite(PORT_BASE, PIN_CLK, PIN_CLK);
		CsrSpiDelay();
		if (GPIOPinRead(PORT_BASE, PIN_MISO))
			nRetval |= 1;
	}
	//CSR code re-enables interrupts here
	return nRetval & 0xFFFF;
}

//Equivalent of function at 0279
int CsrSpiReadBasic(unsigned short nAddress, unsigned short nLength, unsigned short *pnOutput) {
	nLength--; //Do one loop less, last word is read differently
	CsrSpiStart();
	unsigned int nCommand = ((g_nReadBits | 3) << 16) | nAddress;
	unsigned int nControl;
	CsrSpiSendBits(nCommand, 24);
	/*
	 * Inbetween these commands is an if-statement with some high-speed optimizations for if the spi delay = 0
	 */
	nControl = CsrSpiReadBits(16);
	if (nControl != ((nCommand >> 8) & 0xFFFF)) {
		//In CSR's code, this is at the bottom, 02B7
		CsrSpiStart(); //Not sure why we call this again
		CsrSpiStop();
		return 0;
	}
	//Loop at 296 - 029B
	while (nLength--) {
		*(pnOutput++)=CsrSpiReadBits(16);
	}
	//From 19D to 2A2 some more speed optimized code that we don't need
	unsigned short nLast = CsrSpiReadBits(15);
	GPIOPinWrite(PORT_BASE, PIN_CLK, 0);
	CsrSpiDelay(); CsrSpiDelay(); CsrSpiDelay(); CsrSpiDelay();
	nLast <<= 1;
	if (GPIOPinRead(PORT_BASE, PIN_MISO))
		nLast |= 1;
	*(pnOutput++) = nLast;
	CsrSpiStart(); //Not sure why we call this again
	CsrSpiStop();
	return 1;
}

//Equivalent of function at 02BB
int CsrSpiRead(unsigned short nAddress, unsigned short nLength, unsigned short *pnOutput) {
	int nRet = 1;
	if (!g_nUseSpecialRead)
		return CsrSpiReadBasic(nAddress,nLength,pnOutput);
	nLength--;
	nRet &= CsrSpiReadBasic(nAddress, nLength, pnOutput);
	g_nReadBits |= 0x20;
	nRet &= CsrSpiReadBasic(nAddress + nLength, 1, &pnOutput[nLength]);
	g_nReadBits &= ~0x20;
	return nRet;
}

//Equivalent of 02CE
void CsrSpiWrite(unsigned short nAddress, unsigned short nLength, unsigned short *pnInput) {
	CsrSpiStart();
	// Some speed optimizing code from 02D2 to 02D8
	unsigned int nCommand = ((g_nWriteBits | 2) << 16) | nAddress;
	CsrSpiSendBits(nCommand, 24);
	while (nLength--)
		CsrSpiSendBits(*(pnInput++), 16);
	CsrSpiStart();
	CsrSpiStop();
}

int CsrSpiIsStopped() {
	unsigned short nOldSpeed = g_nSpeed;
	g_nSpeed += 32;
	CsrSpiStart();
	g_nSpeed = nOldSpeed;
	unsigned char nRead = GPIOPinRead(PORT_BASE, PIN_MISO);
	CsrSpiStop();
	if (nRead) return 1;
	return 0;
}

//0x47F
//R1 seems to be used as return value
int CsrSpiBcOperation(unsigned short nOperation /* R2 */) {
	unsigned short var0, var1 = 0;
	int i;
	CsrSpiRead(g_nBcA, 1, &var0);
	CsrSpiWrite(g_nBcB, 1, &nOperation);
	CsrSpiWrite(g_nBcA, 1, &var0);
	for (i=0; i<30; i++) {
		CsrSpiRead(g_nBcB, 1, &var1);
		if (nOperation != var1)
			return var1;
	}
	return var1;
}

int CsrSpiBcCmd(unsigned short nLength, unsigned short *pnData) {
	unsigned short var1,var2;
	int i;
	if (CsrSpiBcOperation(0x7) != 0)
		return 0;
	CsrSpiWrite(g_nBcB + 1, 1, &nLength);
	if (CsrSpiBcOperation(0x1) != 2)
		return 0;
	if (!CsrSpiRead(g_nBcB + 2, 1, &var1))
		return 0;
	CsrSpiWrite(var1, nLength, pnData);
	CsrSpiBcOperation(0x4);
	for (i=0; i<30; i++) {
		if (CsrSpiRead(g_nBcB, 1, &var2) && var2 == 0x6) {
			CsrSpiRead(var1, nLength, pnData);
			CsrSpiBcOperation(0x7);
			return 1;
		}
	}
	return 0;
}
/*
void CsrReset() {
	SET_CS(); CsrDelay();
	SET_CLK(); CsrDelay();
	CLR_CLK(); CsrDelay();
	SET_CLK(); CsrDelay();
	CLR_CLK(); CsrDelay();
}

void CsrStart() {
	CsrReset();
	CLR_CS(); CsrDelay();
}

void CsrStop() {
	SET_CS();
}

unsigned int CsrTransfer(unsigned int nInput, unsigned int msb) {
	unsigned int nOutput = 0;
	while (msb) {
		if (nInput & msb) SET_MOSI();
		else CLR_MOSI();
		msb >>= 1;
		CsrDelay();
		SET_CLK();
		CsrDelay();
		nOutput <<= 1;
		if (GET_MISO()) nOutput |= 1;
		CLR_CLK();
	}
	return nOutput;
}

int CsrRead(unsigned short nAddress, unsigned short nLength, unsigned short *pnOutput) {
	unsigned int nCommand;
	unsigned int nControl;
	nCommand = 3 | (g_nReadBits & ~3);
	CsrStart();
	CsrTransfer(nCommand, 0x80);
	CsrTransfer(nAddress, 0x8000);
	nControl = CsrTransfer(0, 0x8000);
	unsigned int nExpected = ((nCommand & 0xFF) << 8) | ((nAddress >> 8) & 0xFF);
	//UARTprintf("Expected: %04X, Control: %04X\n",nExpected,nControl);
	if (nExpected != nControl) {
		CsrStop();
		return 0;
	}
	while (nLength--) {
		*(pnOutput++) = CsrTransfer(0, 0x8000);
	}
	CsrStop();
	return 1;
}

int CsrWrite(unsigned short nAddress, unsigned short nLength, unsigned short *pnInput) {
	unsigned int nCommand;
	nCommand = 2;
	CsrStart();
	CsrTransfer(nCommand, 0x80);
	CsrTransfer(nAddress, 0x8000);
	while (nLength--)
		CsrTransfer(*(pnInput++), 0x8000);
	CsrStop();
	return 1;
}

int CsrStopped() {
	unsigned int nControl, nCheck;
	CsrStart();
	nCheck = CsrTransfer(3, 0x80);
	CsrTransfer(0xFF9A, 0x8000);
	nControl = CsrTransfer(0, 0x8000);
	if (nControl != 0x3FF) {
		return -1;
	}
	return (nCheck ? 1 : 0);
}*/
