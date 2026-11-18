# INTEGER Data Type
keys: INTEGER, %

Syntax
    DIM x AS INTEGER
    x% = 40000

A whole number from -2147483648 to 2147483647, in 4 bytes. The type
suffix is %, and DEFINT makes untyped names INTEGER.

This differs from QBasic, where INTEGER is 16 bits. In QuickrBasic
INTEGER and [LONG](LONG) are the same type, so 32767 + 1 is 32768 and
does not give Overflow. A program that relied on the 16-bit wraparound
or error needs [SHORT](SHORT) variables instead.

Where the size shows: a TYPE field or a record in a file takes 4
bytes; MKI$ and CVI still work on 2 bytes and give Overflow for a
value outside -32768 to 32767. For files laid out as QBasic's, declare
the fields AS SHORT.

Assigning a fraction rounds it to the nearest whole number. A value
outside the range is an Overflow error.

## Example
    DIM big AS INTEGER
    big = 32767 + 1
    PRINT big               ' 32768

See also [LONG](LONG), [SHORT](SHORT), [BYTE](BYTE),
[data types](data types), [MKI$](MKI$)
