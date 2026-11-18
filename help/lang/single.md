# SINGLE Keyword
keys: SINGLE

Syntax
    DIM var AS SINGLE
    DEFSNG letterrange
    name!

The single-precision floating-point type: 4 bytes, about 7 digits,
magnitudes from about 1.4E-45 to 3.4E+38. It is the default type of
a variable with no suffix, no AS clause and no DEF statement.

See also [data types](data types), [DOUBLE](DOUBLE), [CSNG](CSNG),
[DEFINT](DEFINT)

## Details

The single-precision floating-point type: 4 bytes, about 7 digits,
magnitudes from about 1.4E-45 to 3.4E+38. It is the default type of
a variable with no suffix, no AS clause and no DEF statement.

Its suffix is !. A constant such as 1.5 or 2E3 is SINGLE unless it
has more than 7 digits or a D exponent.

Single precision shows up to 7 digits; PRINT 1 / 3 gives .3333333.
Use DOUBLE for 15 or more. DEFSNG makes names starting with the given
letters SINGLE, which is already the default.

## Example
    DIM r AS SINGLE
    r = 1 / 3
    PRINT r                             ' .3333333
