# AS Clause
keys: AS

AS joins a name to a type or a file number. It appears in:

    DIM a AS INTEGER             variable type
    x AS SINGLE                  field inside TYPE
    SUB Foo (n AS LONG)          parameter type
    DECLARE SUB Foo (n AS ANY)   see ANY
    OPEN f$ FOR INPUT AS #1      file number
    NAME old$ AS new$            the new name
    FIELD #1, 10 AS n$           random-file buffer

Types after AS: INTEGER, LONG, SHORT, BYTE, SINGLE, DOUBLE, STRING,
STRING * n (fixed length), or a name made with TYPE.

See also [data types](data types), [DIM](DIM), [TYPE](TYPE),
[OPEN](OPEN), [NAME](NAME), [FIELD](FIELD), [ANY](ANY)
