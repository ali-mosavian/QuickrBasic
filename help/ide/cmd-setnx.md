# Set Next Statement Command
keys: Set Next Statement Command

Debug, Set Next Statement

While stopped, makes the cursor's line the next to run.

See also [Continue Command](Continue Command).

## Details
Skips or repeats code by moving the point where the program resumes.
The line must be in the procedure that is stopped; otherwise "Cannot
cross module/procedure boundary" appears. A program that is not
stopped gives "Cannot continue". Variables keep their values.

## Example
    F9             on the line  PRINT "A"  and shift+F5 to run
    Down           move the cursor to the next line
    Alt+D, N       Set Next Statement
    F5             the PRINT "A" is skipped
