# Variable required
keys: Variable required, Simple or array variable expected, error 40

Error 40

A variable is required here, and something else was given.

See also: [READ](READ), [INPUT](INPUT), [SWAP](SWAP)

## Details

Cause: A place that must hold a variable (the target of READ, INPUT,
SWAP, a counter) was given a constant, expression or other
non-variable.

Remedy: Name a variable or an array element there.

READ, INPUT, SWAP and counters take variables or array elements.

## Example
    READ 5              ' error 40
    READ n%
    DATA 5
