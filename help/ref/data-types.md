# Data Types
keys: data types, data type, type suffix, type characters, numeric types

Every variable, constant and function result has a type. It is set by
a suffix character, by `AS type`, or by a DEF statement.

```
Type      Suffix  AS      Holds                       Size
BYTE      none    BYTE    0 to 255                    1
SHORT     none    SHORT   -32768 to 32767             2
INTEGER   %       INTEGER -2147483648 to 2147483647   4
LONG      &       LONG    same as INTEGER             4
SINGLE    !       SINGLE  +-3.4E38, 7 digits          4
DOUBLE    #       DOUBLE  +-1.8E308, 15-16 digits     8
STRING    $       STRING  text                        varies
```

See also [Operators](operators), [Expressions](expressions),
[DIM](DIM), [Size limits](size limits)

## Details
INTEGER and LONG are one type
In QuickrBasic INTEGER is 32 bits, the same type as LONG. Code that
relied on QBasic's 16-bit wrap or Overflow at 32767 behaves
differently: 32767 + 1 is simply 32768.

Use [SHORT](SHORT) where the 16 bits matter, for example in a TYPE
that must match a file written by QBasic. Use [BYTE](BYTE) for 0 to
255; a result outside that range is an Overflow error. BYTE and SHORT
have no suffix character; declare them with AS, or with
[DEFBYTE](DEFBYTE) and DEFSHORT. [CBYTE](CBYTE) converts to BYTE.

Strings
A variable-length string grows as needed. A fixed-length string is
declared `STRING * n`, n from 1 to 32767, and is padded with spaces
or cut to fit.

Default type
A name with no suffix and no declaration is SINGLE. A DEFINT, DEFLNG,
DEFSNG, DEFDBL or DEFSTR statement changes the default by first
letter. See [DEFtype](DEFINT).

User-defined types
TYPE ... END TYPE groups fields of any type into a record. A BYTE
field takes 1 byte and a SHORT field 2 bytes, in memory and in a file.

Conversion
Numbers convert to each other when assigned. A float becomes an
integer by rounding. A string never converts to a number or back
without a function: [VAL](VAL), [STR$](STR$), [CINT](CINT),
[CLNG](CLNG), [CSNG](CSNG), [CDBL](CDBL), [CBYTE](CBYTE).
A string and a number in one expression is a
[Type mismatch](Type mismatch).

Constants
```
255   &HFF   &O377   (decimal, hex, octal)
3.5E3  3.5D3          (SINGLE, DOUBLE exponent)
"text"                (string)
```

## Example
    x% = 32767 + 1
    n$ = "12"
    PRINT x%, VAL(n$) + 1, 7 / 2, 7 \ 2
    ' 32768        13            3.5           3
