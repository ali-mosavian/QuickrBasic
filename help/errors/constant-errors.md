# Invalid constant / Feature unavailable
keys: Invalid constant, Feature unavailable

A place that needs a constant holds something else.

See also: [CONST](CONST), [DATA](DATA)

## Details

Cause: A place that needs a constant (CONST, a fixed bound, DATA)
holds something else, or the line uses a feature this version lacks.

Remedy: Use a literal or a [CONST](CONST) defined above. Remove the
unavailable feature.

Invalid constant: CONST, DATA and fixed bounds take literals or
earlier CONSTs, not variables. Feature unavailable: the line uses a
feature this version lacks.

## Example
    n% = 5
    CONST size = n%      ' error: not a constant
    CONST size = 5       ' fixed
    DIM a%(size)
