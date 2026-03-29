/* keyboard.c
 * PS/2 keyboard driver using USART0 in Synchronous Slave mode.
 * Translates PS/2 Set-2 scan codes to ASCII via compile-time-selected
 * layout tables (EN, DE, or HU).  Results are queued in a small ring
 * buffer and consumed via keyboard_getc() / keyboard_available().
 */
 
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "keyboard.h"

#define BUFFER_SIZE 32
static uint8_t kbd_buffer[BUFFER_SIZE];
static volatile uint8_t head = 0, tail = 0;
static uint8_t kbd_state = 0;

#define BREAK     0x01
#define MODIFIER  0x02
#define SHIFT     0x04

/* ==========================================================================
 * Scan-code -> ASCII tables  (PS/2 Set 2)
 *
 * Each layout provides two 128-entry PROGMEM arrays:
 *   scan_noshift[]  - unshifted characters
 *   scan_shift[]    - shifted characters
 *
 * IMPORTANT:  The numpad / special-key region 0x60-0x7F follows the
 *   official PS/2 Set 2 assignments:
 *     0x66 = Backspace      -> 127  (DEL character)
 *     0x69 = KP 1           -> '1'
 *     0x6B = KP 4           -> '4'
 *     0x6C = KP 7           -> '7'
 *     0x70 = KP 0           -> '0'
 *     0x71 = KP .           -> '.'
 *     0x72 = KP 2           -> '2'
 *     0x73 = KP 5           -> '5'
 *     0x74 = KP 6           -> '6'
 *     0x75 = KP 8           -> '8'
 *     0x76 = Escape         -> 27
 *     0x79 = KP +           -> '+'
 *     0x7A = KP 3           -> '3'
 *     0x7B = KP -           -> '-'
 *     0x7C = KP *           -> '*'
 *     0x7D = KP 9           -> '9'
 *
 * Layout is selected at compile time via the macro defined in keyboard.h.
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * English (US) layout
 * -------------------------------------------------------------------------- */
#if defined(KB_LAYOUT_EN)

const char scan_noshift[] PROGMEM = {
    /*          0     1     2     3     4     5     6     7  */
    /*00*/      0,    0,    0,    0,    0,    0,    0,    0,
    /*08*/      0,    0,    0,    0,    0,    9,  '`',    0,
    /*10*/      0,    0,    0,    0,    0,  'q',  '1',    0,
    /*18*/      0,    0,  'z',  's',  'a',  'w',  '2',    0,
    /*20*/      0,  'c',  'x',  'd',  'e',  '4',  '3',    0,
    /*28*/      0,  ' ',  'v',  'f',  't',  'r',  '5',    0,
    /*30*/      0,  'n',  'b',  'h',  'g',  'y',  '6',    0,
    /*38*/      0,    0,  'm',  'j',  'u',  '7',  '8',    0,
    /*40*/      0,  ',',  'k',  'i',  'o',  '0',  '9',    0,
    /*48*/      0,  '.',  '/',  'l',  ';',  'p',  '-',    0,
    /*50*/      0,    0, '\'',    0,  '[',  '=',    0,    0,
    /*58*/      0,    0,   13,  ']',    0, '\\',    0,    0,
    /*60*/      0,    0,    0,    0,    0,    0,  127,    0,
    /*68*/      0,  '1',    0,  '4',  '7',    0,    0,    0,
    /*70*/    '0',  '.',  '2',  '5',  '6',  '8',   27,    0,
    /*78*/      0,  '+',  '3',  '-',  '*',  '9',    0,    0
};

const char scan_shift[] PROGMEM = {
    /*          0     1     2     3     4     5     6     7  */
    /*00*/      0,    0,    0,    0,    0,    0,    0,    0,
    /*08*/      0,    0,    0,    0,    0,    9,  '~',    0,
    /*10*/      0,    0,    0,    0,    0,  'Q',  '!',    0,
    /*18*/      0,    0,  'Z',  'S',  'A',  'W',  '@',    0,
    /*20*/      0,  'C',  'X',  'D',  'E',  '$',  '#',    0,
    /*28*/      0,  ' ',  'V',  'F',  'T',  'R',  '%',    0,
    /*30*/      0,  'N',  'B',  'H',  'G',  'Y',  '^',    0,
    /*38*/      0,    0,  'M',  'J',  'U',  '&',  '*',    0,
    /*40*/      0,  '<',  'K',  'I',  'O',  ')',  '(',    0,
    /*48*/      0,  '>',  '?',  'L',  ':',  'P',  '_',    0,
    /*50*/      0,    0,  '"',    0,  '{',  '+',    0,    0,
    /*58*/      0,    0,   13,  '}',    0,  '|',    0,    0,
    /*60*/      0,    0,    0,    0,    0,    0,  127,    0,
    /*68*/      0,  '1',    0,  '4',  '7',    0,    0,    0,
    /*70*/    '0',  '.',  '2',  '5',  '6',  '8',   27,    0,
    /*78*/      0,  '+',  '3',  '-',  '*',  '9',    0,    0
};

