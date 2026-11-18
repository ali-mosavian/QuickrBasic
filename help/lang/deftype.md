# DEFtype Statements
keys: DEFINT, DEFLNG, DEFSNG, DEFDBL, DEFSTR, DEFtype

Syntax
    DEFINT  letterrange[, letterrange]...
    DEFLNG  letterrange[, letterrange]...
    DEFSNG  letterrange[, letterrange]...
    DEFDBL  letterrange[, letterrange]...
    DEFSTR  letterrange[, letterrange]...

A DEF statement sets the default type of every variable, array and
function whose name starts with a letter in the range and has no
suffix and no AS clause. A range is a letter (A) or two joined by a
hyphen (A-F); case does not matter.

See also [data types](data types)

## Details
A DEF statement sets the default type of every variable, array and
function whose name starts with a letter in the range and has no
suffix and no AS clause. A range is a letter (A) or two joined by a
hyphen (A-F); case does not matter.

    DEFINT   integer          DEFSNG   single
    DEFLNG   long             DEFDBL   double
    DEFSTR   string

Without any DEF, names are SINGLE. An explicit suffix or AS type
always wins over the default. The statement acts from its line down,
for the rest of the module. DEFINT and DEFLNG make the same 32-bit
type here.

QuickrBasic adds [DEFBYTE](DEFBYTE) and DEFSHORT, for the unsigned
BYTE and 16-bit [SHORT](SHORT) types.

## Example
    DEFINT I-N
    DEFSTR S
    i = 7: s = "seven"
    PRINT i; s               ' 7 seven
