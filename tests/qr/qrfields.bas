' A Quickr record's array fields: a fixed array a fixed one from 0, a list a dynamic one
IMPORT qland
FROM qland IMPORT Land
DIM ld AS Land
qland.fill ld
PRINT ld.sea; ld.heights(3); UBOUND(ld.heights); ld.marks(2); ld.names(0); UBOUND(ld.names)
ld.heights(7) = 10
REDIM PRESERVE ld.names(0 TO 1): ld.names(1) = "vale"
PRINT qland.total(ld)
DIM copy AS Land
copy = ld
PRINT copy.names(1); copy.heights(7)
