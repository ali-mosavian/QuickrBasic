# Array Assignment
keys: array assignment, a() = b(), a(:) = b(:)

Syntax
    array() = array()
    array(:) = array(:)
    array() = function(arguments)
    record.array() = array()

An array assignment gives the array on the left the bounds and
elements of the one on the right. QuickrBasic adds it to QBasic.

See also [DIM](DIM), [REDIM](REDIM), [ERASE](ERASE),
[FUNCTION](FUNCTION)

## Details
a() = b() moves: a takes b's array, no element copied, and b is left
empty, as ERASE leaves it. a(:) = b(:) copies: changing one later
leaves the other as it was. Either way the left array's old elements
are freed first, and both hold the same type.

From a FUNCTION whose result is an array, a() = f(..) takes the
result over.

Both sides are written alike: a() = b(:) is a Syntax error. A
record's dynamic array element written alone, v.pts = w.pts, copies.

The left array must be dynamic: one made by DIM name(), REDIM or an
earlier assignment. One DIM gave bounds keeps them: assigning to it
is Duplicate definition. A record's dynamic array element, such as
v.pts(), is an array on either side; see [TYPE](TYPE).

## Example
    DIM a() AS LONG, b() AS LONG, c() AS LONG
    REDIM a(1 TO 3)
    a(2) = 5
    b(:) = a(:)
    a(2) = 9
    PRINT b(2); UBOUND(b)     ' 5  3
    c() = a()
    PRINT c(2)                ' 9; a() is empty now
