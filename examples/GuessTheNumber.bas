10 CLS
20 PRINT "=== GUESS THE NUMBER ==="
30 PRINT "I'm thinking of a number"
40 PRINT "between 1 and 100."
50 PRINT
60 RSEED 0
70 N = RND(100) + 1
80 T = 0
100 T = T + 1
110 PRINT "Guess #",T,": ";
120 INPUT G
130 IF G = N GOTO 200
140 IF G < N PRINT "Too LOW!  Try higher."
150 IF G > N PRINT "Too HIGH! Try lower."
160 GOTO 100
200 PRINT
210 PRINT "*** CORRECT! ***"
220 PRINT "You got it in ",T," tries!"
230 PRINT
240 PRINT "Play again? (1=yes 0=no)"
250 INPUT A
260 IF A = 1 GOTO 10
270 END