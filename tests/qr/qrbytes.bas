' QBasic uses a Quickr record of ubyte fields and calls Quickr
' functions taking ubyte lists with its BYTE arrays.
IMPORT ql
DIM x AS ql.L
x.a(3) = 200: x.one = 255: x.sea = 9
REDIM x.g(1): x.g(1) = 7
DIM b(3) AS BYTE
b(0) = 250: b(3) = 5
PRINT ql.sum(b()); ql.sum(x.a()); ql.sum(x.g())
ql.bump b()
PRINT UBOUND(b); b(4); x.one; x.sea
