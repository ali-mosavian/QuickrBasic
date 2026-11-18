# STRING Keyword
keys: STRING

Syntax
    DIM var AS STRING
    DIM var AS STRING * length
    name$

The text type. A variable-length string (suffix $) grows as needed;
its size is limited only by memory.

See also [data types](data types), [STRING$](STRING$),
[LSET](LSET), [TYPE](TYPE)

## Details

The text type. A variable-length string (suffix $) grows as needed;
its size is limited only by memory.

STRING * length is a fixed-length string, always exactly length
characters. Assigning shorter text pads it with blanks, longer text
is cut. It can be an element of a TYPE, and it is stored in a file
as length bytes.

A fixed-length string compared with a variable-length one compares the
text with the blanks included; RTRIM$ it first to ignore them. Use
variable-length strings unless a record layout needs a fixed size.

## Example
    TYPE Rec
        id AS STRING * 4
    END TYPE
    DIM r AS Rec
    r.id = "AB"
    PRINT "[" + r.id + "]"              ' [AB  ]
