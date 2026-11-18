# TO Keyword
keys: TO

TO separates the two ends of a range in FOR, CASE, DIM and LOCK. The
ends are included.

See also [FOR](FOR), [SELECT CASE](SELECT CASE), [DIM](DIM),
[LOCK](LOCK)

## Details

TO separates the two ends of a range in:

    FOR i = first TO last           the loop's limits
    CASE low TO high                a SELECT CASE range
    DIM a(low TO high)              an array's bounds
    LOCK #1, first TO last          a span of records

The ends are included. Bounds in DIM may be negative: DIM a(-5 TO 5).
With OPTION BASE 1, DIM a(10) has 10 elements. See each statement for
its own rules.

## Example
    FOR i = 1 TO 3: PRINT i; : NEXT    ' 1  2  3
    DIM a(-2 TO 2)
    PRINT LBOUND(a); UBOUND(a)         ' -2  2
    SELECT CASE 5
    CASE 1 TO 9: PRINT "digit"         ' digit
    END SELECT
