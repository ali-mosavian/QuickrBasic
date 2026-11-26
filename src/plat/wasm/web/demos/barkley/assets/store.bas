' The picture store: the font and the HUD built,
' CHARLIE.DAT's pictures loaded, a zone's own swapped in.
defint A-Z
option explicit

from engine.datafile import OpenData, Word
from engine.video import Flop, Mirror, NewPic, PicText, ReadZones
from engine.video import Reuse, Seal, SkipPic, Store, Unpack
from barkley.model.images import IGLYPH, IHUD, ITILT, NGLYPH
from barkley.model.game import Tones
from barkley.assets.palette import CINK, CWHITE, CYELLOW, KeepDay
from barkley.assets.palette import ReadTones

const HUDW = 108
const HUDH = 40

' The pictures every zone shows: CHARLIE.DAT's, then those
' built from them or drawn here. A zone's own come after
' them with its level.
sub BuildArt (art as Store, tn as Tones)
    dim i as integer

    LoadArt art, tn

    ' the turned run, as drawn and mirrored: each upside
    ' down too, for the far side of a loop
    for i = 0 to 159
        Flop art, ITILT + i, ITILT + 160 + i
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
' to the store: their count, then each and its mirror. A
' picture's colour 0 is clear, so its mask is passed over.
private sub ReadPics (art as Store)
    dim n as integer, i as integer, k as integer
    dim masked as integer, k2 as integer

    n = Word&(art.dat)

    for i = 1 to n
        k = Word&(art.dat)
        ' its width and height, which its picture holds
        seek #art.dat.num, seek(art.dat.num) + 8
        masked = Word&(art.dat)
        k2 = Word&(art.dat)
        Unpack art, k
        if masked then SkipPic art
        if k2 >= 0 then Mirror art, k, k2
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
' then white. And the HUD's picture.
private sub BuildGlyphs (art as Store)
    dim i as integer, c as integer, ink as integer, k as integer
    dim ch as string

    for c = 0 to 1
        ink = CYELLOW
        if c then ink = CWHITE

        for i = 0 to NGLYPH - 1
            k = IGLYPH + (c * NGLYPH + i) * 2
            ch = mid$("0123456789:xABCDEHILMNOPRST", i + 1, 1)
            NewPic art, k, 9, 9
            PicText art, k, 1, 1, ch, CINK
            PicText art, k, 0, 0, ch, ink
        next
    next

    NewPic art, IHUD, HUDW, HUDH
end sub
