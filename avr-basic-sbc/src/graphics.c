/* graphics.c
 * 2-D graphics primitives operating on the shared framebuffer.
 * All drawing respects the global gfx_draw_mode (SET / CLEAR / XOR).
 * Coordinates are in pixels; (0,0) is the top-left corner.
 */
 
#include <avr/pgmspace.h>
#include <stdlib.h>

#include "font8x8.h"
#include "graphics.h"
#include "video_config.h"

draw_mode_t gfx_draw_mode = DRAW_SET;

/* Sets, clears, or XORs a single pixel at (x, y) according to gfx_draw_mode.
 * Out-of-bounds coordinates are silently ignored. */
void gfx_pixel(int16_t x, int16_t y) {
  if ((uint16_t)x >= HRES_PIXELS || (uint16_t)y >= VRES) return;
  uint16_t i = (uint16_t)y * HRES_BYTES + (x >> 3);
  uint8_t m = 0x80 >> (x & 7);
  switch (gfx_draw_mode) {
    case DRAW_SET:
      framebuffer[i] |= m;
      break;
    case DRAW_CLEAR:
      framebuffer[i] &= ~m;
      break;
    case DRAW_XOR:
      framebuffer[i] ^= m;
      break;
  }
}

/* Returns the minimum of two int16_t values. */
static inline int16_t _mn(int16_t a, int16_t b) { return a < b ? a : b; }

/* Returns the maximum of two int16_t values. */
static inline int16_t _mx(int16_t a, int16_t b) { return a > b ? a : b; }

