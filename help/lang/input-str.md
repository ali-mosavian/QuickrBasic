# INPUT$ Function
keys: INPUT$

Syntax
    INPUT$(n [, [#]filenumber])

Returns a string of n characters read from a file, or from the
keyboard when filenumber is left out.

See also [INPUT](INPUT), [INKEY$](INKEY$), [LOF](LOF), [files](files)

## Details
The keyboard form waits until n keys are typed and does not echo
them. From an open file, every byte counts, control characters and
end-of-line included. Reading past the end of a file is an Input
past end of file error; test with [EOF](EOF) first, or compare n
with the bytes left (LOF minus LOC).

## Example
    OPEN "DATA.BIN" FOR BINARY AS #1
    head$ = INPUT$(4, #1)
    CLOSE #1
    PRINT "Press a key": k$ = INPUT$(1)
    PRINT "got "; k$        ' got q  (after pressing q)
