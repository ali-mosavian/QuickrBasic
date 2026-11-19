# Bad file name or number / Bad file name
keys: Bad file name or number, Bad file name, error 52, error 64

Errors 52 and 64

A file number is not open or out of range, or a file name is invalid.

See also: [OPEN](OPEN), [CLOSE](CLOSE), [FREEFILE](FREEFILE)

## Details

Cause: A statement used a file number that is not open or is out of
range, or a file name that is empty or has characters DOS does not
allow.

Remedy: Open the file first, keep its number in a variable, and check
the name. Do not use a number again after CLOSE.

Error 52 is Bad file name or number; error 64 is Bad file name. File
numbers run 1 to 255.

Trap it with ON ERROR; ERR returns the number.

## Example
    PRINT #3, "x"       ' error 52: #3 is not open
    OPEN "A.TXT" FOR OUTPUT AS #3
    PRINT #3, "x"        ' fixed
    CLOSE #3
