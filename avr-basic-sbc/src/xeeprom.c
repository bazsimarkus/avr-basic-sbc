/*
 * xeeprom.c  -  25LC640 SPI EEPROM driver
 *
 * Uses hardware SPI (SPCR/SPDR) at F_CPU/4 = 4 MHz (safe for 25LC640 at 5V).
 */

#include <avr/io.h>
#include <util/delay.h>
#include "xeeprom.h"
#include "board_pins.h"

/* 25LC640 opcodes */
#define XEEP_READ   0x03
#define XEEP_WRITE  0x02
#define XEEP_WRDI   0x04
#define XEEP_WREN   0x06
#define XEEP_RDSR   0x05
#define XEEP_WRSR   0x01

/* Status register bit */
#define XEEP_WIP    0x01    /* Write In Progress */

/* Sends one byte over SPI and returns the byte clocked in.
 * Blocks until the SPIF flag is set. */
static uint8_t spi_transfer(uint8_t data)
{
    SPDR = data;
    while (!(SPSR & (1 << SPIF)));
    return SPDR;
}

/* Polls the EEPROM status register until the Write-In-Progress
 * (WIP) bit clears, indicating the device is ready for a new command. */
static void xeep_wait_ready(void)
{
    XEEP_CS_LOW();
    spi_transfer(XEEP_RDSR);
    while (spi_transfer(0xFF) & XEEP_WIP);
    XEEP_CS_HIGH();
}

/* Issues the WREN (Write Enable) opcode to the EEPROM,
 * which must be done before every write or erase operation. */
static void xeep_write_enable(void)
{
    XEEP_CS_LOW();
    spi_transfer(XEEP_WREN);
    XEEP_CS_HIGH();
}

/* Initialises the SPI bus and /CS pin for the 25LC640 EEPROM.
 * Configures SPI as master, Mode 0,0 at F_CPU/4. */
void xeep_init(void)
{
    /* Set MOSI, SCK, /CS as outputs; MISO as input */
    SPI_DDR   |=  (1 << SPI_MOSI) | (1 << SPI_SCK);
    SPI_DDR   &= ~(1 << SPI_MISO);
    XEEP_CS_DDR  |= (1 << XEEP_CS_BIT);
    XEEP_CS_HIGH();   /* deselect */

    /* Enable SPI, Master, Mode 0,0, F_CPU/4 */
    SPCR = (1 << SPE) | (1 << MSTR);
    SPSR = 0;         /* no 2x speed */
}

/* Reads and returns the byte at the given 16-bit EEPROM address. */
uint8_t xeep_read_byte(uint16_t addr)
{
    xeep_wait_ready();
    XEEP_CS_LOW();
    spi_transfer(XEEP_READ);
    spi_transfer((uint8_t)(addr >> 8));
    spi_transfer((uint8_t)(addr & 0xFF));
    uint8_t val = spi_transfer(0xFF);
    XEEP_CS_HIGH();
    return val;
}

/* Writes one byte to the given 16-bit EEPROM address.
 * Handles the write-enable sequence and waits for the write to complete. */
void xeep_write_byte(uint16_t addr, uint8_t data)
{
    xeep_wait_ready();
    xeep_write_enable();
    XEEP_CS_LOW();
    spi_transfer(XEEP_WRITE);
    spi_transfer((uint8_t)(addr >> 8));
    spi_transfer((uint8_t)(addr & 0xFF));
    spi_transfer(data);
    XEEP_CS_HIGH();
    xeep_wait_ready();
}

/* Erases the entire EEPROM by writing 0x00 to every address.
 * This is a byte-by-byte software erase (no bulk-erase opcode). */
void xeep_chip_erase(void)
{
    for (uint16_t i = 0; i < XEEPROM_SIZE; i++) {
        xeep_write_byte(i, 0x00);
    }
}
