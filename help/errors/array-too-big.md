# Array too big / TYPE more than 65535 bytes
keys: Array too big, TYPE more than 65535 bytes, error 238

QBasic's: a record type is larger than 65535 bytes. QuickrBasic
raises neither.

See also: [DIM](DIM), [TYPE](TYPE), [Out of memory](Out of memory)

## Details

Cause: In QBasic, a TYPE holds at most 65535 bytes. In QuickrBasic a
TYPE holds up to 16 MB, and one larger is Out of memory when the
program is checked. An array has no limit but memory; one larger is
Out of memory when its DIM runs.

Remedy: Use fewer or smaller fields. A dynamic array element takes a
few bytes in the record, however many elements it holds.

## Example
    TYPE Big
        w(1 TO 70000) AS BYTE     ' fine here; QBasic refuses it
    END TYPE
    TYPE Fits
        w() AS BYTE               ' REDIM v.w(1 TO 20000000)
    END TYPE
