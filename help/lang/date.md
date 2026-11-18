# DATE$ Function and Statement
keys: DATE$, date

Syntax
    DATE$
    DATE$ = stringexpression

As a function, DATE$ returns today's date as "mm-dd-yyyy" (ten
characters).

See also [TIME$](TIME$), [TIMER](TIMER)

## Details
As a function, DATE$ returns today's date as "mm-dd-yyyy" (ten
characters).

As a statement it sets the system date. The string takes the forms
mm-dd-yy, mm-dd-yyyy, mm/dd/yy or mm/dd/yyyy. A two-digit year is
read as 19yy. A malformed string is an Illegal function call.

The year of a returned date always has four digits.

## Example
    PRINT "Today is "; DATE$
    PRINT "Month is "; LEFT$(DATE$, 2)
