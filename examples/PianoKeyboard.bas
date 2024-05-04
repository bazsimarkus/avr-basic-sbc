10 REM PianoKeyboard
20 REM This program simulates a piano keyboard where you can input numbers 1-8 to play different musical notes. The tones are played using the onboard buzzer of the AVR-SBC.
30 REM Author: Balazs Markus
40 CLS
50 PRINT "Piano Keyboard"
60 PRINT "Input 1-8 to play music:"
70 INCHRN T
80 IF T < 1 THEN END
90 IF T > 8 THEN END
100 IF T = 1 THEN TONE 523, 200
110 IF T = 2 THEN TONE 587, 200
120 IF T = 3 THEN TONE 659, 200
130 IF T = 4 THEN TONE 698, 200
140 IF T = 5 THEN TONE 784, 200
150 IF T = 6 THEN TONE 880, 200
160 IF T = 7 THEN TONE 988, 200
170 IF T = 8 THEN TONE 1047, 200
180 CLS
190 GOTO 40