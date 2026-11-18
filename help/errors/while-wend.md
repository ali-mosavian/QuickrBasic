# WHILE without WEND / WEND without WHILE
keys: WHILE without WEND, WEND without WHILE, error 29, error 30

Error 29 (WHILE without WEND) and 30 (WEND without WHILE)

A WHILE has no WEND, or a WEND has no WHILE.

See also: [WHILE](WHILE), [WEND](WEND), [DO](DO)

## Details

Cause: A [WHILE](WHILE) loop is never closed by a [WEND](WEND), or a
WEND has no WHILE open above it.

Remedy: Pair every WHILE with one WEND. Remove a stray WEND, or add
the WHILE it should close.

Error 29 WHILE without WEND; 30 WEND without WHILE.

## Example
    WHILE n% < 3
      n% = n% + 1
    ' error 29: WEND missing
    WHILE n% < 3
      n% = n% + 1
    WEND                 ' fixed
    PRINT n%             ' 3
