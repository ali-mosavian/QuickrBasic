# Change Dialog
keys: Change Dialog

Opened by [Change Command](Change Command): what to find and what to
put there.

See also [Verify Dialog](Verify Dialog).

## Details
    Find What                 the text to look for
    Change To                 what replaces it (may be empty)
    Match Upper/Lowercase, Whole Word   as in [Find Dialog](Find Dialog)

    Find and Verify   stop at each match and ask
    Change All        replace every match without asking

An empty Change To deletes each match. Cancel or Esc leaves.

## Example
    Alt+S, C       open the box
    Find What:     i
    Tab            to Change To; type  idx
    Tab x2         to Whole Word; Space ticks it
    Alt+A          Change All: only the whole word  i  is replaced
