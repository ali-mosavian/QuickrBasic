# UCASE$ Function
keys: UCASE$

Syntax
    UCASE$(text$)

Returns text$ with its letters a to z in capitals.

See also [LCASE$](LCASE$), [ASC](ASC), [CHR$](CHR$)

## Details

Returns text$ with its letters a to z in capitals. Other characters
are unchanged.

Accented letters are not changed. Compare text without case by
converting both sides: UCASE$(a$) = UCASE$(b$).

## Example
    PRINT UCASE$("Ask me"); "!"         ' ASK ME!
