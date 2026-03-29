/*
 * uart.c  --  UART1 driver for ATmega1284P
 *
 * Pins:  PD2 = RXD1,  PD3 = TXD1
 * Baud:  9600  (fixed, 8N1)
 * Clock: F_CPU must be defined (typically 16 MHz)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "uart.h"

#define UART1_BAUD 9600UL
#define UART1_UBRR ((F_CPU / 16 / UART1_BAUD) - 1)

/* ---- 128-byte interrupt-driven receive ring buffer ---- */
#define RX_BUF_SIZE 128
#define RX_BUF_MASK (RX_BUF_SIZE - 1)

static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head = 0;   /* ISR writes here  */
static volatile uint8_t rx_tail = 0;   /* main reads here  */

static uint8_t serial1_open = 0;

/* USART1 receive ISR: stores the incoming byte in the ring buffer.
 * Silently drops the byte if the buffer is full. */
ISR(USART1_RX_vect)
{
    uint8_t c = UDR1;
    uint8_t next = (rx_head + 1) & RX_BUF_MASK;
    if (next != rx_tail) {          /* drop byte if buffer full */
        rx_buf[rx_head] = c;
        rx_head = next;
    }
}

/* Opens UART1 at 9600 baud 8N1, enables TX, RX, and the
 * RX-complete interrupt, and resets the ring buffer. */
void uart1_open(void)
{
    uint16_t ubrr = UART1_UBRR;
    UBRR1H = (uint8_t)(ubrr >> 8);
    UBRR1L = (uint8_t)(ubrr);
    UCSR1B = (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1);  /* RX interrupt ON */
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
    rx_head = 0;
    rx_tail = 0;
    serial1_open = 1;
}

/* Waits for any pending transmit to finish, then disables
 * the UART and marks it closed. */
void uart1_close(void)
{
    while (!(UCSR1A & (1 << UDRE1)));
    UCSR1B = 0;
    serial1_open = 0;
}

/* Transmits one byte over UART1. Blocks until the data register
 * is empty. Does nothing if the UART is not open. */
void uart1_putc(uint8_t c)
{
    if (!serial1_open) return;
    while (!(UCSR1A & (1 << UDRE1)));
    UDR1 = c;
}

/* Transmits a null-terminated string over UART1. */
void uart1_puts(const char *s)
{
    while (*s) uart1_putc((uint8_t)*s++);
}

/* Returns 1 if at least one byte is waiting in the receive
 * ring buffer, 0 otherwise. */
uint8_t uart1_available(void)
{
    if (!serial1_open) return 0;
    return (rx_head != rx_tail) ? 1 : 0;
}

/* Blocks until a byte is available in the ring buffer, then
 * removes and returns it. */
uint8_t uart1_getc(void)
{
    while (rx_head == rx_tail);     /* wait for data */
    uint8_t c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) & RX_BUF_MASK;
    return c;
}

/* Returns 1 if UART1 has been opened with uart1_open(), 0 otherwise. */
uint8_t uart1_is_open(void)
{
    return serial1_open;
}
