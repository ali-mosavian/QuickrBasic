# Start Command
keys: Start Command

Run, Start    Shift+F5

Clears all variables and runs the program from its first line.

See also [Continue Command](Continue Command),
[Restart Command](Restart Command).

## Details
When the program ends, any key returns to the editor. A runtime error
stops it at the line concerned, with a message box. A breakpoint or
STOP stops it with the [debug pane](debug pane) open.

## Example
    Type:   PRINT "Hello"
    Shift+F5       the output screen shows Hello
    any key        back to the editor
