10 REM BlinkLED
20 REM This program blinks the onboard LED on the AVR-SBC single board computer.
30 REM Author: Balazs Markus
40 DWRITE 14,1
50 DELAY 500
60 DWRITE 14,0
70 DELAY 500
80 GOTO 40