# Syntax Checking
keys: syntax checking, syntax check, syntax errors, Ctrl+F8

A line is checked when the cursor leaves it.

See also [shortcut keys](shortcut keys).

## Details
A line with an error is kept as typed and the cursor moves on. The
error's word is lit. On that line the status bar says what is wrong;
elsewhere it counts the errors. Ctrl+F8 goes to the next error.

Checking also capitalises keywords and cuts spaces at the line end.

Options, Syntax Checking turns it on and off. See
[Syntax Checking Command](Syntax Checking Command). Syntax
highlighting colours the line as you type and is separate:
[Syntax Highlighting Command](Syntax Highlighting Command).

## Example
    Type:   x = = 1    then press Down
    The second = is lit; the status bar shows (1 error)
    Ctrl+F8 goes back to it; fix it:  x = 1
