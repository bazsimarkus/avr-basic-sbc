10 REM GuessingGame
20 REM This program is a simple number guessing game where the player tries to guess a randomly generated number between 1-100.
30 REM Author: Balazs Markus
40 CLS
50 PRINT "Welcome to the Guessing Game!"
60 PRINT "I'm thinking of a number."
70 PRINT "The number is between 1-100."
80 LET X = RND(100)
90 LET S = 1
100 PRINT "Can you guess what it is?"
110 PRINT "Your guess: "
120 INPUT G
130 IF G = X THEN PRINT "Congratulations! You won!":PRINT "You guessed ",S," times.": END
140 IF G < X THEN PRINT "Too low! Try again.":S=S+1: GOTO 110
150 IF G > X THEN PRINT "Too high! Try again.":S=S+1: GOTO 110