# ============================================================
#  Makefile  --  AVR-SBC TinyBasicPlus
# ============================================================
#
#  QUICK REFERENCE - common build commands:
#
#    make                   	Build with the default keyboard layout (Hungarian)
#    make KB_LAYOUT=EN      	Build with US/English QWERTY layout
#    make KB_LAYOUT=DE      	Build with German QWERTZ layout
#    make KB_LAYOUT=HU      	Build with Hungarian QWERTZ layout (default)
#
#    make en                	Shorthand alias for KB_LAYOUT=EN
#    make de                	Shorthand alias for KB_LAYOUT=DE
#    make hu                	Shorthand alias for KB_LAYOUT=HU
#
#    make flash             	Compile (default layout) and upload via USBasp
#    make flash KB_LAYOUT=DE  	Compile with German layout and upload
#    make size              	Show flash/RAM usage breakdown
#    make clean             	Remove all generated object and binary files
#
# ============================================================


# ------------------------------------------------------------
#  OS detection
#
#  Windows always sets the environment variable OS=Windows_NT.
#  On Linux and macOS that variable is absent, so we fall into
#  the else branch.
#
#  RM_F     : Delete one or more files (ignore errors if absent)
#  DEVNULL  : The null device used to suppress unwanted output
#
#  NOTE: On Windows, 'del' does not accept forward-slash paths,
#  so we also define FIX_PATH which converts / to \ for targets
#  that pass paths to shell commands.  Pure make syntax ($(wildcard),
#  $(patsubst) etc.) always uses forward slashes and is unaffected.
# ------------------------------------------------------------
ifeq ($(OS), Windows_NT)
    RM_F     = del /f /q
    DEVNULL  = nul
    FIX_PATH = $(subst /,\,$1)
    $(info [OS] Detected Windows - using cmd.exe commands)
else
    RM_F     = rm -f
    DEVNULL  = /dev/null
    FIX_PATH = $1
    $(info [OS] Detected Linux / macOS - using POSIX commands)
endif


# ------------------------------------------------------------
#  Target MCU and clock frequency
#
#  MCU    : avr-gcc device name for the ATmega1284P.
#           Change this if you port to a different AVR (e.g.
#           atmega2560, atmega644p).
#
#  F_CPU  : CPU clock in Hz.  Must exactly match your crystal
#           and fuse settings.  Used by <util/delay.h> and the
#           UART baud-rate divider.  Common values:
#             8000000UL  - 8 MHz internal RC oscillator
#            16000000UL  - 16 MHz external crystal (default)
# ------------------------------------------------------------
MCU   = atmega1284p
F_CPU = 16000000UL


# ------------------------------------------------------------
#  Toolchain binaries
#
#  Adjust these paths if your avr-gcc toolchain is not on the
#  system PATH (e.g. on Windows with WinAVR or AVRGCC from
#  Microchip Studio).
# ------------------------------------------------------------
CC      = avr-gcc
OBJCOPY = avr-objcopy


# ------------------------------------------------------------
#  Keyboard layout selection
#
#  Passed to the compiler as -DKB_LAYOUT_<XX>.
#  The keyboard driver (keyboard.h / keyboard.c) uses this
#  flag to select the correct PS/2 Set-2 scan-code table:
#
#    EN  -  US/English QWERTY
#    DE  -  German QWERTZ
#    HU  -  Hungarian QWERTZ  (default if not specified)
#
#  Override on the command line:
#    make KB_LAYOUT=DE
# ------------------------------------------------------------
KB_LAYOUT ?= HU
KB_FLAG    = -DKB_LAYOUT_$(KB_LAYOUT)


