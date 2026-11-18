# NEXT without FOR / FOR without NEXT
keys: NEXT without FOR, FOR without NEXT, error 1, error 26

Error 1 (NEXT without FOR) and 26 (FOR without NEXT)

## Cause

A [NEXT](NEXT) has no [FOR](FOR) loop open above it, or names a
different counter than the open loop. Or a FOR is never closed by its
NEXT.

## Remedy

Give every FOR exactly one NEXT. Make the counter after NEXT match the
FOR's, or leave it out. The box points at the offending line.

## Example
    FOR i% = 1 TO 3
      PRINT i%
    NEXT j%      ' wrong counter

See also: [FOR](FOR), [NEXT](NEXT), [error codes](error codes)
