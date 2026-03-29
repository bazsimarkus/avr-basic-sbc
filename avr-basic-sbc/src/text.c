/* text.c
 * Terminal-style text rendering layer built on top of the video framebuffer.
 * Provides character output, cursor management, scrolling, and a minimal
 * printf-style formatter using the 8x8 bitmap font.
 */
 
#include <avr/pgmspace.h>
#include <stdarg.h>
#include <string.h>

#include "font8x8.h"
#include "text.h"
#include "video.h"

terminal_t term;

/* Initialises the terminal state: places the cursor at (0,0),
 * enables cursor visibility, wrap-around, and scrolling. */
void text_init(void) {
  term.cursor_col = 0;
  term.cursor_row = 0;
  term.cursor_visible = 1;
  term.inverse = 0;
  term.wrap = 1;
  term.scroll = 1;
}

/* Renders 8x8 font character ch at text cell (col, row).
 * If inverse is non-zero, pixel bits are inverted. */
void text_draw_char(uint8_t col, uint8_t row, uint8_t ch, uint8_t inverse) {
  if (col >= TEXT_COLS || row >= TEXT_ROWS) return;
  const uint8_t* g = font8x8_glyph(ch);
  uint16_t fr = (uint16_t)row * 8;
  for (uint8_t r = 0; r < 8; r++) {
    uint8_t b = pgm_read_byte(&g[r]);
    if (inverse) b = ~b;
    framebuffer[(fr + r) * HRES_BYTES + col] = b;
  }
}

/* Erases the 8x8 cell at (col, row) by filling it with
 * 0xFF (inverse) or 0x00 (normal). */
void text_clear_char(uint8_t col, uint8_t row, uint8_t inverse) {
  if (col >= TEXT_COLS || row >= TEXT_ROWS) return;
  uint16_t fr = (uint16_t)row * 8;
  uint8_t f = inverse ? 0xFF : 0x00;
  for (uint8_t r = 0; r < 8; r++) framebuffer[(fr + r) * HRES_BYTES + col] = f;
}

/* Scrolls the entire framebuffer up by 8 pixel rows (one text row). */
void text_scroll_up_one(void) { video_scroll_up(8); }

/* Advances the cursor one column to the right, wrapping to the next
 * row (and scrolling if needed) when the end of a line is reached. */
static void cursor_advance(void) {
  term.cursor_col++;
  if (term.cursor_col >= TEXT_COLS) {
    if (term.wrap) {
      term.cursor_col = 0;
      term.cursor_row++;
      if (term.cursor_row >= TEXT_ROWS) {
        if (term.scroll) {
          text_scroll_up_one();
          term.cursor_row = TEXT_ROWS - 1;
        } else
          term.cursor_row = TEXT_ROWS - 1;
      }
    } else
      term.cursor_col = TEXT_COLS - 1;
  }
}

/* Outputs a single character to the terminal, honouring control
 * characters: \n (newline), \r (carriage return), \b (backspace),
 * \t (tab to next 4-column boundary), and printable characters. */
void text_putc(char c) {
  switch (c) {
    case '\n':
      term.cursor_col = 0;
      term.cursor_row++;
      if (term.cursor_row >= TEXT_ROWS) {
        if (term.scroll) {
          text_scroll_up_one();
          term.cursor_row = TEXT_ROWS - 1;
        } else
          term.cursor_row = TEXT_ROWS - 1;
      }
      break;
    case '\r':
      term.cursor_col = 0;
      break;
    case '\b':
      if (term.cursor_col > 0) {
        term.cursor_col--;
        text_clear_char(term.cursor_col, term.cursor_row, term.inverse);
      }
      break;
    case '\t':
      do {
        text_putc(' ');
      } while (term.cursor_col & 3);
      break;
    default:
      text_draw_char(term.cursor_col, term.cursor_row, (uint8_t)c,
                     term.inverse);
      cursor_advance();
      break;
  }
}

/* Outputs a null-terminated string from RAM to the terminal. */
void text_puts(const char* s) {
  while (*s) text_putc(*s++);
}

/* Outputs a null-terminated string stored in PROGMEM to the terminal. */
void text_puts_P(const char* s) {
  char c;
  while ((c = pgm_read_byte(s++))) text_putc(c);
}

/* Internal helper: prints unsigned integer v in the given base.
 * up != 0 uses uppercase hex digits. */
