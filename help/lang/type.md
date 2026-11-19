# TYPE Statement
keys: TYPE, END TYPE

Syntax
    TYPE name
        element[(bounds)] AS type
        ...
    END TYPE

Defines a record: a fixed group of named elements. Declare a variable
with DIM v AS name and reach an element as v.element.

See also [DIM](DIM), [SHORT](SHORT), [BYTE](BYTE), [STRING](STRING),
[data types](data types), [GET](GET), [PUT](PUT)

## Details

Defines a record: a fixed group of named elements. Declare a variable
with DIM v AS name and reach an element as v.element.

type is INTEGER, LONG, SINGLE, DOUBLE, SHORT, BYTE, STRING * n, or
another TYPE. Elements are laid out in order with no padding: a
record holds exactly the sum of its elements' sizes, and that is what
PUT and GET write. INTEGER takes 4 bytes here; use SHORT for a
2-byte field to match files made by QBasic.

TYPE goes at module level, before any use. A STRING element without
* n holds a string of any length, as a variable does: QuickrBasic
adds it. Copying the record copies its strings; PUT and GET refuse a
record with one, its bytes not all its data.

An element with bounds is a fixed array inside the record, as in
QuickBASIC PDS: pts(1 TO 4) AS Pt. The bounds are numbers, the lower
OPTION BASE's when left out. Reach an element as v.pts(i).x; outside
the bounds is Subscript out of range.

Nested records: an element AS another TYPE. Whole-record assignment
(a = b) copies every element, and works between two variables of the
same TYPE. A record cannot be PRINTed as a whole; print its elements.

## Example
    TYPE Pt
        x AS SHORT
        y AS SHORT
    END TYPE
    DIM p AS Pt
    p.x = 3: p.y = 4
    PRINT LEN(p)                        ' 4
