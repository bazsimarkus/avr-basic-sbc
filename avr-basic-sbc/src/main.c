/*
 * main.c  --  ATmega1284P entry point for TinyBasicPlus bare-metal port
 *
 * Initialises video, keyboard, text subsystems, then hands control
 * to the BASIC interpreter.
 *
 * The interpreter blocks inside basic_init() -> basic_loop() which
 * contains its own input loop with cursor blinking.  If BYE is typed,
 * basic_init() returns, we clear the screen and restart.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "video.h"
#include "keyboard.h"
#include "text.h"
#include "basic_core.h"

int main(void)
{
    /* Initialise hardware */
    video_init();
    keyboard_init();
    text_init();

    sei();

    /* The interpreter blocks inside basic_init().
     * If BYE is executed, it returns and we restart. */
    while (1) {
        basic_init();

        /* If we get here, BYE was typed.  Re-init and restart. */
        text_cls();
    }
}
