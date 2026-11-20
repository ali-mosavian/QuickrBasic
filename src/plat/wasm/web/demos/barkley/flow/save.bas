' The zones reached, kept in CHARLIE.SAV; the zone picked on the title.
defint A-Z
option explicit

from barkley.model.hero import Pad
from barkley.model.level import ZONES
from barkley.model.game import Session

private const SAVEMARK = 1128398848

' The furthest zone reached, from CHARLIE.SAV: the first without the file, or with one that is not the game's.
sub LoadSave (se as Session)
    dim v as long
    se.reached = 0
    open "CHARLIE.SAV" for binary as #2
    if lof(2) >= 4 then get #2, , v
    if lof(2) = 0 then
        close #2
        kill "CHARLIE.SAV"
    else
        close #2
    end if
    v = v - SAVEMARK
    if v > 0 and v < ZONES then se.reached = v
end sub

' Zone z is reached: kept in CHARLIE.SAV when it is further than any before.
sub Reach (se as Session, z as integer)
    dim v as long
    if z <= se.reached then exit sub
    se.reached = z
    v = SAVEMARK + z
    open "CHARLIE.SAV" for binary as #2
    put #2, , v
    close #2
end sub

' On the title Left and Right pick the zone a game starts at, of those reached: a step a press.
sub PickZone (se as Session, k as Pad)
    static leaning as integer
    if (k.l or k.r) and not leaning then
        se.pick = se.pick + k.l - k.r
        if se.pick < 0 then se.pick = 0
        if se.pick > se.reached then se.pick = se.reached
    end if
    leaning = k.l or k.r
end sub
