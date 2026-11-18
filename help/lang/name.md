# NAME Statement
keys: NAME

Syntax
    NAME oldname AS newname

Renames a file or directory, and may move it to another directory of
the same drive.

See also [KILL](KILL), [MKDIR](MKDIR), [FILES](FILES)

## Details
oldname must exist (File not found) and newname must not (File
already exists). The file must not be open.

## Example
    NAME "DRAFT.TXT" AS "FINAL.TXT"
    NAME "FINAL.TXT" AS "OLD\FINAL.TXT"     ' move into OLD
