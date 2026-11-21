' What a touch does to Charlie: the bounce, the stop or the
' hurt.
defint A-Z
option explicit

from engine.clock import STEPT
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

' Into a burrow, for its other end, d.
sub Burrow (p as Hero, d as integer)
    p.dig = d
    p.ground = 0
    p.crouch = 0
    p.gsp = 0
    p.vel.y = 0
end sub

' On the ground, v at least the way he goes.
sub Rush (p as Hero, v as single)
    dim s as integer

    s = 1 + 2 * (p.gsp < 0)
    if p.ground and p.gsp * s < v then p.gsp = s * v
end sub

' Onto a sled, curled, off to the right at v at least.
sub Aboard (p as Hero, v as single)
    p.sled = -1
    p.roll = -1
    p.jumped = 0
    p.face = 0

    if p.ground then
        if p.gsp < v then p.gsp = v
    elseif p.vel.x < v then
        p.vel.x = v
    end if
end sub

' Stopped dead in deep snow: uncurled, a quarter of his
' speed left.
sub Mire (p as Hero)
    p.roll = 0
    p.gsp = p.gsp * .25
end sub

' Thrown back to x at v.
sub Repel (p as Hero, x as single, v as single)
    p.at.x = x

    if p.ground then
        p.gsp = v
    else
        p.vel.x = v
    end if
end sub

' A hop more in a row, up to the most less one.
sub Hop (p as Hero, most as integer)
    if p.hops < most - 1 then p.hops = p.hops + 1
end sub

' Faster by acc a second till he goes at v.
sub Gather (p as Hero, v as single, acc as single)
    if p.gsp < v then p.gsp = p.gsp + acc * STEPT
end sub

' Down a chimney, for the one it leads to, d.
sub Descend (p as Hero, d as integer)
    p.dig = d
    p.vel.y = 0
end sub

' Thrown up from height y at v, his way kept.
sub Toss (p as Hero, y as integer, v as single)
    p.at.y = y
    p.vel.y = -v
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
end sub

' Thrown off at vx, vy, sprung.
sub Fling (p as Hero, vx as single, vy as single)
    p.ground = 0
    p.vel.x = vx
    p.vel.y = vy
    p.jumped = 0
    p.sprung = -1
end sub

' On through a hoop at v, the way he was going, and up.
sub Spur (p as Hero, v as single)
    p.vel.x = v * (1 + 2 * (p.vel.x < 0))
    p.vel.y = -180
    p.sprung = -1
end sub

' Checked to v the way he goes, on the ground or not.
sub Check (p as Hero, v as single)
    if p.ground then
        p.gsp = v
    else
        p.vel.x = v
    end if
end sub

' Thrown off the ground at vx, vy.
sub Knock (p as Hero, vx as single, vy as single)
    p.ground = 0
    p.jumped = 0
    p.vel.x = vx
    p.vel.y = vy
end sub

' Knocked aside in the air at vx, vy.
sub Bat (p as Hero, vx as single, vy as single)
    p.vel.x = vx
    p.vel.y = vy
end sub

' Falling onto something, up off it at v.
sub Recoil (p as Hero, v as single)
    if not p.ground and p.vel.y > 0 then p.vel.y = -v
end sub

' Charlie stopped r from x, on the side of it he is.
sub Balk (p as Hero, x as integer, r as integer)
    dim s as integer

    s = 1 + 2 * (p.at.x < x)
    p.at.x = x + s * r

    if p.ground then
        if p.gsp * s < 0 then p.gsp = 0
    elseif p.vel.x * s < 0 then
        p.vel.x = 0
    end if
end sub

' Stopped dead at x, the furthest he goes.
sub Halt (p as Hero, x as single)
    if p.at.x <= x then exit sub

    p.at.x = x
    p.gsp = 0
    p.vel.x = 0
    p.roll = 0
end sub

' Stood still at x or past it: there, glad, facing right.
sub Arrive (p as Hero, x as single)
    if not p.ground or abs(p.gsp) >= 5 or p.at.x < x then exit sub

    p.win = -1
    p.face = 0
end sub
