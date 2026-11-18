# Overflow
keys: Overflow, error 6

Error 6

## Cause

A result does not fit its type: an integer past its range, a BYTE
outside 0..255, a number too large for [CINT](CINT), or a floating-
point result too big to hold.

## Remedy

Use a wider type (LONG, DOUBLE), or check the value before converting.

See also: [data types](data types), [CINT](CINT)
