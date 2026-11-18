# QuickrBASIC extensions are not in this parser
keys: QuickrBASIC extensions are not in this parser

The line uses a QuickrBasic addition under the QuickBASIC 4.5 dialect.

See also: [BYTE](BYTE), [SHORT](SHORT)

## Details

Cause: The line uses a QuickrBasic addition while compiling as plain
QuickBASIC 4.5.

Remedy: Compile it with `--dialect quickr`.

Additions such as BYTE and SHORT are only parsed with the quickr
dialect.

## Example
    ' compile with: --dialect quickr
    DIM b AS BYTE
    b = 200
    PRINT b              ' 200
