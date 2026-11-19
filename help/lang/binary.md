# BINARY Mode
keys: BINARY

Syntax
    OPEN file$ FOR BINARY [ACCESS ...] AS #n

Opens a file as a plain sequence of bytes, with no line or record
structure. The file is created if missing. GET and PUT read and
write at any byte position; without a position they continue from
the last one. The number of bytes moved is the size of the variable.

See also [OPEN](OPEN), [GET](GET), [PUT](PUT), [SEEK](SEEK), [files](files)

## Details
Opens a file as a plain sequence of bytes, with no line or record
structure. The file is created if missing. GET and PUT read and
write at any byte position; without a position they continue from
the last one. The number of bytes moved is the size of the variable.

INPUT$ and EOF also work, and SEEK #n, pos moves the file pointer
(the first byte is 1). Unlike RANDOM, there is no record length.

n% is 4 bytes here because INTEGER is 32-bit; use SHORT for the
16-bit value QBasic would read.

## Example
    OPEN "DATA.BIN" FOR BINARY AS #1
    GET #1, 5, n%          ' the 4 bytes at position 5
    CLOSE #1
