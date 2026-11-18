# Cannot continue
keys: Cannot continue, error 17

Error 17

Continue was chosen but the program cannot go on.

See also: [survival](survival)

## Details

Cause: Continue was chosen after the program was edited so that it
cannot go on, or after it ended.

Remedy: Start the program again with Start on the Run menu.

Editing a line that changes the program's structure, or a program that
has ended, ends the ability to continue.

Trap it with ON ERROR; ERR returns the number.

## Example
    PRINT "a"
    STOP                 ' F5 continues
    ' edit a line here, then Continue
    ' -> Cannot continue: choose Run > Start
