' The palette: built, tinted, shaded by zone, cycled, shimmering.
defint A-Z
option explicit

from barkley.model.level import Realm
from barkley.model.game import Session, Tones

const CFLAKE = 118
const CSNOW = 117, CICE = 123
const CFLY = 163
const CVINE = 147, CVINED = 156
const CLEAF = 135
const CLOAM = 124
const CNUT = 130
const CBEE = 116, CWING = 102
const CSEA = 141

' The quarter's: its ink, its linen from white, its poles lit to dark, its smoke.
const CQINK = 133, CLINEN = 156, CPOLE = 161, CSOOT = 164

' The palette's ramps, dark to light, as charlieart.py's.
const CHUD = 0, CWHITE = 1, CINK = 2, CYELLOW = 3
private const CRED = 4
const CSCARF = 21
private const CGRASS = 25
const CEARTH = 30, CSTONE = 38, CGOLD = 42
const CWATER = 144, CHEDGE = 245
private const CCHASE = 213

' Colour k is c, shown dusk sixteenths as bright.
sub Tint (tn as Tones, k as integer, c as long)
    dim d as integer
    tn.now(k) = c
    d = tn.dusk
    palette k, (c and 63) * d \ 16 + 256& * ((c \ 256 and 63) * d \ 16) + 65536 * ((c \ 65536) * d \ 16)
end sub

' The tiles that move: a deck's lights run along it, a fan's blades turn, glints go down a pipe; by their colours.
sub Cycle (tn as Tones, se as Session)
    dim i as integer
    for i = 0 to 3
        if i = (se.ticks \ 8 and 3) then
            Tint tn, CCHASE + i, 4144952
        else
            Tint tn, CCHASE + i, 3550218
        end if
        if (i + se.ticks \ 4) and 2 then
            Tint tn, CCHASE + 4 + i, 2235666
        else
            Tint tn, CCHASE + 4 + i, 525317
        end if
        if i = (se.ticks \ 4 and 3) then
            Tint tn, CCHASE + 8 + i, 2242111
        else
            Tint tn, CCHASE + 8 + i, 991026
        end if
    next
end sub

sub Shade (tn as Tones)
    dim k as integer
    for k = 0 to 255
        Tint tn, k, tn.now(k)
    next
end sub

' The Downs' lake glints: its three tones change places, every 16 steps.
sub Shimmer (zn as Realm, tn as Tones, se as Session)
    dim i as integer
    if zn.id <> 0 or (se.ticks and 15) <> 0 then exit sub
    i = se.ticks \ 16
    Tint tn, CWATER + 1, tn.glint(i mod 3)
    Tint tn, CWATER + 2, tn.glint((i + 1) mod 3)
    Tint tn, CWATER + 3, tn.glint((i + 2) mod 3)
end sub
