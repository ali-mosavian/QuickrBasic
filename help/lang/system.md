# SYSTEM Statement
keys: SYSTEM

Syntax
    SYSTEM

Closes all open files and ends the program, returning to the operating
system.

See also [END](END), [STOP](STOP)

## Details

Closes all open files and ends the program, returning to the
operating system.

To stop and stay in the program, use END. Open files are closed by
both.

## Example
    PRINT "Done."                   ' Done.
    SYSTEM                          ' back to the operating system
