' OPTION DIALECT QUICKR
' io: the console; print needs no import
native function read_line() as string
' the cursor to a row and a column, each from 1
native function at(row as int, col as int)
native function color(fg as int, bg as int = 0)
native function clear()
