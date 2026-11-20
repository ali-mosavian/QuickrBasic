# Array too big / TYPE more than 65535 bytes
keys: Array too big, TYPE more than 65535 bytes, error 238

A record type is larger than 65535 bytes.

See also: [DIM](DIM), [TYPE](TYPE), [Out of memory](Out of memory)

## Details

Cause: A TYPE holds more than 65535 bytes: TYPE more than 65535
bytes, error 238, when the program is checked. An array has no such
limit in QuickrBasic; one larger than memory is Out of memory when
its DIM runs.

Remedy: Use fewer or smaller fields. A dynamic array element takes a
few bytes in the record, however many elements it holds.

## Example
    TYPE Big
        w(1 TO 70000) AS BYTE    ' TYPE more than 65535 bytes
    END TYPE
    TYPE Fits
        w() AS BYTE              ' REDIM v.w(1 TO 70000)
    END TYPE
