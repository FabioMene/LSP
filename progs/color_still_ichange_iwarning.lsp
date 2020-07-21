color_loop:
    sob @R, 0
    sob @G, 0
    sob @B, 255

  _cl_start:
    sr %A, 5000
  _cl_loop0:
    mo @R,  13
    mo @B, -13
    cmt
    drjnz %A, _cl_loop0

    sob @R, 255
    sob @B, 0

    sr %A, 5000
  _cl_loop1:
    mo @R, -13
    mo @G,  13
    cmt
    drjnz %A, _cl_loop1

    sob @R, 0
    sob @G, 255

    sr %A, 5000
  _cl_loop2:
    mo @G, -13
    mo @B,  13
    cmt
    drjnz %A, _cl_loop2

    sob @G, 0
    sob @B, 255

    j _cl_start



still_white:
    sob @R, 255
    sob @G, 255
    sob @B, 255

  _sw_loop:
    cmt
    j _sw_loop



# arg: 1 -> color loop, 2 -> still_white
ivector 0:
    drjnz %A, _i0_not_color_loop
    isetpc color_loop
    j _i0_exit

  _i0_not_color_loop:
    drjnz %A, _i0_exit
    isetpc still_white

  _i0_exit:
    iret


# warning led, arg: repetitions
ivector 1:
    sob @R, 0
    sob @G, 0
    sob @B, 0
    cmt

  _i1_repeat:
  
    sr %B, 250
  _i1_to_white:
    mo @R, 261
    mo @G, 261
    mo @B, 261
    cmt
    drjnz %B, _i1_to_white

    sob @R, 255
    sob @G, 255
    sob @B, 255
    cmt

    sr %B, 250
  _i1_to_black:
    mo @R, -261
    mo @G, -261
    mo @B, -261
    cmt
    drjnz %B, _i1_to_black

    sob @R, 0
    sob @G, 0
    sob @B, 0
    cmt

    drjnz %A, _i1_repeat

    iret

