# Step Out Command
keys: Step Out Command

Debug, Step Out    Shift+F8

Runs until the current procedure returns.

See also [Step Command](Step Command), [Step Over Command](Step Over Command).

## Details
The rest of the SUB or FUNCTION runs, and the program stops on the
statement after the call in its caller. A breakpoint on the way still
stops it first.

## Example
    F8             step into a Greet SUB
    Shift+F8       the SUB finishes; stopped in the caller
