# RMDIR Statement
keys: RMDIR

Syntax
    RMDIR path$

Deletes the directory path$, which must be empty and must not be the
current one. Errors: "Path not found" (no such directory),
"Path/File access error" (not empty, or in use).

## Example
    MKDIR "TMPDIR"
    RMDIR "TMPDIR"

See also [MKDIR](MKDIR), [CHDIR](CHDIR), [KILL](KILL), [files](files)
