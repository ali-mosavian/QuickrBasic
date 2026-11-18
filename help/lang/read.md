# READ Statement
keys: READ

Syntax
    READ variablelist

Assigns to each variable, in turn, the next value from the program's
[DATA](DATA) statements. The values are taken in the order the DATA
lines occur, anywhere in the program. A pointer remembers where the
next READ continues; [RESTORE](RESTORE) moves it.

Types must agree: a numeric variable reading "abc" is "Syntax error"
on the DATA line. A string variable accepts any item, a number
included. Reading past the last item is "Out of DATA".

## Example
    DATA Ada, 36, Bob, 41
    FOR i = 1 TO 2
        READ n$, age
        PRINT n$; " is"; age
    NEXT

See also [DATA](DATA), [RESTORE](RESTORE), [INPUT](INPUT)
