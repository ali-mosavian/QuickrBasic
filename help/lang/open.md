# OPEN Statement
keys: OPEN, OUTPUT, RANDOM, file modes

Syntax
    OPEN file$ [FOR mode] [ACCESS access] [lock] AS [#]n [LEN = reclen]

Prepares a file or device for I/O under file number n (1 to 255).

See also [CLOSE](CLOSE), [INPUT #](INPUT #), [PRINT #](PRINT #),
[GET](GET), [PUT](PUT), [files](files)

## Details
  mode      INPUT    read a sequential file; it must exist
            OUTPUT   write a sequential file; replaces the old one
            APPEND   write at the end of a sequential file
            RANDOM   records of reclen bytes, read and written with
                     GET and PUT (the default when FOR is left out)
            BINARY   bytes, read and written at any position

  access    READ, WRITE or READ WRITE. Without it, RANDOM and BINARY
            try read/write, then fall back to the one that works.

  lock      SHARED, LOCK READ, LOCK WRITE or LOCK READ WRITE: what
            other processes may do with the file while it is open.

reclen is the record size for RANDOM (default 128) and the buffer size
for sequential files. file$ may be a device such as LPT1: or CONS:.

Errors: "File not found" (INPUT, or a path that does not exist),
"File already open" (same file number or file twice), "Bad file name
or number", "Permission denied" (locked or read-only).

## Example
    OPEN "NAMES.TXT" FOR OUTPUT AS #1
    PRINT #1, "Ada"
    CLOSE #1
    OPEN "NAMES.TXT" FOR INPUT AS #1
    LINE INPUT #1, n$
    PRINT n$                ' Ada
    CLOSE #1
