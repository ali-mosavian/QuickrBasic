' OPTION DIALECT QUICKR
' io: the console; print needs no import
public native function read_line() as string
' the cursor to a row and a column, each from 1
public native function at(row as int, col as int)
public native function color(fg as int, bg as int = 0)
public native function clear()
