/* video.h
 * Public API for the composite video driver (video.c).
 *
 * The driver owns Timer1, which generates the horizontal sync pulse and
 * calls a scanline ISR at 15.625 kHz (PAL) to output pixel data from the
 * shared framebuffer[] array.
 *
 * Typical call sequence:
 *   video_init();          // start the video signal
 *   // draw into framebuffer via graphics / text functions ...
 *   video_wait_vblank();   // wait for the safe drawing window (optional)
 *   video_stop();          // shut down (rarely needed)
 *
 * Framebuffer layout (1 bit per pixel, MSB = leftmost pixel):
 *   Byte address : framebuffer[y * HRES_BYTES + (x >> 3)]
 *   Bit  mask    : 0x80 >> (x & 7)
 */
#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

#include "video_config.h"

/* framebuffer[]: The raw pixel memory shared by the video ISR and all
 * graphics/text functions.  Size = HRES_BYTES * VRES bytes (9600 at
 * 320x240).  The ISR reads it during active scan lines; application
 * code writes it freely (ideally during vblank to avoid tearing). */
extern uint8_t framebuffer[FB_SIZE];
/* scanLine: Current scan line counter, updated inside the Timer1 ISR.
 * Treat as read-only from application code.
 * Range: 0 .. LINES_PER_FRAME-1. */
extern volatile int scanLine;
/* frames: Total video frames rendered since video_init().
 * Incremented once per vsync.  Use video_get_frame() for an
 * atomic (interrupt-safe) read. */
extern volatile unsigned long frames;
/* renderLine: Byte offset into framebuffer[] for the row currently
 * being rendered by the ISR.  Managed entirely by the driver;
 * do not read or write from application code. */
extern int renderLine;

/* video_init(): Initialise and start the video signal.
 * Clears the framebuffer, configures Timer1 and GPIO pins,
 * enables the overflow ISR, and starts generating PAL composite
 * sync and pixel output.  Call once at system startup. */
void video_init(void);
/* video_stop(): Disable the video signal and release Timer1.
 * De-asserts the sync and data pins and stops the ISR.
 * Use before entering sleep mode or repurposing Timer1. */
void video_stop(void);
/* video_get_line(): Return the current scanLine value.
 * Performs an atomic (cli/sei) read.  Safe from application context. */
uint16_t video_get_line(void);
/* video_wait_vblank(): Block until the active pixel region ends.
 * Call this before updating the framebuffer to prevent visible
 * tearing.  The function returns at the start of the vertical
 * blanking interval, giving approx. (LINES_PER_FRAME - VRES)
 * scan lines of safe drawing time. */
void video_wait_vblank(void);
/* video_get_frame(): Return total frames since video_init().
 * Atomically reads the 32-bit 'frames' counter.
 * Useful for animation timing: 50 frames/sec (PAL). */
uint32_t video_get_frame(void);
/* video_clear(): Fill framebuffer with 0x00 - all pixels black. */
void video_clear(void);
/* video_fill(): Fill framebuffer with 0xFF - all pixels white. */
void video_fill(void);
/* video_invert(): XOR every framebuffer byte with 0xFF, inverting
 * all pixels on screen. */
void video_invert(void);
/* video_scroll_up(lines): Scroll the framebuffer up by 'lines' pixel rows.
 * Vacated rows at the bottom are cleared to 0x00.
 * lines == 0  -> no-op.
 * lines >= VRES -> equivalent to video_clear(). */
void video_scroll_up(uint8_t lines);

/* video_set_pixel(x, y, c): Set (c != 0) or clear (c == 0) a single pixel.
 * Out-of-bounds coordinates are silently ignored.
 * Inline for speed; note that gfx_pixel() also respects gfx_draw_mode. */
static inline void video_set_pixel(int16_t x, int16_t y, uint8_t c) {
  if ((uint16_t)x >= HRES_PIXELS || (uint16_t)y >= VRES) return;
  uint16_t bi = (uint16_t)y * HRES_BYTES + (x >> 3);
  uint8_t m = 0x80 >> (x & 7);
  if (c)
    framebuffer[bi] |= m;
  else
    framebuffer[bi] &= ~m;
}

/* video_get_pixel(x, y): Return 1 if the pixel at (x,y) is set, 0 if clear.
 * Out-of-bounds coordinates return 0. */
static inline uint8_t video_get_pixel(int16_t x, int16_t y) {
  if ((uint16_t)x >= HRES_PIXELS || (uint16_t)y >= VRES) return 0;
  return (framebuffer[(uint16_t)y * HRES_BYTES + (x >> 3)] & (0x80 >> (x & 7)))
             ? 1
             : 0;
}

#endif
