# Verify Dialog
keys: Verify Dialog

Shown at each match by Find and Verify in the
[Change Dialog](Change Dialog).

See also [Change Command](Change Command).

## Details
The match is selected in the program behind the box.

    Change   replace it and go on
    Skip     leave it and go on
    Cancel   stop

At the end "Change complete" appears, or "Match not found" if
nothing matched.

## Example
    Alt+S, C       Find What: x   Change To: y
    Alt+V          Find and Verify: the first x is selected
    C              change it
    S              skip the next
    "Change complete" at the end
