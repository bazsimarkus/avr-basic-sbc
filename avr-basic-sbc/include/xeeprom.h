/*
 * xeeprom.h  -  25LC640 SPI EEPROM driver
 */
#ifndef XEEPROM_H
#define XEEPROM_H

#include <stdint.h>

/* Total capacity of the 25LC640 in bytes */
/* XEEPROM_SIZE: Total storage capacity of the 25LC640 in bytes (8 kB = 8192).
 * Valid address range for xeep_read_byte / xeep_write_byte: 0 .. 8191. */
#define XEEPROM_SIZE  8192

/* xeep_init(): Initialise the SPI bus and the /CS pin for the 25LC640.
 * Configures MOSI, SCK as outputs, MISO as input, /CS as output (deasserted),
 * and enables the SPI peripheral as master in Mode 0,0 at F_CPU/4. */
void    xeep_init(void);

/* xeep_read_byte(addr): Read and return the single byte at EEPROM address 'addr'.
 * Waits for any pending write to complete first (polls WIP bit).
 * addr: 0 .. XEEPROM_SIZE-1 (0x0000 .. 0x1FFF). */
uint8_t xeep_read_byte(uint16_t addr);

/* xeep_write_byte(addr, data): Write one byte to EEPROM address 'addr'.
 * Handles the Write-Enable (WREN) sequence automatically.
 * Blocks until the device finishes the internal write cycle (~5 ms max).
 * addr: 0 .. XEEPROM_SIZE-1. */
void    xeep_write_byte(uint16_t addr, uint8_t data);

/* xeep_chip_erase(): Erase the entire EEPROM by writing 0x00 to every address.
 * This is a sequential byte-by-byte software erase; it is SLOW (~40 s for 8 kB).
 * The BASIC command XFORMAT calls this function. */
void    xeep_chip_erase(void);

#endif /* XEEPROM_H */