/* Clamps v to the range [lo, hi]. */
static inline int16_t _cl(int16_t v, int16_t lo, int16_t hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

/* Swaps the values pointed to by a and b. */
static inline void _sw(int16_t *a, int16_t *b) {
  int16_t t = *a;
  *a = *b;
  *b = t;
}

/* Draws a horizontal line from (x0, y) to (x1, y).
 * Coordinates are clamped to the framebuffer bounds.
 * Uses byte-wide writes where possible for speed. */
void gfx_hline(int16_t x0, int16_t x1, int16_t y) {
  if ((uint16_t)y >= VRES) return;
  if (x0 > x1) _sw(&x0, &x1);
  x0 = _cl(x0, 0, HRES_PIXELS - 1);
  x1 = _cl(x1, 0, HRES_PIXELS - 1);
  uint8_t *row = framebuffer + (uint16_t)y * HRES_BYTES;
  for (int16_t x = x0; x <= x1;) {
    uint8_t bit = x & 7;
    uint16_t bi = x >> 3;
    if (bit == 0 && (x + 7) <= x1) {
      switch (gfx_draw_mode) {
        case DRAW_SET:
          row[bi] = 0xFF;
          break;
        case DRAW_CLEAR:
          row[bi] = 0;
          break;
        case DRAW_XOR:
          row[bi] ^= 0xFF;
          break;
      }
      x += 8;
    } else {
      uint8_t m = 0x80 >> bit;
      switch (gfx_draw_mode) {
        case DRAW_SET:
          row[bi] |= m;
          break;
        case DRAW_CLEAR:
          row[bi] &= ~m;
          break;
        case DRAW_XOR:
          row[bi] ^= m;
          break;
      }
      x++;
    }
  }
}

/* Draws a vertical line from (x, y0) to (x, y1).
 * Coordinates are clamped to the framebuffer bounds. */
void gfx_vline(int16_t x, int16_t y0, int16_t y1) {
  if ((uint16_t)x >= HRES_PIXELS) return;
  if (y0 > y1) _sw(&y0, &y1);
  y0 = _cl(y0, 0, VRES - 1);
  y1 = _cl(y1, 0, VRES - 1);
  uint8_t mask = 0x80 >> (x & 7);
  uint16_t cb = x >> 3;
  uint8_t *p = framebuffer + (uint16_t)y0 * HRES_BYTES + cb;
  for (int16_t y = y0; y <= y1; y++, p += HRES_BYTES) {
    switch (gfx_draw_mode) {
      case DRAW_SET:
        *p |= mask;
        break;
      case DRAW_CLEAR:
        *p &= ~mask;
        break;
      case DRAW_XOR:
        *p ^= mask;
        break;
    }
  }
}

/* Draws an arbitrary line from (x0, y0) to (x1, y1) using
 * Bresenham's algorithm. Delegates to gfx_hline/gfx_vline
 * for axis-aligned cases. */
void gfx_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  if (y0 == y1) {
    gfx_hline(x0, x1, y0);
    return;
  }
  if (x0 == x1) {
    gfx_vline(x0, y0, y1);
    return;
  }
  int16_t dx = abs(x1 - x0), dy = -abs(y1 - y0);
  int8_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  for (;;) {
    gfx_pixel(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

/* Draws an unfilled rectangle with top-left corner (x, y),
 * width w, and height h. */
void gfx_rect(int16_t x, int16_t y, int16_t w, int16_t h) {
  gfx_hline(x, x + w - 1, y);
  gfx_hline(x, x + w - 1, y + h - 1);
  gfx_vline(x, y, y + h - 1);
  gfx_vline(x + w - 1, y, y + h - 1);
}

/* Draws a filled rectangle with top-left corner (x, y),
 * width w, and height h. */
void gfx_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h) {
  for (int16_t r = y; r < y + h; r++) gfx_hline(x, x + w - 1, r);
}

/* Draws an unfilled circle with centre (cx, cy) and radius r
 * using the midpoint circle algorithm. */
void gfx_circle(int16_t cx, int16_t cy, int16_t r) {
  if (r <= 0) {
    gfx_pixel(cx, cy);
    return;
  }
  int16_t x = 0, y = r, d = 3 - 2 * r;
  while (x <= y) {
    gfx_pixel(cx + x, cy + y);
    gfx_pixel(cx - x, cy + y);
    gfx_pixel(cx + x, cy - y);
    gfx_pixel(cx - x, cy - y);
    gfx_pixel(cx + y, cy + x);
    gfx_pixel(cx - y, cy + x);
    gfx_pixel(cx + y, cy - x);
    gfx_pixel(cx - y, cy - x);
    if (d < 0)
      d += 4 * x + 6;
    else {
      d += 4 * (x - y) + 10;
      y--;
    }
    x++;
  }
}

/* Draws a filled circle with centre (cx, cy) and radius r
 * using horizontal spans. */
void gfx_fill_circle(int16_t cx, int16_t cy, int16_t r) {
  if (r <= 0) {
    gfx_pixel(cx, cy);
    return;
  }
  int16_t x = 0, y = r, d = 3 - 2 * r;
  while (x <= y) {
    gfx_hline(cx - x, cx + x, cy + y);
    gfx_hline(cx - x, cx + x, cy - y);
    gfx_hline(cx - y, cx + y, cy + x);
    gfx_hline(cx - y, cx + y, cy - x);
    if (d < 0)
      d += 4 * x + 6;
    else {
      d += 4 * (x - y) + 10;
      y--;
    }
    x++;
  }
}

/* Draws an unfilled axis-aligned ellipse with centre (cx, cy)
 * and semi-axes rx (horizontal) and ry (vertical). */
void gfx_ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry) {
  if (rx <= 0 || ry <= 0) {
    gfx_pixel(cx, cy);
    return;
  }
  int32_t rx2 = (int32_t)rx * rx, ry2 = (int32_t)ry * ry;
  int16_t x = 0, y = ry;
  int32_t px = 0, py = 2 * rx2 * y, p = ry2 - rx2 * ry + rx2 / 4;
  while (px < py) {
    gfx_pixel(cx + x, cy + y);
    gfx_pixel(cx - x, cy + y);
    gfx_pixel(cx + x, cy - y);
    gfx_pixel(cx - x, cy - y);
    x++;
    px += 2 * ry2;
    if (p < 0)
      p += ry2 + px;
    else {
      y--;
      py -= 2 * rx2;
      p += ry2 + px - py;
    }
  }
  p = ry2 * ((int32_t)(2 * x + 1) * (2 * x + 1)) / 4 +
      rx2 * ((int32_t)(y - 1) * (y - 1) - ry2);
  while (y >= 0) {
    gfx_pixel(cx + x, cy + y);
    gfx_pixel(cx - x, cy + y);
    gfx_pixel(cx + x, cy - y);
    gfx_pixel(cx - x, cy - y);
    y--;
    py -= 2 * rx2;
    if (p > 0)
      p += rx2 - py;
    else {
      x++;
      px += 2 * ry2;
      p += rx2 - py + px;
    }
  }
}

/* Draws a filled axis-aligned ellipse with centre (cx, cy)
 * and semi-axes rx and ry using horizontal spans. */
void gfx_fill_ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry) {
  if (rx <= 0 || ry <= 0) {
    gfx_pixel(cx, cy);
    return;
  }
  int32_t rx2 = (int32_t)rx * rx, ry2 = (int32_t)ry * ry;
  int16_t x = 0, y = ry, ly = -1;
  int32_t px = 0, py = 2 * rx2 * y, p = ry2 - rx2 * ry + rx2 / 4;
  while (px < py) {
    if (y != ly) {
      gfx_hline(cx - x, cx + x, cy + y);
      gfx_hline(cx - x, cx + x, cy - y);
      ly = y;
    }
    x++;
    px += 2 * ry2;
    if (p < 0)
      p += ry2 + px;
    else {
      y--;
      py -= 2 * rx2;
      p += ry2 + px - py;
    }
  }
  p = ry2 * ((int32_t)(2 * x + 1) * (2 * x + 1)) / 4 +
      rx2 * ((int32_t)(y - 1) * (y - 1) - ry2);
  while (y >= 0) {
    gfx_hline(cx - x, cx + x, cy + y);
    gfx_hline(cx - x, cx + x, cy - y);
    y--;
    py -= 2 * rx2;
    if (p > 0)
      p += rx2 - py;
    else {
      x++;
      px += 2 * ry2;
      p += rx2 - py + px;
    }
  }
}

