# OCT$ Function
keys: OCT$, oct

Syntax
    OCT$(n)

Returns the octal digits of n as a string, with no leading zeros and
no `&O` prefix. n is an integer; a fraction is rounded first.

A negative n gives its 32-bit two's-complement digits.

## Example
    PRINT OCT$(8), OCT$(255)       ' 10   377

See also [HEX$](HEX$), [STR$](STR$), [data types](data types)
