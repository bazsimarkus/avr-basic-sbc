/* graphics.h
 * 2-D graphics primitives that draw into the shared video framebuffer.
 *
 * All drawing operations respect the global gfx_draw_mode variable:
 *   DRAW_SET   - turn pixels ON   (logical OR  with framebuffer)
 *   DRAW_CLEAR - turn pixels OFF  (logical AND-NOT with framebuffer)
 *   DRAW_XOR   - toggle pixels    (logical XOR, useful for rubber-band drawing)
 *
 * Coordinate system: (0,0) is the top-left corner; x increases right,
 * y increases downward.  Out-of-bounds coordinates are clipped.
 *
 * All functions are blocking (no DMA) and safe to call from the main
 * loop; call video_wait_vblank() first to avoid visible tearing.
 */
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include "video.h"

/* draw_mode_t: Pixel-write mode applied by all gfx_* functions.
 *   DRAW_SET   (0) - write 1 (turn pixel white/on).
 *   DRAW_CLEAR (1) - write 0 (turn pixel black/off).
 *   DRAW_XOR   (2) - invert the existing pixel value. */
typedef enum { DRAW_SET, DRAW_CLEAR, DRAW_XOR } draw_mode_t;
/* gfx_draw_mode: The active draw mode for all gfx_* functions.
 * Initialised to DRAW_SET.  Change at any time; affects all
 * subsequent draw calls until changed again. */
extern draw_mode_t gfx_draw_mode;

/* gfx_pixel(x, y): Plot a single pixel at pixel coordinates (x, y). */
void gfx_pixel(int16_t x, int16_t y);
/* gfx_hline(x0, x1, y): Horizontal line across row y, from column x0 to x1.
 * Uses full-byte writes internally for speed; x0 and x1 may be in any order. */
void gfx_hline(int16_t x0, int16_t x1, int16_t y);
/* gfx_vline(x, y0, y1): Vertical line down column x, from row y0 to y1. */
void gfx_vline(int16_t x, int16_t y0, int16_t y1);
/* gfx_line(x0,y0, x1,y1): Arbitrary line using Bresenham's algorithm.
 * Delegates to gfx_hline/gfx_vline for perfectly horizontal/vertical lines. */
void gfx_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
/* gfx_rect(x, y, w, h): Unfilled rectangle.
 * (x,y) = top-left corner; w = width, h = height in pixels. */
void gfx_rect(int16_t x, int16_t y, int16_t w, int16_t h);
/* gfx_fill_rect(x, y, w, h): Solid filled rectangle. */
void gfx_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h);
/* gfx_circle(cx, cy, r): Unfilled circle, centre (cx,cy), radius r pixels.
 * Uses the midpoint (Bresenham) circle algorithm. */
void gfx_circle(int16_t cx, int16_t cy, int16_t r);
/* gfx_fill_circle(cx, cy, r): Solid filled circle. */
void gfx_fill_circle(int16_t cx, int16_t cy, int16_t r);
/* gfx_ellipse(cx, cy, rx, ry): Unfilled axis-aligned ellipse.
 * rx = horizontal semi-axis, ry = vertical semi-axis (pixels). */
void gfx_ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry);
/* gfx_fill_ellipse(cx, cy, rx, ry): Solid filled axis-aligned ellipse. */
void gfx_fill_ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry);
/* gfx_triangle(x0,y0, x1,y1, x2,y2): Unfilled triangle. */
void gfx_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
/* gfx_fill_triangle(x0,y0, x1,y1, x2,y2): Solid filled triangle.
 * Vertices are sorted by Y internally; argument order does not matter. */
void gfx_fill_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
/* gfx_round_rect(x, y, w, h, r): Unfilled rectangle with rounded corners.
 * r = corner radius in pixels.  Degrades to gfx_rect() when r <= 0. */
void gfx_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r);
/* gfx_bitmap_P(x, y, bmp, w, h): Blit a 1-bpp bitmap from PROGMEM at (x,y).
 * bmp: row-major bitmap data (1 bit = 1 pixel, MSB = leftmost).
 * w must be a multiple of 8.  Only set bits are drawn (transparent bg). */
void gfx_bitmap_P(int16_t x, int16_t y, const uint8_t *bmp, int16_t w, int16_t h);
/* gfx_bitmap(x, y, bmp, w, h): Same as gfx_bitmap_P() but reads from SRAM. */
void gfx_bitmap(int16_t x, int16_t y, const uint8_t *bmp, int16_t w, int16_t h);
/* gfx_draw_char(x, y, ch, c): Draw the 8x8 font glyph for ASCII character ch
 * at pixel position (x,y).  The 'c' parameter is unused; rendering mode
 * is controlled by gfx_draw_mode. */
void gfx_draw_char(int16_t x, int16_t y, uint8_t ch, uint8_t c);

#endif /* GRAPHICS_H */
