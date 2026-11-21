' The picture store: the font and the HUD built,
' CHARLIE.DAT's pictures loaded, a zone's own swapped in.
defint A-Z
option explicit

from engine.datafile import OpenData, Word
from engine.video import Flop, Grab, MaskGrab, Mirror, ReadZones
from engine.video import Reuse, Seal, Store, Unpack
from barkley.model.images import IGLYPH, IHUD, ITILT, NGLYPH
from barkley.model.game import Tones
from barkley.assets.palette import CINK, CWHITE, CYELLOW, KeepDay
from barkley.assets.palette import ReadTones

const HUDW = 108
const HUDH = 40

' The pictures every zone shows: CHARLIE.DAT's, then those
' built from them or drawn here on the hidden page. A zone's
' own come after them with its level.
sub BuildArt (art as Store, tn as Tones)
    dim i as integer

    cls
    LoadArt art, tn

    ' the turned run, as drawn and mirrored: each upside
    ' down too, for the far side of a loop
    for i = 0 to 159
        Flop art, ITILT + i, ITILT + 160 + i, 32, 32
    next

    BuildGlyphs art
    Seal art

    KeepDay tn
end sub

' The palette and the pictures every zone shows from
' CHARLIE.DAT.
private sub LoadArt (art as Store, tn as Tones)
    OpenData art.dat
    ' past where the tunes, the levels and the kinds start
    seek #art.dat.num, 13
    ReadTones tn, art.dat
    ReadZones art
    ReadPics art
    close #art.dat.num
end sub

' A group of CHARLIE.DAT's pictures, from where the file is,
' to the store: their count, then each, its mask and its
' mirror with it.
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
        Unpack art, k
        if masked then Unpack art, k + 1

        if k2 < 0 then continue for

        Mirror art, k, k2, w, h
        Mirror art, k + 1, k2 + 1, w, h
    next
end sub

' Zone z's own pictures in the store, where the last zone's
' were.
sub LoadZoneArt (art as Store, z as integer)
    if art.zone = z + 1 then exit sub

    Reuse art, z
    OpenData art.dat
    seek #art.dat.num, art.dat_at(z)
    ReadPics art
    close #art.dat.num
end sub

' The HUD's characters, each 9 by 9 over a shadow: yellow,
' then white. And room for the HUD's picture and mask.
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
                    if point(x, 184 + y) = 0 then continue for

                    pset (x + 1, y + 1), CINK
                next
            next

            for y = 0 to 7
                for x = 0 to 7
                    if point(x, 184 + y) then pset (x, y), ink
                next
            next

            MaskGrab art, 0, 0, 9, 9, 20, 0, _
                IGLYPH + (c * NGLYPH + i) * 2
        next
    next

    line (0, 184)-(8, 191), 0, BF
    Grab art, 0, 0, HUDW - 1, HUDH - 1, IHUD
    Grab art, 0, 0, HUDW - 1, HUDH - 1, IHUD + 1
end sub
