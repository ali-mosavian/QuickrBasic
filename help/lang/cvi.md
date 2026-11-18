# CVI, CVL, CVS, CVD, CVSMBF, CVDMBF Functions
keys: CVI, CVL, CVS, CVD, CVSMBF, CVDMBF

Syntax
    CVI(s$)    CVL(s$)    CVS(s$)    CVD(s$)
    CVSMBF(s$) CVDMBF(s$)

Turn a string of bytes into a number: the reverse of the MKI$ group,
used to read values back from a random-access or binary file.

See also [MKI$](MKI$), [MKS$](MKS$), [MKSMBF$](MKSMBF$),
[FIELD](FIELD), [GET](GET)

## Details
Turn a string of bytes into a number: the reverse of the MKI$ group,
used to read values back from a random-access or binary file.

    CVI, CVL   whole number, from 4 bytes
    CVS        SINGLE, from 4 bytes
    CVD        DOUBLE, from 8 bytes
    CVSMBF     SINGLE, from 4 bytes in Microsoft Binary format
    CVDMBF     DOUBLE, from 8 bytes in Microsoft Binary format

INTEGER and LONG are one 32-bit type here, so CVI and CVL both read
4 bytes. A string shorter than needed gives Illegal function call.
For QBasic's 2-byte values read a SHORT with GET instead.

The MBF forms read numbers saved by old BASIC programs.

## Example
    n$ = MKL$(123456)
    PRINT CVL(n$)       ' 123456
