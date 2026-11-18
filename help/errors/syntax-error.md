# Syntax error
keys: Syntax error, error 2, ~Syntax error /OPTION, ~Syntax error /COMMON, ~Syntax error /DECLARE

Error 2

The line is not valid BASIC.

See also: [syntax notation](syntax notation), [OPTION](OPTION), [COMMON](COMMON), [DECLARE](DECLARE)

## Details

Cause: The line is not valid BASIC: a misspelled keyword, a missing or
extra comma, bracket or operator, or a reserved word used as a name.
OPTION, COMMON and DECLARE give it when used where they are not
allowed.

Remedy: Check the highlighted spot against the statement's syntax with
F1 on its keyword. OPTION, COMMON and DECLARE go at module level,
above the first procedure.

Check the highlighted spot; F1 on its keyword shows the syntax.

## Example
    PRINT "a" "b"       ' error 2: missing ;
    PRINT "a"; "b"       ' ab
