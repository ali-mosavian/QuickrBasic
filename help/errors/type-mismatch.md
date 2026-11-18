# Type mismatch
keys: Type mismatch, error 13

Error 13

## Cause

A value has the wrong type for its place: a string where a number is
needed or the reverse, or a record used as a value. READ or INPUT
meeting text for a numeric variable gives it at run time.

## Remedy

Convert with [VAL](VAL), [STR$](STR$), [CINT](CINT) and friends, or
use a variable of the right type. Use a field of a record, not the
record.

See also: [data types](data types), [VAL](VAL), [STR$](STR$)
