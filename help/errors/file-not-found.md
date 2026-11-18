# File not found / Path not found
keys: File not found, Path not found, ~Path/File access error, error 53, error 75, error 76

Errors 53, 75 and 76

The file or directory does not exist.

See also: [OPEN](OPEN), [FILES](FILES), [CHDIR](CHDIR)

## Details

Cause: The file or directory named does not exist, or cannot be
reached in that path.

Remedy: Check the spelling and the current directory. Test with
[FILES](FILES) or by opening the file and trapping the error.

Error 53 File not found; 75 Path/File access error; 76 Path not found.

Trap it with ON ERROR; ERR returns the number.

## Example
    OPEN "NOPE.TXT" FOR INPUT AS #1   ' error 53
    ' fix: test first
    ON ERROR GOTO h
    OPEN "NOPE.TXT" FOR INPUT AS #1
    END
    h:
    PRINT "missing"
    RESUME NEXT
