' Charlie hurt: his bones knocked loose and he knocked back;
' with none, a life lost.
defint A-Z
option explicit

from engine.events import Events
from engine.clock import STEPT
from barkley.model.events import EVSHAKE, EVSOUND, Tell
from barkley.model.game import Game, Loose, NLOOSE, Session
from barkley.model.hero import Hero
from barkley.model.level import GRAV
from barkley.assets.tunes import SDIE, SHISS, SHURT
from barkley.stage.fx.parts import Shed

' Hit: the bones fly loose and Charlie is knocked back; with
' none, it is out.
sub Hurt ( _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    from_x as integer, _
    lo() as Loose, _
    snd as integer _
)
    dim n as integer

    if p.inv > 0 or p.dig then exit sub

    ' a snowball takes the hit for him
    if p.snow < 0 then
        p.snow = 1
        p.inv = 1
        exit sub
    end if

    if snd = SHISS then Tell ev, EVSOUND, snd

    if g.bones = 0 then
        Lose ev, se, p
        exit sub
    end if

    n = g.bones
    if n > NLOOSE then n = NLOOSE
    g.bones = 0

    Shed lo(), se, p.at.x, p.at.y - 13, n
    p.ground = 0
    p.roll = 0
    p.charging = 0
    p.crouch = 0
    p.jumped = 0
    p.stun = -1
    p.inv = 2
    p.vel.y = -260

    if p.at.x < from_x then
        p.vel.x = -140
    else
        p.vel.x = 140
    end if

    Tell ev, EVSHAKE, 25
    if snd = SHURT then Tell ev, EVSOUND, snd
end sub

sub Lose (ev as Events, se as Session, p as Hero)
    if p.dead then exit sub

    p.dead = -1
    p.dead_t = 0
    p.vel.y = -340
    p.ground = 0
    Tell ev, EVSOUND, SDIE
end sub

' Out, he rises and falls through everything.
sub Fall (p as Hero)
    p.dead_t = p.dead_t + STEPT
    p.vel.y = p.vel.y + GRAV * STEPT
    p.at.y = p.at.y + p.vel.y * STEPT
end sub

' A fresh Charlie standing at x, y, on stretch sg.
sub Stand (p as Hero, x as integer, y as integer, sg as integer)
    dim fresh as Hero

    p = fresh
    p.at.x = x
    p.at.y = y
    p.ground = -1
    p.sg = sg
end sub
