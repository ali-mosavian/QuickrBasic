# Input past end of file
keys: Input past end of file, error 62

Error 62

A read went beyond the last data of the file.

See also: [EOF](EOF), [INPUT](INPUT), [OPEN](OPEN)

## Details

Cause: INPUT, LINE INPUT or GET read beyond the last data of the file.

Remedy: Test [EOF](EOF) before each read.

Reading the last line leaves EOF true; the next read fails.

Trap it with ON ERROR; ERR returns the number.

## Example
    OPEN "A.TXT" FOR INPUT AS #1
    LINE INPUT #1, a$
    LINE INPUT #1, b$    ' error 62 if only one line
    CLOSE #1
    OPEN "A.TXT" FOR INPUT AS #1   ' fixed
    DO WHILE NOT EOF(1)
      LINE INPUT #1, a$
    LOOP
    CLOSE #1
