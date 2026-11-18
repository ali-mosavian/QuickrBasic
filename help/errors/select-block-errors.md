# SELECT CASE block errors
keys: SELECT without END SELECT, END SELECT without SELECT, CASE without SELECT, ~Statements/labels illegal between SELECT CASE and CASE, error 229, error 230, error 231, error 235

Errors 229, 230, 231 and 235

## Cause

A [SELECT CASE](SELECT CASE) is not closed by END SELECT, an END
SELECT or [CASE](CASE) has no SELECT CASE above it, or a statement or
label sits between SELECT CASE and its first CASE.

## Remedy

Close every SELECT CASE with END SELECT. Put the first CASE directly
after the SELECT CASE line; only comments may stand between.

See also: [SELECT CASE](SELECT CASE), [CASE](CASE)
