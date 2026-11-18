# Step Over Command
keys: Step Over Command

Debug, Step Over    F7 or F10

Runs one statement, treating a procedure call as one step.

See also [Step Command](Step Command), [Step Out Command](Step Out Command).

## Details
Like [Step Command](Step Command), but a call to a SUB or FUNCTION
runs through to its end first. The program stops on the next
statement of the same procedure. If the program is not running it
starts and stops at the first statement. A breakpoint inside the call
still stops it.

## Example
    F8             start; stopped on the first line
    F10            over a  Greet  call: its output appears, and
                   the stop is on the line after the call
