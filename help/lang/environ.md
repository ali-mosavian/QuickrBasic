# ENVIRON Statement and ENVIRON$ Function
keys: ENVIRON, ENVIRON$

Syntax
    ENVIRON "name=value"
    ENVIRON$(name$)
    ENVIRON$(n)

The statement sets an environment variable of the running program.
Give it "NAME=value", or "NAME=" or "NAME" to remove it. The change
lasts only while the program runs, and is seen by programs it starts
with SHELL.

The function reads one: with a string it returns that variable's
value, or "" if unset (case matters); with a number n >= 1 it
returns the whole nth entry as "NAME=value", or "" past the last.

## Example
    ENVIRON "GREETING=hello"
    PRINT ENVIRON$("GREETING")
    i = 1
    DO WHILE ENVIRON$(i) <> ""
      PRINT ENVIRON$(i): i = i + 1
    LOOP

See also [SHELL](SHELL)
