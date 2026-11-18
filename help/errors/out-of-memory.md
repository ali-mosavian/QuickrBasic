# Out of memory
keys: Out of memory, error 7

Error 7

The program needs more memory than is available.

See also: [DIM](DIM), [ERASE](ERASE), [REDIM](REDIM)

## Details

Cause: The program, its arrays and its strings need more memory than
the machine can give.

Remedy: Use smaller or fewer arrays, free them with [ERASE](ERASE), or
shrink a dynamic array with [REDIM](REDIM).

Strings, dynamic arrays and the stack all draw on the same memory.

Trap it with ON ERROR; ERR returns the number.

## Example
    DIM a(30000) AS DOUBLE   ' may be too big
    ERASE a                  ' free it when done
    REDIM a(1000) AS DOUBLE  ' smaller
