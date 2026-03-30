/* keyboard.h
 * PS/2 keyboard driver interface.
 *
 * The driver uses USART0 in Synchronous Slave mode (PB0 = XCK0 clock input,
 * PD0 = RXD0 data input) to receive PS/2 Set-2 scan codes without using
 * any external interrupt pin.
 *
 * Scan codes are buffered in a 32-byte ring buffer.  Call keyboard_poll()
 * frequently (every main-loop iteration or in the VBlank handler) to drain
 * the hardware receive register into the ring buffer before it overflows.
 *
 * keyboard_getc() assembles complete characters from scan codes, handles
 * break codes (key-release), and applies the selected keyboard layout table.
 *
 * Layout selection (compile-time, via -D flag or this header):
 *   -DKB_LAYOUT_EN  US/English QWERTY  (default if none specified)
 *   -DKB_LAYOUT_DE  German QWERTZ
 *   -DKB_LAYOUT_HU  Hungarian QWERTZ
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/* --------------------------------------------------------------------------
 * Keyboard layout selection
 *
 * Define exactly ONE of the following before including this header,
 * or pass it on the compiler command-line:
 *
 *   -DKB_LAYOUT_EN   Standard US/English layout  <-- DEFAULT
 *   -DKB_LAYOUT_DE   Standard German (QWERTZ) layout
 *   -DKB_LAYOUT_HU   Standard Hungarian (QWERTZ) layout
 *
 * Example (compiler flag):
 *   avr-gcc ... -DKB_LAYOUT_DE ...
 * -------------------------------------------------------------------------- */

#if !defined(KB_LAYOUT_EN) && !defined(KB_LAYOUT_DE) && !defined(KB_LAYOUT_HU)
#  define KB_LAYOUT_EN   /* US/English QWERTY is the default layout */
#endif

/* keyboard_init(): Configure USART0 as a PS/2 synchronous slave receiver.
 * Sets PB0 (XCK0) as input, enables the receiver with odd parity (PS/2
 * requires odd parity), and resets the ring buffer.
 * Must be called once before keyboard_poll() or keyboard_getc(). */
void    keyboard_init(void);
/* keyboard_poll(): Check for a newly received scan-code byte and enqueue it.
 * Must be called frequently because there is no RX interrupt - the hardware
 * FIFO holds only one byte and will be overwritten if not drained in time.
 * Safe to call from the main loop or the video VBlank handler. */
void    keyboard_poll(void);
/* keyboard_getc(): Block until a complete printable character is available.
 * Internally processes break codes (0xF0 prefix = key release), modifier
 * keys (0xE0 prefix), and shift state.  Returns the ASCII value (> 0) or
 * 0 for non-printing scan codes (e.g. pure modifier keys). */
int     keyboard_getc(void);
/* keyboard_available(): Return non-zero if at least one scan-code byte is
 * waiting in the ring buffer (calls keyboard_poll() first to drain hardware).
 * Use this for non-blocking input before calling keyboard_getc(). */
uint8_t keyboard_available(void);

#endif /* KEYBOARD_H */
