' The picture store: the font and the HUD built, CHARLIE.DAT's pictures loaded, a zone's own swapped in.
defint A-Z
option explicit

from barkley.engine.datafile import OpenData, Word
from barkley.engine.video import Flop, Grab, MaskGrab, Mirror, Store
from barkley.model.images import IGLYPH, IHUD, ITILT, NGLYPH
from barkley.model.level import NSTAGE, ZONES
from barkley.model.game import Tones
from barkley.assets.palette import CINK, CWHITE, CYELLOW, Tint

const HUDW = 108
const HUDH = 40

' The pictures every zone shows: CHARLIE.DAT's, then those built from them or drawn here on the hidden page. A
' zone's own come after them with its level.
sub BuildArt (art as Store, tn as Tones)
    dim i as integer
    cls
    LoadArt art, tn
    ' the turned run, as drawn and mirrored: each upside down too, for the far side of a loop
    for i = 0 to 159
        Flop art, ITILT + i, ITILT + 160 + i, 32, 32
    next
    BuildGlyphs art
    art.zone_at = art.next_at
    for i = 0 to 255
        tn.day(i) = tn.now(i)
    next
end sub

' The palette and the pictures every zone shows from CHARLIE.DAT.
private sub LoadArt (art as Store, tn as Tones)
    dim n as integer, i as integer, k as integer, c as long
    tn.dusk = 16
    OpenData art.dat
    c = Word&(art.dat)
    c = Word&(art.dat)
    n = Word&(art.dat)
    for i = 1 to n
        k = Word&(art.dat)
        Tint tn, k, Word&(art.dat)
    next
    for k = 0 to NSTAGE - 1
        tn.n(k) = Word&(art.dat)
        for i = 0 to tn.n(k) - 1
            tn.which(k, i) = Word&(art.dat)
            tn.own(k, i) = Word&(art.dat)
        next
    next
    for k = 0 to ZONES - 1
        art.dat_at(k) = Word&(art.dat)
    next
    ReadPics art
    close #art.dat.num
end sub

' A group of CHARLIE.DAT's pictures, from where the file is, to the store: their count, then each, its mask and
' its mirror with it.
private sub ReadPics (art as Store)
    dim n as integer, i as integer, k as integer, w as integer
    dim h as integer, masked as integer, k2 as integer
    n = Word&(art.dat)
    for i = 1 to n
        k = Word&(art.dat)
        w = Word&(art.dat)
        h = Word&(art.dat)
        masked = Word&(art.dat)
        k2 = Word&(art.dat)
        art.at(k) = art.next_at
        Unpack art
        if masked then
            art.at(k + 1) = art.next_at
            Unpack art
        end if
        if k2 < 0 then continue for
        Mirror art, k, k2, w, h
        Mirror art, k + 1, k2 + 1, w, h
    next
end sub

' Zone z's own pictures in the store, where the last zone's were.
sub LoadZoneArt (art as Store, z as integer)
    if art.zone = z + 1 then exit sub
    art.zone = z + 1
    OpenData art.dat
    seek #art.dat.num, art.dat_at(z)
    art.next_at = art.zone_at
    ReadPics art
    close #art.dat.num
end sub

' A run-length stream of the file's LONGs to art.img at art.nextAt: n then n values, -n then
' one value n times, 0 the end.
private sub Unpack (art as Store)
    dim n as integer, e as long, v as long
    n = Word&(art.dat)
    do while n
        if n > 0 then
            for e = art.next_at to art.next_at + n - 1
                get #art.dat.num, , art.img(e)
            next
        else
            n = -n
            v = Word&(art.dat)
            for e = art.next_at to art.next_at + n - 1
                art.img(e) = v
            next
        end if
        art.next_at = art.next_at + n
        n = Word&(art.dat)
    loop
end sub

' The HUD's characters, each 9 by 9 over a shadow: yellow, then white.
' And room for the HUD's picture and mask.
private sub BuildGlyphs (art as Store)
    dim i as integer, x as integer, y as integer, c as integer
    dim ink as integer
    for c = 0 to 1
        ink = CYELLOW
        if c then ink = CWHITE
        for i = 0 to NGLYPH - 1
            line (0, 0)-(8, 8), 0, BF
            color CWHITE
            locate 24, 1
            print mid$("0123456789:xABCDEHILMNOPRST", i + 1, 1);
            for y = 0 to 7
                for x = 0 to 7
                    if point(x, 184 + y) then
                        pset (x + 1, y + 1), CINK
                    end if
                next
            next
            for y = 0 to 7
                for x = 0 to 7
                    if point(x, 184 + y) then pset (x, y), ink
                next
            next
            MaskGrab art, 0, 0, 9, 9, 20, 0, IGLYPH + (c * NGLYPH + i) * 2
        next
    next
    line (0, 184)-(8, 191), 0, BF
    Grab art, 0, 0, HUDW - 1, HUDH - 1, IHUD
    Grab art, 0, 0, HUDW - 1, HUDH - 1, IHUD + 1
end sub
