# VARPTR$ Function
keys: VARPTR$

Syntax
    VARPTR$(variable)

Returns a short string that names variable's address. PLAY and DRAW
use it to read a variable inside a command string: the "X" command
runs another string, "=" takes a number.

variable is a numeric or string variable; the string it names must
hold valid PLAY or DRAW commands.

## Example
    tempo% = 150
    riff$ = "CDEFG"
    PLAY "T=" + VARPTR$(tempo%) + "X" + VARPTR$(riff$)

See also [PLAY string](PLAY string), [DRAW string](DRAW string),
[VARPTR](VARPTR), [PLAY](PLAY), [DRAW](DRAW)