/* Draws an unfilled triangle defined by three vertices. */
void gfx_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                  int16_t y2) {
  gfx_line(x0, y0, x1, y1);
  gfx_line(x1, y1, x2, y2);
  gfx_line(x2, y2, x0, y0);
}

/* Draws a filled triangle defined by three vertices.
 * Vertices are sorted by Y before scan-line filling. */
void gfx_fill_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2) {
  if (y0 > y1) {
    _sw(&y0, &y1);
    _sw(&x0, &x1);
  }
  if (y1 > y2) {
    _sw(&y1, &y2);
    _sw(&x1, &x2);
  }
  if (y0 > y1) {
    _sw(&y0, &y1);
    _sw(&x0, &x1);
  }
  if (y0 == y2) {
    gfx_hline(_mn(x0, _mn(x1, x2)), _mx(x0, _mx(x1, x2)), y0);
    return;
  }
  for (int16_t y = y0; y <= y2; y++) {
    int16_t xa, xb;
    if (y2 != y0)
      xa = x0 + (int16_t)((int32_t)(x2 - x0) * (y - y0) / (y2 - y0));
    else
      xa = x0;
    if (y < y1) {
      if (y1 != y0)
        xb = x0 + (int16_t)((int32_t)(x1 - x0) * (y - y0) / (y1 - y0));
      else
        xb = x0;
    } else {
      if (y2 != y1)
        xb = x1 + (int16_t)((int32_t)(x2 - x1) * (y - y1) / (y2 - y1));
      else
        xb = x1;
    }
    gfx_hline(xa, xb, y);
  }
}

/* Draws an unfilled rectangle with rounded corners.
 * r is the corner radius; degrades to gfx_rect() when r <= 0. */
void gfx_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) {
  if (r <= 0) {
    gfx_rect(x, y, w, h);
    return;
  }
  if (2 * r > w) r = w / 2;
  if (2 * r > h) r = h / 2;
  int16_t x1 = x + r, y1 = y + r, x2 = x + w - 1 - r, y2 = y + h - 1 - r;
  gfx_hline(x1, x2, y);
  gfx_hline(x1, x2, y + h - 1);
  gfx_vline(x, y1, y2);
  gfx_vline(x + w - 1, y1, y2);
  int16_t cx = 0, cy = r, d = 3 - 2 * r;
  while (cx <= cy) {
    gfx_pixel(x2 + cx, y1 - cy);
    gfx_pixel(x2 + cy, y1 - cx);
    gfx_pixel(x1 - cx, y1 - cy);
    gfx_pixel(x1 - cy, y1 - cx);
    gfx_pixel(x2 + cx, y2 + cy);
    gfx_pixel(x2 + cy, y2 + cx);
    gfx_pixel(x1 - cx, y2 + cy);
    gfx_pixel(x1 - cy, y2 + cx);
    if (d < 0)
      d += 4 * cx + 6;
    else {
      d += 4 * (cx - cy) + 10;
      cy--;
    }
    cx++;
  }
}

/* Draws a 1-bit-per-pixel bitmap stored in PROGMEM at (x, y).
 * w must be a multiple of 8; h is the number of rows. */
void gfx_bitmap_P(int16_t x, int16_t y, const uint8_t *bmp, int16_t w,
                  int16_t h) {
  int16_t wb = w >> 3;
  for (int16_t r = 0; r < h; r++)
    for (int16_t c = 0; c < wb; c++) {
      uint8_t b = pgm_read_byte(bmp++);
      for (uint8_t bit = 0; bit < 8; bit++)
        if (b & (0x80 >> bit)) gfx_pixel(x + c * 8 + bit, y + r);
    }
}

/* Draws a 1-bit-per-pixel bitmap from RAM at (x, y).
 * w must be a multiple of 8; h is the number of rows. */
void gfx_bitmap(int16_t x, int16_t y, const uint8_t *bmp, int16_t w,
                int16_t h) {
  int16_t wb = w >> 3;
  for (int16_t r = 0; r < h; r++)
    for (int16_t c = 0; c < wb; c++) {
      uint8_t b = *bmp++;
      for (uint8_t bit = 0; bit < 8; bit++)
        if (b & (0x80 >> bit)) gfx_pixel(x + c * 8 + bit, y + r);
    }
}

/* Renders a single 8x8 font character ch at pixel position (x, y).
 * The colour parameter c is unused; gfx_draw_mode controls rendering. */
void gfx_draw_char(int16_t x, int16_t y, uint8_t ch, uint8_t c) {
  (void)c; /* colour comes from gfx_draw_mode, not this param */
  const uint8_t *g = font8x8_glyph(ch);
  for (uint8_t row = 0; row < 8; row++) {
    uint8_t bits = pgm_read_byte(&g[row]);
    for (uint8_t col = 0; col < 8; col++) {
      if (bits & (0x80 >> col)) {
        gfx_pixel(x + col, y + row);
      }
    }
  }
}