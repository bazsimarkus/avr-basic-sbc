/*
 * basic_core.c  --  TinyBasicPlus v0.15 full port to ATmega1284P bare-metal
 *
 * Original: Gordon Brandly (68000), Mike Field (Arduino), Scott Lawrence (TBP)
 * Port:     Pure C, avr-gcc, no Arduino framework.
 *
 * ===================================================================
 *  TIMERS
 * ===================================================================
 *  Timer0  - 1 ms system tick  (CTC, /64, OCR0A=249 -> 1000 Hz)
 *  Timer1  - Video sync / scanline generation  (video.c)
 *  Timer2  - Tone output on OC2A / PD7         (TONE / TONEW)
 *
 * ===================================================================
 *  MEMORY LAYOUT   (kRamSize = 4096 bytes in the program[] array)
 * ===================================================================
 *  program[0]                          <- program_start
 *     ... stored BASIC lines ...
 *  program_end                         <- first free byte
 *     ... free space ...
 *  variables_begin                     <- 27*sizeof(short) = 54 bytes
 *  stack_limit                         <- bottom of software stack
 *     ... stack grows downward ...
 *  program[kRamSize-1]                 <- sp (top of stack)
 * ===================================================================
 */

/* ---- Include basic_core.h FIRST so feature flags are defined ---- */
#include "basic_core.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdlib.h>

#ifdef ENABLE_EEPROM
#include <avr/eeprom.h>
#endif

#include "board_pins.h"
#include "text.h"
#include "keyboard.h"
#include "video.h"
#include "graphics.h"

#ifdef ENABLE_XEEPROM
#include "xeeprom.h"
#endif

#include "uart.h"

/* ------------------------------------------------------------------ */
/*  Version string                                                     */
/* ------------------------------------------------------------------ */
#define kVersion "v1.0"

/* ------------------------------------------------------------------ */
/*  RAM buffer for the BASIC program                                   */
/* ------------------------------------------------------------------ */
#define kRamSize 5500

static unsigned char program[kRamSize];

/* ------------------------------------------------------------------ */
/*  State variables                                                    */
/* ------------------------------------------------------------------ */
static unsigned char *txtpos, *list_line, *tmptxtpos;
static unsigned char  expression_error;
static unsigned char *tempsp;
static unsigned char *stack_limit;
static unsigned char *program_start;
static unsigned char *program_end;
static unsigned char *variables_begin;
static unsigned char *current_line;
static unsigned char *sp;
static unsigned char  table_index;

typedef unsigned short LINENUM;

/* I/O stream redirection */
enum {
    kStreamSerial = 0,
    kStreamEEProm,
    kStreamEEPromX,
    kStreamSercom,
    kStreamFile
};
static unsigned char inStream  = kStreamSerial;
static unsigned char outStream = kStreamSerial;

static uint8_t inhibitOutput = 0;
static uint8_t runAfterLoad  = 0;
static uint8_t triggerRun    = 0;

#ifdef ENABLE_EEPROM
static uint16_t eepos = 0;
#endif

#ifdef ENABLE_XEEPROM
static uint16_t eeposX = 0;
#endif

/* Cursor blink state */
static uint8_t  cursor_blink_on = 0;
#define CURSOR_BLINK_FRAMES 25

/* ------------------------------------------------------------------ */
/*  Timer0 -- 1 ms system tick                                         */
/*  CTC mode, prescaler /64, OCR0A=249                                 */
/*  16 000 000 / 64 / 250 = 1000 Hz  ->  1 ms per interrupt           */
/* ------------------------------------------------------------------ */
static volatile uint32_t sys_millis = 0;

/* Timer0 CTC compare-match ISR: increments the millisecond counter. */
ISR(TIMER0_COMPA_vect)
{
    sys_millis++;
}

/* Configures Timer0 in CTC mode with /64 prescaler and OCR0A=249
 * to generate a 1 kHz interrupt (1 ms tick). */
static void timer0_init(void)
{
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A  = 249;
    TIMSK0 = (1 << OCIE0A);
}

/* Returns the number of milliseconds elapsed since timer0_init().
 * Performs an atomic (interrupt-safe) read of sys_millis. */
static uint32_t millis(void)
{
    uint32_t m;
    uint8_t sreg = SREG;
    cli();
    m = sys_millis;
    SREG = sreg;
    return m;
}

/* ------------------------------------------------------------------ */
/*  ASCII helpers                                                      */
/* ------------------------------------------------------------------ */
#define CR      '\r'
#define NL      '\n'
#define LF      0x0a
#define TAB     '\t'
#define BELL    '\b'
#define SPACE   ' '
#define SQUOTE  '\''
#define DQUOTE  '\"'
#define CTRLC   0x03
#define CTRLH   0x08
#define CTRLS   0x13
#define CTRLX   0x18
#define ESC_KEY 27

/* ------------------------------------------------------------------ */
/*  Keyword table  (last char | 0x80, stored in PROGMEM)               */
/* ------------------------------------------------------------------ */
static const unsigned char keywords[] PROGMEM = {
    'L','I','S','T'+0x80,
    'L','O','A','D'+0x80,
    'N','E','W'+0x80,
    'R','U','N'+0x80,
    'S','A','V','E'+0x80,
    'N','E','X','T'+0x80,
    'L','E','T'+0x80,
    'I','F'+0x80,
    'G','O','T','O'+0x80,
    'G','O','S','U','B'+0x80,
    'R','E','T','U','R','N'+0x80,
    'R','E','M'+0x80,
    'F','O','R'+0x80,
    'I','N','P','U','T'+0x80,
    'P','R','I','N','T'+0x80,
    'P','O','K','E'+0x80,
    'S','T','O','P'+0x80,
    'B','Y','E'+0x80,
    'F','I','L','E','S'+0x80,
    'M','E','M'+0x80,
    '?'+0x80,
    '\''+0x80,
    'A','W','R','I','T','E'+0x80,
    'D','W','R','I','T','E'+0x80,
    'D','E','L','A','Y'+0x80,
    'E','N','D'+0x80,
    'R','S','E','E','D'+0x80,
    'C','H','A','I','N'+0x80,
#ifdef ENABLE_TONES
    'T','O','N','E','W'+0x80,
    'T','O','N','E'+0x80,
    'N','O','T','O','N','E'+0x80,
#endif
#ifdef ENABLE_EEPROM
    'E','L','I','S','T'+0x80,
    'E','L','O','A','D'+0x80,
    'E','F','O','R','M','A','T'+0x80,
    'E','S','A','V','E'+0x80,
#endif
#ifdef ENABLE_XEEPROM
    'X','L','I','S','T'+0x80,
    'X','L','O','A','D'+0x80,
    'X','F','O','R','M','A','T'+0x80,
    'X','S','A','V','E'+0x80,
#endif
    'I','N','K','E','Y'+0x80,
#ifdef ENABLE_XEEPROM
    'X','P','O','K','E'+0x80,
    'X','P','E','E','K'+0x80,
#endif
#ifdef ENABLE_EEPROM
    'E','P','O','K','E'+0x80,
    'E','P','E','E','K'+0x80,
#endif
    'S','E','R','O','P','E','N'+0x80,
    'S','E','R','C','L','O','S','E'+0x80,
    'S','E','R','P','R','I','N','T'+0x80,
    'S','E','R','R','E','A','D'+0x80,
    'S','E','R','L','O','A','D'+0x80,
	'D','R','A','W','P','I','X'+0x80,
    'D','R','A','W','L','I','N','E'+0x80,
    'D','R','A','W','R','E','C','T'+0x80,
    'D','R','A','W','C','I','R','C'+0x80,
    'D','R','A','W','C','H','A','R'+0x80,
    'G','E','T','P','I','X'+0x80,
	'C','L','S'+0x80,
    0
};

