#ifndef _UART_DEFS
#define _UART_DEFS

// FIFO size (data byte packed)
#define UART_RXB	32    // Size of Rx buffer = 2 * UART_RXB bytes
#define UART_TXB	32    // Size of Tx buffer = 2 * UART_TXB bytes

#define CPU_FREQ	100E6
#define LSPCLK_FREQ (CPU_FREQ / 4)
#define CALC_SCI_PRD(x) ( (LSPCLK_FREQ / ((x) * 8)) - 1 )

void uart_init(uint32_t bps_reg); // must be called as uart_init(CALC_SCI_PRD(19200L));

void uart_putc(char d);           // put a character
void uart_puts(const char *str);  // put a null-terminated string

char uart_getc(void);             // get a character

interrupt void uart_rx_isr(void); // rx/tx isr routines
interrupt void uart_tx_isr(void);

#endif
