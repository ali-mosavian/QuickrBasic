# DIM Statement
keys: DIM, DIM SHARED

Syntax
    DIM [SHARED] variable[(subscripts)] [AS type][, ...]

DIM declares variables and arrays. Subscripts are
[lower TO] upper per dimension, up to 60 dimensions. Without TO the
lower bound is 0, or the value set by OPTION BASE.

See also [REDIM](REDIM), [ERASE](ERASE), [LBOUND](LBOUND),
[UBOUND](UBOUND), [TYPE](TYPE), [data types](data types)

## Details
DIM declares variables and arrays. Subscripts are
[lower TO] upper per dimension, up to 60 dimensions. Without TO the
lower bound is 0, or the value set by OPTION BASE.

    DIM a(10)              11 elements, 0 to 10
    DIM b(1 TO 3, 1 TO 4)  a 3 by 4 table
    DIM n AS LONG, s AS STRING * 8

AS type is INTEGER, LONG, SHORT, BYTE, SINGLE, DOUBLE, STRING,
STRING * n, or a name made with TYPE. New numbers are 0, strings are
empty, record fields follow their types.

SHARED makes the names visible in every SUB and FUNCTION of the
module, and is allowed only in module-level code. Array bounds may
be expressions here when the array is dynamic: that is, when no
$STATIC metacommand is in force and the bounds are not constants.
DIM of a name already in use is Duplicate definition. A dynamic
array can be resized with [REDIM](REDIM) and dropped with
[ERASE](ERASE). Arrays are limited only by memory, not 64 KB.

QuickrBasic adds DIM name() [AS type]: a dynamic array with no
elements until [REDIM](REDIM) or an [array assignment](array assignment).

## Example
    DIM SHARED total AS DOUBLE
    DIM grid(1 TO 8, 1 TO 8) AS INTEGER
    grid(8, 8) = 5
    PRINT grid(8, 8), total  ' 5  0
