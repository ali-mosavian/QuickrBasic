' The screen through gfx, drawn off screen and flipped, and
' the pictures: unpacked from the data file into surfaces,
' put, blitted masked, mirrored and turned upside down.
defint A-Z
option explicit
option disable draw

from gfx import display, surface, draw
from engine.datafile import DataFile, Word

' The screen and the pictures. canvas: the surface drawn and
' flipped. pic: each picture's surface, drawn as turn says,
' FLIP_X and FLIP_Y; made where it is its own, not its
' mirror's. held: the surfaces made; sealed: those every
' zone's. A zone's own come after them, own their numbers:
' zone, whose are there, one more than its number; dat_at,
' where each zone's start in the data file; dat, the file
' they are in.
type Store
    canvas as long
    pic() as long
    turn() as integer
    made() as integer
    held as integer
    sealed as integer
    own() as integer
    n_own as integer
    zone as integer
    dat_at() as long
    dat as DataFile
end type

const SW = 320
const SH = 200
private const TWO_PI = 6.283185

' The screen opened: 320 by 200 by 256 colours, each frame
' shown at the screen's refresh.
sub OpenScreen (art as Store)
    art.canvas = display.open(SW, SH, 8, display.VSYNC)
end sub

sub CloseScreen
    display.close
end sub

' Shows the frame drawn; the next is drawn over it.
sub Flip
    display.flip
end sub

' The screen's colours: 256 of the VGA's, each red, green
' and blue of 0 to 63 as PALETTE takes them.
sub SetColours (dac() as long)
    dim k as integer, c as long, rgb(255) as long

    for k = 0 to 255
        c = dac(k)
        rgb(k) = Eight&(c \ 65536 and 63) _
            + 256& * Eight&(c \ 256 and 63) _
            + 65536 * Eight&(c and 63)
    next

    display.set_palette 0, rgb()
end sub

' A DAC's 6 bits as 8.
private function Eight& (v as long)
    Eight& = v * 4 + v \ 16
end function

' Picture k turned upside down as k2.
sub Flop (art as Store, k as integer, k2 as integer)
    Share art, k, k2, draw.FLIP_Y
end sub

' Picture k mirrored as k2.
sub Mirror (art as Store, k as integer, k2 as integer)
    Share art, k, k2, draw.FLIP_X
end sub

' Picture k2 drawn from k's surface: turned as k is, and by
' flips.
private sub Share ( _
    art as Store, _
    k as integer, _
    k2 as integer, _
    flips as integer _
)
    art.pic(k2) = art.pic(k)
    art.turn(k2) = art.turn(k) xor flips
    art.made(k2) = 0
    Own art, k2
end sub

' A blank w by h picture as k, all colour 0.
sub NewPic (art as Store, k as integer, w as integer, h as integer)
    art.pic(k) = surface.new(w, h)
    art.turn(k) = 0
    art.made(k) = -1
    art.held = art.held + 1
    Own art, k
end sub

' Picture k counted the zone's, when it is.
private sub Own (art as Store, k as integer)
    if art.zone = 0 then exit sub

    art.own(art.n_own) = k
    art.n_own = art.n_own + 1
end sub

function PicW% (art as Store, k as integer)
    PicW% = surface.width(art.pic(k))
end function

function PicH% (art as Store, k as integer)
    PicH% = surface.height(art.pic(k))
end function

' Masked image k, w by h, at x, y: its colour 0 clear;
' nothing when wholly off the screen.
sub Blit ( _
    art as Store, _
    x as integer, _
    y as integer, _
    k as integer, _
    w as integer, _
    h as integer _
)
    if x + w <= 0 or x >= SW or y + h <= 0 or y >= SH then exit sub

    draw.sprite art.canvas, x, y, art.pic(k), art.turn(k)
end sub

' Image k at x, y, its colour 0 too.
sub Paste (art as Store, x as integer, y as integer, k as integer)
    draw.sprite art.canvas, x, y, art.pic(k), _
        art.turn(k) or draw.OPAQUE
end sub

' Image k2 into picture k at x, y, its colour 0 too.
sub PicPaste ( _
    art as Store, _
    k as integer, _
    x as integer, _
    y as integer, _
    k2 as integer _
)
    draw.sprite art.pic(k), x, y, art.pic(k2), _
        art.turn(k2) or draw.OPAQUE
end sub

' The box x1, y1 to x2, y2 of picture k filled.
sub PicFill ( _
    art as Store, _
    k as integer, _
    x1 as integer, _
    y1 as integer, _
    x2 as integer, _
    y2 as integer, _
    c as integer _
)
    draw.rect art.pic(k), x1, y1, x2, y2, c, draw.FILL
end sub

' s in picture k, its first character's top left at x, y.
sub PicText ( _
    art as Store, _
    k as integer, _
    x as integer, _
    y as integer, _
    s as string, _
    c as integer _
)
    draw.text art.pic(k), x, y, s, c
end sub

sub DrawDot (art as Store, x as integer, y as integer, c as integer)
    draw.pset art.canvas, x, y, c
end sub

' A line, both its ends.
sub DrawLine ( _
    art as Store, _
    x1 as integer, _
    y1 as integer, _
    x2 as integer, _
    y2 as integer, _
    c as integer _
)
    draw.line art.canvas, x1, y1, x2, y2, c
