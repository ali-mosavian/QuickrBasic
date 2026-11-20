# Records as Values
keys: records as values, dynamic arrays in a TYPE

Syntax
    TYPE name
        element AS STRING
        element(bounds) AS type
        element() AS type
    END TYPE

A record may hold strings of any length and arrays, fixed or
dynamic. It is a value: assignment copies it, and its strings and
arrays are freed with it. QuickrBasic adds this to QBasic.

See also [TYPE](TYPE), [REDIM](REDIM), [FUNCTION](FUNCTION),
[array assignment](array assignment)

## Details
b = a copies a record and everything in it: changing b later leaves
a as it was. A record's strings and arrays live as long as it does;
a procedure's local record frees them when the procedure returns.

A dynamic array element, element() AS type, is empty until
REDIM v.element(bounds). REDIM PRESERVE, ERASE, LBOUND, UBOUND and
array assignment take it as any array, and v.element() passes it to
a SUB's array parameter. Written alone, v.element = w.element copies
it. A TYPE may hold an array of itself.

A FUNCTION AS name returns a record: v = f(1) copies the result in.

Such a record's bytes are not all its data: PUT and GET refuse it
with Type mismatch, and LEN gives the bytes it takes in memory. A
TYPE of numbers and STRING * n keeps QBasic's layout.

## Example
    TYPE Node
        label AS STRING
        kids() AS Node
    END TYPE
    DIM t AS Node, c AS Node
    REDIM t.kids(1 TO 2)
    t.kids(2).label = "leaf"
    c = t
    c.kids(2).label = "copy"
    PRINT t.kids(2).label; UBOUND(c.kids)   ' leaf 2
