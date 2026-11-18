# Syntax error
keys: Syntax error, error 2, ~Syntax error /OPTION, ~Syntax error /COMMON, ~Syntax error /DECLARE

Error 2

## Cause

The line is not valid BASIC: a misspelled keyword, a missing or extra
comma, bracket or operator, or a reserved word used as a name. OPTION,
COMMON and DECLARE give it when used where they are not allowed.

## Remedy

Check the highlighted spot against the statement's syntax with F1 on
its keyword. OPTION, COMMON and DECLARE go at module level, above the
first procedure.

See also: [syntax notation](syntax notation), [OPTION](OPTION), [COMMON](COMMON), [DECLARE](DECLARE)
