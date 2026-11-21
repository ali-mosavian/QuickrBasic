# Find Command
keys: Find Command

Search, Find...

Looks for text in the program.

See also [Repeat Last Find Command](Repeat Last Find Command).

## Details
Opens the [Find Bar](Find Bar). The word at the cursor, or the
one-line selection, is offered as the text. Each key typed selects
the first match; Enter goes on through every SUB, FUNCTION and
module, then wraps. F3 repeats the search after the bar is closed.

Dim unless the cursor is in a program window.

## Example
    Put the cursor on the word  total
    Alt+S, F       the bar offers "total", selected
    Enter          the next "total" is selected
    Esc, F3        the bar closed, the one after
