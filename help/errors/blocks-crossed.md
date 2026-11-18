# Blocks crossed
keys: Blocks crossed

## Cause

A line ends a block while another block started inside it is still
open, as a NEXT that comes before the END IF of an IF opened inside
the loop.

## Remedy

Blocks end in the reverse order they began. Close the inner block
first.

See also: [FOR](FOR), [IF](IF), [DO](DO)
