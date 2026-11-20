# TYPE Statement
keys: TYPE, END TYPE

Syntax
    TYPE name
        element[(bounds)] AS type
        element() AS type
        ...
    END TYPE

Defines a record: a group of named elements. Declare a variable with
DIM v AS name and reach an element as v.element.

See also [DIM](DIM), [SHORT](SHORT), [BYTE](BYTE), [STRING](STRING),
[data types](data types), [records as values](records as values),
[GET](GET), [PUT](PUT)

## Details

Defines a record: a group of named elements. Declare a variable with
DIM v AS name and reach an element as v.element.

type is INTEGER, LONG, SINGLE, DOUBLE, SHORT, BYTE, STRING,
STRING * n, or another TYPE. Elements are laid out in order with no
padding: a record of numbers and STRING * n holds exactly the sum of
its elements' sizes, and that is what PUT and GET write. INTEGER
takes 4 bytes here; use SHORT for a 2-byte field to match files made
by QBasic.

TYPE goes at module level, before any use. A STRING element without
* n holds a string of any length, as a variable does: QuickrBasic
adds it.

An element with bounds is a fixed array inside the record, as in
QuickBASIC PDS: pts(1 TO 4) AS Pt. The bounds are numbers, the lower
OPTION BASE's when left out. Reach an element as v.pts(i).x; outside
the bounds is Subscript out of range. LBOUND, UBOUND, ERASE and the
graphics GET and PUT take v.pts whole, in QuickrBasic.

An element with empty parentheses is a dynamic array, QuickrBasic's:
pts() AS Pt. It is empty until REDIM v.pts(1 TO n), and grows with
REDIM PRESERVE. LBOUND, UBOUND, ERASE and a() = v.pts() take it as
any array; a TYPE may hold an array of itself.

A record with a STRING of any length or a dynamic array owns them:
copying it copies them, and they are freed with it. PUT and GET
refuse it. See [records as values](records as values).

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

    TYPE Path
        pts() AS Pt
    END TYPE
    DIM w AS Path
    REDIM w.pts(1 TO 10)
    w.pts(10).x = 5
    PRINT UBOUND(w.pts)                 ' 10
