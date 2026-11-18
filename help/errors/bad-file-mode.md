# Bad file mode
keys: Bad file mode, error 54

Error 54

## Cause

The statement does not fit how the file was opened: PRINT# to a file
opened for INPUT, GET on a sequential file, INPUT# from a random file,
or OPEN with a mode it cannot use.

## Remedy

Open the file in the mode the statement needs: INPUT to read, OUTPUT
or APPEND to write text, RANDOM or BINARY for GET and PUT.

See also: [OPEN](OPEN), [GET](GET), [PUT](PUT)
