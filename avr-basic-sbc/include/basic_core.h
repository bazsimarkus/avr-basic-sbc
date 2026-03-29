/*
 * basic_core.h  -  TinyBasicPlus interpreter, ATmega1284P bare-metal port
 *
 * Public API consumed by main.c
 */
 
#ifndef BASIC_CORE_H
#define BASIC_CORE_H

#include <stdint.h>

/* --------------------------------------------------------------- */
/*  Feature switches  (comment/uncomment to enable/disable)        */
/* --------------------------------------------------------------- */

/* ENABLE_EEPROM: Set to 1 to compile in the internal AVR EEPROM commands.
 * Enables: EFORMAT (erase), ELIST (list stored program), ELOAD (load from EEPROM),
 *          ESAVE (save program to EEPROM), EPOKE (write byte), EPEEK (read byte).
 * The ATmega1284P has 4 kB of internal EEPROM.  Comment out or set to 0 to save
 * approx. 500 bytes of flash. */
#define ENABLE_EEPROM   1   /* Internal EEPROM: EFORMAT, ELIST, ELOAD, ESAVE, EPOKE, EPEEK */

/* ENABLE_XEEPROM: Set to 1 to compile in the external 25LC640 SPI EEPROM commands.
 * Enables: XFORMAT, XLIST, XLOAD, XSAVE, XPOKE, XPEEK - same semantics as the
 *          internal EEPROM commands but targeting the 8 kB external chip (U3).
 * Requires ENABLE_XEEPROM hardware to be present and xeep_init() to be called.
 * Comment out or set to 0 if the SPI EEPROM is not populated. */
#define ENABLE_XEEPROM  1   /* External 25LC640: XFORMAT, XLIST, XLOAD, XSAVE, XPOKE, XPEEK */

/* ENABLE_TONES: Set to 1 to compile in the buzzer / piezo tone commands.
 * Enables: TONE freq, dur  (start tone, return immediately)
 *          TONEW freq, dur (start tone, wait for it to finish)
 *          NOTONE          (stop any playing tone immediately)
 * Uses Timer2 and OC2A (PD7).  Comment out to free Timer2 for other use. */
#define ENABLE_TONES    1   /* TONE, TONEW, NOTONE                   */

/* ENABLE_FILEIO: Uncomment to enable SD-card file I/O commands (LOAD/SAVE to file).
 * NOT wired on this board - leave commented out unless you add SD hardware. */
/* #define ENABLE_FILEIO 1 */  /* SD card (not wired on this board)   */

/* ENABLE_EAUTORUN: Uncomment to auto-run the BASIC program stored in internal
 * EEPROM immediately after reset, without showing the '>' prompt first.
 * Useful for stand-alone appliance mode.  Disabled by default. */
/* #define ENABLE_EAUTORUN 1 */  /* auto-run from internal EEPROM on boot (DISABLED) */


/* --------------------------------------------------------------- */
/* basic_init(): Initialise all BASIC subsystems and enter the interpreter loop.
 * Initialises Timer0 (1 ms tick), optional EEPROM/XEEPROM, UART, tones, and GPIO.
 * Then calls basic_loop() which blocks indefinitely, running the interactive
 * BASIC prompt and executing programs.  Returns only when the BYE statement
 * is executed, allowing main() to restart the interpreter. */
void basic_init(void);

/* basic_run_iteration(): Reserved for future use / cooperative scheduling.
 * Currently not implemented in basic_core.c.  Do not call. */
void basic_run_iteration(void);

#endif /* BASIC_CORE_H */
