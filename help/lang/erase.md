# ERASE Statement
keys: ERASE

Syntax
    ERASE arrayname[, arrayname]...

ERASE resets arrays. A dynamic array is freed completely; its
memory is returned and it must be DIMensioned or REDIMed again
before use. A static array stays, with every number set to 0 and
every string to "". Arrays are named without parentheses. Erasing a
name that is not an array is an error.

## Example
    DIM a(100)
    ERASE a
    REDIM a(50)

See also [DIM](DIM), [REDIM](REDIM)
