# LSET Statement
keys: LSET

Syntax
    LSET stringvariable = stringexpression

Copies stringexpression into stringvariable, left-justified, without
changing the variable's length. A longer value is cut at the right;
a shorter one leaves the rest padded with spaces.

Used mostly with FIELD buffers and fixed-length strings of random
files: a FIELD variable assigned with = stops being part of the
buffer, LSET keeps it there. To put numbers into such a buffer use
MKI$, MKL$, MKS$ or MKD$.

## Example
    DIM a AS STRING * 8
    LSET a = "ab"           ' "ab      "
    LSET a = "abcdefghij"   ' "abcdefgh"

See also [RSET](RSET), [FIELD](FIELD), [MKI$](MKI$), [MID$](MID$)
