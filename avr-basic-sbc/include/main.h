/* main.h
 * Top-level system definitions shared across the AVR-SBC firmware.
 *
 * Defines the CPU frequency and firmware version string, and declares
 * any global utility functions used outside of a single module.
 *
 * Include this header from any module that needs F_CPU or K_VERSION
 * without pulling in the full subsystem headers.
 */
#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

#define F_CPU 16000000UL
#define K_VERSION "v0.20-GFX-C"

void system_init(void);
uint32_t millis(void); // Implemented via Video frames

#endif