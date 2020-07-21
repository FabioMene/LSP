start:
sob @R, 0
sob @G, 0
sob @B, 255
cmt

srw %A, 5000
loop0:
mob @R, 13
mob @B, -13
cmt
drjnz %A, loop0

sob @R, 255
sob @B, 0
cmt

srw %A, 5000
loop1:
mob @R, -13
mob @G, 13
cmt
drjnz %A, loop1

sob @R, 0
sob @G, 255
cmt

srw %A, 5000
loop2:
mob @G, -13
mob @B, 13
cmt
drjnz %A, loop2

sob @G, 0
sob @B, 255

j start
