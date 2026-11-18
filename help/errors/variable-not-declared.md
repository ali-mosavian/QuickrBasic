# Variable not declared
keys: Variable not declared, error 243

Error 243

A name was used without a declaration under OPTION EXPLICIT.

See also: [DIM](DIM), [OPTION](OPTION)

## Details

Cause: A name is used that was never declared, under OPTION EXPLICIT,
which requires every variable to be declared.

Remedy: Declare it with [DIM](DIM) first, or fix the spelling.

Without OPTION EXPLICIT, names are created on first use.

## Example
    OPTION EXPLICIT
    x% = 1               ' error 243
    DIM x AS INTEGER     ' fixed
    x = 1
