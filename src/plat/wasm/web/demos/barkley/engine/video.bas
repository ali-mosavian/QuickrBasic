' SCREEN 13 on two pages: pictures put, grabbed, masked and mirrored.
defint A-Z
option explicit

from barkley.engine.datafile import DataFile

' The pictures, end to end in img: where each starts, and the next free place. A zone's own go after all the others, at
' zone_at: dat_at, where each zone's start in the data file; zone, whose are there, one more than its number; dat, the file they are in.
type Store
    img() as long
    at() as long
    next_at as long
    zone_at as long
    zone as integer
    dat_at() as long
    dat as DataFile
end type

const SW = 320
const SH = 200

sub OpenScreen
    screen 13, , 1, 0
end sub

' Shows the page just drawn, draws on the other.
sub Flip (page as integer)
    screen , , 1 - page, page
    page = 1 - page
end sub

' Image k upside down as k2.
sub Flop (art as Store, k as integer, k2 as integer, w as integer, h as integer)
    dim y as integer, strip(40) as long
    put (0, 0), art.img(art.at(k)), pset
    for y = 0 to h - 1
        get (0, y)-(w - 1, y), strip
        put (100, h - 1 - y), strip, pset
    next
    Grab art, 100, 0, 100 + w - 1, h - 1, k2
end sub

' Image k, w by h, mirrored as k2: a column at a time.
sub Mirror (art as Store, k as integer, k2 as integer, w as integer, h as integer)
    dim x as integer, strip(40) as long
    put (0, 0), art.img(art.at(k)), pset
    for x = 0 to w - 1
        get (x, 0)-(x, h - 1), strip
        put (100 + w - 1 - x, 0), strip, pset
    next
    Grab art, 100, 0, 100 + w - 1, h - 1, k2
end sub

sub Grab (art as Store, x1 as integer, y1 as integer, x2 as integer, y2 as integer, k as integer)
    art.at(k) = art.next_at
    get (x1, y1)-(x2, y2), art.img(art.next_at)
    art.next_at = art.next_at + (4 + clng(x2 - x1 + 1) * (y2 - y1 + 1) + 3) \ 4
end sub

' The w by h at x, y kept as k, and its mask, drawn at mx, my, as k + 1:
' 255 where it is clear, 0 where drawn.
sub MaskGrab (art as Store, x as integer, y as integer, w as integer, h as integer, mx as integer, my as integer, k as integer)
    dim i as integer, j as integer
    for j = 0 to h - 1
        for i = 0 to w - 1
            if point(x + i, y + j) then
                pset (mx + i, my + j), 0
            else
                pset (mx + i, my + j), 255
            end if
        next
    next
    Grab art, x, y, x + w - 1, y + h - 1, k
    Grab art, mx, my, mx + w - 1, my + h - 1, k + 1
end sub

' Masked image k, w by h, at x, y; nothing when wholly off the screen.
sub Blit (art as Store, x as integer, y as integer, k as integer, w as integer, h as integer)
    if x + w <= 0 or x >= SW or y + h <= 0 or y >= SH then exit sub
    put (x, y), art.img(art.at(k + 1)), and
    put (x, y), art.img(art.at(k)), or
end sub
