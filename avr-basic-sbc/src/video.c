/* video.c
 * Composite video signal generator for ATmega (Timer1-based).
 * Manages the framebuffer and generates sync/pixel output line-by-line
 * via a Timer1 overflow ISR and a chain of scanline handler functions.
 */
 
#include <avr/interrupt.h>
#include <avr/io.h>
#include <string.h>

#include "video.h"
#include "video_config.h"

uint8_t framebuffer[FB_SIZE];
volatile int scanLine;
volatile unsigned long frames;
int renderLine;
static uint8_t start_render;
static uint8_t vscale_const;
static uint8_t vscale;

static void (*volatile line_handler)(void);
static void vsync_line(void);
static void blank_line(void);
static void active_line(void);

extern void render_scanline(void);

/* Busy-wait until Timer1 low byte reaches the given cycle count.
 * Uses inline assembly for cycle-accurate timing of the video output. */
inline static void wait_until(uint8_t time) {
  __asm__ __volatile__(
      "subi  %[time], 10\n"
      "sub   %[time], %[tcnt1l]\n\t"
      "100:\n\t"
      "subi  %[time], 3\n\t"
      "brcc  100b\n\t"
      "subi  %[time], 0-3\n\t"
      "breq  101f\n\t"
      "dec   %[time]\n\t"
      "breq  102f\n\t"
      "rjmp  102f\n"
      "101:\n\t"
      "nop\n"
      "102:\n" ::[time] "a"(time),
      [tcnt1l] "a"(TCNT1L));
}

/* Timer1 overflow ISR: dispatches to the current scanline handler. */
ISR(TIMER1_OVF_vect) { line_handler(); }

/* Handles vertical sync lines. Resets scanLine at frame end, switches
 * to blank_line handler after the vsync pulse finishes. */
static void vsync_line(void) {
  if (scanLine >= LINES_PER_FRAME) {
    OCR1A = VSYNC_CYCLES;
    scanLine = 0;
    frames++;
  } else if (scanLine == VSYNC_END) {
    OCR1A = HSYNC_CYCLES;
    line_handler = &blank_line;
  }
  scanLine++;
}

/* Handles non-active (blank) lines above and below the visible area.
 * Transitions to active_line when the render window starts, or back
 * to vsync_line at the end of the frame. */
static void blank_line(void) {
  if (scanLine == start_render) {
    renderLine = 0;
    vscale = vscale_const;
    line_handler = &active_line;
  } else if (scanLine == LINES_PER_FRAME) {
    line_handler = &vsync_line;
  }
  scanLine++;
}

/* Handles a visible scanline: waits for the pixel output window, calls
 * render_scanline(), advances renderLine according to the vertical scale
 * factor, and returns to blank_line when the visible region ends. */
static void active_line(void) {
  wait_until(OUTPUT_START);
  render_scanline();
  if (!vscale) {
    vscale = vscale_const;
    renderLine += HRES_BYTES;
  } else {
    vscale--;
  }
  if ((scanLine + 1) == (int)(start_render + (VRES * (vscale_const + 1))))
    line_handler = &blank_line;
  scanLine++;
}

/* Initialises the video subsystem: clears the framebuffer, computes
 * vertical scaling/centering parameters, configures GPIO and Timer1
 * for composite sync generation, then enables the overflow interrupt. */
void video_init(void) {
  cli();
  memset(framebuffer, 0, FB_SIZE);
  frames = 0;
  renderLine = 0;
  vscale_const = (DISPLAY_LINES / VRES) - 1;
  vscale = vscale_const;
  start_render = (LINES_PER_FRAME - DISPLAY_LINES) / 2 + DISPLAY_LINES / 2 -
                 (VRES * (vscale_const + 1)) / 2;
  VIDEO_DDR |= (1 << VIDEO_PIN_BIT);
  SYNC_DDR |= (1 << SYNC_PIN_BIT);
  VIDEO_PORT &= ~(1 << VIDEO_PIN_BIT);
  SYNC_PORT |= (1 << SYNC_PIN_BIT);
  TCCR1A = (1 << COM1A1) | (1 << COM1A0) | (1 << WGM11);
  TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS10);
  ICR1 = LINE_CYCLES - 1;
  OCR1A = HSYNC_CYCLES;
  scanLine = LINES_PER_FRAME + 1;
  line_handler = &vsync_line;
  TIMSK1 = (1 << TOIE1);
  sei();
}

/* Disables the video subsystem: stops Timer1, clears the output pins,
 * and re-enables global interrupts. */
void video_stop(void) {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TIMSK1 = 0;
  VIDEO_PORT &= ~(1 << VIDEO_PIN_BIT);
  SYNC_PORT &= ~(1 << SYNC_PIN_BIT);
  sei();
}

/* Returns the current scanline counter (atomic read). */
uint16_t video_get_line(void) {
  uint16_t l;
  cli();
  l = scanLine;
  sei();
  return l;
}

/* Blocks until the current frame's active region has fully rendered
 * (i.e., until the scanline exits the last visible line). */
void video_wait_vblank(void) {
  int stop = (int)(start_render + (VRES * (vscale_const + 1))) + 1;
  while (scanLine != stop);
  while (scanLine == stop);
}

/* Returns the total number of frames rendered since video_init() (atomic read). */
uint32_t video_get_frame(void) {
  uint32_t f;
  cli();
  f = frames;
  sei();
  return f;
}

/* Fills the entire framebuffer with 0x00 (all pixels off). */
void video_clear(void) { memset(framebuffer, 0x00, FB_SIZE); }

/* Fills the entire framebuffer with 0xFF (all pixels on). */
void video_fill(void) { memset(framebuffer, 0xFF, FB_SIZE); }

/* Inverts every byte in the framebuffer (XORs each byte with 0xFF). */
void video_invert(void) {
  for (uint16_t i = 0; i < FB_SIZE; i++) framebuffer[i] ^= 0xFF;
}

/* Scrolls the framebuffer up by the given number of pixel rows,
 * filling the vacated rows at the bottom with zeros.
 * Does nothing if lines == 0; clears the screen if lines >= VRES. */
void video_scroll_up(uint8_t lines) {
  if (!lines) return;
  if (lines >= VRES) {
    video_clear();
    return;
  }
  uint16_t s = (uint16_t)lines * HRES_BYTES, m = FB_SIZE - s;
  memmove(framebuffer, framebuffer + s, m);
  memset(framebuffer + m, 0x00, s);
}
