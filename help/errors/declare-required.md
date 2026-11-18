# DECLARE required / Equal sign missing
keys: DECLARE required, Equal sign missing

## Cause

A name is called as a SUB before the SUB is defined or DECLAREd, so
the compiler reads it as an array or an assignment.

## Remedy

Put `DECLARE SUB name (...)` at the top of the file, or define the SUB
above its first call.

See also: [DECLARE](DECLARE), [SUB](SUB), [CALL](CALL)
