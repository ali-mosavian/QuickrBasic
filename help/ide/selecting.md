# Selecting Text
keys: selecting text, selection, select text

Hold Shift while moving the cursor, or drag the mouse.

See also [editing keys](editing keys), [mouse](mouse).

## Details
Hold Shift while moving the cursor with any movement key, or drag
the mouse, or Shift+click to extend. The selected text is shown
reversed. Any move without Shift drops the selection, as does Esc.

Typing a character replaces a selection inside one line.
Backspace and Del clear it.

    Shift+Del   [Cut Command](Cut Command)
    Ctrl+Ins    [Copy Command](Copy Command)
    Shift+Ins   [Paste Command](Paste Command)
    Del         [Clear Command](Clear Command)

A selection spanning several lines selects whole lines. Dim menu
items need a selection; the items come alive as soon as there is
one.

## Example
    Ctrl+Right     move to the next word
    Shift+Ctrl+Right   select it
    Shift+Del      cut it
    Esc            would have dropped the selection instead
