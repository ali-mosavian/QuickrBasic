# Toggle Breakpoint Command
keys: Toggle Breakpoint Command

Debug, Toggle Breakpoint    F9

Sets or clears a breakpoint on the cursor's line.

See also [Clear All Breakpoints Command](Clear All Breakpoints Command),
[debug pane](debug pane).

## Details
The program stops before a line with a breakpoint runs. The mark
moves with its line as you edit. Pressing F9 again on the line
clears it. The command is dim unless the cursor is in a program
window.

At the stop the [debug pane](debug pane) opens. Continue with F5, or
step with F8, F7 or Shift+F8.

## Example
    Move the cursor to the line  total = total + n
    F9             the line is marked
    Shift+F5       run: stops before that line
    F5             run on to the next stop (or the end)
    F9             on the same line clears the mark
