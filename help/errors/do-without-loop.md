# DO without LOOP / LOOP without DO
keys: DO without LOOP, LOOP without DO, error 227, error 228

Errors 227 and 228

A DO has no LOOP, or a LOOP has no DO.

See also: [DO](DO), [LOOP](LOOP), [WHILE](WHILE)

## Details

Cause: A [DO](DO) block is never closed by a [LOOP](LOOP), or a LOOP
has no DO open above it.

Remedy: Pair every DO with one LOOP. Remove a stray LOOP, or add the
DO it should close.

DO ... LOOP may carry WHILE or UNTIL on either line, not both.

## Example
    DO
      n% = n% + 1
    ' error 227: no LOOP
    DO
      n% = n% + 1
    LOOP UNTIL n% = 3    ' fixed
    PRINT n%             ' 3
