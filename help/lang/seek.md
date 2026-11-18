# SEEK Function and Statement
keys: SEEK

Syntax
    SEEK(filenumber)
    SEEK [#]filenumber, position

The function returns where the next read or write will happen. The
statement moves that place.

See also [LOC](LOC), [LOF](LOF), [GET](GET), [PUT](PUT),
[OPEN](OPEN), [files](files)

## Details

The function returns where the next read or write will happen. The
statement moves that place.

  filenumber   an open file.
  position     for a RANDOM file, a record number; for any other
               file, a byte offset. The first record or byte is 1.

After SEEK, the next GET, PUT, INPUT or PRINT starts there. A
position under 1 is the error Bad record number.

For a RANDOM file the function returns the number of the next record;
for a sequential or BINARY file, the byte offset of the next read or
write.

## Example
    a% = 7
    OPEN "T.DAT" FOR BINARY AS #1
    PUT #1, , a%
    SEEK #1, 1                          ' back to the start
    GET #1, , b%
    PRINT b%                            ' 7
    CLOSE #1
