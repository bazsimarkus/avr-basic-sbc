10 CLS
20 PRINT "=== EAT THE TARGETS ==="
25 PRINT "WASD=move, Q=quit"
26 PRINT "Eat the targets!"
27 DELAY 2000
28 CLS
30 ' -- init position --
40 X = 160
50 Y = 120
60 P = 0
70 RSEED 0
80 ' -- place first target --
90 A = RND(310) + 5
95 B = RND(230) + 5
100 DRAWCIRC A, B, 3, 1, 1
110 ' -- draw player --
120 DRAWCIRC X, Y, 2, 1, 1
200 ' -- game loop --
210 INKEY K
220 IF K = 0 GOTO 210
230 ' -- erase player --
240 DRAWCIRC X, Y, 2, 0, 0
250 ' -- move (WASD) --
260 IF K = 119 Y = Y - 3
270 IF K = 115 Y = Y + 3
280 IF K = 97 X = X - 3
290 IF K = 100 X = X + 3
295 IF K = 87 Y = Y - 3
296 IF K = 83 Y = Y + 3
297 IF K = 65 X = X - 3
298 IF K = 68 X = X + 3
300 IF K = 113 GOTO 800
305 IF K = 81 GOTO 800
310 ' -- clamp to screen --
320 IF X < 3 X = 3
330 IF X > 316 X = 316
340 IF Y < 3 Y = 3
350 IF Y > 236 Y = 236
360 ' -- check eat target --
370 D = (X-A)*(X-A)+(Y-B)*(Y-B)
380 IF D > 36 GOTO 500
390 ' -- scored! --
400 P = P + 10
410 TONEW 880, 50
420 DRAWCIRC A, B, 3, 0, 0
430 A = RND(310) + 5
440 B = RND(230) + 5
450 DRAWCIRC A, B, 3, 1, 1
500 ' -- draw player --
510 DRAWCIRC X, Y, 2, 1, 1
520 GOTO 210
800 CLS
810 PRINT "GAME OVER!"
820 PRINT "Score: ",P
830 END

