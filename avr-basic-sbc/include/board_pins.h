/*
 * board_pins.h  -  Hardware pin definitions for AVR-SBC v1.2
 *
 * Centralises all port/DDR/bit assignments so that changing the PCB layout
 * only requires editing this one file.  The BASIC interpreter accesses GPIO
 * through the AWRITE/DWRITE/AREAD/DREAD commands, which call the inline
 * helper functions defined at the bottom of this header.
 *
 * All assignments read directly from avr-basic-sbc.net (KiCad netlist).
 *
 * ============================================================
 * MightyCore Arduino-style pin number mapping
 * ============================================================
 *  Pin  Port/Bit  Notes
 *  ---  --------  -----------------------------------
 *   0   PB0       PS/2 keyboard clock  -- NOT AVAILABLE
 *   1   PB1       GPIO (J7)
 *   2   PB2       GPIO (J7)
 *   3   PB3       GPIO (J7)
 *   4   PB4       SPI /CS for EEPROM   -- shared with SPI
 *   5   PB5       SPI MOSI             -- shared with SPI
 *   6   PB6       SPI MISO             -- shared with SPI
 *   7   PB7       SPI SCK              -- shared with SPI
 *   8   PD0       PS/2 keyboard data   -- NOT AVAILABLE
 *   9   PD1       UART1 TX (J9)
 *  10   PD2       UART1 RX (J9)
 *  11   PD3       GPIO (J9)
 *  12   PD4       GPIO (J9)
 *  13   PD5       Video sync           -- NOT AVAILABLE
 *  14   PD6       Yellow LED (J9)
 *  15   PD7       Buzzer / OC2A
 *  16   PC0       GPIO (J8)
 *  17   PC1       GPIO (J8)
 *  18   PC2       GPIO (J8)
 *  19   PC3       GPIO (J8)
 *  20   PC4       GPIO (J8)
 *  21   PC5       GPIO (J8)
 *  22   PC6       GPIO (J8)
 *  23   PC7       GPIO (J8)
 *  24   PA0       GPIO (J6)
 *  25   PA1       GPIO (J6)
 *  26   PA2       GPIO (J6)
 *  27   PA3       GPIO (J6)
 *  28   PA4       GPIO (J6)
 *  29   PA5       GPIO (J6)
 *  30   PA6       GPIO (J6)
 *  31   PA7       Video data           -- NOT AVAILABLE
 *
 * ============================================================
 * SPI / 25LC640 External EEPROM (U3)
 * ============================================================
 *  MOSI    PB5 (pin 5)
 *  MISO    PB6 (pin 6)
 *  SCK     PB7 (pin 7)
 *  /CS     PB4 (pin 4)
 *
 * ============================================================
 * Buzzer / Piezo (BZ1)
 * ============================================================
 *  OC2A = PD7 (pin 15)  -> Timer2 CTC tone generation
 *
 * ============================================================
 * Video (J4 RCA)
 * ============================================================
 *  Video  -> PA7 (pin 31)
 *  Sync   -> PD5 (pin 13)
 *
 * ============================================================
 * PS/2 Keyboard (J2)
 * ============================================================
 *  Clock  -> PB0 (pin 0)
 *  Data   -> PD0 (pin 8)
 */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <avr/io.h>

/* ------------------------------------------------------------------ */
/* Piezo / Buzzer  (BZ1 via R8 and slide-switch S3)                   */
/* OC2A on PD7 - Timer2 CTC toggles this pin for tone generation      */
/* ------------------------------------------------------------------ */
#define PIEZO_DDR    DDRD
#define PIEZO_PORT   PORTD
#define PIEZO_BIT    PD7

/* ------------------------------------------------------------------ */
/* SPI / 25LC640 external EEPROM (U3)                                 */
/* Hardware SPI on PB4(CS) PB5(MOSI) PB6(MISO) PB7(SCK)             */
/* ------------------------------------------------------------------ */
#define XEEP_CS_DDR   DDRB
#define XEEP_CS_PORT  PORTB
#define XEEP_CS_BIT   PB4

#define SPI_DDR       DDRB
#define SPI_PORT      PORTB
#define SPI_MOSI      PB5
#define SPI_MISO      PB6
#define SPI_SCK       PB7

/* Convenience macros */
#define XEEP_CS_LOW()   (XEEP_CS_PORT &= ~(1<<XEEP_CS_BIT))
#define XEEP_CS_HIGH()  (XEEP_CS_PORT |=  (1<<XEEP_CS_BIT))

/* ------------------------------------------------------------------ */
/* UART1  (PD2=RXD1, PD3=TXD1)                                       */
/* ------------------------------------------------------------------ */
#define UART1_BAUD_RATE   9600UL
#define UART1_UBRR_VAL    ((F_CPU / 16 / UART1_BAUD_RATE) - 1)

