# DO without LOOP / LOOP without DO
keys: DO without LOOP, LOOP without DO, error 227, error 228

Errors 227 and 228

## Cause

A [DO](DO) block is never closed by a [LOOP](LOOP), or a LOOP has no
DO open above it.

## Remedy

Pair every DO with one LOOP. Remove a stray LOOP, or add the DO it
should close.

See also: [DO](DO), [LOOP](LOOP), [WHILE](WHILE)