# ------------------------------------------------------------
#  Feature switches
#
#  Each flag enables an optional subsystem.  Disabling one
#  reduces flash usage and simplifies the build.
#
#  FEAT_EEPROM   - Internal AVR EEPROM commands
#                  (EFORMAT, ELIST, ELOAD, ESAVE, EPOKE, EPEEK)
#                  Saves ~500 bytes of flash when disabled.
#
#  FEAT_XEEPROM  - External 25LC640 SPI EEPROM commands
#                  (XFORMAT, XLIST, XLOAD, XSAVE, XPOKE, XPEEK)
#                  Requires SPI EEPROM chip (U3) to be populated.
#                  Set to 0 if the chip is absent on your board.
#
#  FEAT_TONES    - Piezo buzzer tone commands
#                  (TONE freq dur, TONEW freq dur, NOTONE)
#                  Uses Timer2 and OC2A (PD7).  Set to 0 to
#                  free Timer2 for another purpose.
#
#  To disable a feature, override on the command line, e.g.:
#    make FEAT_TONES=0
#
#  NOT YET WIRED (add manually to FEAT_FLAGS if needed):
#    -DENABLE_FILEIO=1    SD-card LOAD/SAVE (hardware absent)
#    -DENABLE_EAUTORUN=1  Auto-run from internal EEPROM on boot
# ------------------------------------------------------------
FEAT_EEPROM  ?= 1
FEAT_XEEPROM ?= 1
FEAT_TONES   ?= 1

FEAT_FLAGS = -DENABLE_EEPROM=$(FEAT_EEPROM)   \
             -DENABLE_XEEPROM=$(FEAT_XEEPROM) \
             -DENABLE_TONES=$(FEAT_TONES)


# ------------------------------------------------------------
#  Optimisation level
#
#  -O2   Good balance of size and speed (default).
#  -Os   Optimise for minimum flash size - use this when
#        approaching the 128 KB flash limit.
#  -O1   Faster compile, less aggressive optimisation.
#  -O0   No optimisation - only for debugging with a JTAG probe.
#
#  Override on the command line:  make OPT=-Os
# ------------------------------------------------------------
OPT ?= -O2


# ------------------------------------------------------------
#  Compiler flags
#
#  -mmcu              : Selects the target MCU for code generation
#                       and links in the correct device start-up file.
#  -DF_CPU            : Exposes the clock frequency to all headers
#                       that need it (util/delay.h, uart.c, etc.).
#  -Iinclude          : Add the project's include/ directory to the
#                       header search path.
#  -Wall              : Enable all standard compiler warnings.
#  -Wextra            : Additional warnings (uninitialised vars, etc.)
#  -std=gnu99         : C99 with GNU extensions (required for some
#                       AVR-libc constructs and inline ASM).
#  -ffunction-sections
#  -fdata-sections    : Place each function/variable in its own
#                       linker section so the linker can dead-strip
#                       unreferenced code.  Reduces final binary size.
#                       Must be paired with -Wl,--gc-sections below.
#  $(KB_FLAG)         : Injects the selected keyboard layout define.
#  $(FEAT_FLAGS)      : Injects the selected feature-enable defines.
#  $(OPT)             : Injects the selected optimisation level.
# ------------------------------------------------------------
CFLAGS = -mmcu=$(MCU)        \
         -DF_CPU=$(F_CPU)    \
         $(OPT)              \
         -Iinclude           \
         -Wall               \
         -Wextra             \
         -std=gnu99          \
         -ffunction-sections \
         -fdata-sections     \
         $(KB_FLAG)          \
         $(FEAT_FLAGS)


# ------------------------------------------------------------
#  Assembler flags
#
#  -x assembler-with-cpp : Run the C preprocessor over .S
#    files before assembling.  Required by render_line.S which
#    uses #include "video_config.h" for timing constants.
# ------------------------------------------------------------
ASFLAGS = -mmcu=$(MCU) -x assembler-with-cpp


# ------------------------------------------------------------
#  Linker flags
#
#  -Wl,--gc-sections : Tell the linker to discard any function
#    or data section not reachable from an entry point.
#    Works together with -ffunction-sections / -fdata-sections
#    to eliminate dead code from the final binary.
# ------------------------------------------------------------
LDFLAGS = -Wl,--gc-sections


# ------------------------------------------------------------
#  Source files
#
#  SRCS : All C translation units.
#  ASMS : Assembly files (render_line.S - the cycle-exact
#         pixel-output routine that must not be compiled as C).
#  OBJS : Derived object file list (do not edit manually).
# ------------------------------------------------------------
SRCS = src/main.c       \
       src/video.c      \
       src/graphics.c   \
       src/text.c       \
       src/font8x8.c    \
       src/keyboard.c   \
       src/basic_core.c \
       src/xeeprom.c    \
       src/uart.c