/* --------------------------------------------------------------------------
 * German (DE) QWERTZ layout
 * -------------------------------------------------------------------------- */
#elif defined(KB_LAYOUT_DE)

const char scan_noshift[] PROGMEM = {
    /*          0     1     2     3     4     5     6     7  */
    /*00*/      0,    0,    0,    0,    0,    0,    0,    0,
    /*08*/      0,    0,    0,    0,    0,    9,  '^',    0,
    /*10*/      0,    0,    0,    0,    0,  'q',  '1',    0,
    /*18*/      0,    0,  'y',  's',  'a',  'w',  '2',    0,
    /*20*/      0,  'c',  'x',  'd',  'e',  '4',  '3',    0,
    /*28*/      0,  ' ',  'v',  'f',  't',  'r',  '5',    0,
    /*30*/      0,  'n',  'b',  'h',  'g',  'z',  '6',    0,
    /*38*/      0,    0,  'm',  'j',  'u',  '7',  '8',    0,
    /*40*/      0,  ',',  'k',  'i',  'o',  '0',  '9',    0,
    /*48*/      0,  '.',  '-',  'l',  (char)0xF6,  'p',  (char)0xDF,  0,
    /*50*/      0,    0,  (char)0xE4,  0,  '+',    0,    0,    0,
    /*58*/      0,    0,   13,  '#',    0,  '<',    0,    0,
    /*60*/      0,    0,    0,    0,    0,    0,  127,    0,
    /*68*/      0,  '1',    0,  '4',  '7',    0,    0,    0,
    /*70*/    '0',  '.',  '2',  '5',  '6',  '8',   27,    0,
    /*78*/      0,  '+',  '3',  '-',  '*',  '9',    0,    0
};

const char scan_shift[] PROGMEM = {
    /*          0     1     2     3     4     5     6     7  */
    /*00*/      0,    0,    0,    0,    0,    0,    0,    0,
    /*08*/      0,    0,    0,    0,    0,    9,  (char)0xB0,  0,
    /*10*/      0,    0,    0,    0,    0,  'Q',  '!',    0,
    /*18*/      0,    0,  'Y',  'S',  'A',  'W',  '"',    0,
    /*20*/      0,  'C',  'X',  'D',  'E',  '$',  (char)0xA7,  0,
    /*28*/      0,  ' ',  'V',  'F',  'T',  'R',  '%',    0,
    /*30*/      0,  'N',  'B',  'H',  'G',  'Z',  '&',    0,
    /*38*/      0,    0,  'M',  'J',  'U',  '/',  '(',    0,
    /*40*/      0,  ';',  'K',  'I',  'O',  '=',  ')',    0,
    /*48*/      0,  ':',  '_',  'L',  (char)0xD6,  'P',  '?',  0,
    /*50*/      0,    0,  (char)0xC4,  0,  '*',    0,    0,    0,
    /*58*/      0,    0,   13, '\'',    0,  '>',    0,    0,
    /*60*/      0,    0,    0,    0,    0,    0,  127,    0,
    /*68*/      0,  '1',    0,  '4',  '7',    0,    0,    0,
    /*70*/    '0',  '.',  '2',  '5',  '6',  '8',   27,    0,
    /*78*/      0,  '+',  '3',  '-',  '*',  '9',    0,    0
};

/* --------------------------------------------------------------------------
 * Hungarian (HU) QWERTZ layout  [DEFAULT]
 *
 * Characters are stored as ISO-8859-2 (Latin-2) byte values:
 *   a=0xE1 A=0xC1  e=0xE9 E=0xC9  i=0xED I=0xCD
 *   o=0xF3 O=0xD3  o=0xF6 O=0xD6  o=0x91 O=0x81  (ISO-8859-2)
 *   u=0xFA U=0xDA  u=0xFC U=0xDC  u=0x9B U=0x8B  (ISO-8859-2)
 * -------------------------------------------------------------------------- */
#elif defined(KB_LAYOUT_HU)

const char scan_noshift[] PROGMEM = {
    /*          0     1     2     3     4     5     6     7  */
    /*00*/      0,    0,    0,    0,    0,    0,    0,    0,
    /*08*/      0,    0,    0,    0,    0,    9,  '0',    0,
    /*10*/      0,    0,    0,    0,    0,  'q',  '1',    0,
    /*18*/      0,    0,  'y',  's',  'a',  'w',  '2',    0,
    /*20*/      0,  'c',  'x',  'd',  'e',  '4',  '3',    0,
    /*28*/      0,  ' ',  'v',  'f',  't',  'r',  '5',    0,
    /*30*/      0,  'n',  'b',  'h',  'g',  'z',  '6',    0,
    /*38*/      0,    0,  'm',  'j',  'u',  '7',  '8',    0,
    /*40*/      0,  ',',  'k',  'i',  'o',  (char)0xF6,  '9',    0,
    /*48*/      0,  '.',  (char)0xED,  'l',  (char)0xE9,  'p',  (char)0xFA,  0,
    /*50*/      0,    0,  (char)0xE1,    0,  (char)0x9B,    0,    0,    0,
    /*58*/      0,    0,   13,  (char)0xFC,    0,  (char)0x91,    0,    0,
    /*60*/      0,    0,    0,    0,    0,    0,  127,    0,
    /*68*/      0,  '1',    0,  '4',  '7',    0,    0,    0,
    /*70*/    '0',  '.',  '2',  '5',  '6',  '8',   27,    0,
    /*78*/      0,  '+',  '3',  '-',  '*',  '9',    0,    0
};

