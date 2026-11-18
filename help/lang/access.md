# ACCESS Clause
keys: ACCESS

Syntax
    OPEN file$ [FOR mode] ACCESS {READ | WRITE | READ WRITE} AS #n

An OPEN clause that says what the program will do with the file.
READ allows only reading, WRITE only writing, READ WRITE both.

See also [OPEN](OPEN), [LOCK](LOCK), [files](files)

## Details
An OPEN clause that says what the program will do with the file.
READ allows only reading, WRITE only writing, READ WRITE both.

Without ACCESS, the file is opened for the mode's natural use:
INPUT reads, OUTPUT and APPEND write, RANDOM and BINARY try
READ WRITE first, then WRITE, then READ.

An operation the access forbids fails with Permission denied. A
file that cannot be opened with the access asked for gives the same
error; so can another program holding the file, see
[LOCK](LOCK).

## Example
    OPEN "LOG.TXT" FOR BINARY ACCESS READ AS #1
    GET #1, 1, n%              ' allowed
    PUT #1, 1, n%              ' Permission denied
    CLOSE #1
