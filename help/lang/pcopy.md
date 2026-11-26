# PCOPY Statement
keys: PCOPY

Syntax
    PCOPY source, destination

Copies the whole video page number source onto page destination,
replacing it.

See also [SCREEN](SCREEN), [video pages](video pages)

## Details
Page numbers run from 0 up to the last page of the current SCREEN
mode. A page number outside the mode's range is "Illegal function
call".

Use it to draw a frame unseen on one page and show it with SCREEN's
active and visual page arguments, or to keep a copy of a picture.

## Example
    SCREEN 7, , 0, 0
    CIRCLE (160, 100), 40
    PCOPY 0, 1                    ' save the picture on page 1
    CLS
    PCOPY 1, 0                    ' bring it back
