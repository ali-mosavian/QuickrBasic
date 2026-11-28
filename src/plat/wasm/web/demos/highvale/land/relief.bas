' The vale's relief by rule, 0 to 1 over a grid that
' wraps: ridged gradient noise for mountains where a slow
' field lets them rise, rolling noise for the land
' between, the two warped by slow fields so no grid
' shows, then worn by drops of rain.
option explicit

from highvale.model.land import MAP, MAP_MASK

' the mountains' first period, in texels, and their
' octaves; a ridge's offset and the gain of its weight
' on the next octave, Musgrave's ridged multifractal
const RIDGE_PERIOD = 128
const RIDGE_OCTAVES = 4
const RIDGE_OFFSET = 1
const RIDGE_GAIN = 2
' the rolling land's first period and octaves, and its
' share of the relief
const ROLL_PERIOD = 64
const ROLL_OCTAVES = 3
const ROLL_SHARE = 0.5
' the slow fields' period; mountains rise where the
' rising field passes RISE_FROM, fully RISE_SPAN on
const SLOW_PERIOD = 128
const RISE_FROM = 0.05
const RISE_SPAN = 0.35
' how far the warp reads a texel from, in texels
const WARP = 14
' rain: drops, each's moves; a drop keeps INERTIA of its
' way; it carries CAPACITY times its fall, speed and
' water, at least as if falling MIN_FALL; it wears and
' drops ERODE and DEPOSIT of the difference, loses
' EVAPORATE of its water a move, and speeds by GRAVITY
const DROPS = 8000
const DROP_LIFE = 30
const INERTIA = 0.05
const CAPACITY = 4
const MIN_FALL = 0.01
const ERODE = 0.3
const DEPOSIT = 0.3
const EVAPORATE = 0.02
const GRAVITY = 4

' A field of gradient noise of period c, a power of two,
' about -0.7 to 0.7: a random slope at each c-th texel,
' eased between them.
sub gradient_field (f() as single, byval c as long)
    dim n as long
    dim x as long
    dim y as long
    dim k as long
    dim a as single
    dim gx(MAP * MAP \ (c * c) - 1) as single
    dim gy(MAP * MAP \ (c * c) - 1) as single
    dim part(c - 1) as single
    dim ease(c - 1) as single
    dim row0 as long
    dim row1 as long
    dim cx as long
    dim x1 as long
    dim tx as single
    dim ty as single
    dim v as single
    dim p as single
    dim q as single
    dim r as single
    dim s as single

    n = MAP \ c

    for k = 0 to n * n - 1
        a = rnd * 6.2831853
        gx(k) = cos(a)
        gy(k) = sin(a)
    next

    for k = 0 to c - 1
        part(k) = k / c
        ease(k) = part(k) ^ 3 * (part(k) * (part(k) * 6 - 15) + 10)
    next

    for y = 0 to MAP - 1
        row0 = (y \ c) * n
        row1 = ((y \ c + 1) and (n - 1)) * n
        ty = part(y and (c - 1))
        v = ease(y and (c - 1))

        for x = 0 to MAP - 1
            cx = x \ c
            x1 = (cx + 1) and (n - 1)
            tx = part(x and (c - 1))
            p = gx(row0 + cx) * tx + gy(row0 + cx) * ty
            q = gx(row0 + x1) * (tx - 1) + gy(row0 + x1) * ty
            r = gx(row1 + cx) * tx + gy(row1 + cx) * (ty - 1)
            s = gx(row1 + x1) * (tx - 1) + gy(row1 + x1) * (ty - 1)
            p = p + (q - p) * ease(x and (c - 1))
            r = r + (s - r) * ease(x and (c - 1))
            f(y * MAP + x) = p + (r - p) * v
        next
    next
end sub

' Ridged multifractal noise from period c down: each
' octave a ridge where its noise crosses 0, weighted by
' the ridge of the octave above, so ridges branch off
' ridges.
sub ridge_field (f() as single, byval c as long)
    dim n(MAP * MAP - 1) as single
    dim weight(MAP * MAP - 1) as single
    dim i as long
    dim o as integer
    dim amp as single
    dim s as single

    amp = 1

    for i = 0 to MAP * MAP - 1
        f(i) = 0
        weight(i) = 1
    next

    for o = 1 to RIDGE_OCTAVES
        gradient_field n(), c

        for i = 0 to MAP * MAP - 1
            s = RIDGE_OFFSET - abs(n(i))
            s = s * s * weight(i)
            f(i) = f(i) + s * amp
            s = s * RIDGE_GAIN
            if s > 1 then s = 1
            weight(i) = s
        next

        c = c \ 2
        amp = amp / 2
    next
end sub

' Gradient noise summed from period c down, each octave
' half the last.
sub roll_field (f() as single, byval c as long)
    dim n(MAP * MAP - 1) as single
    dim i as long
    dim o as integer
    dim amp as single

    amp = 1

    for i = 0 to MAP * MAP - 1
        f(i) = 0
    next

    for o = 1 to ROLL_OCTAVES
        gradient_field n(), c

        for i = 0 to MAP * MAP - 1
            f(i) = f(i) + n(i) * amp
        next

        c = c \ 2
        amp = amp / 2
    next
end sub

