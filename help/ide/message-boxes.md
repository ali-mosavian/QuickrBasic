# Message Boxes
keys: message boxes, message box, Message Dialog

A short box with a message and buttons: OK, or Yes, No and Cancel.

See also [Open Dialog](Open Dialog),
[Set Next Statement Command](Set Next Statement Command).

## Details
Messages the editor shows:

    File not found
    Path/File access error
    Match not found
    Change complete
    File already exists. Overwrite?
    Loaded file is not saved. Save it now?
    Cannot continue
    Cannot cross module/procedure boundary

Enter takes the default button; Esc cancels. Yes, No and Cancel can
also be chosen by their lit letters.

A syntax error is not: it is lit in the line; see
[syntax checking](syntax checking).

## Example
    Alt+F, O       open the Open box
    type  nosuch   Enter: "File not found" appears
    Enter          close the message
