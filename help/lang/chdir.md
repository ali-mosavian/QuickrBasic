# CHDIR Statement
keys: CHDIR

Syntax
    CHDIR path$

Makes path$ the current directory. File names without a path are
looked up there afterwards. A path that does not exist gives Path
not found.

## Example
    CHDIR "..\DATA"

See also [MKDIR](MKDIR), [RMDIR](RMDIR), [FILES](FILES)
