' The ground's line, built from the level's columns.
defint A-Z
option explicit

from barkley.model.level import Ground, KBUTTON, KLOOP, KMOVE, KPIPE
from barkley.model.level import KPLAT, KSEAT, KSPAN, KTRAM, KWAVE
from barkley.model.level import KWAVE2, Level, MH, MW, NONE, PI, TS
from barkley.world.terrain.ground import Bearing, LOOPN, LOOPR, WAVEL

const PIPER = 48
private const PIPEN = 12
const MOVEW = 48
const WAVEA = 96
const NLIMB = 6
const LIMBW = 48

' The ground's line from the columns' tops, left to right, broken where they do not meet; a loop spliced in where
' it stands, a quarter pipe and its wall over the three columns before the wall; then the platforms' tops.
' Where a flat column stands over a lower flat one, or over none, its side shows: for how many tiles down.
sub Faces (lv as Level)
    dim c as integer
    for c = 0 to lv.w - 1
        lv.face_l(c) = 0
        lv.face_r(c) = 0
        if lv.vis(c) <> 1 and lv.vis(c) <> 2 then continue for
        if c > 0 then lv.face_l(c) = Side%(lv, c, c - 1)
        lv.face_r(c) = Side%(lv, c, c + 1)
    next
end sub

private function Side% (lv as Level, c as integer, n as integer)
    Side% = 0
    if lv.top(n) = MH then
        Side% = MH - lv.top(c)
    elseif (lv.vis(n) = 1 or lv.vis(n) = 2) and lv.top(n) > lv.top(c) then
        Side% = lv.top(n) - lv.top(c)
    end if
end function

sub Track (lv as Level, gr as Ground)
    dim c as integer, i as integer
    gr.n = 0
    gr.n_cv = 0
    for c = 0 to MW
        gr.col(c) = -1
    next
    for i = 0 to lv.n_things - 1
        if lv.things(i).kind = KPIPE then
            gr.col((lv.things(i).at.x - PIPER) \ TS) = -2 - i
        end if
    next
    for c = 0 to lv.w - 1
        if gr.col(c) < -1 then
            TrackPipe lv, gr, c
            c = c + PIPER \ TS - 1
        elseif lv.top(c) < MH then
            TrackTile lv, gr, c
        end if
    next
    gr.plat_a = gr.n
    TrackPlats lv, gr
    TrackLimbs lv, gr
    TrackMovers lv, gr
    TrackRibbons lv, gr
    TrackSlides lv, gr
end sub

' A quarter pipe from column c, marked there with the thing that is it: up its curve, then its wall.
private sub TrackPipe (lv as Level, gr as Ground, c as integer)
    dim x as integer, y as integer, j as integer, a as single
    x = c * TS
    y = lv.things(-2 - gr.col(c)).at.y
    gr.col(c) = -1
    if gr.pt(gr.n - 1).at.x <> x or gr.pt(gr.n - 1).at.y <> y then
        Knot gr, csng(x), csng(y), 0
    end if
    gr.cv(gr.n_cv).first = gr.n - 1
    gr.cv(gr.n_cv).far = -1
    gr.cv(gr.n_cv).x_lo = x
    gr.cv(gr.n_cv).x_hi = x + PIPER
    for j = 1 to PIPEN - 1
        a = j * PI / 2 / PIPEN
        Knot gr, x + PIPER * sin(a), y - PIPER + PIPER * cos(a), 2
    next
    Knot gr, csng(x + PIPER), csng(y - PIPER), 2
    j = lv.top(c + PIPER \ TS) * TS
    if j < y - PIPER then Knot gr, csng(x + PIPER), csng(j), 2
    gr.cv(gr.n_cv).last = gr.n - 2
    gr.n_cv = gr.n_cv + 1
end sub

