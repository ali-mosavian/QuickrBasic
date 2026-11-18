# Menus
keys: menus, menu bar, menu

The menu bar is the top row: File, Edit, View, Search, Run, Debug,
Options, Help.

See also [File Menu](File Menu), [Edit Menu](Edit Menu),
[View Menu](View Menu), [Search Menu](Search Menu).

## Details
Open one with Alt, Alt plus its lit letter, or a click.
Inside a menu:

    Left, Right   neighbouring menu
    Up, Down      item (a separator is skipped)
    Enter         run the item
    letter        run the item with that letter lit
    Esc           close

A dim item cannot be used now: Cut, Copy and Clear want a
selection, Paste wants something copied, and the Find, Change and
breakpoint items want the cursor in a program window. A bullet
before an item means the option is on. An item with ... opens a
dialog box; an arrow opens a sub-menu.

The status bar shows one line about the lit item. F1 on an item
shows its help topic.

The menus: [Run Menu](Run Menu), [Debug Menu](Debug Menu),
[Options Menu](Options Menu), [Help Menu](Help Menu).

## Example
    Alt            the menu bar lights; File is open
    Right x2       move to View
    Down, Enter    run SUBs...
    Esc            close
