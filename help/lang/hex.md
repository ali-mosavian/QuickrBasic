# HEX$ Function
keys: HEX$

Syntax
    HEX$(n)

Returns the hexadecimal digits of the number n as a string, without a
leading &H and without leading zeros.

n is rounded to a whole number first. A negative n is shown in two's
complement form, as 32 bits wide: HEX$(-1) is "FFFFFFFF".

## Example
    PRINT HEX$(255)         ' FF
    PRINT HEX$(4096)        ' 1000
    PRINT VAL("&H" + HEX$(255))

See also [OCT$](OCT$), [VAL](VAL), [data types](data types)
