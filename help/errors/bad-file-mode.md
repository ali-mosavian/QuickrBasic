# Bad file mode
keys: Bad file mode, error 54

Error 54

The statement does not fit the mode the file was opened in.

See also: [OPEN](OPEN), [GET](GET), [PUT](PUT)

## Details

Cause: The statement does not fit how the file was opened: PRINT# to a
file opened for INPUT, GET on a sequential file, INPUT# from a random
file, or OPEN with a mode it cannot use.

Remedy: Open the file in the mode the statement needs: INPUT to read,
OUTPUT or APPEND to write text, RANDOM or BINARY for GET and PUT.

Modes: INPUT reads text; OUTPUT and APPEND write text; RANDOM and
BINARY allow GET and PUT.

Trap it with ON ERROR; ERR returns the number.

## Example
    OPEN "T.TXT" FOR INPUT AS #1
    PRINT #1, "hi"       ' error 54: file is open for reading
    CLOSE #1
    OPEN "T.TXT" FOR OUTPUT AS #1
    PRINT #1, "hi"       ' fixed
    CLOSE #1
