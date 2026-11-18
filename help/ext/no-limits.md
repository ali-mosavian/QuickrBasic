# No Limits: Memory and Integers
keys: no limits, memory, 64 KB, 64K, limits, 32-bit INTEGER

QBasic keeps programs, strings and arrays in 64 KB segments.
QuickrBasic has none: a program takes free memory until it runs out,
then the error is Out of memory. INTEGER is 32 bits.

See also [data types](data types), [QuickrBasic](QuickrBasic)

## Details
What grows
 - the program: its length, its lines (any length) and its symbols
 - arrays: the number of elements and of dimensions; no 64 KB array
 - the number of strings and the total string space
 - the editor's buffers, the Open box's file list, completion's names
 - the code of a procedure, however large the module

Programs written for the 64 KB model run unchanged; none of the old
limits is a rule they have to keep.

What stays
A single string still holds at most 32767 characters. A longer result
is an error.

INTEGER is 32 bits
INTEGER and LONG are one type here, in the machine's own size:
-2147483648 to 2147483647. So 32767 + 1 is 32768, not an Overflow;
only a result beyond 32 bits overflows.

Consequences for old programs:
 - A TYPE field AS INTEGER takes 4 bytes. To keep QBasic's 2-byte
   layout in records and files, declare the field AS [SHORT](SHORT).
 - &HFFFF is 65535, not -1; HEX$(-1) is FFFFFFFF.
 - MKI$ and CVI still use 2 bytes; MKL$ and CVL use 4.
 - SINGLE, DOUBLE and STRING are as in QBasic.

For one-byte values use [BYTE](BYTE).

## Example
    DIM big(1 TO 2000000) AS LONG   ' fine: 8 MB
    x% = 32767 + 1
    PRINT x%, UBOUND(big)
    ' 32768         2000000