ASMS = src/render_line.S

OBJS = $(SRCS:.c=.o) $(ASMS:.S=.o)


# ============================================================
#  Build targets
# ============================================================

# ------------------------------------------------------------
#  Default target - build firmware.hex and print a summary
# ------------------------------------------------------------
all: firmware.hex
	@echo ""
	@echo "  Build complete"
	@echo "  Keyboard : $(KB_LAYOUT)  |  EEPROM=$(FEAT_EEPROM)  XEEPROM=$(FEAT_XEEPROM)  TONES=$(FEAT_TONES)  OPT=$(OPT)"
	@echo ""

# ------------------------------------------------------------
#  Keyboard layout shorthand targets
#  Usage:  make en  /  make de  /  make hu
# ------------------------------------------------------------
en:
	$(MAKE) KB_LAYOUT=EN

de:
	$(MAKE) KB_LAYOUT=DE

hu:
	$(MAKE) KB_LAYOUT=HU

# ------------------------------------------------------------
#  Compile C source files
# ------------------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ------------------------------------------------------------
#  Assemble .S files (with C preprocessor pass)
# ------------------------------------------------------------
%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

# ------------------------------------------------------------
#  Link all objects into an ELF binary
# ------------------------------------------------------------
firmware.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o firmware.elf

# ------------------------------------------------------------
#  Convert ELF to Intel HEX (the format avrdude expects)
# ------------------------------------------------------------
firmware.hex: firmware.elf
	$(OBJCOPY) -O ihex firmware.elf firmware.hex

# ------------------------------------------------------------
#  Print flash / SRAM / EEPROM usage
#  The -A flag gives a per-section breakdown.
# ------------------------------------------------------------
size: firmware.elf
	avr-size -A firmware.elf

# ------------------------------------------------------------
#  Flash the MCU via USBasp programmer
#
#  -c usbasp   : Programmer type.  Change to yours if needed:
#                  usbasp       - USBasp (default here)
#                  arduino      - Arduino as ISP
#                  stk500v2     - STK500v2 / Atmel ICE
#                  dragon_isp   - AVR Dragon in ISP mode
#  -p m1284p   : Part number (must match MCU above).
#  -U flash:w  : Write the HEX file to flash memory.
# ------------------------------------------------------------
flash: firmware.hex
	avrdude -c usbasp -p m1284p -U flash:w:firmware.hex:i

# ------------------------------------------------------------
#  Remove all generated files
#
#  The leading '-' on each command tells make to continue even
#  if a file does not exist (both rm -f and del /f /q will
#  exit with an error code when there is nothing to delete).
#
#  FIX_PATH converts forward slashes to backslashes on Windows
#  so that cmd.exe can understand the paths passed to 'del'.
#  On Linux FIX_PATH is a no-op and the paths are left as-is.
# ------------------------------------------------------------
clean:
	-$(RM_F) $(call FIX_PATH, src/main.o)
	-$(RM_F) $(call FIX_PATH, src/video.o)
	-$(RM_F) $(call FIX_PATH, src/graphics.o)
	-$(RM_F) $(call FIX_PATH, src/text.o)
	-$(RM_F) $(call FIX_PATH, src/font8x8.o)
	-$(RM_F) $(call FIX_PATH, src/keyboard.o)
	-$(RM_F) $(call FIX_PATH, src/basic_core.o)
	-$(RM_F) $(call FIX_PATH, src/xeeprom.o)
	-$(RM_F) $(call FIX_PATH, src/uart.o)
	-$(RM_F) $(call FIX_PATH, src/render_line.o)
	-$(RM_F) firmware.elf
	-$(RM_F) firmware.hex
	@echo "  Clean complete"


# ------------------------------------------------------------
#  Phony targets
#  Prevents make from confusing these names with actual files.
# ------------------------------------------------------------
.PHONY: all en de hu size flash clean
