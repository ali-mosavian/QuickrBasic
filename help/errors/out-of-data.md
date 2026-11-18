# Out of DATA
keys: Out of DATA, error 4

Error 4

READ has no DATA item left.

See also: [READ](READ), [DATA](DATA), [RESTORE](RESTORE)

## Details

Cause: [READ](READ) wants another value, but every [DATA](DATA) item
has been used.

Remedy: Add DATA items, read fewer, or rewind with [RESTORE](RESTORE).
Test with a count or a sentinel value in the DATA.

RESTORE rewinds to the first DATA; a sentinel value ends a loop
safely.

Trap it with ON ERROR; ERR returns the number.

## Example
    READ a%, b%, c%
    DATA 1, 2             ' error 4
    ' fix
    DATA 1, 2, 3
