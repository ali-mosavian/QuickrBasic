# PRINT USING Formats
keys: PRINT USING, USING, format specifiers, format string, ~print using formats

PRINT USING and LPRINT USING print values through a format string:

```
PRINT USING format$; expression[, | ;]...
```
The format is text with special characters. Each field in it takes
the next value from the list; text between fields is printed as is.
If the list is longer than the fields, the format starts again.

## Numeric fields
```
#      a digit position; unused ones on the left show spaces
.      decimal point; digits after it are rounded to fit
,      left of the point: a comma between every three digits
+      first or last: print the sign, + or -
-      last: print a trailing minus, or a space if positive
**     first: fill unused left positions with *
$$     first: a dollar sign right before the number
**$    both of the above
^^^^   exponent: E+nn after the digits
_      next character is printed literally
```
`**` and `$$` each add two positions to the field, and the dollar
sign takes one of them; a `,` adds one. If the number is too wide for its
field, a `%` is printed before it, and the whole number follows.

```
PRINT USING "###.##"; 3.14159      ' "  3.14"
PRINT USING "+#"; 5                ' "+5"
PRINT USING "**###.##"; 3.5        ' "****3.50"
PRINT USING "$$###.##"; 45.5       ' "  $45.50"
PRINT USING "#.##^^^^"; 12345      ' "1.23E+04"
PRINT USING "###-"; -7             ' "  7-"
```

## String fields
```
!          the first character only
\  \       n+2 characters: the backslashes and the spaces between
&          the whole string
```
`\` plus n spaces plus `\` is n+2 wide. A short string is padded with
spaces on the right; a long one is cut.

```
PRINT USING "!. \   \"; "Ada"; "Lovelace"   ' "A. Lovel"
PRINT USING "&"; "all of it"
```

## Literal text
Any other character prints as itself. To print a character that has a
meaning in a format, put `_` in front:
```
PRINT USING "_#1: ###"; 42      ' "#1:  42"
```

A value of the wrong kind for its field is a
[Type mismatch](Type mismatch). A format with no field is an
[Illegal function call](Illegal function call).

See also [PRINT](PRINT), [Data types](data types)
