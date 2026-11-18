# Overflow
keys: Overflow, error 6

Error 6

A result does not fit its type.

See also: [data types](data types), [CINT](CINT)

## Details

Cause: A result does not fit its type: an integer past its range, a
BYTE outside 0..255, a number too large for [CINT](CINT), or a
floating- point result too big to hold.

Remedy: Use a wider type (LONG, DOUBLE), or check the value before
converting.

Ranges: INTEGER -32768..32767, LONG about +-2.1 billion, BYTE 0..255.

Trap it with ON ERROR; ERR returns the number.

## Example
    x% = 32767
    x% = x% + 1          ' error 6
    x& = 32767&
    x& = x& + 1          ' fixed: 32768
