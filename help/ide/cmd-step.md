# Step Command
keys: Step Command

Debug, Step    F8

Runs one statement and stops.

See also [Step Over Command](Step Over Command),
[Step Out Command](Step Out Command), [debug pane](debug pane).

## Details
Enters any SUB or FUNCTION the statement calls and stops on its
first statement. If the program is not running it starts and stops at
the first statement. The line about to run is highlighted, and the
[debug pane](debug pane) shows variables, watches and the call stack.

## Example
    F8             start: stopped on the first statement
    F8             run it, stop on the next
    F8             on a call to Greet: stops inside Greet
