# MKDIR Statement
keys: MKDIR

Syntax
    MKDIR pathname

Creates the directory pathname, relative to the current directory or
absolute. If it exists, or its parent does not, the error is Path/File
access error or Path not found.

## Example
    MKDIR "SAVES"
    CHDIR "SAVES"

See also [CHDIR](CHDIR), [RMDIR](RMDIR), [FILES](FILES)
