#include <stdint.h>
#include "DSP280x_Device.h"
#include "DSP280x_Examples.h"
#include "uart.h"

// uart config & control registers
#define UART_REGS 		SciaRegs

#define UART_ISR_CFG 	do {						\
				EALLOW;					\
				PieVectTable.SCIRXINTA = &uart_rx_isr;	\
				PieVectTable.SCITXINTA = &uart_tx_isr;	\
				EDIS;					\
									\
				PieCtrlRegs.PIEIER9.bit.INTx1 = 1;	\
				PieCtrlRegs.PIEIER9.bit.INTx2 = 1;	\
									\
				IER |= M_INT9;				\
									\
				} while(0)

#define UART_ISR_END	PieCtrlRegs.PIEACK.all = PIEACK_GROUP9

static volatile struct {
	uint16_t	txBufTail, txBufHead, txCount;
	uint16_t	rxBufTail, rxBufHead, rxCount;
	uint16_t	tbuf[UART_TXB];
	uint16_t	rbuf[UART_RXB];
} Fifo;

// must be called as uart_init(CALC_SCI_PRD(19200L));
void uart_init(uint32_t bps_reg) {

	Fifo.rxCount = 0;
	Fifo.rxBufHead = 0;
	Fifo.rxBufTail = 0;
	for (uint16_t i = 0; i < UART_RXB; i++) {
		Fifo.rbuf[i] = 0;
	}

	Fifo.txCount = 0;
	Fifo.txBufHead = 0;
	Fifo.txBufTail = 0;
	for (uint16_t i = 0; i < UART_TXB; i++) {
		Fifo.tbuf[i] = 0;
	}
	
	
	EALLOW;

//	GPIO-12 - PIN FUNCTION = --Spare--
//	GpioCtrlRegs.GPAQSEL2.bit.GPIO12 = 3;	// Asynch input
	GpioCtrlRegs.GPAPUD.bit.GPIO12 = 1;	// uncomment if --> Disable pull-up
//	GpioCtrlRegs.GPAPUD.bit.GPIO12 = 0;	// uncomment if --> Enable pull-up
	GpioDataRegs.GPACLEAR.bit.GPIO12 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO12 = 1;	// uncomment if --> Set High initially
	GpioCtrlRegs.GPADIR.bit.GPIO12 = 1;	// 1=OUTput,  0=INput
	GpioCtrlRegs.GPAMUX1.bit.GPIO12 = 2;	// 0=GPIO,  1=TZ1,  2=SCITX-A,  3=Resv

//	GPIO-28 - PIN FUNCTION = --Spare--
	GpioCtrlRegs.GPAQSEL2.bit.GPIO28 = 3;	// Asynch input
//	GpioCtrlRegs.GPAPUD.bit.GPIO28 = 1;	// uncomment if --> Disable pull-up
	GpioCtrlRegs.GPAPUD.bit.GPIO28 = 0;	// uncomment if --> Enable pull-up
//	GpioDataRegs.GPACLEAR.bit.GPIO28 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO28 = 1;	// uncomment if --> Set High initially
	GpioCtrlRegs.GPADIR.bit.GPIO28 = 0;	// 1=OUTput,  0=INput
	GpioCtrlRegs.GPAMUX2.bit.GPIO28 = 1;	// 0=GPIO,  1=SCIRX-A,  2=I2C-SDA,  3=TZ2

	EDIS;

	UART_REGS.SCICCR.all = 0x0007;		// 8 bit data, 1 stop bit, no parity, no LOOPBACK, IDLE-mode
	UART_REGS.SCICTL1.all = 0x0003;		// SW_RESET=0
	UART_REGS.SCICTL2.all = 0x0002;		// RX isr enable

	UART_REGS.SCIHBAUD = (bps_reg >> 8);	// setup baudrate
	UART_REGS.SCILBAUD = bps_reg & 0xFF;
	
	UART_REGS.SCIFFTX.all = 0x8000;		// FIFO disable
	UART_REGS.SCIFFRX.all = 0x0000;		// FIFO disable
	UART_REGS.SCIFFCT.all = 0x0000;		// Auto-baud detect disable
	UART_REGS.SCICTL1.all = 0x0023;		// RX - ON, TX - OFF, SW_RESET=1
	
	
	UART_ISR_CFG;
}