end sub

' The box x1, y1 to x2, y2 filled.
sub FillBox ( _
    art as Store, _
    x1 as integer, _
    y1 as integer, _
    x2 as integer, _
    y2 as integer, _
    c as integer _
)
    draw.rect art.canvas, x1, y1, x2, y2, c, draw.FILL
end sub

' The box x1, y1 to x2, y2's edge.
sub DrawBox ( _
    art as Store, _
    x1 as integer, _
    y1 as integer, _
    x2 as integer, _
    y2 as integer, _
    c as integer _
)
    draw.rect art.canvas, x1, y1, x2, y2, c, 0
end sub

' Every other pixel of the box x1, y1 to x2, y2, those whose
' x + y is even: a line at a time along them, corner to
' corner.
sub Checker ( _
    art as Store, _
    x1 as integer, _
    y1 as integer, _
    x2 as integer, _
    y2 as integer, _
    c as integer _
)
    dim s as integer, xa as integer, xb as integer

    for s = x1 + y1 + ((x1 + y1) and 1) to x2 + y2 step 2
        xa = s - y1
        if xa > x2 then xa = x2
        xb = s - y2
        if xb < x1 then xb = x1
        draw.line art.canvas, xa, s - xa, xb, s - xb, c
    next
end sub

' A circle's edge, r round x, y.
sub DrawCircle ( _
    art as Store, _
    x as integer, _
    y as integer, _
    r as integer, _
    c as integer _
)
    draw.circle art.canvas, x, y, r, c, 0
end sub

' An arc of a circle r round x, y, as CIRCLE's: from angle
' a1 to a2 anticlockwise, 0 to the right.
sub DrawArc ( _
    art as Store, _
    x as integer, _
    y as integer, _
    r as integer, _
    a1 as single, _
    a2 as single, _
    c as integer _
)
    draw.arc art.canvas, x, y, r, TWO_PI - a2, TWO_PI - a1, c
end sub

' s at row, col of the screen's 8 by 8 cells, from 1, as
' PRINT after LOCATE: each cell's ground colour 0.
sub Txt ( _
    art as Store, _
    row as integer, _
    col as integer, _
    s as string, _
    c as integer _
)
    dim x as integer, y as integer

    x = (col - 1) * 8
    y = (row - 1) * 8
    draw.rect art.canvas, x, y, x + len(s) * 8 - 1, y + 7, 0, _
        draw.FILL
    draw.text art.canvas, x, y, s, c
end sub

' Store art sized: n pictures, from a data file of zones
' zones.
sub AllotStore (art as Store, n as integer, zones as integer)
    redim art.pic(n)
    redim art.turn(n)
    redim art.made(n)
    redim art.own(n)
    redim art.dat_at(zones - 1)
end sub

' Picture k from the data file, made a surface: a run-length
' stream of LONGs, n then n values, -n then one value n
' times, 0 the end. Its first LONG is its width in bits and
' its height, as GET keeps them; then its pixels, four a
' LONG, by rows.
sub Unpack (art as Store, k as integer)
    dim n as long, i as long, e as long, v as long
    dim w as integer, h as integer
    dim px() as byte

    n = Word&(art.dat)

    do while n
        if n < 0 then v = Word&(art.dat)

        for i = 1 to abs(n)
            if n > 0 then v = Word&(art.dat)

            if e then
                Lay px(), (e - 1) * 4, v
            else
                w = (v and 65535) \ 8
                h = v \ 65536
                redim px(((clng(w) * h + 3) \ 4) * 4 - 1) as byte
            end if

            e = e + 1
        next

        n = Word&(art.dat)
    loop

    NewPic art, k, w, h
    surface.write art.pic(k), 0, 0, w, h, px()
end sub

' The LONG v's four bytes at px(at), lowest first.
private sub Lay (px() as byte, at as long, v as long)
    px(at) = v and 255
    px(at + 1) = (v and &HFF00&) \ 256
    px(at + 2) = (v and &HFF0000) \ 65536
    px(at + 3) = (v and &H7F000000) \ &H1000000 - 128 * (v < 0)
end sub

' Past a picture in the data file, as Unpack reads it.
sub SkipPic (art as Store)
    dim n as long

    n = Word&(art.dat)

    do while n
        if n < 0 then n = 1
        seek #art.dat.num, seek(art.dat.num) + 4 * n
        n = Word&(art.dat)
    loop
end sub

' Where each zone's pictures start in the data file.
sub ReadZones (art as Store)
    dim z as integer

    for z = 0 to ubound(art.dat_at)
        art.dat_at(z) = Word&(art.dat)
    next
end sub

' The pictures so far every zone's: a zone's own go after
' them.
sub Seal (art as Store)
    art.sealed = art.held
end sub

' The last zone's pictures freed, for zone z's.
sub Reuse (art as Store, z as integer)
    dim i as integer, k as integer

    for i = 0 to art.n_own - 1
        k = art.own(i)

        if art.made(k) then
            surface.free art.pic(k)
            art.held = art.held - 1
        end if

        art.pic(k) = 0
        art.turn(k) = 0
        art.made(k) = 0
    next

    art.n_own = 0
    art.zone = z + 1
end sub