/* ------------------------------------------------------------------ */
/* Video  (PA7=data via R2-470R, PD5=sync via R3-1k)                  */
/* ------------------------------------------------------------------ */
#define VIDEO_DATA_PORT   PORTA
#define VIDEO_DATA_DDR    DDRA
#define VIDEO_DATA_BIT    PA7
#define VIDEO_SYNC_PORT   PORTD
#define VIDEO_SYNC_DDR    DDRD
#define VIDEO_SYNC_BIT    PD5

/* ------------------------------------------------------------------ */
/* PS/2 Keyboard  (PB0=Clock, PD0=Data)                               */
/* ------------------------------------------------------------------ */
#define KBD_CLK_DDR   DDRB
#define KBD_CLK_PORT  PORTB
#define KBD_CLK_PIN   PINB
#define KBD_CLK_BIT   PB0

#define KBD_DATA_DDR  DDRD
#define KBD_DATA_PORT PORTD
#define KBD_DATA_PIN  PIND
#define KBD_DATA_BIT  PD0

/* ------------------------------------------------------------------ */
/* Yellow LED D3  (PD6 via R5-470R, active high)                      */
/* ------------------------------------------------------------------ */
#define LED_DDR   DDRD
#define LED_PORT  PORTD
#define LED_BIT   PD6

/* ------------------------------------------------------------------ */
/* AWRITE / DWRITE / AREAD / DREAD logical pin mapping                */
/*                                                                     */
/* MightyCore standard pinout for ATmega1284P:                         */
/*   0-7   = PB0-PB7                                                   */
/*   8-15  = PD0-PD7                                                   */
/*  16-23  = PC0-PC7                                                   */
/*  24-31  = PA0-PA7                                                   */
/*                                                                     */
/* Reserved / unavailable pins:                                        */
/*   0  = PB0  (PS/2 keyboard clock)                                   */
/*   8  = PD0  (PS/2 keyboard data)                                    */
/*  13  = PD5  (video sync)                                            */
/*  31  = PA7  (video data)                                            */
/* ------------------------------------------------------------------ */

/* Bit that tells pin_is_available() whether a pin can be used */
#define PIN_AVAIL_MASK_PB  0xFE  /* PB1-PB7 ok, PB0 reserved */
#define PIN_AVAIL_MASK_PD  0xDE  /* PD1-PD4,PD6,PD7 ok, PD0,PD5 reserved */
#define PIN_AVAIL_MASK_PC  0xFF  /* PC0-PC7 all ok */
#define PIN_AVAIL_MASK_PA  0x7F  /* PA0-PA6 ok, PA7 reserved */

/* Returns 1 if the logical pin number is usable for GPIO, 0 if reserved */
/* pin_is_available(pin): Return 1 if the MightyCore logical pin number 'pin'
 * is safe to use as a general-purpose I/O pin, 0 if it is reserved by the
 * system (keyboard, video, SPI).  Used by DREAD/DWRITE/AWRITE to validate
 * the pin argument before touching hardware. */
static inline uint8_t pin_is_available(uint8_t pin)
{
    uint8_t bit = pin & 7;
    if (pin < 8)  return (PIN_AVAIL_MASK_PB >> bit) & 1;
    if (pin < 16) return (PIN_AVAIL_MASK_PD >> bit) & 1;
    if (pin < 24) return (PIN_AVAIL_MASK_PC >> bit) & 1;
    if (pin < 32) return (PIN_AVAIL_MASK_PA >> bit) & 1;
    return 0;  /* pin >= 32: invalid */
}

/* pin_to_ddr(pin): Return a pointer to the DDR register for logical pin 'pin'.
 * Used by DWRITE/AWRITE to set the pin direction to output. */
static inline volatile uint8_t *pin_to_ddr(uint8_t pin)
{
    if (pin < 8)  return &DDRB;
    if (pin < 16) return &DDRD;
    if (pin < 24) return &DDRC;
    return &DDRA;
}

/* pin_to_port(pin): Return a pointer to the PORT register for logical pin 'pin'.
 * Used by DWRITE/AWRITE to drive the output level. */
static inline volatile uint8_t *pin_to_port(uint8_t pin)
{
    if (pin < 8)  return &PORTB;
    if (pin < 16) return &PORTD;
    if (pin < 24) return &PORTC;
    return &PORTA;
}

/* pin_to_pin(pin): Return a pointer to the PIN (input) register for logical pin
 * 'pin'.  Used by DREAD to sample the digital input level. */
static inline volatile uint8_t *pin_to_pin(uint8_t pin)
{
    if (pin < 8)  return &PINB;
    if (pin < 16) return &PIND;
    if (pin < 24) return &PINC;
    return &PINA;
}

/* pin_to_bit(pin): Return the bit number (0-7) within the port registers for
 * logical pin 'pin'.  The bit is always pin & 7 regardless of port. */
static inline uint8_t pin_to_bit(uint8_t pin) { return pin & 7; }

#endif /* BOARD_PINS_H */
