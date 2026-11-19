# HEX$ Function
keys: HEX$

Syntax
    HEX$(n)

Returns the hexadecimal digits of the number n as a string.

See also [OCT$](OCT$), [VAL](VAL), [data types](data types)

## Details
The result has no leading &H and no leading zeros. n is rounded to a
whole number first. A negative n is shown in two's complement form,
as 32 bits wide: HEX$(-1) is "FFFFFFFF". To turn the digits back
into a number, use VAL("&H" + h$).

## Example
    PRINT HEX$(255)         ' FF
    PRINT HEX$(4096)        ' 1000
    PRINT HEX$(-1)          ' FFFFFFFF
    PRINT VAL("&H" + HEX$(255))     ' 255
