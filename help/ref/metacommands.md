# Metacommands
keys: metacommands, metacommand, $STATIC, $DYNAMIC, $INCLUDE

A metacommand is an instruction to the compiler, written in a comment
so that other BASICs ignore it:

```
'$INCLUDE: 'filespec'
'$STATIC
'$DYNAMIC
```
The comment begins with `'` or REM, directly followed by `$`. It
takes the rest of the line.

## $INCLUDE
Reads the text of another file into the program at that line, as if
typed there. Declarations, TYPEs, CONSTs and DECLAREs are the usual
contents.

```
'$INCLUDE: 'COMMON.BI'
```
A file that can not be found is a [File not found](File not found)
error. A wrong form is a metacommand error.

## $STATIC and $DYNAMIC
They set how arrays that are dimensioned with constant bounds are
stored from that line on. $STATIC makes them fixed arrays; $DYNAMIC
makes them resizable with REDIM and ERASE, with the bounds taken at run
time.

An array dimensioned with variable bounds is always dynamic.

See also [DIM](DIM), [Procedures](procedures)
