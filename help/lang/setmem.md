# SETMEM Function
keys: SETMEM

Syntax
    SETMEM(n)

In QBasic, resizes the far heap by n bytes. QuickrBasic has no such
limit: it ignores n, collects garbage strings and returns the free
bytes.

## Example
    PRINT SETMEM(0)

See also [FRE](FRE), [CLEAR](CLEAR)
