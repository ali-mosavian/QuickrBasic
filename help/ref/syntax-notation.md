# Syntax Notation
keys: syntax notation, syntax conventions, notation

Each keyword's topic starts with its syntax. These marks are not part
of what you type:

```
CAPITALS     a keyword; type it as shown
lowercase    a placeholder: replace it with your own value
[ ]          optional; leave out the brackets
{ a | b }    choose exactly one
...          the part before may repeat
, ; ( ) = #  type these as written
```

A keyword may be typed in any case; the editor capitalizes it.

## Example
```
LINE [[STEP](x1, y1)]-[STEP](x2, y2)[, [color][, [B|BF][, style]]]
```
`LINE` and the `-` are required. Everything in brackets may be
dropped, but each comma that separates two kept parts stays.

## Placeholder names
```
expression          any numeric or string expression
numeric expression  a number
string expression   a string
variable            a variable name, with or without a type suffix
label               a line number, or a name followed by a colon
filenum             a file number, 1 to 255
filespec            a string naming a file or device
```

See also [Data types](data types), [Expressions](expressions),
[Keyword lists](keyword lists)