' Column c's top tile, a stretch from its left edge to its right, and the loops stood on it.
private sub TrackTile (lv as Level, gr as Ground, c as integer)
    dim i as integer, j as integer, k as integer, x as integer
    dim y as integer, yl as integer, yr as integer
    x = c * TS
    k = lv.map(c, lv.top(c))
    y = (lv.top(c) + 1) * TS
    ' a tile's top at its edges: its profile's, to the even pixel
    yl = y - (lv.prof(k, 0) and 30)
    yr = y - (lv.prof(k, TS - 1) and 30)
    j = gr.n = 0
    if not j then
        j = gr.pt(gr.n - 1).at.x <> x or gr.pt(gr.n - 1).at.y <> yl
    end if
    if j then Knot gr, csng(x), csng(yl), 0
    gr.col(c) = gr.n - 1
    for i = lv.first(c) to lv.n_things - 1
        if lv.things(i).at.x >= x + TS then exit for
        if lv.things(i).kind = KLOOP then
            TrackLoop gr, lv.things(i).at.x, yl
        end if
    next
    Knot gr, csng(x + TS), csng(yr), 1
end sub

' A loop stood at x, its foot at y: a ring of LOOPN stretches, a curve.
private sub TrackLoop (gr as Ground, x as integer, y as integer)
    dim j as integer, a as single
    Knot gr, csng(x), csng(y), 1
    gr.cv(gr.n_cv).first = gr.n - 1
    gr.cv(gr.n_cv).far = gr.n - 1 + LOOPN \ 2
    gr.cv(gr.n_cv).x_lo = x - LOOPR
    gr.cv(gr.n_cv).x_hi = x + LOOPR
    for j = 1 to LOOPN - 1
        a = j * 2 * PI / LOOPN
        Knot gr, x + LOOPR * sin(a), y - LOOPR + LOOPR * cos(a), 2
    next
    Knot gr, csng(x), csng(y), 2
    gr.cv(gr.n_cv).last = gr.n - 2
    gr.n_cv = gr.n_cv + 1
end sub

' The platforms: those side by side at one height are one stretch.
private sub TrackPlats (lv as Level, gr as Ground)
    dim i as integer, x as integer, x1 as integer, y as integer
    x = NONE
    for i = 0 to lv.n_things - 1
        if lv.things(i).kind <> KPLAT then continue for
        if x <> NONE and lv.things(i).at.x = x1 and lv.things(i).at.y = y then
            x1 = x1 + TS
            continue for
        end if
        Plank gr, x, x1, y
        x = lv.things(i).at.x
        x1 = x + TS
        y = lv.things(i).at.y
    next
    Plank gr, x, x1, y
end sub

' A platform's stretch from x to x1 at height y, where there is one.
private sub Plank (gr as Ground, x as integer, x1 as integer, y as integer)
    if x = NONE then exit sub
    Knot gr, csng(x), csng(y), 0
    Knot gr, csng(x1), csng(y), 1
end sub

' The great oak's limbs' flats, a stretch each.
private sub TrackLimbs (lv as Level, gr as Ground)
    dim n as integer, x as integer
    if lv.oak.x = NONE then exit sub
    for n = 0 to NLIMB - 1
        x = lv.oak.x + LimbX%(n)
        Plank gr, x, x + LIMBW, lv.oak.y - LimbUp%(n)
    next
end sub

' The moving platforms: a stretch each, a curve of one, in the order they were made.
private sub TrackMovers (lv as Level, gr as Ground)
    dim i as integer, k as integer
    for i = 0 to lv.n_things - 1
        if lv.things(i).kind <> KMOVE and lv.things(i).kind <> KSPAN and lv.things(i).kind <> KSEAT and lv.things(i).kind <> KTRAM then continue for
        Knot gr, csng(lv.things(i).at.x), csng(lv.things(i).at.y), 0
        Knot gr, csng(lv.things(i).at.x + MOVEW), csng(lv.things(i).at.y), 3
        lv.movers(k).thing = i
        lv.movers(k).sg = gr.n - 2
        lv.movers(k).cv = gr.n_cv
        lv.movers(k).t = 0
        if lv.movers(k).kind = 4 then
            lv.movers(k).button = ButtonAt%(lv, lv.movers(k).button)
        end if
        gr.cv(gr.n_cv).first = gr.n - 2
        gr.cv(gr.n_cv).last = gr.n - 2
        gr.cv(gr.n_cv).far = -1
        gr.cv(gr.n_cv).x_lo = lv.things(i).at.x
        gr.cv(gr.n_cv).x_hi = lv.things(i).at.x + MOVEW
        gr.n_cv = gr.n_cv + 1
        k = k + 1
    next
end sub

