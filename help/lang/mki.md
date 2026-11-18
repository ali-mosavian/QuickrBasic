# MKI$, MKL$, MKS$, MKD$ Functions
keys: MKI$, MKL$, MKS$, MKD$, MKSMBF$, MKDMBF$

Syntax
    MKI$(integer)      2 characters
    MKL$(long)         4 characters
    MKS$(single)       4 characters
    MKD$(double)       8 characters
    MKSMBF$(single)    4 characters
    MKDMBF$(double)    8 characters

Pack a number into the string of bytes it occupies in memory, to be
read back with CVI, CVL, CVS, CVD, CVSMBF and CVDMBF.

See also [CVI](CVI), [LSET](LSET), [FIELD](FIELD), [INTEGER](INTEGER)

## Details
The string can be put in a FIELD buffer or any string and later read
back with the CV functions.

MKSMBF$ and MKDMBF$ use the Microsoft Binary Format of old QuickBASIC
files, MKS$ and MKD$ the IEEE format.

MKI$ writes 16 bits: with a 32-bit INTEGER, a value outside
-32768 to 32767 is an Overflow. Use MKL$ for those.

## Example
    a$ = MKL$(100000)
    PRINT LEN(a$), CVL(a$)      ' 4  100000
