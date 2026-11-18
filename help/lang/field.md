# FIELD Statement
keys: FIELD

Syntax
    FIELD [#]filenumber, width AS stringvar[, width AS stringvar]...

FIELD maps parts of a random-access file's record buffer to string
variables. Each width is a byte count; the variables follow one
another from the start of the buffer, and their widths together may
not exceed the record length given in OPEN (Field overflow).

Fielded variables point into the buffer: GET fills them, and to
store data put it in with [LSET](LSET) or [RSET](RSET) before PUT.
Assigning with = makes the name an ordinary string and breaks the
link. For new code a TYPE record with GET and PUT is simpler.

## Example
    OPEN "people.dat" FOR RANDOM AS #1 LEN = 30
    FIELD #1, 20 AS nm$, 10 AS ph$
    LSET nm$ = "Ann": LSET ph$ = "555-0100"
    PUT #1, 1
    CLOSE #1

See also [OPEN](OPEN), [GET and PUT](GET), [LSET](LSET), [RSET](RSET)
