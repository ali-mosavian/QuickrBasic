# GET and PUT Statements (files)
keys: GET, PUT, GET #, PUT #

Syntax
    GET [#]filenumber[, [position][, variable]]
    PUT [#]filenumber[, [position][, variable]]

GET reads, and PUT writes, one record of a RANDOM file or a run of
bytes of a BINARY file.

See also [OPEN](OPEN), [FIELD](FIELD), [SEEK](SEEK), [LOF](LOF)

## Details
GET reads, and PUT writes, one record of a RANDOM file or a run of
bytes of a BINARY file. Position is the record number (from 1) for
RANDOM, or the byte (from 1) for BINARY; leave it out to continue
after the last access.

With a variable, the data is moved to or from it directly:
    - a numeric variable moves its own size;
    - a fixed-length string moves its length, a variable-length
      string in BINARY mode moves its current length;
    - a TYPE record moves the whole record. One with a STRING of
      any length or a dynamic array is a Type mismatch: its bytes
      are not all its data.
Without one, RANDOM files use the FIELD buffer.

A RANDOM record is the LEN given in OPEN; a variable longer than
that is a Bad record length. GET past the end gives zeros or
Input past end of file. Fields in a TYPE are laid out as QBasic's
are, so DOS files interchange; a BYTE takes 1 byte, a SHORT 2.
For the graphics versions, see [GET and PUT (graphics)](GET graphics).

## Example
    TYPE Rec: id AS LONG: nm AS STRING * 12: END TYPE
    DIM r AS Rec
    OPEN "r.dat" FOR RANDOM AS #1 LEN = LEN(r)
    r.id = 7: r.nm = "Ann": PUT #1, 3, r
    GET #1, 3, r: PRINT r.nm      ' Ann
