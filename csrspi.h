/*
 * csrspi.h
 *
 *  Created on: 2 jan. 2013
 *      Author: Frans-Willem
 */

#ifndef CSRSPI_H_
#define CSRSPI_H_

extern unsigned short g_nSpeed;
extern unsigned short g_nReadBits;
extern unsigned short g_nWriteBits;
extern unsigned short g_nBcA;
extern unsigned short g_nBcB;

void CsrInit();
int CsrSpiRead(unsigned short nAddress, unsigned short nLength, unsigned short *pnOutput);
void CsrSpiWrite(unsigned short nAddress, unsigned short nLength, unsigned short *pnInput);
int CsrSpiIsStopped();
int CsrSpiBcOperation(unsigned short nOperation);
int CsrSpiBcCmd(unsigned short nLength, unsigned short *pnData);
#endif /* CSRSPI_H_ */
