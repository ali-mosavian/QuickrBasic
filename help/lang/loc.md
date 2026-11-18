# LOC Function
keys: LOC

Syntax
    LOC(filenumber)

Returns where an open file stands. What the number means depends on
the file mode:

    RANDOM      number of the last record read or written
    BINARY      position of the last byte read or written
    sequential  bytes read or written so far, divided by 128

## Example
    OPEN "T.DAT" FOR BINARY AS #1
    a$ = INPUT$(10, #1)
    PRINT LOC(1)            ' 10

See also [SEEK](SEEK), [LOF](LOF), [EOF](EOF), [files](files)
