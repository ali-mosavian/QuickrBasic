' Sparks, dust and the bones knocked loose.
defint A-Z
option explicit

from barkley.model.level import GRAV, Level, MH, PI, TS
from barkley.model.game import Loose, NLOOSE, NPART, Part, Session
from barkley.world.terrain.ground import Solid, Surf

' The bones knocked loose, dt on: each falls and bounces till its time is up or it is off the world's foot.
sub LooseBones (lv as Level, se as Session, lo() as Loose, dt as single)
    dim i as integer, x as integer, y as integer, s as integer
    for i = 0 to NLOOSE - 1
        if lo(i).t <= 0 then continue for
        lo(i).t = lo(i).t - dt
        lo(i).vel.y = lo(i).vel.y + GRAV * .6 * dt
        lo(i).at.x = lo(i).at.x + lo(i).vel.x * dt
        if Solid%(lv, cint(lo(i).at.x), cint(lo(i).at.y) - 3) then lo(i).vel.x = -lo(i).vel.x
        lo(i).at.y = lo(i).at.y + lo(i).vel.y * dt
        x = cint(lo(i).at.x)
        y = cint(lo(i).at.y) + 4
        s = Surf%(lv, x, y)
        if s <= y and y - s < 14 and lo(i).vel.y > 0 then
            lo(i).at.y = s - 4
            lo(i).vel.y = -lo(i).vel.y * .7
        end if
        if lo(i).at.y > MH * TS + 16 then lo(i).t = 0
        if lo(i).t <= 0 then se.loose_n = se.loose_n - 1
    next
end sub

' The particles, dt on: each on its arc till its time is up.
sub Particles (se as Session, pt() as Part, dt as single)
    dim i as integer
    for i = 0 to NPART - 1
        if pt(i).t <= 0 then continue for
        pt(i).t = pt(i).t - dt
        pt(i).at.x = pt(i).at.x + pt(i).vel.x * dt
        pt(i).at.y = pt(i).at.y + pt(i).vel.y * dt
        pt(i).vel.y = pt(i).vel.y + 300 * dt
        if pt(i).t <= 0 then se.part_n = se.part_n - 1
    next
end sub

sub Emit (se as Session, pt() as Part, x as single, y as single, vx as single, vy as single, t as single, c as integer)
    dim i as integer
    if se.part_n >= NPART then exit sub
    for i = 0 to NPART - 1
        if pt(i).t > 0 then continue for
        pt(i).at.x = x
        pt(i).at.y = y
        pt(i).vel.x = vx
        pt(i).vel.y = vy
        pt(i).t = t
        pt(i).c = c
        se.part_n = se.part_n + 1
        exit sub
    next
end sub

' n sparks out from x, y at speed v.
sub Burst (se as Session, pt() as Part, x as single, y as single, n as integer, v as single, c as integer)
    dim i as integer, a as single
    for i = 0 to n - 1
        a = 2 * PI * (i + rnd * .5) / n
        Emit se, pt(), x, y, cos(a) * v, sin(a) * v - v * .5, .5, c
    next
end sub
