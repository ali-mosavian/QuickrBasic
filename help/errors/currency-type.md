# User-defined type not defined
keys: User-defined type not defined, ~User-defined type not defined /CURRENCY, TYPE not defined

## Cause

An AS clause names a type that no TYPE statement defines above it, or
whose definition was refused. CURRENCY is not a type here.

## Remedy

Define it with TYPE ... END TYPE above the line. Use DOUBLE or LONG
for money.

See also: [TYPE](TYPE), [data types](data types)
