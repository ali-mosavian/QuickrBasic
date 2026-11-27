' The dialects cross three times on an import path: QBasic main,
' Quickr qtop, QBasic cmid, Quickr qleaf; qleaf's record is one
' type in all of them.
IMPORT qtop
IMPORT qleaf
DIM c AS qleaf.Cell
c.v = 1
PRINT qtop.b(c)
