# Continue Command
keys: Continue Command

Run, Continue    F5

Carries on from where the program stopped.

See also [Start Command](Start Command),
[Toggle Breakpoint Command](Toggle Breakpoint Command).

## Details
After a breakpoint or a step the program runs on until the next
breakpoint, a STOP, or the end. If it cannot continue (the program
was edited, or has not run) it starts from the top like Start.

## Example
    F9             set a breakpoint inside a loop
    Shift+F5       run: stops at the breakpoint
    F5             continue: stops at it again on the next pass
