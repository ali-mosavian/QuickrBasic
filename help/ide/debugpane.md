# Debug Pane
keys: debug pane, watches, watch, Locals, Call stack, Add Watch Dialog, Break When, Set Value

When the program stops, a pane opens at the right with Watch, Locals
and Call stack windows.

See also [Debug Menu](Debug Menu),
[Toggle Breakpoint Command](Toggle Breakpoint Command).

## Details
It opens when the program stops (a breakpoint, a step, Ctrl+F7).

    Watch       expressions you chose, reevaluated at every stop
    Locals      the variables of the selected frame, parameters first,
                then the module's that its procedure shares
    Call stack  the procedures that led here, with their arguments

Keys, with the pane focused (click it, or F6):

    Tab, Shift+Tab  next, previous window of the pane
    Up, Down        choose a row
    Enter           in Call stack, show that caller's line and
                    variables; in Locals, open a record or array, or
                    set a value
    Right, Left     in Locals, open, close a record or array
    Ins             in Watch, add a watch
    Del             in Watch, drop the chosen watch
    Esc             back to the program window

In the program window:

    Shift+F9        watch the name at the cursor
    Ctrl+F9         Break When

Ins asks for an expression in the Add Watch box. It is evaluated
in the scope of the frame that is stopped; up to eight watches are
kept, and they survive a new run. Pick another frame and Locals
follows it.

Locals names each variable as the program spells it and keeps its
rows in place from stop to stop; a value changed since the last
stop is in its own colour. An array opens ten elements at a time:
Enter on its last row shows ten more. Enter on a number or a string
asks for a new value, any expression, set as an assignment in the
stopped call; an older call's values cannot be set. A string too
long for its row opens in a box, whole.

Ctrl+F9 asks for an expression in the Break When box. The run stops
when it turns true or, with When it changes, each time its value
changes. It is checked at each statement of the procedure it was set
in, which slows the run. With Only at this line it is instead the
condition of a breakpoint on the cursor's line, drawn in its own
colour: the run stops there when it is true. Watch lists these
marked with a diamond; Del drops one, with its breakpoint.

Each line of the stopped procedure shows, dim after its end, the
values of the numbers and strings it names, as Locals has them;
another frame chosen, that frame's lines and values. They go when
the program is changed or runs on.

Resting the mouse on a variable in the program shows its value.
Drag the pane's left border to change its width.

## Example
    F9             breakpoint on a line inside a loop
    Shift+F5       run: the pane opens
    F6             focus the pane; Tab to Watch
    Ins            type  total * 2  and Enter: a watch is added
    F5             continue: the watch shows its new value
