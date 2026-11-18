# Array too big / TYPE more than 65535 bytes
keys: Array too big, TYPE more than 65535 bytes

An array or record type exceeds the compiler's size limit.

See also: [DIM](DIM), [TYPE](TYPE)

## Details

Cause: The array, or the record type, is larger than the compiler's
size limit for it.

Remedy: Use fewer elements, a smaller element type, or shorter fields.

A TYPE may hold at most 65535 bytes. Elements times element size must
fit the limit for one array.

## Example
    DIM a(32767, 32767) AS DOUBLE   ' too big
    DIM a(1000) AS DOUBLE            ' fixed
