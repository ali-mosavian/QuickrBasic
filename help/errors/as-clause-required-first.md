# AS clause required on first declaration
keys: AS clause required on first declaration, AS clause required, AS missing, error 241

Error 241

## Cause

A name was used before the DIM or parameter that gives it an AS type,
or a declaration lacks its AS type.

## Remedy

Declare with `DIM name AS type` before any other use, then use the
plain name.

See also: [DIM](DIM), [data types](data types)
