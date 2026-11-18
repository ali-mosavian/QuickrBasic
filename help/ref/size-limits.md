# Limits
keys: size limits, maximums, ranges

QuickrBasic has none of QBasic's 64 KB limits. Programs, lines,
strings, arrays and symbol tables grow until memory runs out; then
the error is [Out of memory](Out of memory).

## Numbers
```
BYTE      0 to 255
SHORT     -32768 to 32767
INTEGER   -2147483648 to 2147483647 (also LONG)
SINGLE    up to 3.4E38, 7 digits
DOUBLE    up to 1.8E308, 15-16 digits
```
Leaving a range is [Overflow](Overflow). See [Data types](data types).

## Strings and names
```
Fixed-length string    1 to 32767 characters
Variable-length string  as long as memory allows
File number             1 to 255
```

## Arrays
The subscript is an integer; the default lowest is 0, or 1 after
[OPTION BASE](OPTION BASE) 1.

## Screen
Text is 25, 43 or 50 rows. Graphics go up to 640x480; see
[SCREEN modes](SCREEN modes).

## Sound
[PLAY](PLAY) a$, b$, ... plays up to six voices. See
[Six voices](play voices).

See also [Data types](data types), [SCREEN modes](SCREEN modes)
