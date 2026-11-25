' A Quickr module imported before a QBasic one is stored after it.
' The QBasic one's top was taken for the Quickr one's: Out of DATA.
IMPORT q
IMPORT d
PRINT "main"; q.twice(4)
d.readit
READ x
PRINT x
DATA 99
