# COMMAND$ Function
keys: COMMAND$

Syntax
    COMMAND$

Returns the command-line text that followed the program's name when
it was started, without the leading space. It is empty when there
was none. QBasic gives the text in capitals.

## Example
    IF COMMAND$ = "" THEN PRINT "no options"

See also [ENVIRON$](ENVIRON$), [SHELL](SHELL), [RUN](RUN)
