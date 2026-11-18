# GOTO missing / Left parenthesis missing
keys: GOTO missing, Left parenthesis missing

ON is not followed by GOTO or GOSUB, or a ( is missing.

See also: [ON](ON), [GOTO](GOTO)

## Details

Cause: `ON expression` is not followed by GOTO or GOSUB, or a function
or array name needs its `(`.

Remedy: Write `ON n GOTO a, b` and put arguments in parentheses.

Applies to ON expression GOTO/GOSUB lists and to function or array
references.

## Example
    ON n% a, b          ' error: GOTO missing
    ON n% GOTO a, b      ' fixed