static void pu(uint16_t v, uint8_t base, uint8_t up) {
  char buf[6];
  int8_t i = 0;
  if (!v) {
    text_putc('0');
    return;
  }
  while (v) {
    uint8_t d = v % base;
    buf[i++] = d < 10 ? '0' + d : (up ? 'A' : 'a') + d - 10;
    v /= base;
  }
  while (--i >= 0) text_putc(buf[i]);
}

/* Minimal printf to the terminal (RAM format string).
 * Supports: %d (signed), %u (unsigned), %x/%X (hex), %c (char), %s (string), %%. */
void text_printf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char c;
  while ((c = *fmt++)) {
    if (c != '%') {
      text_putc(c);
      continue;
    }
    c = *fmt++;
    switch (c) {
      case 'd': {
        int16_t v = va_arg(ap, int);
        if (v < 0) {
          text_putc('-');
          v = -v;
        }
        pu(v, 10, 0);
        break;
      }
      case 'u':
        pu(va_arg(ap, unsigned), 10, 0);
        break;
      case 'x':
        pu(va_arg(ap, unsigned), 16, 0);
        break;
      case 'X':
        pu(va_arg(ap, unsigned), 16, 1);
        break;
      case 'c':
        text_putc(va_arg(ap, int));
        break;
      case 's':
        text_puts(va_arg(ap, char*));
        break;
      case '%':
        text_putc('%');
        break;
      default:
        text_putc('%');
        text_putc(c);
    }
  }
  va_end(ap);
}

/* Minimal printf to the terminal (PROGMEM format string).
 * Supports the same conversion specifiers as text_printf(). */
void text_printf_P(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char c;
  while ((c = pgm_read_byte(fmt++))) {
    if (c != '%') {
      text_putc(c);
      continue;
    }
    c = pgm_read_byte(fmt++);
    switch (c) {
      case 'd': {
        int16_t v = va_arg(ap, int);
        if (v < 0) {
          text_putc('-');
          v = -v;
        }
        pu(v, 10, 0);
        break;
      }
      case 'u':
        pu(va_arg(ap, unsigned), 10, 0);
        break;
      case 'x':
        pu(va_arg(ap, unsigned), 16, 0);
        break;
      case 'X':
        pu(va_arg(ap, unsigned), 16, 1);
        break;
      case 's':
        text_puts(va_arg(ap, char*));
        break;
      case 'c':
        text_putc(va_arg(ap, int));
        break;
      case '%':
        text_putc('%');
        break;
      default:
        text_putc('%');
        text_putc(c);
    }
  }
  va_end(ap);
}

/* Moves the text cursor to the specified column and row.
 * Out-of-range values are silently ignored. */
void text_set_cursor(uint8_t col, uint8_t row) {
  if (col < TEXT_COLS) term.cursor_col = col;
  if (row < TEXT_ROWS) term.cursor_row = row;
}

/* Retrieves the current cursor position into *col and *row.
 * NULL pointers are safely ignored. */
void text_get_cursor(uint8_t* col, uint8_t* row) {
  if (col) *col = term.cursor_col;
  if (row) *row = term.cursor_row;
}

/* Toggles the two bottom pixel rows of the cursor cell to
 * produce a blinking underline cursor effect. */
void text_cursor_blink(void) {
  if (!term.cursor_visible) return;
  uint16_t py = (uint16_t)term.cursor_row * 8;
  framebuffer[(py + 6) * HRES_BYTES + term.cursor_col] ^= 0xFF;
  framebuffer[(py + 7) * HRES_BYTES + term.cursor_col] ^= 0xFF;
}

/* Clears the screen (video_clear) and moves the cursor to (0,0). */
void text_cls(void) {
  video_clear();
  term.cursor_col = 0;
  term.cursor_row = 0;
}

/* Clears all characters from the cursor position to end of line. */
void text_clear_eol(void) {
  for (uint8_t c = term.cursor_col; c < TEXT_COLS; c++)
    text_clear_char(c, term.cursor_row, term.inverse);
}

/* Clears from the cursor to the end of the screen (end of line
 * then all rows below). */
void text_clear_eos(void) {
  text_clear_eol();
  for (uint8_t r = term.cursor_row + 1; r < TEXT_ROWS; r++)
    for (uint8_t c = 0; c < TEXT_COLS; c++) text_clear_char(c, r, term.inverse);
}