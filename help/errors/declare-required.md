# DECLARE required / Equal sign missing
keys: DECLARE required, Equal sign missing

A SUB is called before it is defined or DECLAREd.

See also: [DECLARE](DECLARE), [SUB](SUB), [CALL](CALL)

## Details

Cause: A name is called as a SUB before the SUB is defined or
DECLAREd, so the compiler reads it as an array or an assignment.

Remedy: Put `DECLARE SUB name (...)` at the top of the file, or define
the SUB above its first call.

The compiler reads the unknown name as an array or an assignment,
hence "Equal sign missing".

## Example
    Greet "Ann"         ' error: Greet unknown here
    DECLARE SUB Greet (n$)
    Greet "Ann"          ' fixed
    SUB Greet (n$)
      PRINT "Hi, "; n$   ' Hi, Ann
    END SUB
