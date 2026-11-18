# Out of DATA
keys: Out of DATA, error 4

Error 4

## Cause

[READ](READ) wants another value, but every [DATA](DATA) item has been
used.

## Remedy

Add DATA items, read fewer, or rewind with [RESTORE](RESTORE). Test
with a count or a sentinel value in the DATA.

See also: [READ](READ), [DATA](DATA), [RESTORE](RESTORE)
