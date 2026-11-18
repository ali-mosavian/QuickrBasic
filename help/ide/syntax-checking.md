# Syntax Checking
keys: syntax checking, syntax check, syntax errors

A line is checked when the cursor leaves it.

See also [message boxes](message boxes).

## Details
On an error a message box names it and the cursor stays on the line.
Fix it, or turn checking off to move on.

Checking also capitalises keywords and cuts spaces at the line end.

Options, Syntax Checking turns it on and off. See
[Syntax Checking Command](Syntax Checking Command). Syntax
highlighting colours the line as you type and is separate:
[Syntax Highlighting Command](Syntax Highlighting Command).

## Example
    Type:   x = = 1    then press Down
    A message box names the error; the cursor stays on the line
    Fix it:  x = 1     the line is accepted
