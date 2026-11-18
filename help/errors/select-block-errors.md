# SELECT CASE block errors
keys: SELECT without END SELECT, END SELECT without SELECT, CASE without SELECT, ~Statements/labels illegal between SELECT CASE and CASE, error 229, error 230, error 231, error 235

Errors 229, 230, 231 and 235

A SELECT CASE block is not closed or not formed correctly.

See also: [SELECT CASE](SELECT CASE), [CASE](CASE)

## Details

Cause: A [SELECT CASE](SELECT CASE) is not closed by END SELECT, an
END SELECT or [CASE](CASE) has no SELECT CASE above it, or a statement
or label sits between SELECT CASE and its first CASE.

Remedy: Close every SELECT CASE with END SELECT. Put the first CASE
directly after the SELECT CASE line; only comments may stand between.

Error 229 SELECT without END SELECT; 230 END SELECT without SELECT;
231 CASE without SELECT; 235 statements between SELECT CASE and the
first CASE.

## Example
    SELECT CASE n%
      CASE 1
        PRINT "one"
    ' error 229: END SELECT missing
    SELECT CASE n%
      CASE 1
        PRINT "one"
    END SELECT           ' fixed
