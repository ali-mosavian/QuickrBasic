# TRON, TROFF Statements
keys: TRON, TROFF

Syntax
    TRON
    TROFF

TRON turns on tracing: in the IDE, each statement is lit in the
editor as it runs. TROFF turns it off. Debug > Trace On does the
same.

See also [Debug Menu](Debug Menu), [STOP](STOP)

## Details

TRON turns on tracing: in the IDE, each statement is lit in the
editor as it runs. TROFF turns it off. Debug > Trace On does the
same.

For finding a bug, the IDE's Step (F8) and the debug pane are easier;
see [Debug Menu](Debug Menu).

Put TRON and TROFF around the part you want to watch.

## Example
    TRON
    a% = 1                          ' each statement lights as it runs
    TROFF
