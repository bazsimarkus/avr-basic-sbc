10 REM CubeAnalogRead
20 REM This program draws a 3D cube on the display connected to the AVR-SBC. The cube can be moved around the screen using potentiometers connected to the pins 25 and 26.
30 REM Author: Balazs Markus
40 A = 0
50 B = 0
60 C = AREAD(25)/10
70 D = AREAD(24)/10
80 IF A = C THEN IF B = D THEN GOTO 60
90 A = C
100 B = D
110 CLS
120 DRAWLINE 25+C, 25+D, 75+C, 25+D, 1
130 DRAWLINE 25+C, 25+D, 25+C, 75+D, 1
140 DRAWLINE 75+C, 25+D, 75+C, 75+D, 1
150 DRAWLINE 25+C, 75+D, 75+C, 75+D, 1
160 DRAWLINE 35+C, 35+D, 85+C, 35+D, 1
170 DRAWLINE 35+C, 35+D, 35+C, 85+D, 1
180 DRAWLINE 85+C, 35+D, 85+C, 85+D, 1
190 DRAWLINE 35+C, 85+D, 85+C, 85+D, 1
200 DRAWLINE 25+C, 25+D, 35+C, 35+D, 1
210 DRAWLINE 75+C, 25+D, 85+C, 35+D, 1
220 DRAWLINE 25+C, 75+D, 35+C, 85+D, 1
230 DRAWLINE 75+C, 75+D, 85+C, 85+D, 1
240 DELAY 10
250 GOTO 60