/* video_config.h
 * Central configuration for the composite video signal generator.
 *
 * This file controls every timing, resolution, and pin-assignment
 * parameter that the video driver (video.c) and the graphics / text
 * layers depend on.  Adjust the values here to target a different
 * resolution, refresh standard (PAL/NTSC), or hardware pin layout.
 *
 * ---------------------------------------------------------------
 * QUICK-START: common changes
 *   Switch to NTSC  -> set LINES_PER_FRAME 262, DISPLAY_LINES 240
 *   Smaller image   -> reduce VRES and/or HRES_PIXELS (saves RAM)
 *   Shift image     -> tweak OUTPUT_START
 *   Different MCU   -> update F_CPU and recalculate cycle counts
 * ---------------------------------------------------------------
 */
#ifndef VIDEO_CONFIG_H
#define VIDEO_CONFIG_H

#include <stdint.h>

/* F_CPU: CPU clock frequency in Hz.
 * Must match the actual crystal/fuse setting of your ATmega.
 * The default is 16 MHz.  If your board runs at 8 MHz set this
 * to 8000000UL and recalculate LINE_CYCLES, HSYNC_CYCLES etc. */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

/* VIDEO_PORT / VIDEO_DDR / VIDEO_PIN_BIT
 * AVR port, DDR register, and bit number that drive the video DATA
 * signal.  On this board PA7 connects to the RCA jack via a 470 Ohm
 * resistor.  Change all three together if you rewire the video output. */
#define VIDEO_PORT      PORTA
#define VIDEO_DDR       DDRA
#define VIDEO_PIN_BIT   7
/* SYNC_PORT / SYNC_DDR / SYNC_PIN_BIT
 * AVR port, DDR register, and bit number that drive the composite SYNC
 * signal.  On this board PD5 connects via a 1 kOhm resistor.
 * Change all three together if you rewire the sync output. */
#define SYNC_PORT       PORTD
#define SYNC_DDR        DDRD
#define SYNC_PIN_BIT    5
/* LINE_CYCLES: Timer1 counts per complete scan line.
 * At 16 MHz one count = 62.5 ns, so 1024 counts = 64 us (PAL line period).
 * Do NOT change this without also retuning HSYNC_CYCLES and VSYNC_CYCLES. */
#define LINE_CYCLES     1024
/* HSYNC_CYCLES: Timer1 counts for the horizontal sync pulse width.
 * 74 counts at 16 MHz ≈ 4.625 us (PAL spec: 4.7 us).
 * Increase to widen the pulse; decrease to narrow it.
 * Too far in either direction will cause the monitor to lose sync. */
#define HSYNC_CYCLES    74
/* VSYNC_CYCLES: OCR1A reload value during the vertical sync interval.
 * Controls how wide the vsync pulse is across the equalising/sync lines.
 * Tune together with VSYNC_END if the picture rolls vertically. */
#define VSYNC_CYCLES    940
/* OUTPUT_START: Timer1 low-byte threshold at which pixel output begins
 * on each active scan line.  This is the primary horizontal-position
 * control:
 *   Increase -> image shifts RIGHT  (later start)
 *   Decrease -> image shifts LEFT   (earlier start)
 * Valid range: 0-255.  Default 199 works well for most PAL monitors. */
#define OUTPUT_START    199
/* HRES_PIXELS: Horizontal resolution in pixels.
 * Must be a multiple of 8 (one byte = 8 pixels in the framebuffer).
 * Increasing this requires more SRAM and a faster pixel-output ISR.
 * Common values: 160, 240, 320. */
#define HRES_PIXELS     320
/* HRES_BYTES: Bytes per framebuffer row (derived, do not set manually).
 * = HRES_PIXELS / 8  because the framebuffer stores 1 bit per pixel. */
#define HRES_BYTES      (HRES_PIXELS / 8)
/* VRES: Vertical resolution in pixel rows stored in the framebuffer.
 * The hardware will repeat each row (vscale+1) times to fill DISPLAY_LINES.
 * Increasing VRES costs SRAM; maximum is DISPLAY_LINES / (vscale+1). */
#define VRES            240
/* FB_SIZE: Total framebuffer size in bytes (derived, do not set manually).
 * = HRES_BYTES x VRES.  At 320x240 this is 9600 bytes of SRAM.
 * Check that FB_SIZE + kRamSize (BASIC buffer, ~5500) fits in 16 KB. */
#define FB_SIZE         ((uint16_t)HRES_BYTES * VRES)
/* TEXT_COLS / TEXT_ROWS: Text terminal dimensions (derived, do not set manually).
 * One character cell = 8x8 pixels.  At 320x240: 40 columns, 30 rows. */
#define TEXT_COLS       (HRES_PIXELS / 8)
#define TEXT_ROWS       (VRES / 8)
/* LINES_PER_FRAME: Total scan lines per video frame.
 * PAL  = 312  (50 Hz, used in Europe)
 * NTSC = 262  (60 Hz, used in North America / Japan)
 * Switching to NTSC: set 262 and also reduce DISPLAY_LINES to 240. */
#define LINES_PER_FRAME 312
/* VSYNC_END: Scan line index at which the vsync pulse ends and the
 * back-porch / blank region begins.  PAL value = 7.
 * Adjust if your monitor shows a partial frame at the top. */
#define VSYNC_END       7
/* DISPLAY_LINES: Number of scan lines in the active display window.
 * The framebuffer is vertically centred within this window.
 * Constraint: DISPLAY_LINES >= VRES * (DISPLAY_LINES/VRES).
 * For NTSC set this to 240. */
#define DISPLAY_LINES   260
/* ACTIVE_START: First scan line of the active video area (top of image).
 * Used only in centring calculations; changing it shifts the image up/down.
 * The real centering is done at runtime from start_render in video.c. */
#define ACTIVE_START    36
/* ACTIVE_END: Last scan line of the active video area (derived, do not set manually). */
#define ACTIVE_END      (ACTIVE_START + VRES - 1)

#endif
