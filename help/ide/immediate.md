# Immediate Window
keys: immediate window, immediate

The window at the bottom of the screen: type one statement and it runs
at once.

See also [windows](windows), [debug pane](debug pane).

## Details
Type one statement and press Enter; it runs at once against the
program's current variables. Nothing here becomes part of the program.

Up and Down recall the earlier lines. F6 moves in and out. Output
appears on the output screen ([Output Screen Command](Output Screen Command)).

Run a program to a stop first and its variables are still there to
print or change before you carry on.

## Example
    F6             move to the immediate window
    x = 10         Enter
    PRINT x * 2    Enter
    F4             the output screen shows  20