const char scan_shift[] PROGMEM = {
    /*          0     1     2     3     4     5     6     7  */
    /*00*/      0,    0,    0,    0,    0,    0,    0,    0,
    /*08*/      0,    0,    0,    0,    0,    9,  (char)0xA7,  0,
    /*10*/      0,    0,    0,    0,    0,  'Q',  '!',    0,
    /*18*/      0,    0,  'Y',  'S',  'A',  'W',  '"',    0,
    /*20*/      0,  'C',  'X',  'D',  'E',  '$',  '+',    0,
    /*28*/      0,  ' ',  'V',  'F',  'T',  'R',  '%',    0,
    /*30*/      0,  'N',  'B',  'H',  'G',  'Z',  '/',    0,
    /*38*/      0,    0,  'M',  'J',  'U',  '=',  '(',    0,
    /*40*/      0,  '?',  'K',  'I',  'O',  (char)0xD6,  ')',    0,
    /*48*/      0,  ':',  (char)0xCD,  'L',  (char)0xC9,  'P',  (char)0xDA,  0,
    /*50*/      0,    0,  (char)0xC1,    0,  (char)0x8B,    0,    0,    0,
    /*58*/      0,    0,   13,  (char)0xDC,    0,  (char)0x81,    0,    0,
    /*60*/      0,    0,    0,    0,    0,    0,  127,    0,
    /*68*/      0,  '1',    0,  '4',  '7',    0,    0,    0,
    /*70*/    '0',  '.',  '2',  '5',  '6',  '8',   27,    0,
    /*78*/      0,  '+',  '3',  '-',  '*',  '9',    0,    0
};

#endif /* layout selection */


/* ==========================================================================
 * Driver implementation (layout-independent)
 * ========================================================================== */

/* Configures USART0 as a synchronous slave receiver for PS/2 input.
 * Sets PB0 (XCK0) as input and enables the receiver with odd parity. */
void keyboard_init(void) {
    /* Port B Pin 0 (XCK0) must be input for Synchronous Slave mode */
    DDRB &= ~(1 << PB0);
    /* Set USART0 to Synchronous Slave Mode
     * 1 Start, 8 Data, Odd Parity, 1 Stop bit (Standard PS/2) */
    UBRR0 = 0;
    UCSR0C = (1 << UMSEL00) | (1 << UPM01) | (1 << UPM00)
           | (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B = (1 << RXEN0);
}

/* This function must be called frequently (e.g., in the VBlank or main loop) */
/* Checks for a newly received scan-code byte from the PS/2 interface
 * and enqueues it in the ring buffer.  Must be called frequently
 * (e.g., in the main loop or VBlank) since there is no RX interrupt. */
void keyboard_poll(void) {
    if (UCSR0A & (1 << RXC0)) {
        uint8_t sc   = UDR0;
        uint8_t next = (head + 1) % BUFFER_SIZE;
        if (next != tail) {
            kbd_buffer[head] = sc;
            head = next;
        }
    }
}

/* Blocks until a complete, printable ASCII character is available,
 * processing break codes, modifier keys, and shift state internally.
 * Returns the ASCII value (> 0) or 0 for non-printing scan codes. */
int keyboard_getc(void) {
    while (tail == head) keyboard_poll(); /* Wait for key */

    uint8_t sc = kbd_buffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;

    if (sc == 0xF0) { kbd_state |= BREAK;    return 0; }
    if (sc == 0xE0) { kbd_state |= MODIFIER; return 0; }

    /* Handle Shift keys (left: 0x12, right: 0x59) */
    if (sc == 0x12 || sc == 0x59) {
        if (kbd_state & BREAK) kbd_state &= ~SHIFT;
        else                   kbd_state |=  SHIFT;
        kbd_state &= ~BREAK;
        return 0;
    }

    if (kbd_state & BREAK) {
        kbd_state &= ~(BREAK | MODIFIER);
        return 0;
    }

    char c = 0;
    if (kbd_state & SHIFT) {
        if (sc < sizeof(scan_shift))   c = pgm_read_byte(&scan_shift[sc]);
    } else {
        if (sc < sizeof(scan_noshift)) c = pgm_read_byte(&scan_noshift[sc]);
    }

    kbd_state &= ~MODIFIER;
    return (unsigned char)c;   /* cast avoids sign-extension of high bytes */
}

/* Returns non-zero if at least one byte is waiting in the scan-code
 * ring buffer (calls keyboard_poll() first to flush hardware FIFO). */
uint8_t keyboard_available(void) {
    keyboard_poll();
    return (head != tail);
}