enum {
    KW_LIST = 0,
    KW_LOAD, KW_NEW, KW_RUN, KW_SAVE,
    KW_NEXT, KW_LET, KW_IF,
    KW_GOTO, KW_GOSUB, KW_RETURN,
    KW_REM,
    KW_FOR,
    KW_INPUT, KW_PRINT,
    KW_POKE,
    KW_STOP, KW_BYE,
    KW_FILES,
    KW_MEM,
    KW_QMARK, KW_QUOTE,
    KW_AWRITE, KW_DWRITE,
    KW_DELAY,
    KW_END,
    KW_RSEED,
    KW_CHAIN,
#ifdef ENABLE_TONES
    KW_TONEW, KW_TONE, KW_NOTONE,
#endif
#ifdef ENABLE_EEPROM
    KW_ELIST, KW_ELOAD, KW_EFORMAT, KW_ESAVE,
#endif
#ifdef ENABLE_XEEPROM
    KW_XLIST, KW_XLOAD, KW_XFORMAT, KW_XSAVE,
#endif
    KW_INKEY,
#ifdef ENABLE_XEEPROM
    KW_XPOKE, KW_XPEEK,
#endif
#ifdef ENABLE_EEPROM
    KW_EPOKE, KW_EPEEK,
#endif
    KW_SEROPEN, KW_SERCLOSE, KW_SERPRINT, KW_SERREAD, KW_SERLOAD,
	KW_DRAWPIX, KW_DRAWLINE, KW_DRAWRECT, KW_DRAWCIRC, KW_DRAWCHAR, KW_GETPIX,
	KW_CLS,
    KW_DEFAULT
};

/* ------------------------------------------------------------------ */
/*  Stack frame structures                                             */
/* ------------------------------------------------------------------ */
struct stack_for_frame {
    char frame_type;
    char for_var;
    short int terminal;
    short int step;
    unsigned char *current_line;
    unsigned char *txtpos;
};

struct stack_gosub_frame {
    char frame_type;
    unsigned char *current_line;
    unsigned char *txtpos;
};

#define STACK_SIZE       (sizeof(struct stack_for_frame) * 5)
#define VAR_SIZE         sizeof(short int)
#define STACK_GOSUB_FLAG 'G'
#define STACK_FOR_FLAG   'F'

/* ------------------------------------------------------------------ */
/*  Function table                                                     */
/* ------------------------------------------------------------------ */
static const unsigned char func_tab[] PROGMEM = {
    'P','E','E','K'+0x80,
    'A','B','S'+0x80,
    'A','R','E','A','D'+0x80,
    'D','R','E','A','D'+0x80,
    'R','N','D'+0x80,
    0
};
#define FUNC_PEEK    0
#define FUNC_ABS     1
#define FUNC_AREAD   2
#define FUNC_DREAD   3
#define FUNC_RND     4
#define FUNC_UNKNOWN 5

static const unsigned char to_tab[] PROGMEM = {
    'T','O'+0x80,
    0
};

static const unsigned char step_tab[] PROGMEM = {
    'S','T','E','P'+0x80,
    0
};

static const unsigned char relop_tab[] PROGMEM = {
    '>','='+0x80,
    '<','>'+0x80,
    '>'+0x80,
    '='+0x80,
    '<','='+0x80,
    '<'+0x80,
    '!','='+0x80,
    0
};
#define RELOP_GE       0
#define RELOP_NE       1
#define RELOP_GT       2
#define RELOP_EQ       3
#define RELOP_LE       4
#define RELOP_LT       5
#define RELOP_NE_BANG  6
#define RELOP_UNKNOWN  7

static const unsigned char highlow_tab[] PROGMEM = {
    'H','I','G','H'+0x80,
    'H','I'+0x80,
    'L','O','W'+0x80,
    'L','O'+0x80,
    0
};
#define HIGHLOW_HIGH    1
#define HIGHLOW_UNKNOWN 4

/* ------------------------------------------------------------------ */
/*  PROGMEM message strings                                            */
/* ------------------------------------------------------------------ */
static const unsigned char okmsg[]            PROGMEM = "OK";
static const unsigned char whatmsg[]          PROGMEM = "What? ";
static const unsigned char howmsg[]           PROGMEM = "How?";
static const unsigned char sorrymsg[]         PROGMEM = "Sorry!";
static const unsigned char initmsg[]          PROGMEM = "AVR-SBC TinyBasic Plus " kVersion;
static const unsigned char memorymsg[]        PROGMEM = " bytes free.";
#ifdef ENABLE_EEPROM
static const unsigned char eeprommsg[]        PROGMEM = " EEProm bytes total.";
static const unsigned char eepromamsg[]       PROGMEM = " EEProm bytes available.";
#endif
#ifdef ENABLE_XEEPROM
static const unsigned char xeeprommsg[]       PROGMEM = " XEEProm bytes total.";
static const unsigned char xeepromamsg[]      PROGMEM = " XEEProm bytes available.";
#endif
static const unsigned char breakmsg[]         PROGMEM = "break!";
static const unsigned char unimplimentedmsg[] PROGMEM = "Unimplemented";
static const unsigned char formatmsg[]        PROGMEM = "Formatting...";
static const unsigned char pinmsg[]           PROGMEM = "Pin N/A";

/* ------------------------------------------------------------------ */
/*  Tone auto-stop state                                               */
/* ------------------------------------------------------------------ */
#ifdef ENABLE_TONES
static volatile uint32_t tone_end_ms   = 0;
static volatile uint8_t  tone_running  = 0;

/* Checks whether the tone duration has expired and, if so,
 * stops Timer2 and silences the piezo output. */
static void tone_check(void)
{
    if (tone_running && millis() >= tone_end_ms) {
        TCCR2A = 0;
        TCCR2B = 0;
        PIEZO_PORT &= ~(1 << PIEZO_BIT);
        tone_running = 0;
    }
}
#endif

/* ------------------------------------------------------------------ */
/*  Low-level I/O                                                      */
/* ------------------------------------------------------------------ */

/* Called regularly to toggle the cursor glyph every
 * CURSOR_BLINK_FRAMES video frames, producing a blinking effect. */
static void cursor_blink_tick(void)
{
    static uint32_t last_blink_frame = 0;
    uint32_t now = video_get_frame();
    if (now - last_blink_frame >= CURSOR_BLINK_FRAMES) {
        text_cursor_blink();
        cursor_blink_on = !cursor_blink_on;
        last_blink_frame = now;
    }
}

/* Ensures the cursor glyph is hidden (erases it if currently visible). */
static void cursor_off(void)
{
    if (cursor_blink_on) {
        text_cursor_blink();
        cursor_blink_on = 0;
    }
}

/* Sends one character to the active output stream:
 * internal EEPROM, external EEPROM, serial UART, or the video terminal. */
static void outchar(unsigned char c)
{
    if (inhibitOutput) return;

#ifdef ENABLE_EEPROM
    if (outStream == kStreamEEProm) {
        eeprom_write_byte((uint8_t *)(uint16_t)eepos, c);
        eepos++;
        return;
    }
#endif
#ifdef ENABLE_XEEPROM
    if (outStream == kStreamEEPromX) {
        xeep_write_byte(eeposX, c);
        eeposX++;
        return;
    }
#endif
    if (outStream == kStreamSercom) {
        uart1_putc(c);
        return;
    }
    cursor_off();
    text_putc((char)c);
}

/* Reads one character from the active input stream:
 * internal EEPROM, external EEPROM, UART (with 5-second timeout),
 * or the PS/2 keyboard (blocking, with cursor blink). */
static int inchar(void)
{
    int v;

#ifdef ENABLE_EEPROM
    if (inStream == kStreamEEProm) {
        v = eeprom_read_byte((const uint8_t *)(uint16_t)eepos);
        eepos++;
        if (v == '\0') {
            inStream = kStreamSerial;
            inhibitOutput = 0;
            if (runAfterLoad) {
                runAfterLoad = 0;
                triggerRun = 1;
            }
            return NL;
        }
        return v;
    }
#endif

#ifdef ENABLE_XEEPROM
    if (inStream == kStreamEEPromX) {
        v = xeep_read_byte(eeposX);
        eeposX++;
        if (v == '\0') {
            inStream = kStreamSerial;
            inhibitOutput = 0;
            if (runAfterLoad) {
                runAfterLoad = 0;
                triggerRun = 1;
            }
            return NL;
        }
        return v;
    }
#endif

    if (inStream == kStreamSercom) {
        if (!uart1_is_open()) {
            inStream = kStreamSerial;
            inhibitOutput = 0;
            return NL;
        }
        /* Wait for a byte with 5-second timeout */
        {
            uint32_t t0 = millis();
            while (!uart1_available()) {
                if ((millis() - t0) > 5000) {
                    inStream = kStreamSerial;
                    inhibitOutput = 0;
                    if (runAfterLoad) {
                        runAfterLoad = 0;
                        triggerRun = 1;
                    }
                    return NL;
                }
            }
        }
        v = uart1_getc();
        if (v == '\0') {
            inStream = kStreamSerial;
            inhibitOutput = 0;
            if (runAfterLoad) {
                runAfterLoad = 0;
                triggerRun = 1;
            }
            return NL;
        }
        return v;
    }

    /* Default: keyboard (blocking) with cursor blink */
    while (1) {
        keyboard_poll();
        if (keyboard_available()) {
            cursor_off();
            v = keyboard_getc();
            if (v > 0) return v;
        }
        cursor_blink_tick();
#ifdef ENABLE_TONES
        tone_check();
#endif
    }
}

