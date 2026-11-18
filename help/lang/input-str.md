# INPUT$ Function
keys: INPUT$

Syntax
    INPUT$(n [, [#]filenumber])

Returns a string of n characters read from the open file filenumber,
or from the keyboard when filenumber is left out.

The keyboard form waits until n keys are typed and does not echo
them. Every byte of a file counts, control characters and end-of-line
included. Reading past the end of a file is an Input past end of file
error; test with [EOF](EOF) first.

## Example
    OPEN "DATA.BIN" FOR BINARY AS #1
    head$ = INPUT$(4, #1)
    CLOSE #1
    PRINT "Press a key": k$ = INPUT$(1)

See also [INPUT](INPUT), [INKEY$](INKEY$), [LOF](LOF), [files](files)
