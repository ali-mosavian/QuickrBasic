' The cast drawn: the family, the baron, cats, the van, the keepsake.
defint A-Z
option explicit

from barkley.engine.video import Blit, Store
from barkley.model.images import FAMF, IBALL, IBARON, IBASKET, ICAMERA
from barkley.model.images import ICAT, IDOOR, IFAM, IFLOAT, IGUITAR
from barkley.model.images import IPEEK, ITEDDY, ITOOLS, IVAN, IWHEEL
from barkley.model.images import NFAM
from barkley.model.level import Realm
from barkley.model.game import Session
from barkley.assets.palette import CHEDGE, CYELLOW

private const FAMSTEP = 36

' The zone's keepsake, its foot's middle at x, y on the screen: the picnic basket, the toolbox, the teddy, the
' camera, the ball.
sub DrawKeep (zn as Realm, art as Store, x as integer, y as integer)
    select case zn.id
    case 0
        Blit art, x - 9, y - 14, IBASKET, 18, 15
    case 1
        Blit art, x - 10, y - 13, ITOOLS, 20, 14
    case 2
        Blit art, x - 7, y - 15, ITEDDY, 14, 16
    case 3
        Blit art, x - 9, y - 13, ICAMERA, 18, 14
    case 4
        Blit art, x - 7, y - 13, IBALL, 14, 14
    case 5
        Blit art, x - 13, y - 12, IGUITAR, 26, 13
    end select
end sub

' The van, its foot's middle at x, y on the screen and wx along the road: it rides a pixel up and down as it goes,
' its wheels turning. a: 1 its back door open, 2 Charlie leaning from its window, open, 4 looking back from its rear one.
sub DrawVan ( _
    art as Store, _
    x as integer, _
    y as integer, _
    wx as integer, _
    a as integer _
)
    dim ry as integer, k as integer
    ry = y - 79 - ((wx \ 40) and 1)
    Blit art, x - 70, ry, IVAN, 140, 80
    k = IWHEEL + ((wx \ 6) and 1) * 2
    Blit art, x - 47, y - 18, k, 18, 18
    Blit art, x + 29, y - 18, k, 18, 18
    if a and 1 then Blit art, x - 73, ry + 31, IDOOR, 12, 36
    if a and 2 then Blit art, x + 13, ry + 32, IPEEK, 30, 20
    if a and 4 then Blit art, x - 17, ry + 32, IPEEK + 2, 30, 20
end sub

' The Mob's float likewise, rattling. a: 1 the Baron sat on its crates.
sub DrawFloat ( _
    art as Store, _
    se as Session, _
    x as integer, _
    y as integer, _
    wx as integer, _
    a as integer _
)
    dim ry as integer, k as integer
    ry = y - 39 - ((wx \ 24) and 1)
    Blit art, x - 44, ry, IFLOAT, 88, 40
    k = IWHEEL + ((wx \ 6) and 1) * 2
    Blit art, x - 33, y - 18, k, 18, 18
    Blit art, x + 15, y - 18, k, 18, 18
    if a and 1 then
        Blit art, x - 42, ry - 35, BaronPic%(se, 0), 40, 44
    end if
end sub

' The Baron's picture: sat, his tail swaying and now and then a blink; or pointing.
function BaronPic% (se as Session, aim as integer)
    dim s as integer, f as integer
    s = (se.ticks \ 12) and 7
    f = s and 3
    if f = 3 then f = 1
    if s = 7 then f = 3
    if aim then f = 4 + ((s \ 2) and 1)
    BaronPic% = IBARON + f * 2
end function

' Who meets Charlie, the middle one's feet at x, y. A picture's first LONG holds its size, as GET left it: each
' stands with its middle column on its place in the line and its last row on the ground.
sub DrawFamily ( _
    art as Store, _
    se as Session, _
    x as integer, _
    y as integer, _
    cheer as integer _
)
    dim n as integer, k as integer, w as integer, h as integer
    for n = 0 to NFAM - 1
        k = FamPic%(se, n, cheer)
        w = (art.img(art.at(k)) and 65535) \ 8
        h = art.img(art.at(k)) \ 65536
        Blit art, x + (n - NFAM \ 2) * FAMSTEP - w \ 2, y - h + 1, k, w, h
    next
end sub

' The picture of the family's n-th: waiting it sways and now and then blinks; cheering it runs its six, each out
' of step with the next.
function FamPic% (se as Session, n as integer, cheer as integer)
    dim s as integer, f as integer
    s = (se.ticks \ 16 + n * 3) mod 12
    f = (s \ 3) and 1
    if s = 10 then f = 2
    if cheer then f = 3 + (se.ticks \ 6 + n * 2) mod 6
    FamPic% = IFAM + (n * FAMF + f) * 2
end function

' A cat of the Mob, a stride of its prowl, west or east. a: 1 the basket on its back, 2 stood still.
sub DrawCat ( _
    zn as Realm, _
    art as Store, _
    se as Session, _
    x as integer, _
    y as integer, _
    a as integer, _
    west as integer _
)
    dim k as integer
    k = ICAT
    if (a and 2) = 0 then k = k + ((se.ticks \ 6) and 1) * 2
    if west then k = k + 4
    Blit art, x - 10, y - 13, k, 20, 14
    if a and 1 then DrawKeep zn, art, x - 2, y - 12
end sub

' Pairs of eyes in a hedge, n of them from x, y, each in a gap of its leaves and blinking in its turn.
sub DrawEyes (se as Session, x as integer, y as integer, n as integer)
    dim i as integer, ex as integer, ey as integer
    for i = 0 to n - 1
        ex = x + i * 26
        ey = y - (i and 1) * 9
        line (ex - 1, ey - 2)-(ex + 7, ey + 3), CHEDGE, BF
        line (ex - 2, ey - 1)-(ex + 8, ey + 2), CHEDGE, BF
        if (se.ticks + i * 37) mod 90 < 6 then continue for
        line (ex, ey)-(ex + 1, ey + 1), CYELLOW, BF
        line (ex + 5, ey)-(ex + 6, ey + 1), CYELLOW, BF
    next
end sub
