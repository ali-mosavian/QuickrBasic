# Parameter type mismatch
keys: Parameter type mismatch, error 244

Error 244

An argument's type differs from the parameter's.

See also: [SUB](SUB), [FUNCTION](FUNCTION), [DECLARE](DECLARE)

## Details

Cause: An argument is not exactly the type of the parameter. A
variable is passed by reference, so no conversion is made.

Remedy: Pass a variable of the declared type, or wrap the argument in
parentheses to pass its value, converted.

Arguments pass by reference, so there is no conversion; (expr) passes
a converted copy.

## Example
    DECLARE SUB T (n AS LONG)
    i% = 5
    CALL T(i%)           ' error 244
    CALL T((i%))         ' fixed: copy, converted
    SUB T (n AS LONG)
    END SUB
