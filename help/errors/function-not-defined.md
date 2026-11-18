# Function not defined
keys: Function not defined, ~Function not defined /declared, error 18

Error 18

A call names a FUNCTION or DEF FN that does not exist.

See also: [FUNCTION](FUNCTION), [DEF FN](DEF FN), [DECLARE](DECLARE)

## Details

Cause: A call names a FUNCTION or DEF FN that does not exist above
this line, or one that is DECLAREd but never defined.

Remedy: Write the [FUNCTION](FUNCTION) or [DEF FN](DEF FN), or fix the
name. Define or [DECLARE](DECLARE) it before its first use.

A DECLAREd FUNCTION that is never defined gives it too.

## Example
    PRINT Twice%(4)     ' error 18: no such function
    FUNCTION Twice% (n%) ' fixed: define it
      Twice% = n% * 2
    END FUNCTION
