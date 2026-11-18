# Video Pages
keys: video pages, pages, page flipping, animation, apage, vpage

Syntax
    SCREEN mode, , apage, vpage
    PCOPY source, destination

A video page is a whole screen held in memory. Drawing goes to the
active page (apage); the visible page (vpage) is on the display.

See also [SCREEN](SCREEN), [PCOPY](PCOPY), [SCREEN modes](SCREEN modes)

## Details
Draw on a hidden page, then show it: the viewer never sees a
half-drawn picture.

QuickrBasic has at least two pages in every SCREEN mode, so PCOPY works
everywhere. QBasic had a single page in modes 1, 2, 12 and 13.

Page counts
    SCREEN 7             8 pages
    SCREEN 8             4 pages
    SCREEN 1, 2, 9, 12, 13   at least 2 (0 and 1)
    SCREEN 0 (text)      8 pages at 25 rows, 4 at 43 or 50 rows

A page number past the mode's count is an Illegal function call, in
SCREEN and in PCOPY alike. Pages count from 0.

Rules
 - Both page numbers are optional: SCREEN , , 1 changes the active
   page only, SCREEN , , , 1 the visible page only.
 - PSET, LINE, PRINT, GET, PUT, POINT and the rest all act on the
   active page. POINT reads the active page too.
 - PCOPY copies page source over page destination.

## Example
    SCREEN 13, , 1, 0           ' draw on page 1, show page 0
    FOR x = 0 TO 300
      CLS
      CIRCLE (x, 100), 20, 12
      PCOPY 1, 0                ' the finished frame appears at once
    NEXT
