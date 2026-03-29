/* font8x8.h
 * Public interface for the 8x8 pixel bitmap font stored in AVR PROGMEM.
 *
 * Usage:
 *   const uint8_t *g = font8x8_glyph('A');   // returns pointer to 8 row bytes
 *   uint8_t row_bits = pgm_read_byte(&g[r]);  // bit 7 = leftmost pixel
 *
 * Character coverage: ASCII 0x20 (' ') through 0x7F (DEL/block).
 * Any character outside that range is silently replaced with the 0x7F
 * block-character glyph.
 */
#ifndef FONT8X8_H
#define FONT8X8_H

#include <stdint.h>
#include <avr/pgmspace.h>

extern const uint8_t font8x8_data[] PROGMEM;

static inline const uint8_t *font8x8_glyph(uint8_t ch)
{
    if (ch < 0x20 || ch > 0x7F) ch = 0x7F; /* use block char for unknowns */
    return &font8x8_data[(ch - 0x20) * 8];
}

#endif /* FONT8X8_H */
