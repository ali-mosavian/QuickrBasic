# RESET Statement
keys: RESET

Syntax
    RESET

Closes every open file and device, writing out what is buffered.
Same as CLOSE with no list.

## Example
    OPEN "A.TXT" FOR OUTPUT AS #1
    OPEN "B.TXT" FOR OUTPUT AS #2
    RESET

See also [CLOSE](CLOSE), [OPEN](OPEN)
