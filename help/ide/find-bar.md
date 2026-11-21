# Find Bar
keys: Find Bar, Find Dialog

Opened by [Find Command](Find Command) on the status row: the text to
look for, found as it is typed.

See also [Repeat Last Find Command](Repeat Last Find Command),
[Change Dialog](Change Dialog).

## Details
    Enter, F3            the next match
    Shift+Enter, Shift+F3  the one before
    Alt+C                Match Case on or off
    Alt+W                Whole Word on or off
    Esc                  close, the match left selected

Each key typed selects the first match from where the search began,
in any SUB, FUNCTION or module; with none the cursor goes back there.
The bar counts the matches, and lights those on screen. Any other key
closes the bar and does what it does.

A whole word has no letter, digit or one of ! # $ % & . beside it.

## Example
    Alt+S, F       the bar opens
    type  count    the first  count  is selected: 1 of 4
    Enter          the next one: 2 of 4
