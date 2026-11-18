# AS clause required on first declaration
keys: AS clause required on first declaration, AS clause required, AS missing, error 241

Error 241

A name was used before the declaration that gives it its AS type.

See also: [DIM](DIM), [data types](data types)

## Details

Cause: A name was used before the DIM or parameter that gives it an AS
type, or a declaration lacks its AS type.

Remedy: Declare with `DIM name AS type` before any other use, then use
the plain name.

Once a name has an AS type, later uses write the plain name without a
suffix or a second AS.

## Example
    n% = 1
    DIM n AS LONG        ' error 241
    DIM total AS LONG    ' fix: declare first
    total = 70000
    PRINT total          ' 70000