interrupt void uart_rx_isr(void) {
	// if error - reset uart
	if (UART_REGS.SCIRXST.bit.RXERROR) {
		UART_REGS.SCICTL1.bit.SWRESET = 0;
		UART_REGS.SCIRXBUF.bit.RXDT;
		UART_REGS.SCICTL1.bit.SWRESET = 1;
	} else {

		if (Fifo.rxCount < UART_RXB) { // if buffer not full 

			if (Fifo.rxBufTail & 1) { // odd
				Fifo.rbuf[(Fifo.rxBufTail >> 1)] &= 0x00FF;
				Fifo.rbuf[(Fifo.rxBufTail >> 1)] |= (uint16_t)(UART_REGS.SCIRXBUF.bit.RXDT) << 8;
			} else {
				Fifo.rbuf[(Fifo.rxBufTail >> 1)] &= 0xFF00;
				Fifo.rbuf[(Fifo.rxBufTail >> 1)] |= UART_REGS.SCIRXBUF.bit.RXDT;
			}

			Fifo.rxCount++;
			Fifo.rxBufTail++;
			if (Fifo.rxBufTail == 2 * UART_RXB) {
				Fifo.rxBufTail = 0;
			}
		}
	}

	UART_ISR_END;
}

interrupt void uart_tx_isr(void) {
	if (Fifo.txCount) {

		if (Fifo.txBufHead & 1) { // odd
			UART_REGS.SCITXBUF = Fifo.tbuf[(Fifo.txBufHead >> 1)] >> 8;
		} else {
			UART_REGS.SCITXBUF = Fifo.tbuf[(Fifo.txBufHead >> 1)];
		}

		Fifo.txCount--;
		Fifo.txBufHead++;
		if (Fifo.txBufHead == 2 * UART_TXB) {
			Fifo.txBufHead = 0;
		}
	} else {
		UART_REGS.SCICTL2.bit.TXINTENA = 0; // disable TX isr
	}

	UART_ISR_END;
}

// get a character
char uart_getc(void) {
	uint8_t d;

	// Wait while rx fifo is empty
	while (!Fifo.rxCount) {
		;
	}

	DINT;

	if (Fifo.rxBufHead & 1) {
		d = Fifo.rbuf[(Fifo.rxBufHead >> 1)] >> 8;
	} else {
		d = Fifo.rbuf[(Fifo.rxBufHead >> 1)] & 0x00FF;
	}

	Fifo.rxCount--;
	Fifo.rxBufHead++;
	if (Fifo.rxBufHead == 2 * UART_RXB) {
		Fifo.rxBufHead = 0;
	}

	EINT;

	return d;
}

// put a character
void uart_putc(char d) {
	
	// Wait for tx fifo is not full
	while (Fifo.txCount >= 2 * UART_TXB) {
		;
	}

	if ( (UART_REGS.SCICTL2.bit.TXRDY) && (!Fifo.txCount)) {
		UART_REGS.SCITXBUF = d;
		UART_REGS.SCICTL2.bit.TXINTENA = 1; // enable TX isr
	} else {
		DINT;

		if (Fifo.txBufTail & 1) {
			Fifo.tbuf[(Fifo.txBufTail >> 1)] &= 0x00FF;
			Fifo.tbuf[(Fifo.txBufTail >> 1)] |= (uint16_t)d << 8;
		} else {
			Fifo.tbuf[(Fifo.txBufTail >> 1)] &= 0xFF00;
			Fifo.tbuf[(Fifo.txBufTail >> 1)] |= d;
		}

		Fifo.txCount++;
		Fifo.txBufTail++;
		if (Fifo.txBufTail == 2 * UART_TXB) {
			Fifo.txBufTail = 0;
		}

		EINT;
	}
}

// put a null-terminated string
void uart_puts(const char *str) {
	while (*str) {
		uart_putc(*str++);
	}
}
