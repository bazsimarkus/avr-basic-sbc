10 CLS
20 X = 30
30 Y = 30
40 R = 8
50 U = 3
60 V = 2
100 ' -- erase old ball --
110 DRAWCIRC X, Y, R, 0, 0
120 ' -- move --
130 X = X + U
140 Y = Y + V
150 ' -- bounce off walls --
160 IF X < R THEN U = ABS(U)
170 IF X > 319 - R THEN U = -ABS(U)
180 IF Y < R THEN V = ABS(V)
190 IF Y > 239 - R THEN V = -ABS(V)
200 ' -- draw new ball --
210 DRAWCIRC X, Y, R, 1, 1
220 DELAY 20
230 GOTO 100