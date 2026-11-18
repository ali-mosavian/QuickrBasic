# STRIG Function and Statement
keys: STRIG, STRIG ON, STRIG OFF, STRIG STOP

Syntax
    STRIG(n)
    STRIG ON | OFF
    STRIG(n) ON | OFF | STOP
    ON STRIG(n) GOSUB line

The function reads a joystick trigger. n is 0 to 7, two numbers per
button:

  0, 1   lower trigger, stick A     4, 5   upper trigger, stick A
  2, 3   lower trigger, stick B     6, 7   upper trigger, stick B

The even number is -1 if the button was pressed since its last read;
the odd one is -1 while it is held. Otherwise 0.

The statements trap a trigger event for the even n (0, 2, 4, 6):
ON STRIG names the handler, STRIG(n) ON enables, OFF disables, STOP
holds events until the next ON. A bare STRIG ON or OFF is accepted
for older programs.

QuickrBasic runs without a joystick: the function returns 0 and the
trap never fires.

## Example
    ON STRIG(0) GOSUB Fire
    STRIG(0) ON

See also [STICK](STICK), [ON KEY](KEY), [GOSUB](GOSUB)
