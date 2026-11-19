# Operators
keys: operators, operator, arithmetic operators, relational operators, logical operators

Operators combine values in an [expression](expressions).

See also [Data types](data types), [Expressions](expressions)

## Details
Arithmetic
```
^         power
-         negation (unary)
*  /      multiply, divide (result is floating point)
\         integer divide: both sides rounded to integers first
MOD       remainder after integer divide
+  -      add, subtract
```
Division by zero is an error. `+` also joins strings.

Relational
```
=  <>  <  >  <=  >=
```
They give -1 for true and 0 for false. Numbers compare by value,
strings character by character by ASCII code; "a" is greater than
"A", and a shorter string that matches the start of a longer one is
smaller.

Logical
Logical operators work on the bits of integers, so with the -1 and 0
of a comparison they act as true and false.
```
NOT   flips every bit
AND   1 where both bits are 1
OR    1 where either bit is 1
XOR   1 where the bits differ
EQV   1 where the bits are equal
IMP   0 only where the left is 1 and the right is 0
```
String
`+` joins two strings. Strings can be compared but not subtracted.

For the order operators are applied, see [Expressions](expressions).

## Example
    PRINT 12 AND 10, NOT 0, 7 \ 2, 7 MOD 2, 7 / 2
    ' 8           -1            3             1             3.5
    PRINT 3 < 4, "a" > "A"
    ' -1           -1
