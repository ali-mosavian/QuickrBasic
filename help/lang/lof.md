# LOF Function
keys: LOF

Syntax
    LOF(filenumber)

Returns the length in bytes of an open file.

See also [LOC](LOC), [EOF](EOF), [OPEN](OPEN), [files](files)

## Details
filenumber must be open, or the error is Bad file name or number.
Divide by the record length to count the records of a RANDOM file.

## Example
    OPEN "T.DAT" FOR RANDOM AS #1 LEN = 16
    PRINT LOF(1) \ 16; "records"     ' 3 records  (48-byte file)
    CLOSE #1
