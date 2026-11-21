' The zones reached, kept in CHARLIE.SAV; the zone picked on
' the title.
defint A-Z
option explicit

from barkley.model.hero import Pad
from barkley.model.level import ZONES
from barkley.model.game import Session

private const SAVEMARK = 1128398848

' The furthest zone reached and the golden bones found, from
' CHARLIE.SAV: the first and none without the file, or with
' one that is not the game's; none from a file of before
' there were any.
sub LoadSave (se as Session)
    dim v as long, lo as long, hi as long, f as integer
    se.reached = 0
    f = freefile
    open "CHARLIE.SAV" for binary as #f
    if lof(f) >= 4 then get #f, , v
    if lof(f) >= 12 then
        get #f, , lo
        get #f, , hi
    end if
    if lof(f) = 0 then
        close #f
        kill "CHARLIE.SAV"
    else
        close #f
    end if
    v = v - SAVEMARK
    se.gold_lo = 0
    se.gold_hi = 0
    se.unkept = 0
    if v < 0 or v >= ZONES then exit sub
    se.reached = v
    se.gold_lo = lo and &H3FFFFFFF
    se.gold_hi = hi and 511
end sub

' The save as it stands, to CHARLIE.SAV.
sub Keep (se as Session)
    dim v as long, lo as long, hi as long, f as integer
    v = SAVEMARK + se.reached
    lo = se.gold_lo
    hi = se.gold_hi
    f = freefile
    open "CHARLIE.SAV" for binary as #f
    put #f, , v
    put #f, , lo
    put #f, , hi
    close #f
    se.unkept = 0
end sub

' Zone z is reached: kept in CHARLIE.SAV when it is further
' than any before.
sub Reach (se as Session, z as integer)
    if z <= se.reached then exit sub
    se.reached = z
    Keep se
end sub

' On the title Left and Right pick the zone a game starts
' at, of those reached: a step a press.
sub PickZone (se as Session, k as Pad)
    if (k.left or k.right) and not se.pick_held then
        se.pick = se.pick + k.left - k.right
        if se.pick < 0 then se.pick = 0
        if se.pick > se.reached then se.pick = se.reached
    end if
    se.pick_held = k.left or k.right
end sub
