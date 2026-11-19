# Clear All Breakpoints Command
keys: Clear All Breakpoints Command

Debug, Clear All Breakpoints

Removes every breakpoint in the program.

See also [Toggle Breakpoint Command](Toggle Breakpoint Command).

## Details
Every mark set with F9 goes at once, in every procedure of the
program. There is no confirmation. Use F9 to remove a single one.

## Example
    F9             on three different lines: three marks
    Alt+D, C       Debug, Clear All Breakpoints: all three go
    Shift+F5       the program now runs to the end
