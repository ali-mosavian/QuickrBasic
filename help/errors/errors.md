# Error Messages
keys: error messages, errors

Press F1 in an error box to reach its topic, or pick one in the
Details.

See also: [error codes](error codes), [survival](survival)

## Details

Press F1 in an error box to reach its topic. Pick a message:

  [NEXT without FOR / FOR without NEXT](NEXT without FOR)
  [WHILE without WEND / WEND without WHILE](WHILE without WEND)
  [DO without LOOP / LOOP without DO](DO without LOOP)
  [Block IF / END IF / ELSE errors](Block IF without END IF)
  [SELECT CASE block errors](SELECT without END SELECT)
  [SUB/FUNCTION without END SUB/FUNCTION](SUB/FUNCTION without END SUB/FUNCTION)
  [EXIT not within matching block](EXIT not within matching block)
  [Blocks crossed](Blocks crossed)
  [Syntax error](Syntax error)
  [RETURN without GOSUB](RETURN without GOSUB)
  [Out of DATA](Out of DATA)
  [Illegal function call](Illegal function call)
  [Overflow](Overflow)
  [Out of memory](Out of memory)
  [Label not defined](Label not defined)
  [Subscript out of range](Subscript out of range)
  [Duplicate definition](Duplicate definition)
  [Division by zero](Division by zero)
  [Illegal in direct mode](Illegal in direct mode)
  [Type mismatch](Type mismatch)
  [Out of string space](Out of string space)
  [Cannot continue](Cannot continue)
  [Function not defined](Function not defined)
  [No RESUME / RESUME without error](No RESUME)
  [Device timeout / Device fault / Out of paper](Device timeout)
  [Duplicate label](Duplicate label)
  [Subprogram not defined](Subprogram not defined)
  [Argument-count mismatch](Argument-count mismatch)
  [Array not defined](Array not defined)
  [Variable required](Variable required)
  [FIELD overflow / FIELD statement active](FIELD overflow)
  [Internal error](Internal error)
  [Bad file name or number / Bad file name](Bad file name or number)
  [File not found / Path not found](File not found)
  [Bad file mode](Bad file mode)
  [File already open](File already open)
  [Device I/O error / Disk full / Disk not ready / Disk-media error](Device I/O error)
  [File already exists](File already exists)
  [Bad record length / Bad record number](Bad record length)
  [Input past end of file](Input past end of file)
  [Too many files](Too many files)
  [Communication-buffer overflow](Communication-buffer overflow)
  [Permission denied](Permission denied)
  [Advanced feature unavailable](Advanced feature unavailable)
  [Rename across disks](Rename across disks)
  [Out of stack space](Out of stack space)
  [Variable not declared](Variable not declared)
  [Parameter type mismatch](Parameter type mismatch)
  [Subprogram error](Subprogram error)
  [AS clause required on first declaration](AS clause required on first declaration)
  [Array already dimensioned](Array already dimensioned)
  [DECLARE required / Equal sign missing](DECLARE required)
  [COMMON and DECLARE must precede executable statements](COMMON and DECLARE must precede executable statements)
  [Array too big / TYPE more than 65535 bytes](Array too big)
  [Name not exported](Name not exported)
  [Dimension errors](Dynamic array illegal)
  [Asterisk missing / Integer between 1 and 32767 required](Asterisk missing)
  [FOR index variable errors](FOR index variable already in use)
  [Formal parameter errors](Formal parameter specification illegal)
  [User-defined type not defined](User-defined type not defined)
  [Identifier errors](Identifier expected)
  [Illegal inside / outside of SUB or FUNCTION](Illegal inside SUB or FUNCTION)
  [Numeric constant errors](Illegal type character in numeric constant)
  [unterminated string](unterminated string)
  [Invalid constant / Feature unavailable](Invalid constant)
  [GOTO missing / Left parenthesis missing](GOTO missing)
  [Record and element errors](Element not defined)
  [String expression errors](String assignment required)
  [Statement unrecognizable](Statement unrecognizable)
  [$Metacommand Error]($Metacommand Error)
  [QuickrBASIC extensions are not in this parser](QuickrBASIC extensions are not in this parser)

Run-time errors can be trapped with ON ERROR; ERR returns the number.

## Example
    ON ERROR GOTO h
    OPEN "NOPE.TXT" FOR INPUT AS #1
    END
    h:
    PRINT "error"; ERR   ' error 53
    RESUME NEXT
