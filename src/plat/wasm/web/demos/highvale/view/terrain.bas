' The vale as a voxel terrain: a ray a column cast front
' to back, each step's height and light read between
' texels, its span drawn up from the lowest row still
' open, its height and light eased from the step drawn
' below it, so each pixel takes its own ground's tone.
' Past an edge, the steps before it hidden, the height is
' eased from the step just before and the light and fog
' are not: the nearer ground's would bleed up over it. The
' frame is built in an array, written to a surface and
' stretched over the view; index 0, the sky, is not drawn.
option explicit

import gfx.surface, gfx.draw
from gfx.draw import Vec2
from highvale.model.land import Land, LEVELS, PEAK, LIGHTS, SCALE
from highvale.model.land import STEP_PARTS
from highvale.land.palette import ground_tone, GROUNDS
from highvale.land.palette import FOG_STEPS
from highvale.land.maps import ground_of, level_side, level_base
from highvale.view.eye import Camera, focal, VIEW_W, VIEW_H
from highvale.view.eye import MID_X, FAR

' rays cast, each VIEW_W / COLUMNS pixels wide
const COLUMNS = 320
' the first step's distance and stride, in texels; the
' stride doubles with the distance past twice its octave
const FIRST = 1
const FIRST_STRIDE = 0.25
const OCTAVE = 32
' the fog's first step this far, its last at FAR
const FOG_NEAR = 150
' places in 65536ths of a texel, read in 256ths; heights'
' parts in 256ths and 65536ths, and a height step in those
const FINE = 256
const FINER = 65536
const WHOLE = FINER * STEP_PARTS
const WRAP = &HFFFFFF&
' a fog step's palette rows
const FOG_ROWS = GROUNDS * LIGHTS

' Its frame, its steps in stretches of one stride, each
' read from the land's level as coarse as its stride,
' and the tones by height.
type Terrain
    surface as long
    pixels() as byte
    blank() as byte
    firsts() as integer
    strides() as single
    shrinks() as integer
    bases() as long
    sides() as long
    reach() as single
    fogs() as long
    tones() as integer
    stretches as integer
    sea as integer
end type

' A column's ray: where it is, in 65536ths of a texel, its
' move a texel ahead, the row of the horizon, the rows
' above open still, and the height, light and fog last
' drawn, in 65536ths of a part, a level and a step; light
' -1 before any; the step last drawn, and the height of
' the step before.
type Ray
    x as long
    y as long
    dx as single
    dy as single
    horizon as single
    column as integer
    rows_open as integer
    height as long
    shade as long
    fog as long
    drawn as integer
    before as long
end type

' The fog at distance z, in 65536ths of a step: none
' nearer than FOG_NEAR, the last step from FAR, the half
' steps' edges where the steps change.
function fog_at (byval z as single) as long
    dim s as single

    s = (z - FOG_NEAR) * (FOG_STEPS - 1) / (FAR - FOG_NEAR) + 0.5
    if s < 0 then s = 0
    if s > FOG_STEPS - 1 then s = FOG_STEPS - 1
    fog_at = s * FINER
end function

' The stride after one of stride at distance z.
function stride_at ( _
    byval z as single, _
    byval stride as single _
) as single
    if z >= 2 * OCTAVE * stride then stride = stride * 2
    stride_at = stride
end function

' The land's level a stride reads: a texel of it as wide
' as the stride, or the first.
function level_of (byval stride as single) as integer
    dim m as integer

    do while 2 ^ (m + 1) <= stride and m < LEVELS - 1
        m = m + 1
    loop

    level_of = m
end function

' The frame's arrays and surface, and the steps' tables.
sub make_terrain (tv as Terrain)
    dim z as single
    dim stride as single
    dim last as single
    dim n as integer
    dim s as integer
    dim m as integer

    tv.surface = gfx.surface.new(COLUMNS, VIEW_H)
    redim tv.pixels(COLUMNS * VIEW_H - 1)
    redim tv.blank(COLUMNS * VIEW_H - 1)
    redim tv.tones(255)
    tv.sea = -1
    z = FIRST
    stride = FIRST_STRIDE
    s = -1

    do while z < FAR
        stride = stride_at(z, stride)

        if stride <> last then
            s = s + 1
            redim preserve tv.firsts(s + 1)
            redim preserve tv.strides(s)
            redim preserve tv.shrinks(s)
            redim preserve tv.bases(s)
            redim preserve tv.sides(s)
            tv.firsts(s) = n
            tv.strides(s) = stride
            m = level_of(stride)
            tv.shrinks(s) = 2 ^ m
            tv.bases(s) = level_base(m)
            tv.sides(s) = level_side(m)
            last = stride
        end if

        redim preserve tv.reach(n)
        redim preserve tv.fogs(n)
        tv.reach(n) = SCALE * focal() / (z * WHOLE)
        tv.fogs(n) = fog_at(z)
        n = n + 1
        z = z + stride
    loop

    tv.stretches = s + 1
    tv.firsts(tv.stretches) = n
end sub

' Each height's tone, unlit and clear, for the sea's level.
sub set_tones (tv as Terrain, byval sea as integer)
    dim h as integer

    for h = 0 to 255
        tv.tones(h) = ground_tone(ground_of(h, sea), 0, 0)
    next

    tv.sea = sea
