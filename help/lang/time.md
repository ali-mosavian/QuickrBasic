# TIME$ Function and Statement
keys: TIME$

Syntax
    TIME$
    TIME$ = timestring$

The function returns the clock time as an 8-character string,
"hh:mm:ss", on a 24-hour clock.

See also [DATE$](DATE$), [TIMER](TIMER)

## Details

The function returns the clock time as an 8-character string,
"hh:mm:ss", on a 24-hour clock.

The statement sets the clock. timestring$ is "hh", "hh:mm" or
"hh:mm:ss"; missing parts are 0. A part out of range, or text that is
not a time, is an Illegal function call.

The clock is that of the machine. TIME$ gives the time to the second;
for fractions of a second use TIMER.

## Example
    TIME$ = "13:30"
    PRINT LEFT$(TIME$, 5)               ' 13:30
