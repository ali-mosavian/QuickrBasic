# STRING Keyword
keys: STRING

Syntax
    DIM var AS STRING
    DIM var AS STRING * length
    name$

The text type. A variable-length string (suffix $) grows as needed;
its size is limited only by memory.

STRING * length is a fixed-length string, always exactly length
characters. Assigning shorter text pads it with blanks, longer text
is cut. It can be an element of a TYPE, and it is stored in a file
as length bytes.

## Example
    TYPE Rec
        id AS STRING * 4
    END TYPE
    DIM r AS Rec
    r.id = "AB"
    PRINT "[" + r.id + "]"              ' [AB  ]

See also [data types](data types), [STRING$](STRING$),
[LSET](LSET), [TYPE](TYPE)
