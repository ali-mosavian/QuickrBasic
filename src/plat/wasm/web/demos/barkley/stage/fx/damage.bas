' Charlie hurt or tackled, a life lost, one of the Mob beaten.
defint A-Z
option explicit

from barkley.engine.audio import Deck
from barkley.model.hero import Hero
from barkley.model.level import Level, PI
from barkley.model.game import Game, Loose, NLOOSE, Part, Session
from barkley.assets.palette import CWHITE
from barkley.assets.tunes import SDIE, Sfx, SHISS, SHURT, SPOP
from barkley.stage.fx.parts import Burst

const BOUNCEV = 330!

' One of the Mob run into: rolled or charged into it is beaten, dy up from its foot, and he bounces off it; else
' it hurts.
sub Tackle (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, dy as integer, lo() as Loose, pt() as Part)
    dim x as integer
    if p.roll = 0 and p.charging = 0 then
        x = lv.things(i).at.x
        Hurt mu, se, g, p, x, lo(), SHISS
        exit sub
    end if
    Beaten lv, mu, se, g, pt(), i, dy
    if not p.ground and p.vel.y > 0 then p.vel.y = -BOUNCEV
end sub

' Hit: the bones fly loose and Charlie is knocked back; with none, it is out.
sub Hurt (mu as Deck, se as Session, g as Game, p as Hero, from_x as integer, lo() as Loose, snd as integer)
    dim i as integer, n as integer, a as single, v as single
    if p.inv > 0 or p.dig then exit sub
    ' a snowball takes the hit for him
    if p.snow < 0 then
        p.snow = 1
        p.inv = 1
        exit sub
    end if
    if snd = SHISS then Sfx mu, se, snd
    if g.bones = 0 then
        Lose mu, se, p
        exit sub
    end if
    n = g.bones
    if n > NLOOSE then n = NLOOSE
    g.bones = 0
    for i = 0 to n - 1
        a = PI * (.15 + .7 * i / n)
        v = 240 - 70 * (i and 1)
        lo(i).at.x = p.at.x
        lo(i).at.y = p.at.y - 13
        lo(i).vel.x = cos(a) * v * (1 - 2 * (i and 1)) * .8
        lo(i).vel.y = -sin(a) * v
        lo(i).t = 4
    next
    se.loose_n = n
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
    g.shake = .25
    if snd = SHURT then Sfx mu, se, snd
end sub

sub Lose (mu as Deck, se as Session, p as Hero)
    if p.dead then exit sub
    p.dead = -1
    p.dead_t = 0
    p.vel.y = -340
    p.ground = 0
    Sfx mu, se, SDIE
end sub

' Thing i beaten: gone, its points, a shake and a puff dy over its foot.
sub Beaten (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, i as integer, dy as integer)
    lv.things(i).state = 0
    g.score = g.score + 100
    g.shake = .15
    Burst se, pt(), csng(lv.things(i).at.x), csng(lv.things(i).at.y - dy), 10, 130, CWHITE
    Sfx mu, se, SPOP
end sub
