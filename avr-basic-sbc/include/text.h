/* text.h
 * Terminal-style text rendering layer for the video framebuffer.
 *
 * Provides character output, cursor management, line wrapping,
 * scrolling, and a minimal printf-style formatter.  All output
 * is written to the shared framebuffer[] using the 8x8 pixel font
 * defined in font8x8.h.
 *
 * Typical usage:
 *   text_init();
 *   text_puts("Hello, world!\n");
 *   text_printf("Value: %d\n", 42);
 *
 * The global 'term' struct holds all terminal state and may be read
 * directly, but should be written only through the API functions.
 */
#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>
#include "video_config.h"

/* terminal_t: Holds all state for the software text terminal.
 * One global instance 'term' is used by all text_* functions. */
typedef struct {
    /* cursor_col: Current cursor column (0 .. TEXT_COLS-1). */
    uint8_t cursor_col;
    /* cursor_row: Current cursor row (0 .. TEXT_ROWS-1). */
    uint8_t cursor_row;
    /* cursor_visible: Non-zero = cursor blink is enabled; 0 = hidden. */
    uint8_t cursor_visible;
    /* inverse: Non-zero = render characters with inverted pixels (white-on-black). */
    uint8_t inverse;
    /* wrap: Non-zero = advance to the next row when column reaches TEXT_COLS. */
    uint8_t wrap;
    /* scroll: Non-zero = scroll the screen up when the cursor passes the last row. */
    uint8_t scroll;
/* End of terminal_t struct. */
} terminal_t;

/* term: The single global terminal state instance used by all text_* functions. */
extern terminal_t term;

/* text_init(): Reset the terminal to its default state.
 * Places the cursor at (0,0), enables cursor visibility, wrap, and scroll.
 * Does NOT clear the screen - call text_cls() if needed. */
void text_init(void);
/* text_draw_char(col, row, ch, inverse): Render the 8x8 glyph for ASCII
 * character 'ch' at text cell (col, row).  If inverse != 0, pixel bits
 * are inverted (black-on-white effect). */
void text_draw_char(uint8_t col, uint8_t row, uint8_t ch, uint8_t inverse);
/* text_clear_char(col, row, inverse): Erase the 8x8 text cell at (col, row)
 * by filling it with 0xFF (inverse mode) or 0x00 (normal mode). */
void text_clear_char(uint8_t col, uint8_t row, uint8_t inverse);
/* text_scroll_up_one(): Scroll the framebuffer up by one text row (8 pixels).
 * Thin wrapper around video_scroll_up(8). */
void text_scroll_up_one(void);
/* text_putc(c): Output a single character to the terminal at the current
 * cursor position.  Handles control characters:
 *   '\n' (0x0A) - move to column 0 of the next row (scroll if needed)
 *   '\r' (0x0D) - move to column 0 of the current row
 *   '\b' (0x08) - backspace: move left one column and erase the cell
 *   '\t' (0x09) - advance to the next 4-column tab stop
 * All other bytes are rendered as glyphs from font8x8. */
void text_putc(char c);
/* text_puts(s): Output a null-terminated string from SRAM to the terminal. */
void text_puts(const char *s);
/* text_puts_P(s): Output a null-terminated string stored in PROGMEM.
 * Use PSTR("...") to create PROGMEM strings at compile time. */
void text_puts_P(const char *s);
/* text_printf(fmt, ...): Minimal printf to the terminal (SRAM format string).
 * Supported format specifiers:
 *   %d  - signed 16-bit decimal integer
 *   %u  - unsigned 16-bit decimal integer
 *   %x  - unsigned hex (lowercase)
 *   %X  - unsigned hex (uppercase)
 *   %c  - single character
 *   %s  - null-terminated SRAM string
 *   %%  - literal percent sign */
void text_printf(const char *fmt, ...);
/* text_printf_P(fmt, ...): Same as text_printf() but reads the format string
 * from PROGMEM.  Use PSTR("...") for the format argument. */
void text_printf_P(const char *fmt, ...);
/* text_set_cursor(col, row): Move the cursor to text cell (col, row).
 * Out-of-range values are silently ignored (cursor unchanged). */
void text_set_cursor(uint8_t col, uint8_t row);
/* text_get_cursor(col, row): Retrieve the current cursor position.
 * Either pointer may be NULL (that coordinate is then not written). */
void text_get_cursor(uint8_t *col, uint8_t *row);
/* text_cursor_blink(): Toggle the two bottom pixel rows of the current
 * cursor cell, producing a blinking underline cursor effect.
 * Call periodically (e.g. every CURSOR_BLINK_FRAMES video frames). */
void text_cursor_blink(void);
/* text_cls(): Clear the screen (fills framebuffer with 0x00) and
 * moves the cursor to (0, 0). */
void text_cls(void);
/* text_clear_eol(): Erase from the current cursor column to the end of
 * the current row.  The cursor position does not change. */
void text_clear_eol(void);
/* text_clear_eos(): Erase from the current cursor position to the end
 * of the screen (rest of current line plus all rows below). */
void text_clear_eos(void);

#endif /* TEXT_H */
