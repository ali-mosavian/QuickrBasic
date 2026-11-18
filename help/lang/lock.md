# LOCK and UNLOCK Statements
keys: LOCK

Syntax
    LOCK [#]filenumber [, [record | start] [TO end]]
    UNLOCK [#]filenumber [, [record | start] [TO end]]

Meant to keep other programs from reading or writing a file or part of
it, a record in RANDOM mode or a byte range in BINARY mode, until
UNLOCK.

QuickrBasic runs one program at a time. LOCK and UNLOCK check that the
file is open (Bad file name or number if not) and then do nothing.
Always pair them in portable programs: every LOCK needs an UNLOCK with
the same arguments before the file is closed.

## Example
    OPEN "BANK.DAT" FOR RANDOM AS #1 LEN = 20
    LOCK #1, 3
    ' ... read and rewrite record 3 ...
    UNLOCK #1, 3
    CLOSE #1

See also [OPEN](OPEN), [files](files)