end sub

' Rows from top to the open ones' end filled, the height
' hh, light lit and fog haze at top eased to those below
' at the end: each pixel its height's tone, lit, fogged.
sub fill_span ( _
    tv as Terrain, _
    r as Ray, _
    byval top as integer, _
    byval hh as long, _
    byval lit as long, _
    byval haze as long _
)
    dim j as long
    dim rows as integer
    dim rise as long
    dim change as long
    dim thicken as long

    if r.shade < 0 then
        r.height = hh
        r.shade = lit
        r.fog = haze
    end if

    rows = r.rows_open - top
    rise = (r.height - hh) \ rows
    change = (r.shade - lit) \ rows
    thicken = (r.fog - haze) \ rows
    r.height = hh
    r.shade = lit
    r.fog = haze

    for j = top * COLUMNS + r.column to _
            (r.rows_open - 1) * COLUMNS + r.column step COLUMNS
        tv.pixels(j) = tv.tones(hh \ WHOLE) _
            + (haze \ FINER) * FOG_ROWS + lit \ FINER
        hh = hh + rise
        lit = lit + change
        haze = haze + thicken
    next

    r.rows_open = top
end sub

' A stretch's steps; false once nothing farther can show.
function march ( _
    tv as Terrain, _
    ld as Land, _
    r as Ray, _
    byval s as integer, _
    byval eye as single, _
    byval rise as single _
) as integer
    dim k as integer
    dim u as long
    dim v as long
    dim fx as long
    dim fy as long
    dim i as long
    dim a as long
    dim b as long
    dim hh as long
    dim before as long
    dim top as single
    dim row as integer
    dim light as long
    dim sx as long
    dim sy as long
    dim shrink as long
    dim side as long

    sx = r.dx * tv.strides(s)
    sy = r.dy * tv.strides(s)
    shrink = tv.shrinks(s) * FINE
    side = tv.sides(s)
    before = r.before

    for k = tv.firsts(s) to tv.firsts(s + 1) - 1
        u = (r.x and WRAP) \ shrink
        v = (r.y and WRAP) \ shrink
        fx = u and 255
        fy = v and 255
        i = tv.bases(s) + (v \ FINE) * side + u \ FINE
        a = ld.heights(i) * FINE
        a = a + (ld.heights(i + 1) - ld.heights(i)) * fx
        b = ld.heights(i + side) * FINE
        b = b + (ld.heights(i + side + 1) - ld.heights(i + side)) * fx
        hh = a * FINE + (b - a) * fy
        top = r.horizon + (eye - hh) * tv.reach(k)
        r.x = r.x + sx
        r.y = r.y + sy
        if top < 0 then top = 0
        row = -int(-top)

        if row < r.rows_open then
            a = ld.lights(i) * FINE
            a = a + (ld.lights(i + 1) - ld.lights(i)) * fx
            b = ld.lights(i + side) * FINE
            b = b + _
                (ld.lights(i + side + 1) - ld.lights(i + side)) * fx
            light = a * FINE + (b - a) * fy
            if k - 1 <> r.drawn and r.shade >= 0 then
                r.height = before
                r.shade = light
                r.fog = tv.fogs(k)
            end if
            fill_span tv, r, row, hh, light, tv.fogs(k)
            if r.rows_open <= 0 then exit function
            r.drawn = k
        end if

        before = hh

        if r.horizon + rise * tv.reach(k) >= r.rows_open then
            exit function
        end if
    next

    r.before = before
    march = -1
end function

' The terrain seen by k drawn over dst's view.
sub draw_terrain ( _
    byval dst as long, _
    tv as Terrain, _
    ld as Land, _
    k as Camera _
)
    dim corners(3) as Vec2
    dim r as Ray
    dim column as integer
    dim s as integer
    dim f as single
    dim ca as single
    dim sa as single
    dim slope as single
    dim eye as single
    dim rise as single
    dim x as single
    dim across as single

    if tv.sea <> ld.sea then set_tones tv, ld.sea
    tv.pixels = tv.blank
    f = focal()
    ca = cos(k.angle)
    sa = sin(k.angle)
    slope = sin(k.roll) / cos(k.roll)
    eye = k.height * WHOLE
    rise = eye - PEAK * WHOLE
    if rise > 0 then rise = 0

    for column = 0 to COLUMNS - 1
        r.column = column
        x = (column + 0.5) * VIEW_W / COLUMNS - 0.5
        across = (x - MID_X) / f
        r.dx = (ca - sa * across) * FINER
        r.dy = (sa + ca * across) * FINER
        r.horizon = k.horizon + (x - MID_X) * slope
        r.rows_open = VIEW_H
        r.shade = -1
        r.drawn = -1
        r.x = k.x * FINER + r.dx * FIRST
        r.y = k.y * FINER + r.dy * FIRST

        for s = 0 to tv.stretches - 1
            if not march(tv, ld, r, s, eye, rise) then exit for
        next
    next

    gfx.surface.write tv.surface, 0, 0, COLUMNS, VIEW_H, tv.pixels()
    corners(1).x = VIEW_W
    corners(2).x = VIEW_W
    corners(2).y = VIEW_H
    corners(3).y = VIEW_H
    gfx.draw.quad dst, corners(), 0, tv.surface, 0
end sub
