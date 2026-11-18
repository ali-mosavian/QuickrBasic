# String expression errors
keys: String assignment required, String expression required

A string is needed but the value is a number.

See also: [STR$](STR$), [CHR$](CHR$)

## Details

Cause: A string is needed here but the value is a number.

Remedy: Convert it with [STR$](STR$) or [CHR$](CHR$), or use a string
variable.

Convert numbers with STR$ or CHR$.

## Example
    a$ = 5              ' error
    a$ = STR$(5)         ' " 5"
