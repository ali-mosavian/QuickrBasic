# KILL Statement
keys: KILL

Syntax
    KILL filespec

Deletes the files named by filespec; ? and * wildcards are allowed.

See also [NAME](NAME), [RMDIR](RMDIR), [FILES](FILES), [files](files)

## Details
A name matching no file is File not found; a read-only file, or one
that is open, gives Permission denied.

Deleted files are not recoverable. Close a file before killing it.

## Example
    OPEN "TEMP.TMP" FOR OUTPUT AS #1
    CLOSE #1
    KILL "TEMP.TMP"
    KILL "*.BAK"            ' every .BAK file
