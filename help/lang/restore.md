# RESTORE Statement
keys: RESTORE

Syntax
    RESTORE [{line | label}]

Moves the pointer that READ uses so the next READ takes its value
from the DATA statement at line or label, or from the very first
DATA in the program if none is given.

See also [READ](READ), [DATA](DATA)

## Details

Moves the pointer that READ uses so the next READ takes its value
from the DATA statement at line or label, or from the very first
DATA in the program if none is given.

Use it to read the same list twice, or to choose a list. Labels name
a DATA line, so several lists can live in one program.

After the last item is read, RESTORE is how you read the list again.

## Example
    RESTORE Fruit
    READ a$: PRINT a$              ' apple
    END
    Veg:   DATA carrot
    Fruit: DATA apple
