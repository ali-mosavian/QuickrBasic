# File Input and Output
keys: file I/O, file concepts, file access, sequential files, random files, binary files

A program keeps data in a file by opening it under a file number,
reading or writing through that number, and closing it. How the data
is laid out depends on the mode given to [OPEN](OPEN).

```
OPEN filespec [FOR mode] [ACCESS access] [lock] AS [#]filenum [LEN = n]
```
The file number is 1 to 255; [FREEFILE](FREEFILE) returns an unused one.
Files must be closed with [CLOSE](CLOSE) to be sure the data is on disk.

## Sequential files
Text read and written in order from the start.
```
OPEN "LOG.TXT" FOR OUTPUT AS #1       ' creates or empties
OPEN "LOG.TXT" FOR APPEND AS #1       ' adds at the end
OPEN "LOG.TXT" FOR INPUT AS #1        ' reads from the start
```
- Write with [PRINT #](PRINT) or [WRITE #](WRITE). WRITE adds commas
  and quotes, so [INPUT #](INPUT) reads the values back exactly.
- Read with INPUT #, [LINE INPUT](LINE) # for a whole line, or
  [INPUT$](INPUT$) for a number of characters.
- [EOF](EOF) tells when nothing is left to read.

## Random-access files
A file of equal-sized records, each reached by number.
```
OPEN "DATA.DAT" FOR RANDOM AS #1 LEN = LEN(rec)
GET #1, 5, rec      ' read record 5
PUT #1, 5, rec      ' write record 5
```
- Records are usually a variable of a TYPE; LEN = gives the size.
- FIELD, LSET and RSET lay out a record in strings instead.
- [LOF](LOF), [LOC](LOC) and [SEEK](SEEK) give the size and position.
- A record number out of range is a [Bad record number](Bad record number).

## Binary files
Any byte can be read or written, at any position.
```
OPEN "IMAGE.BIN" FOR BINARY AS #1
DIM b AS BYTE
GET #1, 1, b          ' reads one byte
```
GET and PUT move as many bytes as the variable holds. A BYTE takes 1,
a SHORT 2, an INTEGER or LONG 4, a SINGLE 4, a DOUBLE 8. A string
variable moves its length in bytes.

INTEGER is 4 bytes in a file as in memory. To exchange files with
QBasic use [SHORT](SHORT) for QBasic's 16-bit integers.

## Devices
SCRN:, KYBD:, LPT1: and CONS: are opened like files.

## Errors
[File not found](File not found), [Bad file name](Bad file name),
[Bad file name or number](Bad file name or number),
[File already open](File already open), [Too many files](Too many files),
[Input past end of file](Input past end of file),
[Permission denied](Permission denied), [Disk full](Disk full).

See also [OPEN](OPEN), [CLOSE](CLOSE), [GET and PUT](GET), [FILES](FILES),
[KILL](KILL), [NAME](NAME)
