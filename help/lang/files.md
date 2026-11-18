# FILES Statement
keys: FILES statement, FILES

Syntax
    FILES [filespec$]

FILES prints the names in a directory, followed by the free space of
the drive.

See also [CHDIR](CHDIR), [KILL](KILL), [NAME](NAME)

## Details
FILES prints the names in a directory, in columns, followed by the
free space of the drive. With no argument it lists everything in the
current directory. The spec may carry a drive and path and the
wildcards * and ?, as in "C:\DATA\*.BAS". A spec naming only a
directory lists that directory. Nothing matching is File not found.
Subdirectories are shown with <DIR> after the name.

## Example
    FILES "*.BAS"      ' the .BAS files, then the free space
