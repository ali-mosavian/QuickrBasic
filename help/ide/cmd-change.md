# Change Command
keys: Change Command

Search, Change...

Replaces text, one match at a time or all at once.

See also [Find Command](Find Command).

## Details
Opens the [Change Dialog](Change Dialog), which takes what to find
and what to put there. Find and Verify stops at each match and asks
([Verify Dialog](Verify Dialog)). Change All replaces every match
without asking, then says "Change complete". "Match not found" means
nothing matched.

Dim unless the cursor is in a program window.

## Example
    Alt+S, C       open the Change box
    Find What:     count
    Change To:     total
    Alt+A          Change All; the box "Change complete" appears