' A bridge's button, the one at x: its number among the things, or x where there is none.
private function ButtonAt% (lv as Level, x as integer)
    dim j as integer
    ButtonAt% = x
    for j = 0 to lv.n_things - 1
        if lv.things(j).kind <> KBUTTON or lv.things(j).at.x <> x then continue for
        ButtonAt% = j
        exit function
    next
end function

' The ribbons: halves that meet are one stretch, 8 of flat at each end to run on and off by.
private sub TrackRibbons (lv as Level, gr as Ground)
    dim i as integer, j as integer, x1 as integer, y as integer
    dim a as single
    x1 = NONE
    for i = 0 to lv.n_things - 1
        if lv.things(i).kind <> KWAVE and lv.things(i).kind <> KWAVE2 then continue for
        if lv.things(i).at.x <> x1 then
            RibbonEnd gr, x1, y
            y = lv.things(i).at.y
            Knot gr, csng(lv.things(i).at.x - 8), csng(y), 0
            gr.cv(gr.n_cv).first = gr.n - 1
            gr.cv(gr.n_cv).far = -2
            gr.cv(gr.n_cv).x_lo = lv.things(i).at.x - 8
            Knot gr, csng(lv.things(i).at.x), csng(y), 2
        end if
        for j = 1 to 12
            a = PI * j / 12 - PI * (lv.things(i).kind = KWAVE2)
            Knot gr, csng(lv.things(i).at.x + j * 16), y - WAVEA * (1 - cos(a)) / 2, 2
        next
        x1 = lv.things(i).at.x + WAVEL
        gr.cv(gr.n_cv).x_hi = x1 + 8
    next
    RibbonEnd gr, x1, y
end sub

' A ribbon's far end at x1, y, where there is one: its flat, and its curve closed.
private sub RibbonEnd (gr as Ground, x1 as integer, y as integer)
    if x1 = NONE then exit sub
    Knot gr, csng(x1 + 8), csng(y), 2
    gr.cv(gr.n_cv).last = gr.n - 2
    gr.n_cv = gr.n_cv + 1
end sub

' The lantern lines: a stretch each from end to end, a curve of one.
private sub TrackSlides (lv as Level, gr as Ground)
    dim i as integer
    for i = 0 to lv.n_ropes - 1
        if lv.ropes(i).fall = 0 then continue for
        Knot gr, csng(lv.ropes(i).at.x), csng(lv.ropes(i).at.y), 0
        Knot gr, csng(lv.ropes(i).at.x + lv.ropes(i).w), csng(lv.ropes(i).at.y + lv.ropes(i).fall), 2
        lv.ropes(i).sg = gr.n - 2
        gr.cv(gr.n_cv).first = gr.n - 2
        gr.cv(gr.n_cv).last = gr.n - 2
        gr.cv(gr.n_cv).far = -1
        gr.cv(gr.n_cv).x_lo = lv.ropes(i).at.x
        gr.cv(gr.n_cv).x_hi = lv.ropes(i).at.x + lv.ropes(i).w
        gr.n_cv = gr.n_cv + 1
    next
end sub

' The line's next point, x, y, a stretch of kind k to it from the last: 0, none.
private sub Knot (gr as Ground, x as single, y as single, k as integer)
    dim i as integer, u as single, v as single, d as single
    gr.pt(gr.n).at.x = x
    gr.pt(gr.n).at.y = y
    gr.pt(gr.n).kind = 0
    if k <> 0 then
        i = gr.n - 1
        u = x - gr.pt(i).at.x
        v = y - gr.pt(i).at.y
        d = sqr(u * u + v * v)
        u = u / d
        v = v / d
        gr.pt(i).dir.x = u
        gr.pt(i).dir.y = v
        gr.pt(i).length = d
        gr.pt(i).kind = k
        gr.pt(i).rise = Bearing!(u, v)
    end if
    gr.n = gr.n + 1
end sub

' Limb n of the oak, 0 its lowest, left and right in turn: its flat's left end from the oak's middle, the middle
' pair's further out.
function LimbX% (n as integer)
    dim d as integer
    d = 40
    if n \ 2 = 1 then d = 88
    if (n and 1) = 0 then d = -d - LIMBW
    LimbX% = d
end function

' And its flat's top over the oak's foot: three tiles a pair, the right a tile over the left.
function LimbUp% (n as integer)
    LimbUp% = (3 + (n \ 2) * 3 + (n and 1)) * TS
end function
