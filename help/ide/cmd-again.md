# Repeat Last Find Command
keys: Repeat Last Find Command

Search, Repeat Last Find    F3, Ctrl+L

Finds the next match of the last Find text.

See also [Find Command](Find Command).

## Details
Searches from just past the cursor for the text last given to the
[Find Dialog](Find Dialog), with the same options (Match
Upper/Lowercase, Whole Word). The match is selected. The search wraps
at the end of the program. "Match not found" means there is none.

Dim unless the cursor is in a program window.

## Example
    Alt+S, F       Find: type PRINT, press Enter
    F3             select the next PRINT
    F3             and the one after, wrapping at the end
