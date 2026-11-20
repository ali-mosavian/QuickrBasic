# LEN Function
keys: LEN

Syntax
    LEN(string)
    LEN(variable)

Returns the number of characters in a string, or the number of bytes a
variable takes up.

See also [LEFT$](LEFT$), [OPEN](OPEN), [data types](data types)

## Details
For a string expression it counts every character, spaces and CHR$(0)
included. For a numeric variable it gives its size: 4 for INTEGER,
LONG and SINGLE, 8 for DOUBLE, 2 for SHORT, 1 for BYTE. For a TYPE
variable it adds up the fields, and a fixed-length string field counts
its declared length. This is the record length to pass to OPEN ... LEN.
A record with a STRING of any length or a dynamic array gives the
bytes it takes in memory, not its data; it cannot be PUT.

## Example
    PRINT LEN("hello")      ' 5
    TYPE Rec
        n AS INTEGER
        nm AS STRING * 10
    END TYPE
    DIM r AS Rec
    PRINT LEN(r)            ' 14
