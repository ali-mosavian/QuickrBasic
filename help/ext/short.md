# SHORT Data Type
keys: SHORT, AS SHORT

Syntax
    DIM name AS SHORT
    ... AS SHORT           (TYPE fields, parameters, arrays)

A SHORT is a 16-bit integer, -32768 to 32767: what INTEGER means in
QBasic.

See also [BYTE](BYTE), [data types](data types), [TYPE](TYPE)

## Details
In QuickrBasic INTEGER is 32-bit (and the same type as LONG), so SHORT
exists for data laid out as QBasic laid it out.

Rules
 - No type-suffix character: use AS SHORT, or [DEFSHORT](DEFSHORT).
 - A SHORT takes 2 bytes in a TYPE, a file and an array. A TYPE
   built from SHORTs matches a QBasic record byte for byte, so old
   data files read correctly.
 - Storing a value outside 16 bits is an Overflow error. So is
   arithmetic on SHORTs (+ - * \ MOD) whose result does not fit.
 - HEX$ and OCT$ of a SHORT show its 16 bits.
 - A fraction is rounded, as CINT does.

## Example
    TYPE Rec
      id AS SHORT
      score AS SHORT
    END TYPE
    DIM r AS Rec
    r.score = 32767
    PRINT LEN(r)           ' 4
    r.score = r.score + 1  ' Overflow
