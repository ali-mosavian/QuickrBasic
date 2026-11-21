# Undo Command
keys: Undo Command, undo

Edit, Undo    Ctrl+Z, Alt+Backspace

Takes back the last change to the program.

See also [Redo Command](Redo Command), [editing keys](editing keys).

## Details
While the cursor stays on a line, Undo takes back a word typed, or
a run of deletes, at a time. Once the cursor has left the line, all
its changes are one step. A command that changes many lines, such as
a paste or Change All, is one step too.

The cursor goes back to where the change was. Undo goes back to when
the program was opened, 256 steps at most.

## Example
    Type:   print total
    Ctrl+Z         the line reads  print
    Ctrl+Z         the line is as it was
