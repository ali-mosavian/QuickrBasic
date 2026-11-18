# LOF Function
keys: LOF

Syntax
    LOF(filenumber)

Returns the length in bytes of the open file filenumber.

Divide by the record length to count the records of a RANDOM file.

## Example
    OPEN "T.DAT" FOR RANDOM AS #1 LEN = 16
    PRINT LOF(1) \ 16; "records"
    CLOSE #1

See also [LOC](LOC), [EOF](EOF), [OPEN](OPEN), [files](files)
