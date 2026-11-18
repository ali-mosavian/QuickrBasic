# SHELL Statement
keys: SHELL

Syntax
    SHELL [command$]

Runs an operating-system command, then returns to the program. With
no command, SHELL starts an interactive command interpreter; leave
it with EXIT to resume.

The command runs through the command interpreter (COMSPEC), so
built-ins, batch files and programs all work. The child gets a copy
of the environment, so ENVIRON changes made by the program are
seen by it, not the reverse. A command too long for the interpreter
(about 120 characters) is an Illegal function call.

## Example
    SHELL "DIR /W"

See also [ENVIRON](ENVIRON), [COMMAND$](COMMAND$), [SYSTEM](SYSTEM),
[CHDIR](CHDIR)
