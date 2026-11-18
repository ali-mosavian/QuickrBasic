# STICK Function
keys: STICK

Syntax
    STICK(n)

Reads a joystick axis.

  n = 0   x of stick A     n = 2   x of stick B
  n = 1   y of stick A     n = 3   y of stick B

Call STICK(0) first: it samples both sticks, and STICK(1) to STICK(3)
return that sample. Other values of n are an Illegal function call.

QuickrBasic runs without a joystick, so every reading is 0.

## Example
    s% = STICK(0)
    PRINT STICK(0), STICK(1)

See also [STRIG](STRIG), [PEN](PEN)
