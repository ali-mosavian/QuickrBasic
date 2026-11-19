# Asterisk missing / Integer between 1 and 32767 required
keys: Asterisk missing, Integer between 1 and 32767 required, Fixed-length string illegal, Variable-length string required

A fixed-length string is written wrong.

See also: [DIM](DIM), [data types](data types)

## Details

Cause: A fixed-length string is written without `* length`, its length
is outside 1..32767, or one is used where only a variable-length
string is allowed (or the reverse).

Remedy: Write `AS STRING * 10`. Declare a plain `AS STRING` where a
variable- length string is needed.

The length goes after * and must be 1..32767.

## Example
    DIM s AS STRING     ' fine: variable length
    DIM t AS STRING * 0  ' error: length 0
    DIM t AS STRING * 10
    t = "hi"
    PRINT t; "|"         ' hi        |
