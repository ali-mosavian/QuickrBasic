# PMAP Function
keys: PMAP

Syntax
    PMAP(coordinate, function)

Converts a coordinate between the WINDOW's (logical) space and the
screen's physical pixels.

See also [WINDOW](WINDOW), [VIEW](VIEW), [POINT](POINT)

## Details
  function   result
     0       logical x to physical x
     1       logical y to physical y
     2       physical x to logical x
     3       physical y to logical y

Without a WINDOW, logical and physical are the same.

## Example
    SCREEN 1
    WINDOW (0, 0)-(100, 100)
    PRINT PMAP(50, 0)              ' 160: pixel column of x = 50