/* Outputs a newline (NL) followed by a carriage return (CR). */
static void line_terminator(void)
{
    outchar(NL);
    outchar(CR);
}

/* Polls the keyboard for Ctrl-C or ESC and returns 1 if pressed,
 * allowing long-running operations to be interrupted. */
static unsigned char breakcheck(void)
{
#ifdef ENABLE_TONES
    tone_check();
#endif
    keyboard_poll();
    if (keyboard_available()) {
        int c = keyboard_getc();
        if (c == CTRLC || c == ESC_KEY) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Accurate delay using Timer0 millis counter (breakable)             */
/* ------------------------------------------------------------------ */
/* Delays for the specified number of milliseconds using the Timer0
 * millis counter.  Checks for Ctrl-C / ESC and returns 1 if the
 * delay was aborted, 0 on normal completion. */
static uint8_t delay_ms(unsigned int ms)
{
    uint32_t start = millis();
    while ((millis() - start) < (uint32_t)ms) {
#ifdef ENABLE_TONES
        tone_check();
#endif
        keyboard_poll();
        if (keyboard_available()) {
            int c = keyboard_getc();
            if (c == CTRLC || c == ESC_KEY) return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tone generation via Timer2 / OC2A on PD7                           */
/* ------------------------------------------------------------------ */
#ifdef ENABLE_TONES

/* Starts tone generation on OC2A (PD7) at the given frequency (Hz)
 * by selecting the appropriate Timer2 prescaler for the closest match. */
static void tone_start(unsigned int freq)
{
    if (freq == 0) return;
    PIEZO_DDR |= (1 << PIEZO_BIT);

    uint32_t ocr;
    uint8_t prescaler_bits;

    ocr = (uint32_t)F_CPU / (2UL * 1UL * (uint32_t)freq) - 1;
    if (ocr <= 255) {
        prescaler_bits = (1 << CS20);
    } else {
        ocr = (uint32_t)F_CPU / (2UL * 8UL * (uint32_t)freq) - 1;
        if (ocr <= 255) {
            prescaler_bits = (1 << CS21);
        } else {
            ocr = (uint32_t)F_CPU / (2UL * 64UL * (uint32_t)freq) - 1;
            if (ocr <= 255) {
                prescaler_bits = (1 << CS22);
            } else {
                ocr = (uint32_t)F_CPU / (2UL * 256UL * (uint32_t)freq) - 1;
                if (ocr <= 255) {
                    prescaler_bits = (1 << CS22) | (1 << CS21);
                } else {
                    ocr = (uint32_t)F_CPU / (2UL * 1024UL * (uint32_t)freq) - 1;
                    if (ocr > 255) ocr = 255;
                    prescaler_bits = (1 << CS22) | (1 << CS21) | (1 << CS20);
                }
            }
        }
    }

    TCCR2A = (1 << COM2A0) | (1 << WGM21);
    TCCR2B = prescaler_bits;
    OCR2A  = (uint8_t)ocr;
}

/* Stops tone generation: disables Timer2 and pulls the piezo pin low. */
static void tone_stop(void)
{
    TCCR2A = 0;
    TCCR2B = 0;
    PIEZO_PORT &= ~(1 << PIEZO_BIT);
    tone_running = 0;
    tone_end_ms  = 0;
}

#endif /* ENABLE_TONES */

/* ------------------------------------------------------------------ */
/*  Parser helpers                                                     */
/* ------------------------------------------------------------------ */

/* Advances txtpos past any SPACE or TAB characters. */
static void ignore_blanks(void)
{
    while (*txtpos == SPACE || *txtpos == TAB)
        txtpos++;
}

/* Scans a PROGMEM keyword table for a match starting at txtpos.
 * Sets table_index to the matched entry's index, or the "unknown"
 * sentinel if no match is found.  Advances txtpos on a match. */
static void scantable(const unsigned char *table)
{
    int i = 0;
    table_index = 0;
    while (1) {
        if (pgm_read_byte(table) == 0)
            return;
        if (txtpos[i] == pgm_read_byte(table)) {
            i++;
            table++;
        } else {
            if (txtpos[i] + 0x80 == pgm_read_byte(table)) {
                txtpos += i + 1;
                ignore_blanks();
                return;
            }
            while ((pgm_read_byte(table) & 0x80) == 0)
                table++;
            table++;
            table_index++;
            ignore_blanks();
            i = 0;
        }
    }
}

/* Pushes one byte onto the software stack (sp grows downward). */
static void pushb(unsigned char b)
{
    sp--;
    *sp = b;
}

/* Pops and returns one byte from the software stack. */
static unsigned char popb(void)
{
    unsigned char b;
    b = *sp;
    sp++;
    return b;
}

/* ------------------------------------------------------------------ */
/*  Number printing                                                    */
/* ------------------------------------------------------------------ */

/* Prints a signed decimal integer to the active output stream. */
static void printnum(int num)
{
    int digits = 0;
    if (num < 0) {
        num = -num;
        outchar('-');
    }
    do {
        pushb(num % 10 + '0');
        num = num / 10;
        digits++;
    } while (num > 0);
    while (digits > 0) {
        outchar(popb());
        digits--;
    }
}

/* Prints an unsigned decimal integer to the active output stream. */
static void printUnum(unsigned int num)
{
    int digits = 0;
    do {
        pushb(num % 10 + '0');
        num = num / 10;
        digits++;
    } while (num > 0);
    while (digits > 0) {
        outchar(popb());
        digits--;
    }
}

/* ------------------------------------------------------------------ */
/*  Number parsing                                                     */
/* ------------------------------------------------------------------ */

/* Parses an unsigned decimal integer starting at txtpos and returns
 * its value; caps at 0xFFFF on overflow.  Advances txtpos. */
static unsigned short testnum(void)
{
    unsigned short num = 0;
    ignore_blanks();
    while (*txtpos >= '0' && *txtpos <= '9') {
        if (num >= 0xFFFF / 10) {
            num = 0xFFFF;
            break;
        }
        num = num * 10 + *txtpos - '0';
        txtpos++;
    }
    return num;
}

/* ------------------------------------------------------------------ */
/*  Quoted string printing                                             */
/* ------------------------------------------------------------------ */

/* Outputs the quoted string (delimited by ' or ") that starts at
 * txtpos, advancing txtpos past the closing delimiter.
 * Returns 1 on success, 0 if the delimiter is missing or malformed. */
static unsigned char print_quoted_string(void)
{
    int i = 0;
    unsigned char delim = *txtpos;
    if (delim != '"' && delim != '\'')
        return 0;
    txtpos++;
    while (txtpos[i] != delim) {
        if (txtpos[i] == NL)
            return 0;
        i++;
    }
    while (*txtpos != delim) {
        outchar(*txtpos);
        txtpos++;
    }
    txtpos++;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  PROGMEM message printing                                           */
/* ------------------------------------------------------------------ */

/* Prints a PROGMEM message string without a trailing newline. */
static void printmsgNoNL(const unsigned char *msg)
{
    while (pgm_read_byte(msg) != 0)
        outchar(pgm_read_byte(msg++));
}

/* Prints a PROGMEM message string followed by a line terminator. */
static void printmsg(const unsigned char *msg)
{
    printmsgNoNL(msg);
    line_terminator();
}

/* ------------------------------------------------------------------ */
/*  Filename word extractor  (for LOAD/SAVE stubs, ifdef FILEIO)       */
/* ------------------------------------------------------------------ */
#ifdef ENABLE_FILEIO

/* Returns 1 if c is a valid filename character (alphanumeric, _, +, ., ~). */
static int isValidFnChar(char c)
{
    if (c >= '0' && c <= '9') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= 'a' && c <= 'z') return 1;
    if (c == '_' || c == '+' || c == '.' || c == '~') return 1;
    return 0;
}

/* Extracts the next whitespace-delimited filename token from txtpos.
 * Null-terminates the token in-place and returns a pointer to its start.
 * Sets expression_error on failure. */
static unsigned char *filenameWord(void)
{
    unsigned char *ret;
    expression_error = 0;
    while (!isValidFnChar(*txtpos)) txtpos++;
    ret = txtpos;
    if (*ret == '\0') { expression_error = 1; return ret; }
    txtpos++;
    while (isValidFnChar(*txtpos)) txtpos++;
    if (txtpos != ret) *txtpos = '\0';
    if (*ret == '\0') expression_error = 1;
    return ret;
}

#endif /* ENABLE_FILEIO */

/* ------------------------------------------------------------------ */
/*  Serial output helpers  (for SERPRINT)                              */
/* ------------------------------------------------------------------ */

/* Transmits one byte over UART1 (used by SERPRINT). */
static void sendchar(unsigned char c)
{
    uart1_putc(c);
}

/* Transmits a signed decimal integer over UART1. */
static void sendnum(int num)
{
    char buf[8];
    int i = 0;
    if (num < 0) {
        sendchar('-');
        num = -num;
    }
    do {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    } while (num > 0);
    while (i > 0) {
        sendchar(buf[--i]);
    }
}

/* Sends the quoted string at txtpos over UART1.
 * Advances txtpos past the closing delimiter.
 * Returns 1 on success, 0 if malformed. */
static unsigned char serialsend_quoted_string(void)
{
    int i = 0;
    unsigned char delim = *txtpos;
    if (delim != '"' && delim != '\'')
        return 0;
    txtpos++;
    while (txtpos[i] != delim) {
        if (txtpos[i] == NL)
            return 0;
        i++;
    }
    while (*txtpos != delim) {
        sendchar(*txtpos);
        txtpos++;
    }
    txtpos++;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Line input from keyboard  (with blinking cursor & backspace)       */
/* ------------------------------------------------------------------ */

static LINENUM linenum;

/* Displays prompt, then reads a line of input from the keyboard
 * into the program buffer.  Handles backspace and line-length limits.
 * Stores the result at program_end + sizeof(LINENUM). */
static void getln(char prompt)
{
    unsigned char *line_start;

    outchar(prompt);

    txtpos = program_end + sizeof(LINENUM);
    line_start = txtpos;

    while (1) {
        char c = (char)inchar();
        switch (c) {
        case NL:
        case CR:
            cursor_off();
            line_terminator();
            txtpos[0] = NL;
            return;

        case CTRLH:
        case 127:
            if (txtpos <= line_start)
                break;
            txtpos--;
            outchar('\b');
            outchar(' ');
            outchar('\b');
            break;

        default:
            if (txtpos >= variables_begin - 2) {
                outchar(BELL);
            } else {
                txtpos[0] = c;
                txtpos++;
                outchar(c);
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Find a line by number                                              */
/* ------------------------------------------------------------------ */

/* Searches the stored program for the line whose number is >= linenum.
 * Returns a pointer to that line, or program_end if not found. */
static unsigned char *findline(void)
{
    unsigned char *line = program_start;
    while (1) {
        if (line == program_end)
            return line;
        if (((LINENUM *)line)[0] >= linenum)
            return line;
        line += line[sizeof(LINENUM)];
    }
}

/* ------------------------------------------------------------------ */
/*  To-uppercase the input buffer (skip quoted strings)                */
/* ------------------------------------------------------------------ */

/* Converts the input buffer (program_end + sizeof(LINENUM) onwards)
 * to uppercase, skipping characters inside quoted strings. */
static void toUppercaseBuffer(void)
{
    unsigned char *c = program_end + sizeof(LINENUM);
    unsigned char quote = 0;
    while (*c != NL) {
        if (*c == quote)
            quote = 0;
        else if (*c == '"' || *c == '\'')
            quote = *c;
        else if (quote == 0 && *c >= 'a' && *c <= 'z')
            *c = *c + 'A' - 'a';
        c++;
    }
}

/* ------------------------------------------------------------------ */
/*  Print a stored program line                                        */
/* ------------------------------------------------------------------ */

/* Prints one stored BASIC program line (line number + text) to
 * the active output stream, advancing list_line to the next line. */
static void printline(void)
{
    LINENUM line_num;
    line_num = *((LINENUM *)(list_line));
    list_line += sizeof(LINENUM) + sizeof(char);
    printnum(line_num);
    outchar(' ');
    while (*list_line != NL) {
        outchar(*list_line);
        list_line++;
    }
    list_line++;
    line_terminator();
}

/* ------------------------------------------------------------------ */
/*  Expression parser                                                  */
/* ------------------------------------------------------------------ */

/* Parses a full expression including relational operators
 * (>=, <>, >, =, <=, <, !=).  Returns the integer result. */
static short int expression(void);

/* Parses the highest-precedence expression atoms: unary minus, numeric
 * literals, single-letter variables, built-in functions, and
 * parenthesised sub-expressions. */
static short int expr4(void)
{
    ignore_blanks();

    if (*txtpos == '-') {
        txtpos++;
        return -expr4();
    }

    if (*txtpos == '0') {
        txtpos++;
        return 0;
    }

    if (*txtpos >= '1' && *txtpos <= '9') {
        short int a = 0;
        do {
            a = a * 10 + *txtpos - '0';
            txtpos++;
        } while (*txtpos >= '0' && *txtpos <= '9');
        return a;
    }

    if (txtpos[0] >= 'A' && txtpos[0] <= 'Z') {
        short int a;
        if (txtpos[1] < 'A' || txtpos[1] > 'Z') {
            a = ((short int *)variables_begin)[*txtpos - 'A'];
            txtpos++;
            return a;
        }
        scantable(func_tab);
        if (table_index == FUNC_UNKNOWN)
            goto expr4_error;

        unsigned char f = table_index;

        if (*txtpos != '(')
            goto expr4_error;
        txtpos++;
        a = expression();
        if (*txtpos != ')')
            goto expr4_error;
        txtpos++;

        switch (f) {
        case FUNC_PEEK:
            return program[a];
        case FUNC_ABS:
            if (a < 0) return -a;
            return a;
        case FUNC_AREAD:
            {
                uint8_t pin = (uint8_t)a;
                if (pin < 24 || pin > 30) return -1;
                uint8_t ch = pin - 24;
                DDRA  &= ~(1 << ch);
                PORTA &= ~(1 << ch);
                ADMUX  = (1 << REFS0) | (ch & 0x07);
                ADCSRA = (1 << ADEN) | (1 << ADSC) |
                         (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
                while (ADCSRA & (1 << ADSC));
                return (short int)ADC;
            }
        case FUNC_DREAD:
            {
                uint8_t pin = (uint8_t)a;
                if (!pin_is_available(pin)) return -1;
                volatile uint8_t *ddr  = pin_to_ddr(pin);
                volatile uint8_t *port = pin_to_port(pin);
                volatile uint8_t *pinr = pin_to_pin(pin);
                uint8_t bit = pin_to_bit(pin);
                *ddr  &= ~(1 << bit);
                *port |=  (1 << bit);
                return (*pinr & (1 << bit)) ? 1 : 0;
            }
        case FUNC_RND:
            return (short int)(rand() % a);
        }
    }

    if (*txtpos == '(') {
        short int a;
        txtpos++;
        a = expression();
        if (*txtpos != ')')
            goto expr4_error;
        txtpos++;
        return a;
    }

expr4_error:
    expression_error = 1;
    return 0;
}

/* Parses multiplicative expressions (* and /), built on top of expr4(). */
static short int expr3(void)
{
    short int a, b;
    a = expr4();
    ignore_blanks();
    while (1) {
        if (*txtpos == '*') {
            txtpos++;
            b = expr4();
            a *= b;
        } else if (*txtpos == '/') {
            txtpos++;
            b = expr4();
            if (b != 0)
                a /= b;
            else
                expression_error = 1;
        } else {
            return a;
        }
    }
}

/* Parses additive expressions (+ and -), built on top of expr3(). */
static short int expr2(void)
{
    short int a, b;
    if (*txtpos == '-' || *txtpos == '+')
        a = 0;
    else
        a = expr3();
    while (1) {
        if (*txtpos == '-') {
            txtpos++;
            b = expr3();
            a -= b;
        } else if (*txtpos == '+') {
            txtpos++;
            b = expr3();
            a += b;
        } else {
            return a;
        }
    }
}

/* Parses a full expression including relational operators
 * (>=, <>, >, =, <=, <, !=).  Returns the integer result. */
static short int expression(void)
{
    short int a, b;
    a = expr2();
    if (expression_error) return a;
    scantable(relop_tab);
    if (table_index == RELOP_UNKNOWN)
        return a;
    switch (table_index) {
    case RELOP_GE:      b = expr2(); if (a >= b) return 1; break;
    case RELOP_NE:
    case RELOP_NE_BANG: b = expr2(); if (a != b) return 1; break;
    case RELOP_GT:      b = expr2(); if (a >  b) return 1; break;
    case RELOP_EQ:      b = expr2(); if (a == b) return 1; break;
    case RELOP_LE:      b = expr2(); if (a <= b) return 1; break;
    case RELOP_LT:      b = expr2(); if (a <  b) return 1; break;
    }
    return 0;
}

/* ================================================================== */
/*  The main interpreter function                                      */
/* ================================================================== */

/* The main BASIC interpreter loop.  Handles the interactive prompt,
 * line storage/deletion, direct execution, and all BASIC statements
 * via a large switch/goto dispatch table. */
static void basic_loop(void)
{
    unsigned char *start;
    unsigned char *newEnd;
    unsigned char linelen;
    uint8_t isDigital;
    uint8_t alsoWait = 0;
    int val;

#ifdef ENABLE_TONES
    tone_stop();
#endif

    program_start = program;
    program_end = program_start;
    sp = program + sizeof(program);

    stack_limit     = program + sizeof(program) - STACK_SIZE;
    variables_begin = stack_limit - 27 * VAR_SIZE;

    printnum(variables_begin - program_end);
    printmsg(memorymsg);

#ifdef ENABLE_EEPROM
    printUnum(E2END + 1);
    printmsg(eeprommsg);
#endif
#ifdef ENABLE_XEEPROM
    printUnum(XEEPROM_SIZE);
    printmsg(xeeprommsg);
#endif

warmstart:
    current_line = 0;
    sp = program + sizeof(program);
    printmsg(okmsg);

prompt:
    if (triggerRun) {
        triggerRun = 0;
        current_line = program_start;
        goto execline;
    }

    getln('>');
    toUppercaseBuffer();

    txtpos = program_end + sizeof(unsigned short);

    while (*txtpos != NL)
        txtpos++;

    {
        unsigned char *dest;
        dest = variables_begin - 1;
        while (1) {
            *dest = *txtpos;
            if (txtpos == program_end + sizeof(unsigned short))
                break;
            dest--;
            txtpos--;
        }
        txtpos = dest;
    }

    linenum = testnum();
    ignore_blanks();
    if (linenum == 0)
        goto direct;
    if (linenum == 0xFFFF)
        goto qhow;

    linelen = 0;
    while (txtpos[linelen] != NL)
        linelen++;
    linelen++;
    linelen += sizeof(unsigned short) + sizeof(char);

    txtpos -= 3;
    *((unsigned short *)txtpos) = linenum;
    txtpos[sizeof(LINENUM)] = linelen;

    start = findline();

    if (start != program_end && *((LINENUM *)start) == linenum) {
        unsigned char *dest2, *from;
        unsigned int tomove;
        from = start + start[sizeof(LINENUM)];
        dest2 = start;
        tomove = program_end - from;
        while (tomove > 0) {
            *dest2 = *from;
            from++;
            dest2++;
            tomove--;
        }
        program_end = dest2;
    }

    if (txtpos[sizeof(LINENUM) + sizeof(char)] == NL)
        goto prompt;

    while (linelen > 0) {
        unsigned int tomove;
        unsigned char *from, *dest2;
        unsigned int space_to_make;

        space_to_make = txtpos - program_end;
        if (space_to_make > linelen)
            space_to_make = linelen;
        newEnd = program_end + space_to_make;
        tomove = program_end - start;

        from = program_end;
        dest2 = newEnd;
        while (tomove > 0) {
            from--;
            dest2--;
            *dest2 = *from;
            tomove--;
        }

        for (tomove = 0; tomove < space_to_make; tomove++) {
            *start = *txtpos;
            txtpos++;
            start++;
            linelen--;
        }
        program_end = newEnd;
    }
    goto prompt;

unimplemented:
    printmsg(unimplimentedmsg);
    goto prompt;

qhow:
    printmsg(howmsg);
    goto prompt;

qwhat:
    printmsgNoNL(whatmsg);
    if (current_line != (unsigned char *)0) {
        unsigned char tmp = *txtpos;
        if (*txtpos != NL)
            *txtpos = '^';
        list_line = current_line;
        printline();
        *txtpos = tmp;
    }
    line_terminator();
    goto prompt;

qsorry:
    printmsg(sorrymsg);
    goto warmstart;

run_next_statement:
    while (*txtpos == ':')
        txtpos++;
    ignore_blanks();
    if (*txtpos == NL)
        goto execnextline;
    goto interperateAtTxtpos;

direct:
    txtpos = program_end + sizeof(LINENUM);
    if (*txtpos == NL)
        goto prompt;

interperateAtTxtpos:
    if (breakcheck()) {
        printmsg(breakmsg);
        goto warmstart;
    }

    scantable(keywords);

    switch (table_index) {
    case KW_DELAY:
        goto do_delay;
    case KW_FILES:
        goto files;
    case KW_LIST:
        goto list;
    case KW_CHAIN:
        goto chain;
    case KW_LOAD:
        goto load;
    case KW_MEM:
        goto mem;
    case KW_NEW:
        if (txtpos[0] != NL)
            goto qwhat;
        program_end = program_start;
        goto prompt;
    case KW_RUN:
        current_line = program_start;
        goto execline;
    case KW_SAVE:
        goto save;
    case KW_NEXT:
        goto next;
    case KW_LET:
        goto assignment;
    case KW_IF:
        {
            short int val2;
            expression_error = 0;
            val2 = expression();
            if (expression_error || *txtpos == NL)
                goto qhow;
            if (val2 != 0)
                goto interperateAtTxtpos;
            goto execnextline;
        }
    case KW_GOTO:
        expression_error = 0;
        linenum = expression();
        if (expression_error || *txtpos != NL)
            goto qhow;
        current_line = findline();
        goto execline;
    case KW_GOSUB:
        goto gosub;
    case KW_RETURN:
        goto gosub_return;
    case KW_REM:
    case KW_QUOTE:
        goto execnextline;
    case KW_FOR:
        goto forloop;
    case KW_INPUT:
        goto input;
    case KW_PRINT:
    case KW_QMARK:
        goto print;
    case KW_POKE:
        goto poke;
    case KW_END:
    case KW_STOP:
        if (txtpos[0] != NL)
            goto qwhat;
        current_line = program_end;
        goto execline;
    case KW_BYE:
        return;
    case KW_AWRITE:
        isDigital = 0;
        goto awrite;
    case KW_DWRITE:
        isDigital = 1;
        goto dwrite;
    case KW_RSEED:
        goto rseed;
#ifdef ENABLE_TONES
    case KW_TONEW:
        alsoWait = 1;
        goto tonegen;
    case KW_TONE:
        goto tonegen;
    case KW_NOTONE:
        goto tonestop;
#endif
#ifdef ENABLE_EEPROM
    case KW_EFORMAT:
        goto eformat;
    case KW_ESAVE:
        goto esave;
    case KW_ELOAD:
        goto eload;
    case KW_ELIST:
        goto elist;
    case KW_EPOKE:
        goto epoke;
    case KW_EPEEK:
        goto epeek;
#endif
#ifdef ENABLE_XEEPROM
    case KW_XFORMAT:
        goto xformat;
    case KW_XSAVE:
        goto xsave;
    case KW_XLOAD:
        goto xload;
    case KW_XLIST:
        goto xlist;
    case KW_XPOKE:
        goto xpoke;
    case KW_XPEEK:
        goto xpeek;
#endif
    case KW_INKEY:
        goto inkey;

    case KW_SEROPEN:
        goto seropen;
    case KW_SERCLOSE:
        goto serclose;
    case KW_SERPRINT:
        goto serprint;
    case KW_SERREAD:
        goto serread;
    case KW_SERLOAD:
        goto serload;
	case KW_DRAWPIX:
		goto drawpix;
    case KW_DRAWLINE:
		goto drawline;
    case KW_DRAWRECT:
		goto drawrect;
    case KW_DRAWCIRC:
		goto drawcirc;
    case KW_DRAWCHAR:
		goto drawchar;
    case KW_GETPIX:
		goto getpix;
	case KW_CLS:
		goto cls;
    case KW_DEFAULT:
        goto assignment;
    default:
        break;
    }

execnextline:
    if (current_line == (unsigned char *)0)
        goto prompt;
    current_line += current_line[sizeof(LINENUM)];

execline:
    if (current_line == program_end)
        goto warmstart;
    txtpos = current_line + sizeof(LINENUM) + sizeof(char);
    goto interperateAtTxtpos;

/* ================================================================ */
/*  DELAY  (millis-based, breakable with ESC / Ctrl-C)              */
/* ================================================================ */
do_delay:
    {
        expression_error = 0;
        val = expression();
        if (expression_error) goto qwhat;
        if (delay_ms((unsigned int)val)) {
            printmsg(breakmsg);
            goto warmstart;
        }
        goto execnextline;
    }

/* ================================================================ */
/*  INTERNAL EEPROM commands                                        */
/* ================================================================ */
#ifdef ENABLE_EEPROM

elist:
    {
        uint16_t i;
        for (i = 0; i < (uint16_t)(E2END + 1); i++) {
            val = eeprom_read_byte((const uint8_t *)(uint16_t)i);
            if (val == '\0')
                goto execnextline;
            if (((val < ' ') || (val > '~')) && (val != NL) && (val != CR))
                outchar('?');
            else
                outchar(val);
        }
    }
    goto execnextline;

eformat:
    {
        printmsgNoNL(formatmsg);
        uint16_t i;
        for (i = 0; i < (uint16_t)E2END; i++) {
            if ((i & 0x03f) == 0x20) outchar('.');
            eeprom_write_byte((uint8_t *)(uint16_t)i, 0);
        }
        outchar(LF);
    }
    goto execnextline;

esave:
    {
        outStream = kStreamEEProm;
        eepos = 0;
        list_line = findline();
        while (list_line != program_end)
            printline();
        outchar('\0');
        outStream = kStreamSerial;
        goto warmstart;
    }

eload:
    program_end = program_start;
    eepos = 0;
    inStream = kStreamEEProm;
    inhibitOutput = 0;
    goto warmstart;

epoke:
    {
        unsigned int x;
        uint16_t z;
        expression_error = 0;
        x = (unsigned int)expression();
        if (expression_error) goto qwhat;
        ignore_blanks();
        if (*txtpos != ',') goto qwhat;
        txtpos++;
        ignore_blanks();
        expression_error = 0;
        z = (uint16_t)expression();
        if (expression_error) goto qwhat;
        eeprom_write_byte((uint8_t *)(uint16_t)z, (uint8_t)x);
        goto run_next_statement;
    }

epeek:
    {
        uint16_t z;
        short int value;
        short int *var;
        if (*txtpos < 'A' || *txtpos > 'Z') goto qwhat;
        var = (short int *)variables_begin + *txtpos - 'A';
        txtpos++;
        ignore_blanks();
        if (*txtpos != ',') goto qwhat;
        txtpos++;
        ignore_blanks();
        expression_error = 0;
        z = (uint16_t)expression();
        if (expression_error) goto qwhat;
        value = eeprom_read_byte((const uint8_t *)(uint16_t)z);
        *var = value;
        goto run_next_statement;
    }

#endif /* ENABLE_EEPROM */

/* ================================================================ */
/*  EXTERNAL EEPROM commands                                        */
/* ================================================================ */
#ifdef ENABLE_XEEPROM

xlist:
    {
        uint16_t i;
        for (i = 0; i < XEEPROM_SIZE; i++) {
            val = xeep_read_byte(i);
            if (val == '\0')
                goto execnextline;
            if (((val < ' ') || (val > '~')) && (val != NL) && (val != CR))
                outchar('?');
            else
                outchar(val);
        }
    }
    goto execnextline;

xformat:
    {
        printmsgNoNL(formatmsg);
        uint16_t i;
        for (i = 0; i < XEEPROM_SIZE; i++) {
            if ((i & 0x03f) == 0x20) outchar('.');
            xeep_write_byte(i, 0);
        }
        outchar(LF);
    }
    goto execnextline;

xsave:
    {
        outStream = kStreamEEPromX;
        eeposX = 0;
        list_line = findline();
        while (list_line != program_end)
            printline();
        outchar('\0');
        outStream = kStreamSerial;
        goto warmstart;
    }

xload:
    program_end = program_start;
    eeposX = 0;
    inStream = kStreamEEPromX;
    inhibitOutput = 0;
    goto warmstart;

xpoke:
    {
        unsigned int x;
        int z;
        expression_error = 0;
        x = (unsigned int)expression();
        if (expression_error) goto qwhat;
        ignore_blanks();
        if (*txtpos != ',') goto qwhat;
        txtpos++;
        ignore_blanks();
        expression_error = 0;
        z = expression();
        if (expression_error) goto qwhat;
        xeep_write_byte((uint16_t)z, (uint8_t)x);
        goto run_next_statement;
    }

xpeek:
    {
        int z;
        short int value;
        short int *var;
        if (*txtpos < 'A' || *txtpos > 'Z') goto qwhat;
        var = (short int *)variables_begin + *txtpos - 'A';
        txtpos++;
        ignore_blanks();
        if (*txtpos != ',') goto qwhat;
        txtpos++;
        ignore_blanks();
        expression_error = 0;
        z = expression();
        if (expression_error) goto qwhat;
        value = xeep_read_byte((uint16_t)z);
        *var = value;
        goto run_next_statement;
    }

#endif /* ENABLE_XEEPROM */

/* ================================================================ */
/*  INKEY                                                           */
/* ================================================================ */
inkey:
    {
        short int *var;
        ignore_blanks();
        if (*txtpos < 'A' || *txtpos > 'Z') goto qwhat;
        var = (short int *)variables_begin + *txtpos - 'A';
        txtpos++;
        keyboard_poll();
        if (keyboard_available()) {
            int c = keyboard_getc();
            *var = (c > 0) ? (short int)c : 0;
        } else {
            *var = 0;
        }
        goto run_next_statement;
    }

/* ---- INPUT ---- */
input:
    {
        unsigned char var;
        int value;
        ignore_blanks();
        if (*txtpos < 'A' || *txtpos > 'Z') goto qwhat;
        var = *txtpos;
        txtpos++;
        ignore_blanks();
        if (*txtpos != NL && *txtpos != ':') goto qwhat;
    inputagain:
        tmptxtpos = txtpos;
        getln('?');
        toUppercaseBuffer();
        txtpos = program_end + sizeof(unsigned short);
        ignore_blanks();
        expression_error = 0;
        value = expression();
        if (expression_error) goto inputagain;
        ((short int *)variables_begin)[var - 'A'] = value;
        txtpos = tmptxtpos;
        goto run_next_statement;
    }

/* ---- FOR ---- */
forloop:
    {
        unsigned char var;
        short int initial, step2, terminal;
        ignore_blanks();
        if (*txtpos < 'A' || *txtpos > 'Z') goto qwhat;
        var = *txtpos;
        txtpos++;
        ignore_blanks();
        if (*txtpos != '=') goto qwhat;
        txtpos++;
        ignore_blanks();

        expression_error = 0;
        initial = expression();
        if (expression_error) goto qwhat;

        scantable(to_tab);
        if (table_index != 0) goto qwhat;

        terminal = expression();
        if (expression_error) goto qwhat;

        scantable(step_tab);
        if (table_index == 0) {
            step2 = expression();
            if (expression_error) goto qwhat;
        } else {
            step2 = 1;
        }

        ignore_blanks();
        if (*txtpos != NL && *txtpos != ':') goto qwhat;

        if (!expression_error && *txtpos == NL) {
            struct stack_for_frame *f;
            if (sp + sizeof(struct stack_for_frame) < stack_limit)
                goto qsorry;
            sp -= sizeof(struct stack_for_frame);
            f = (struct stack_for_frame *)sp;
            ((short int *)variables_begin)[var - 'A'] = initial;
            f->frame_type   = STACK_FOR_FLAG;
            f->for_var      = var;
            f->terminal     = terminal;
            f->step         = step2;
            f->txtpos       = txtpos;
            f->current_line = current_line;
            goto run_next_statement;
        }
    }
    goto qhow;

/* ---- GOSUB ---- */
gosub:
    expression_error = 0;
    linenum = expression();
    if (!expression_error && *txtpos == NL) {
        struct stack_gosub_frame *f;
        if (sp + sizeof(struct stack_gosub_frame) < stack_limit)
            goto qsorry;
        sp -= sizeof(struct stack_gosub_frame);
        f = (struct stack_gosub_frame *)sp;
        f->frame_type   = STACK_GOSUB_FLAG;
        f->txtpos       = txtpos;
        f->current_line = current_line;
        current_line = findline();
        goto execline;
    }
    goto qhow;

/* ---- NEXT / RETURN ---- */
next:
    ignore_blanks();
    if (*txtpos < 'A' || *txtpos > 'Z') goto qhow;
    txtpos++;
    ignore_blanks();
    if (*txtpos != ':' && *txtpos != NL) goto qwhat;

gosub_return:
    tempsp = sp;
    while (tempsp < program + sizeof(program) - 1) {
        switch (tempsp[0]) {
        case STACK_GOSUB_FLAG:
            if (table_index == KW_RETURN) {
                struct stack_gosub_frame *f = (struct stack_gosub_frame *)tempsp;
                current_line = f->current_line;
                txtpos       = f->txtpos;
                sp += sizeof(struct stack_gosub_frame);
                goto run_next_statement;
            }
            tempsp += sizeof(struct stack_gosub_frame);
            break;
        case STACK_FOR_FLAG:
            if (table_index == KW_NEXT) {
                struct stack_for_frame *f = (struct stack_for_frame *)tempsp;
                if (txtpos[-1] == f->for_var) {
                    short int *varaddr = ((short int *)variables_begin) + txtpos[-1] - 'A';
                    *varaddr = *varaddr + f->step;
                    if ((f->step > 0 && *varaddr <= f->terminal) ||
                        (f->step < 0 && *varaddr >= f->terminal)) {
                        txtpos       = f->txtpos;
                        current_line = f->current_line;
                        goto run_next_statement;
                    }
                    sp = tempsp + sizeof(struct stack_for_frame);
                    goto run_next_statement;
                }
            }
            tempsp += sizeof(struct stack_for_frame);
            break;
        default:
            goto warmstart;
        }
    }
    goto qhow;

/* ---- LET / assignment ---- */
assignment:
    {
        short int value;
        short int *var;
        if (*txtpos < 'A' || *txtpos > 'Z') goto qhow;
        var = (short int *)variables_begin + *txtpos - 'A';
        txtpos++;
        ignore_blanks();
        if (*txtpos != '=') goto qwhat;
        txtpos++;
        ignore_blanks();
        expression_error = 0;
        value = expression();
        if (expression_error) goto qwhat;
        if (*txtpos != NL && *txtpos != ':') goto qwhat;
        *var = value;
    }
    goto run_next_statement;

/* ---- POKE ---- */
poke:
    {
        short int value;
        unsigned char *address;
        expression_error = 0;
        value = expression();
        if (expression_error) goto qwhat;
        address = (unsigned char *)(unsigned int)value;
        ignore_blanks();
        if (*txtpos != ',') goto qwhat;
        txtpos++;
        ignore_blanks();
        expression_error = 0;
        value = expression();
        if (expression_error) goto qwhat;
        if (*txtpos != NL && *txtpos != ':') goto qwhat;
        *address = (unsigned char)value;
    }
    goto run_next_statement;

/* ---- LIST ---- */
list:
    linenum = testnum();
    if (txtpos[0] != NL) goto qwhat;
    list_line = findline();
    while (list_line != program_end)
        printline();
    goto warmstart;

/* ---- PRINT ---- */
print:
    if (*txtpos == ':') {
        line_terminator();
        txtpos++;
        goto run_next_statement;
    }
    if (*txtpos == NL)
        goto execnextline;
    while (1) {
        ignore_blanks();
        if (print_quoted_string()) {
            ;
        } else if (*txtpos == '"' || *txtpos == '\'') {
            goto qwhat;
        } else {
            short int e;
            expression_error = 0;
            e = expression();
            if (expression_error) goto qwhat;
            printnum(e);
        }
        if (*txtpos == ',')
            txtpos++;
        else if (txtpos[0] == ';' && (txtpos[1] == NL || txtpos[1] == ':')) {
            txtpos++;
            break;
        } else if (*txtpos == NL || *txtpos == ':') {
            line_terminator();
            break;
        } else {
            goto qwhat;
        }
    }
    goto run_next_statement;

/* ---- MEM ---- */
mem:
    printnum(variables_begin - program_end);
    printmsg(memorymsg);
#ifdef ENABLE_EEPROM
    {
        uint16_t i;
        printUnum(E2END + 1);
        printmsg(eeprommsg);
        val = ' ';
        for (i = 0; (i < (uint16_t)(E2END + 1)) && (val != '\0'); i++)
            val = eeprom_read_byte((const uint8_t *)(uint16_t)i);
        printUnum((E2END + 1) - (i - 1));
        printmsg(eepromamsg);
    }
#endif
#ifdef ENABLE_XEEPROM
    {
        uint16_t i;
        printUnum(XEEPROM_SIZE);
        printmsg(xeeprommsg);
        val = ' ';
        for (i = 0; (i < XEEPROM_SIZE) && (val != '\0'); i++)
            val = xeep_read_byte(i);
        printUnum(XEEPROM_SIZE - (i - 1));
        printmsg(xeepromamsg);
    }
#endif
    goto run_next_statement;

/* ---- AWRITE / DWRITE ---- */
awrite:
dwrite:
    {
        short int pinNo;
        short int value;
        expression_error = 0;
        pinNo = expression();
        if (expression_error) goto qwhat;
        ignore_blanks();
        if (*txtpos != ',') goto qwhat;
        txtpos++;
        ignore_blanks();
        scantable(highlow_tab);
        if (table_index != HIGHLOW_UNKNOWN) {
            if (table_index <= HIGHLOW_HIGH)
                value = 1;
            else
                value = 0;
        } else {
            expression_error = 0;
            value = expression();
            if (expression_error) goto qwhat;
        }
        {
            uint8_t pin = (uint8_t)pinNo;
            if (!pin_is_available(pin)) {
                printmsg(pinmsg);
                goto run_next_statement;
            }
            volatile uint8_t *ddr  = pin_to_ddr(pin);
            volatile uint8_t *port = pin_to_port(pin);
            uint8_t bit = pin_to_bit(pin);
            *ddr |= (1 << bit);
            if (isDigital) {
                if (value) *port |=  (1 << bit);
                else       *port &= ~(1 << bit);
            } else {
                if (value) *port |=  (1 << bit);
                else       *port &= ~(1 << bit);
            }
        }
    }
    goto run_next_statement;

/* ---- FILES ---- */
files:
    goto unimplemented;

/* ---- CHAIN ---- */
chain:
    runAfterLoad = 1;

/* ---- LOAD ---- */
load:
    program_end = program_start;
    goto unimplemented;

/* ---- SAVE ---- */
save:
    goto unimplemented;

/* ---- RSEED ---- */
rseed:
    {
        short int value;
        expression_error = 0;
        value = expression();
        if (expression_error) goto qwhat;
        srand((unsigned int)value);
        goto run_next_statement;
    }

/* ================================================================ */
/*  Serial communication commands  (UART1: PD2=RX, PD3=TX, 9600)   */
/* ================================================================ */

seropen:
    uart1_open();
    goto run_next_statement;

serclose:
    uart1_close();
    goto run_next_statement;

serprint:
    {
        if (!uart1_is_open()) goto run_next_statement;

        if (*txtpos == ':') {
            sendchar(NL);
            sendchar(CR);
            txtpos++;
            goto run_next_statement;
        }
        if (*txtpos == NL)
            goto execnextline;

        while (1) {
            ignore_blanks();
            if (serialsend_quoted_string()) {
                ;
            } else if (*txtpos == '"' || *txtpos == '\'') {
                goto qwhat;
            } else {
                short int e;
                expression_error = 0;
                e = expression();
                if (expression_error)
                    goto qwhat;
                sendnum(e);
            }

            if (*txtpos == ',')
                txtpos++;
            else if (txtpos[0] == ';' && (txtpos[1] == NL || txtpos[1] == ':')) {
                txtpos++;
                break;
            } else if (*txtpos == NL || *txtpos == ':') {
                break;
            } else {
                goto qwhat;
            }
        }
    }
    goto run_next_statement;

serread:
    {
        if (!uart1_is_open()) goto run_next_statement;

        /* Read characters from UART1 until LF or 1-second timeout */
        while (1) {
            uint32_t t0 = millis();
            while (!uart1_available()) {
                if ((millis() - t0) > 1000) goto serread_done;
                keyboard_poll();
                if (keyboard_available()) {
                    int k = keyboard_getc();
                    if (k == CTRLC || k == ESC_KEY) {
                        printmsg(breakmsg);
                        goto warmstart;
                    }
                }
            }
            {
                char ch = (char)uart1_getc();
                if (ch == '\n') {
                    line_terminator();
                    break;
                }
                if (ch >= 32 && ch <= 126) {
                    outchar(ch);
                }
            }
        }
    serread_done:;
    }
    goto run_next_statement;

serload:
    {
        if (!uart1_is_open()) goto run_next_statement;
        program_end = program_start;
        inStream = kStreamSercom;
        inhibitOutput = 1;
        goto warmstart;
    }

/* ================================================================ */
/*  Graphics commands                                        		*/
/* ================================================================ */

drawpix:{short int x,y,c;
    expression_error=0;x=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;y=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;c=expression();if(expression_error)goto qwhat;
    if(x<0||x>=HRES_PIXELS||y<0||y>=VRES||c<0||c>2)goto qhow;
    gfx_draw_mode=(c==0)?DRAW_CLEAR:(c==1)?DRAW_SET:DRAW_XOR;
    gfx_pixel(x,y);
    goto run_next_statement;}

drawline:{short int x0,y0,x1,y1,c;
    expression_error=0;x0=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;y0=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;x1=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;y1=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;c=expression();if(expression_error)goto qwhat;
    if(x0<0||x0>=HRES_PIXELS||y0<0||y0>=VRES||x1<0||x1>=HRES_PIXELS||y1<0||y1>=VRES||c<0||c>2)goto qhow;
    gfx_draw_mode=(c==0)?DRAW_CLEAR:(c==1)?DRAW_SET:DRAW_XOR;
    gfx_line(x0,y0,x1,y1);
    goto run_next_statement;}

drawrect:{short int x0,y0,w,h,c,f;
    expression_error=0;x0=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;y0=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;w=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;h=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;c=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;f=expression();if(expression_error)goto qwhat;
    if(x0<0||x0>=HRES_PIXELS||y0<0||y0>=VRES||w<0||h<0||c<0||c>2)goto qhow;
    /* Draw outline with colour c */
    gfx_draw_mode=(c==0)?DRAW_CLEAR:(c==1)?DRAW_SET:DRAW_XOR;
    gfx_rect(x0,y0,w,h);
    /* Fill interior if f is 0, 1, or 2 */
    if(f>=0&&f<=2&&w>2&&h>2){
        gfx_draw_mode=(f==0)?DRAW_CLEAR:(f==1)?DRAW_SET:DRAW_XOR;
        gfx_fill_rect(x0+1,y0+1,w-2,h-2);}
    goto run_next_statement;}

drawcirc:{short int cx,cy,r,c,f;
    expression_error=0;cx=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;cy=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;r=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;c=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;f=expression();if(expression_error)goto qwhat;
    if(cx<0||cx>=HRES_PIXELS||cy<0||cy>=VRES||r<0||c<0||c>2)goto qhow;
    /* Draw outline */
    gfx_draw_mode=(c==0)?DRAW_CLEAR:(c==1)?DRAW_SET:DRAW_XOR;
    gfx_circle(cx,cy,r);
    /* Fill if f is 0, 1, or 2 */
    if(f>=0&&f<=2&&r>1){
        gfx_draw_mode=(f==0)?DRAW_CLEAR:(f==1)?DRAW_SET:DRAW_XOR;
        gfx_fill_circle(cx,cy,r-1);}
    goto run_next_statement;}

drawchar:{short int x,y;unsigned char ch;
    expression_error=0;x=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;y=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    ch=(unsigned char)*txtpos;txtpos++;
    if(x<0||x>=HRES_PIXELS||y<0||y>=VRES)goto qhow;
    gfx_draw_mode=DRAW_SET;
    gfx_draw_char(x,y,ch,1);
    goto run_next_statement;}

getpix:{short int x,y;
    expression_error=0;x=expression();if(expression_error)goto qwhat;
    ignore_blanks();if(*txtpos!=',')goto qwhat;txtpos++;ignore_blanks();
    expression_error=0;y=expression();if(expression_error)goto qwhat;
    if(x<0||x>=HRES_PIXELS||y<0||y>=VRES)goto qhow;
    printnum((int)video_get_pixel(x,y));line_terminator();
    goto run_next_statement;}
	
cls:
    text_cls();
    goto run_next_statement;


/* ================================================================ */
/*  TONE / TONEW / NOTONE                                           */
/* ================================================================ */
#ifdef ENABLE_TONES
tonestop:
    tone_stop();
    goto run_next_statement;

tonegen:
    {
        short int freq;
        short int duration;

        expression_error = 0;
        freq = expression();
        if (expression_error) goto qwhat;

        ignore_blanks();
        if (*txtpos != ',') goto qwhat;
        txtpos++;
        ignore_blanks();

        expression_error = 0;
        duration = expression();
        if (expression_error) goto qwhat;

        if (freq == 0 || duration == 0)
            goto tonestop;

        tone_start((unsigned int)freq);

        tone_end_ms  = millis() + (uint32_t)(unsigned int)duration;
        tone_running = 1;

        if (alsoWait) {
            if (delay_ms((unsigned int)duration)) {
                tone_stop();
                alsoWait = 0;
                printmsg(breakmsg);
                goto warmstart;
            }
            tone_stop();
            alsoWait = 0;
        }
        goto run_next_statement;
    }
#endif /* ENABLE_TONES */

} /* end basic_loop() */


/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

/* Initialises all BASIC subsystems (Timer0, optional EEPROM, UART,
 * tones, GPIO) and enters the interpreter loop.
 * Returns only when the BYE statement is executed. */
void basic_init(void)
{
    timer0_init();

#ifdef ENABLE_XEEPROM
    xeep_init();
#endif

#ifdef ENABLE_TONES
    tone_stop();
#endif

    printmsg(initmsg);

#ifdef ENABLE_EEPROM
#ifdef ENABLE_EAUTORUN
    {
        int val = eeprom_read_byte((const uint8_t *)0);
        if (val >= '0' && val <= '9') {
            program_start = program;
            program_end   = program_start;
            inStream      = kStreamEEProm;
            eepos         = 0;
            inhibitOutput = 0;
            runAfterLoad  = 1;
        }
    }
#endif
#endif

    basic_loop();
}

void basic_run_iteration(void)
{
    keyboard_poll();
}
