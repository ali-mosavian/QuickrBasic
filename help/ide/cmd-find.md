# Find Command
keys: Find Command

Search, Find...

Looks for text in the program.

See also [Repeat Last Find Command](Repeat Last Find Command).

## Details
Opens the [Find Dialog](Find Dialog). The word at the cursor, or the
one-line selection, is offered as the text. OK selects the first
match after the cursor; the search wraps at the end of the program.
"Match not found" means there is none. F3 repeats the search.

Dim unless the cursor is in a program window.

## Example
    Put the cursor on the word  total
    Alt+S, F       the box offers "total"
    Enter          the next "total" is selected
    F3             the one after
