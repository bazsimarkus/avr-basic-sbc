; render_line.S - Fully unrolled pixel output (no loop = no jiggle)
; 40 bytes = 320 pixels, each byte output inline.
; Eliminates the 7-cycle loop overhead at 32-pixel boundaries.
; Only the inherent 1-cycle stretch at byte boundaries remains
; (3cy vs 2cy = barely visible on screen).
;
; AVR ABI: r16 is callee-saved, must push/pop.
; r18 is caller-saved, no need to save.

#include <avr/io.h>

#define PORT_VID_IO  _SFR_IO_ADDR(PORTA)

.extern framebuffer
.extern renderLine

.section .text
.global render_scanline

render_scanline:
    push  r16

    ; X = framebuffer + renderLine
    lds   r26, renderLine
    lds   r27, renderLine+1
    subi  r26, lo8(-(framebuffer))
    sbci  r27, hi8(-(framebuffer))

    ; --- Byte 0 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 1 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 2 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 3 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 4 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 5 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 6 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 7 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 8 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 9 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 10 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 11 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 12 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 13 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 14 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 15 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 16 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 17 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 18 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 19 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 20 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 21 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 22 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 23 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 24 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 25 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 26 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 27 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 28 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 29 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 30 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 31 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 32 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 33 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 34 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 35 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 36 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 37 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 38 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    ; --- Byte 39 ---
    ld    r16, X+
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16
    lsl   r16
    out   PORT_VID_IO, r16

    nop
    cbi   PORT_VID_IO, 7

    pop   r16
    ret
