# Error Codes
keys: error codes, error numbers, run-time errors, trappable errors, ERR codes

A run-time error stops the program with a message, unless
[ON ERROR](ON ERROR) catches it. [ERR](ERR) then holds the number
below and [ERL](ERL) the line. [ERROR](ERROR) n raises an error by
number. Follow a message's link for its cause and remedy.

See also [Message boxes](message boxes), [ON ERROR](ON ERROR),
[RESUME](RESUME), [Survival guide](survival)

## Details
1  [NEXT without FOR](NEXT without FOR)
  2  [Syntax error](Syntax error)
  3  [RETURN without GOSUB](RETURN without GOSUB)
  4  [Out of DATA](Out of DATA)
  5  [Illegal function call](Illegal function call)
  6  [Overflow](Overflow)
  7  [Out of memory](Out of memory)
  8  [Label not defined](Label not defined)
  9  [Subscript out of range](Subscript out of range)
 10  [Duplicate definition](Duplicate definition)
 11  [Division by zero](Division by zero)
 12  [Illegal in direct mode](Illegal in direct mode)
 13  [Type mismatch](Type mismatch)
 14  [Out of string space](Out of string space)
 16  [String formula too complex](String formula too complex)
 17  [Cannot continue](Cannot continue)
 18  [Function not defined](Function not defined)
 19  [No RESUME](No RESUME)
 20  [RESUME without error](RESUME without error)
 24  [Device timeout](Device timeout)
 25  [Device fault](Device fault)
 26  [FOR without NEXT](FOR without NEXT)
 27  [Out of paper](Out of paper)
 29  [WHILE without WEND](WHILE without WEND)
 30  [WEND without WHILE](WEND without WHILE)
 33  [Duplicate label](Duplicate label)
 35  [Subprogram not defined](Subprogram not defined)
 37  [Argument-count mismatch](Argument-count mismatch)
 38  [Array not defined](Array not defined)
 40  [Variable required](Variable required)
 50  [FIELD overflow](FIELD overflow)
 51  [Internal error](Internal error)
 52  [Bad file name or number](Bad file name or number)
 53  [File not found](File not found)
 54  [Bad file mode](Bad file mode)
 55  [File already open](File already open)
 56  [FIELD statement active](FIELD statement active)
 57  [Device I/O error](Device I/O error)
 58  [File already exists](File already exists)
 59  [Bad record length](Bad record length)
 61  [Disk full](Disk full)
 62  [Input past end of file](Input past end of file)
 63  [Bad record number](Bad record number)
 64  [Bad file name](Bad file name)
 67  [Too many files](Too many files)
 68  [Device unavailable](Device unavailable)
 70  [Permission denied](Permission denied)
 71  [Disk not ready](Disk not ready)
 72  [Disk-media error](Disk-media error)
 73  [Advanced feature unavailable](Advanced feature unavailable)
 74  [Rename across disks](Rename across disks)
 75  [Path/File access error](Path/File access error)
 76  [Path not found](Path not found)

Notes
- A number not in the list gives "Unprintable error".
- Messages for mistakes in the program text, such as "Block IF
  without END IF", are found as the line is entered and show in a
  message box.
- Error 51 means QuickrBasic itself failed; please report it.

## Example
    ON ERROR GOTO oops
    PRINT 1 / 0
    PRINT "after"
    END
    oops:
    PRINT ERR              ' 11: Division by zero
    RESUME NEXT
    ' 11
    ' after
