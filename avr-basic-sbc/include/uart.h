/*
 * uart.h  --  UART1 driver for ATmega1284P (PD2=RXD1, PD3=TXD1)
 *
 * Used by the BASIC interpreter for:
 *   SEROPEN   - open serial port at 9600 baud
 *   SERCLOSE  - close serial port
 *   SERPRINT  - print to serial
 *   SERREAD   - read a line from serial (with timeout)
 *   SERLOAD   - load a BASIC program from serial
 */
 
#ifndef UART_H
#define UART_H

#include <stdint.h>

/* uart1_open(): Open UART1 at 9600 baud, 8 data bits, no parity, 1 stop bit (8N1).
 * Configures UBRRn for 9600 at F_CPU, enables TX, RX, and the RX-complete interrupt,
 * and resets the receive ring buffer.  Call once before uart1_putc/uart1_getc.
 * After this call uart1_is_open() returns 1. */
void    uart1_open(void);

/* uart1_close(): Wait for any pending transmit to complete, then disable UART1
 * and mark the port as closed.  After this call uart1_is_open() returns 0.
 * Necessary before re-opening at a different baud rate (future use). */
void    uart1_close(void);

/* uart1_putc(c): Transmit one byte over UART1.  Blocks (busy-waits) until the
 * UDRE1 (data register empty) flag is set, then writes 'c' to UDR1.
 * Does nothing if the port has not been opened with uart1_open(). */
void    uart1_putc(uint8_t c);

/* uart1_puts(s): Transmit a null-terminated SRAM string over UART1 by calling
 * uart1_putc() for each character.  Stops at the null terminator (not transmitted). */
void    uart1_puts(const char *s);

/* uart1_available(): Return 1 if at least one byte is waiting in the 128-byte
 * interrupt-driven receive ring buffer, 0 otherwise.
 * Returns 0 immediately if the port is not open. */
uint8_t uart1_available(void);

/* uart1_getc(): Remove and return one byte from the receive ring buffer.
 * Blocks (busy-waits) until a byte is available.  Does not disable interrupts
 * during the read; the ISR may add more bytes concurrently.
 * Call uart1_available() first to avoid blocking. */
uint8_t uart1_getc(void);

/* uart1_is_open(): Return 1 if uart1_open() has been called and uart1_close()
 * has not been called since, 0 otherwise. */
uint8_t uart1_is_open(void);

#endif /* UART_H */