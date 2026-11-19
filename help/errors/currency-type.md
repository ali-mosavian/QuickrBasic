# User-defined type not defined
keys: User-defined type not defined, ~User-defined type not defined /CURRENCY, TYPE not defined

An AS clause names a type that no TYPE statement defines.

See also: [TYPE](TYPE), [data types](data types)

## Details

Cause: An AS clause names a type that no TYPE statement defines above
it, or whose definition was refused. CURRENCY is not a type here.

Remedy: Define it with TYPE ... END TYPE above the line. Use DOUBLE or
LONG for money.

CURRENCY is not a type in QuickrBasic.

## Example
    DIM p AS Point      ' error: Point not defined
    TYPE Point
      x AS INTEGER
      y AS INTEGER
    END TYPE
    DIM p AS Point       ' fixed: TYPE above the use
    p.x = 3
    PRINT p.x            ' 3
