# SELECT CASE Statement
keys: SELECT CASE, CASE, SELECT, END SELECT

Syntax
    SELECT CASE testexpr
    CASE list
        statements
    [CASE list
        statements]...
    [CASE ELSE
        statements]
    END SELECT

Runs the block of the first CASE whose list matches testexpr. If none
matches, CASE ELSE runs, else nothing. Control then leaves the
SELECT; it never falls into the next CASE.

testexpr is a number or a string. A list is one or more items,
separated by commas:

    value               equal to value
    low TO high         from low to high, inclusive
    IS op value         op is one of  =  <>  <  <=  >  >=

All items must have testexpr's kind. Strings compare as in
[expressions](expressions). testexpr is evaluated once.

## Example
    SELECT CASE n%
    CASE 1, 3, 5
        PRINT "small odd"
    CASE 6 TO 10
        PRINT "medium"
    CASE IS < 0
        PRINT "negative"
    CASE ELSE
        PRINT "other"
    END SELECT

See also [CASE](CASE), [IF](IF), [ON GOTO](ON), [TO](TO)
