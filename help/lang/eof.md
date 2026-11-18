# EOF Function
keys: EOF

Syntax
    EOF(filenumber)

EOF returns -1 (true) when the next read from the file would pass
its end, otherwise 0.

See also [LOF](LOF), [LOC](LOC), [OPEN](OPEN)

## Details
EOF returns -1 (true) when the next read from the file would pass
its end, otherwise 0. Test it before each INPUT #, LINE INPUT # or
GET, not after, or the last read fails with Input past end of file.

For a file opened FOR RANDOM or BINARY it is true once the last
record or byte has been read. For a keyboard device (KYBD:) it is
true when no key is waiting.

## Example
    OPEN "list.txt" FOR INPUT AS #1
    DO UNTIL EOF(1)
      LINE INPUT #1, a$
      PRINT a$           ' each line of the file
    LOOP
    CLOSE #1
