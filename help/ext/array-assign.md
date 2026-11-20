# Array Assignment
keys: array assignment, a() = b()

Syntax
    array() = array()
    array() = function(arguments)
    record.array() = array()

An array assignment gives the array on the left the bounds and
elements of the one on the right. QuickrBasic adds it to QBasic.

See also [DIM](DIM), [REDIM](REDIM), [ERASE](ERASE),
[FUNCTION](FUNCTION)

## Details
The left array gets its own copy: changing one later leaves the
other as it was. Its old elements are freed first. Both arrays hold
the same type.

From a FUNCTION whose result is an array, the result is taken over,
not copied.

The left array must be dynamic: one made by DIM name(), REDIM or an
earlier assignment. One DIM gave bounds keeps them: assigning to it
is Duplicate definition. A record's dynamic array element, such as
v.pts(), is an array on either side; see [TYPE](TYPE).

## Example
    DIM a() AS LONG, b() AS LONG
    REDIM a(1 TO 3)
    a(2) = 5
    b() = a()
    a(2) = 9
    PRINT b(2); UBOUND(b)     ' 5  3
