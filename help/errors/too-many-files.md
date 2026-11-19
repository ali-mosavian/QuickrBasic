# Too many files
keys: Too many files, error 67

Error 67

More files are open than the system allows.

See also: [OPEN](OPEN), [CLOSE](CLOSE)

## Details

Cause: More files are open at once than the system allows.

Remedy: CLOSE files you are done with.

Close files as soon as you are done with them.

Trap it with ON ERROR; ERR returns the number.

## Example
    FOR i% = 1 TO 100
      OPEN "F.TXT" FOR INPUT AS #i%   ' error 67 past the limit
    NEXT
    ' fix: CLOSE #i% after each use
