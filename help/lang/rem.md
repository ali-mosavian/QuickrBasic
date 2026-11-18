# REM Statement
keys: REM, ', comment

Syntax
    REM remark
    ' remark

Marks the rest of the line as a comment. The program ignores it.
An apostrophe may follow a statement on the same line. REM there must
be set apart with a colon: PRINT x : REM show x.

Neither starts a comment inside a quoted string. A comment does not
continue onto the next line. Text after ' $ is a [metacommand](metacommands),
not a plain comment.

## Example
    ' Print the answer
    PRINT 6 * 7        ' six times seven

See also [metacommands](metacommands)
