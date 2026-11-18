# Invalid constant / Feature unavailable
keys: Invalid constant, Feature unavailable

## Cause

A place that needs a constant (CONST, a fixed bound, DATA) holds
something else, or the line uses a feature this version lacks.

## Remedy

Use a literal or a [CONST](CONST) defined above. Remove the
unavailable feature.

See also: [CONST](CONST), [DATA](DATA)
