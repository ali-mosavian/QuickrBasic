# SETMEM Function
keys: SETMEM

Syntax
    SETMEM(n)

In QBasic, resizes the far heap by n bytes.

See also [FRE](FRE), [CLEAR](CLEAR)

## Details

In QBasic, resizes the far heap by n bytes. QuickrBasic has no such
limit: it ignores n, collects garbage strings and returns the free
bytes.

SETMEM is kept so old programs run. Use FRE for the free memory.

## Example
    PRINT SETMEM(0) > 0                 ' -1 (true)