' f's value at x, y, between texels, the map wrapping.
function sample ( _
    f() as single, _
    byval x as single, _
    byval y as single _
) as single
    dim ix as long
    dim iy as long
    dim fx as single
    dim fy as single
    dim row0 as long
    dim row1 as long
    dim x1 as long
    dim a as single
    dim b as single

    ix = int(x)
    iy = int(y)
    fx = x - ix
    fy = y - iy
    row0 = (iy and MAP_MASK) * MAP
    row1 = ((iy + 1) and MAP_MASK) * MAP
    x1 = (ix + 1) and MAP_MASK
    ix = ix and MAP_MASK
    a = f(row0 + ix) + (f(row0 + x1) - f(row0 + ix)) * fx
    b = f(row1 + ix) + (f(row1 + x1) - f(row1 + ix)) * fx
    sample = a + (b - a) * fy
end function

' f scaled to 0 to 1 in place.
sub normalize (f() as single)
    dim lo as single
    dim hi as single
    dim i as long

    lo = f(0)
    hi = f(0)

    for i = 1 to MAP * MAP - 1
        if f(i) < lo then lo = f(i)
        if f(i) > hi then hi = f(i)
    next

    if hi = lo then hi = lo + 1

    for i = 0 to MAP * MAP - 1
        f(i) = (f(i) - lo) / (hi - lo)
    next
end sub

' h worn by DROPS drops of rain, each rolling downhill
' from a random texel, wearing the ground where it can
' carry more and leaving what it can't, at the corners
' of the texel it left.
sub rain (h() as single)
    dim d as long
    dim life as integer
    dim px as single
    dim py as single
    dim dx as single
    dim dy as single
    dim speed as single
    dim water as single
    dim carried as single
    dim ix as long
    dim iy as long
    dim fx as single
    dim fy as single
    dim j00 as long
    dim j10 as long
    dim j01 as long
    dim j11 as long
    dim i00 as long
    dim i10 as long
    dim i01 as long
    dim i11 as long
    dim wx as single
    dim wy as single
    dim here as single
    dim last as single
    dim fall as single
    dim room as single
    dim amount as single
    dim l as single

    for d = 1 to DROPS
        px = rnd * MAP
        py = rnd * MAP
        dx = 0
        dy = 0
        speed = 1
        water = 1
        carried = 0

        for life = 0 to DROP_LIFE
            ix = int(px)
            iy = int(py)
            fx = px - ix
            fy = py - iy
            j00 = (iy and MAP_MASK) * MAP + (ix and MAP_MASK)
            j10 = (iy and MAP_MASK) * MAP + ((ix + 1) and MAP_MASK)
            j01 = ((iy + 1) and MAP_MASK) * MAP + (ix and MAP_MASK)
            j11 = ((iy + 1) and MAP_MASK) * MAP _
                + ((ix + 1) and MAP_MASK)
            here = h(j00) + (h(j10) - h(j00)) * fx
            here = here + (h(j01) + (h(j11) - h(j01)) * fx _
                - here) * fy

            if life > 0 then
                fall = last - here
                room = fall
                if room < MIN_FALL then room = MIN_FALL
                room = room * speed * water * CAPACITY

                if carried > room or fall < 0 then
                    if fall < 0 then
                        amount = -fall
                        if amount > carried then amount = carried
                    else
                        amount = (carried - room) * DEPOSIT
                    end if
                    carried = carried - amount
                else
                    amount = (room - carried) * ERODE
                    if amount > fall then amount = fall
                    carried = carried + amount
                    amount = -amount
                end if

                h(i00) = h(i00) + amount * (1 - wx) * (1 - wy)
                h(i10) = h(i10) + amount * wx * (1 - wy)
                h(i01) = h(i01) + amount * (1 - wx) * wy
                h(i11) = h(i11) + amount * wx * wy
                speed = speed * speed + fall * GRAVITY
                if speed < 0 then speed = 0
                speed = sqr(speed)
                water = water * (1 - EVAPORATE)
            end if

            dx = dx * INERTIA - (1 - INERTIA) _
                * ((h(j10) - h(j00)) * (1 - fy) _
                + (h(j11) - h(j01)) * fy)
            dy = dy * INERTIA - (1 - INERTIA) _
                * ((h(j01) - h(j00)) * (1 - fx) _
                + (h(j11) - h(j10)) * fx)
            l = sqr(dx * dx + dy * dy)
            if l = 0 then exit for
            dx = dx / l
            dy = dy / l
            px = px + dx
            py = py + dy
            i00 = j00
            i10 = j10
            i01 = j01
            i11 = j11
            wx = fx
            wy = fy
            last = here
        next
    next
end sub

' The relief, 0 to 1: rolling land with mountains where
' the rising field lets them, warped, then rained on.
sub make_relief (h() as single)
    dim ridge(MAP * MAP - 1) as single
    dim slow(MAP * MAP - 1) as single
    dim warp_x(MAP * MAP - 1) as single
    dim warp_y(MAP * MAP - 1) as single
    dim x as long
    dim y as long
    dim i as long
    dim t as single

    ridge_field ridge(), RIDGE_PERIOD
    normalize ridge()
    roll_field h(), ROLL_PERIOD
    normalize h()
    gradient_field slow(), SLOW_PERIOD

    for i = 0 to MAP * MAP - 1
        t = (slow(i) - RISE_FROM) / RISE_SPAN
        if t < 0 then t = 0
        if t > 1 then t = 1
        t = t * t * (3 - 2 * t)
        slow(i) = h(i) * ROLL_SHARE + ridge(i) * t
    next

    gradient_field warp_x(), SLOW_PERIOD
    gradient_field warp_y(), SLOW_PERIOD

    for y = 0 to MAP - 1
        for x = 0 to MAP - 1
            i = y * MAP + x
            h(i) = sample(slow(), x + warp_x(i) * WARP, _
                y + warp_y(i) * WARP)
        next
    next

    normalize h()
    rain h()
    normalize h()
end sub
