# BLOAD and BSAVE Statements
keys: BLOAD, BSAVE

Syntax
    BSAVE file$, offset, length
    BLOAD file$ [, offset]

BSAVE writes length bytes of memory, starting at offset, to a file
as a memory image. BLOAD reads such a file back into memory,
starting at offset, or at the address the file was saved from when
offset is left out.

The segment comes from the last DEF SEG. QuickrBasic has no 16-bit
segments, so DEF SEG gives a warning and has no effect, and an
address is a plain 32-bit offset. The 65535-byte limit on length
is gone as well.

A missing file gives File not found; a bad path gives Path not
found.

## Example
    BSAVE "SCREEN.IMG", 0, 4000
    BLOAD "SCREEN.IMG", 0

See also [DEF SEG](DEF SEG), [PEEK](PEEK), [POKE](POKE),
[VARPTR](VARPTR)
