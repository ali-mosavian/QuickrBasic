# Debug Pane
keys: debug pane, watches, watch, Locals, Call stack, Add Watch Dialog

When the program stops (a breakpoint, a step, Ctrl+F7) a pane opens
at the right with three windows.

    Watch       expressions you chose, reevaluated at every stop
    Locals      the variables of the selected frame, parameters first
    Call stack  the procedures that led here

Keys, with the pane focused (click it, or F6):

    Tab, Shift+Tab  next, previous window of the pane
    Up, Down        choose a row
    Enter           in Call stack, show that caller's line and
                    variables
    Ins             in Watch, add a watch
    Del             in Watch, drop the chosen watch
    Esc             back to the program window

Ins asks for an expression in the Add Watch box. It is evaluated
in the scope of the frame that is stopped; up to eight watches are
kept, and they survive a new run. Pick another frame and Locals
and the watches follow it.

Resting the mouse on a variable in the program shows its value.
Drag the pane's left border to change its width.

See also [Debug Menu](Debug Menu), [Toggle Breakpoint Command](Toggle Breakpoint Command).
