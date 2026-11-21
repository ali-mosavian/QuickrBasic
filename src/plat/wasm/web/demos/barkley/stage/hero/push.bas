' What a touch does to Charlie: the bounce, the stop or the
' hurt.
defint A-Z
option explicit

from barkley.model.hero import Hero

' Sprung up at v, and gain of his speed along, most at most:
' a run carried into the air.
sub Launch (p as Hero, v as single, gain as single, most as single)
    dim up as single

    if p.ground then p.vel.x = p.gsp
    up = v + gain * abs(p.vel.x)
    if up > most then up = most
    p.ground = 0
    p.vel.y = -up
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
end sub

' His feet at y.
sub Perch (p as Hero, y as integer)
    p.at.y = y
end sub

' Sped up to v the way he faces.
sub Boost (p as Hero, v as single)
    p.gsp = v * (1 - 2 * p.face)
end sub

' Off something's top at v, his jump his again; sprung, as
' by a spring.
sub Rebound (p as Hero, v as single, sprung as integer)
    p.vel.y = -v
    p.jumped = 0
    if sprung then p.sprung = -1
end sub

' Down a flume, curled, rightward at v at least.
sub Shoot (p as Hero, v as single)
    p.roll = -1
    p.face = 0
    if p.gsp < v then p.gsp = v
end sub
