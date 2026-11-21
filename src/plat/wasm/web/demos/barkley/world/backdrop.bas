' The sky and the tiles drawn.
defint A-Z
option explicit

from engine.video import Blit, SH, Store, SW
from barkley.model.images import IAC, IAE, IAF, IAL, IAS, IAU, IDK
from barkley.model.images import IWET, IWS, IWT, IZS, IZU
from barkley.model.level import Level, Realm, TS

private const STRIPH = 192

' The sands' ground again as it is under the tide, picture
' for picture, and the water's edge.
private const WETO = IWET - IAF

' A bare side of the ground from x, y0, n tiles down:
' picture k its turf's end, k + 2 the comb's.
private sub Bare ( _
    art as Store, _
    x as integer, _
    y0 as integer, _
    n as integer, _
    k as integer _
)
    dim i as integer, y as integer, j as integer

    j = k
    y = y0

    for i = 1 to n
        if y >= SH then exit for

        if y > -TS then Blit art, x, y, j, TS, TS
        j = k + 2
        y = y + TS
    next
end sub

' The far backdrop an eighth of the view's scroll, the near
' hills half, both wrapping; the hills sink as the view
' rises.
sub DrawBack (zn as Realm, art as Store, cx as integer, cy as integer)
    dim o as integer, y as integer

    o = (cx \ 8) mod SW
    put (-o, 0), art.img(art.at(zn.back)), pset
    if o then put (SW - o, 0), art.img(art.at(zn.back)), pset
    o = (cx \ 2) mod SW
    y = 132 - cy \ 6
    if y >= SH then exit sub

    put (-o, y), art.img(art.at(zn.back + 2)), and
    put (-o, y), art.img(art.at(zn.back + 1)), or
    if o = 0 then exit sub

    put (SW - o, y), art.img(art.at(zn.back + 2)), and
    put (SW - o, y), art.img(art.at(zn.back + 1)), or
end sub

' Each column in view: its top tile, then its earth in
' strips below.
sub DrawTiles ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    cx as integer, _
    cy as integer _
)
    dim tx as integer, tx1 as integer

    tx1 = cx \ TS + SW \ TS
    if tx1 > lv.w - 1 then tx1 = lv.w - 1

    for tx = cx \ TS to tx1
        if lv.top(tx) * TS - cy >= SH then continue for

        if zn.decked then
            DeckColumn lv, art, tx, cx, cy
        else
            CombColumn lv, art, tx, cx, cy
        end if
    next
end sub

' A column of the works: its deck's top, then its wall,
' which hangs from the world, not the deck: single tiles
' down to its next 128, then strips. A slope's deck runs on
' over the wall under it.
private sub DeckColumn ( _
    lv as Level, _
    art as Store, _
    tx as integer, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, y as integer, v as integer, k as integer
    dim r as integer

    x = tx * TS - cx
    y = lv.top(tx) * TS - cy
    v = lv.vis(tx)

    if y > -TS and v <= 2 then
        put (x, y), art.img(art.at(IDK + (tx and 3))), pset
    end if

    if y > -TS and v > 2 then
        Blit art, x, y, IZS + ((v - 4) * 4 + (tx and 3)) * 2, TS, TS
    end if

    y = y + TS
    k = (tx and 3) * 8
    r = lv.top(tx) + 1

    do while (r and 7) <> 0 and y < SH
        if y > -TS then
            put (x, y), art.img(art.at(IWT + k + (r and 7))), pset
        end if

        r = r + 1
        y = y + TS
    loop

    do while y < SH
        if y > -128 then
            put (x, y), art.img(art.at(IWS + (tx and 3))), pset
        end if

        y = y + 128
    loop

    y = (lv.top(tx) + 1) * TS - cy

    if v <= 2 or y >= SH or y <= -TS then exit sub

    Blit art, x, y, IZU + ((v - 4) * 4 + (tx and 3)) * 2, TS, TS
end sub

' A column of the downs: its turf, then its comb, which
' hangs from the world too, of period 64 by 48: a slope's
' turf runs on into the tile under it; under a pipe, 0, the
' pipe's own picture is the ground. A column whose top the
' tide is over is of the wet pictures, w on from the dry; so
' is the pass's bare ice.
private sub CombColumn ( _
    lv as Level, _
    art as Store, _
    tx as integer, _
    cx as integer, _
    cy as integer _
)
    dim x as integer, y as integer, v as integer, k as integer
    dim r as integer, w as integer

    x = tx * TS - cx
    y = lv.top(tx) * TS - cy
    v = lv.vis(tx)
    r = lv.top(tx)
    k = (tx and 3) * 3
    if r * TS > lv.tide or lv.glaze(tx) then w = WETO

    if y > -TS and v > 2 then
        Blit art, x, y, IAS + w + ((v - 4) * 12 + k + r mod 3) * 2, _
            TS, TS
    end if

    if y > -TS and v > 0 and v <= 2 then
        put (x, y), art.img(art.at(IAF + w + k + r mod 3)), pset
    end if

    y = y + TS

    if v > 2 and y < SH then
        if y > -TS then
            put (x, y), _
                art.img(art.at(IAU + w + (v - 4) * 12 + k _
                + r mod 3)), pset
        end if

        y = y + TS
        r = r + 1
    end if

    r = r + 1

    do while r mod 3 <> 0 and y < SH
        if y > -TS then
            put (x, y), art.img(art.at(IAC + w + k + r mod 3)), pset
        end if

        r = r + 1
        y = y + TS
    loop

    do while y < SH
        if y > -STRIPH then
            put (x, y), art.img(art.at(IAL + w + (tx and 3))), pset
        end if

        y = y + STRIPH
    loop

    if lv.face_l(tx) then
        Bare art, x, lv.top(tx) * TS - cy, lv.face_l(tx), IAE
    end if

    if lv.face_r(tx) = 0 then exit sub

    Bare art, x, lv.top(tx) * TS - cy, lv.face_r(tx), IAE + 4
end sub
