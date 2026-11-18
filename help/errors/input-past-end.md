# Input past end of file
keys: Input past end of file, error 62

Error 62

## Cause

INPUT, LINE INPUT or GET read beyond the last data of the file.

## Remedy

Test [EOF](EOF) before each read.

## Example
    DO WHILE NOT EOF(1)
      LINE INPUT #1, a$
    LOOP

See also: [EOF](EOF), [INPUT](INPUT), [OPEN](OPEN)
