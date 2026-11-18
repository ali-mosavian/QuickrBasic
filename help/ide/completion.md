# Completion
keys: completion, auto complete, autocomplete

Tab completes the word at the cursor; a list offers only what is valid
there.

See also [editing keys](editing keys), [syntax checking](syntax checking).

## Details
QuickrBasic offers only what is valid at the cursor: after
`x$ = l` the list holds string functions and variables, not
statements.

    Tab         complete the word
    2nd letter  typing a word's second letter opens the list
    Up, Down    choose; PgUp, PgDn move a page
    Tab         take the choice (Enter too, if Tab opened it)
    Esc         close the list
    typing      narrows the list; Backspace widens it

Tab with one fit completes it; with several it inserts what they
share, and pressing it again lists them. A function gets its "(" and
its form on the status bar. The likeliest rest of the word is also
shown dim after the cursor. A line number lists its line.
With nothing to complete, Tab indents.

## Example
    Type:   x$ = le
    The list shows LEFT$, LEN, ... valid here
    Down    choose LEFT$
    Tab     the line reads  x$ = LEFT$(  and the status bar shows
            LEFT$(stringexpression$, n%)
