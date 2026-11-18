# Exit Command
keys: Exit Command

File, Exit

Leaves QuickrBasic. Unsaved changes are asked about first.

See also [Save Command](Save Command).

## Details
If the program has unsaved changes, a box asks to save them. Yes
saves (and asks for a name if the program is Untitled), No leaves
without saving, Cancel returns to the editor.

## Example
    Alt+F, X       File, Exit
    "Loaded file is not saved. Save it now?" appears
    N              leave without saving
