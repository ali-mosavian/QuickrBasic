' The Amber Downs: the Mob's cats, bones, springs, boost pads and marker posts.
defint A-Z
option explicit

from barkley.engine.audio import Deck
from barkley.model.images import ICAT, ICATF
from barkley.model.hero import Hero
from barkley.model.level import Level, Realm
from barkley.model.game import Found, Game, Gild, Loose, Part, Session
from barkley.assets.palette import CGOLD
from barkley.assets.tunes import SBONE, SBOOST, Sfx, SGOLD, SMARK
from barkley.assets.tunes import SSPRING
from barkley.world.terrain.ground import Solid, Surf
from barkley.world.level import GoldNo
from barkley.stage.fx.parts import Burst
from barkley.stage.fx.damage import Tackle

private const SPRINGV = 560!

' A cat's beat: how far from its home it prowls.
const BEAT = 40
const GOLDPTS = 1000
private const BOOSTV = 660!

' A bone where the level put it: taken.
sub MeetBone ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    pt() as Part _
)
    if lv.things(i).state = 0 or abs(lv.things(i).at.x - cint(p.at.x)) >= 12 then exit sub
    if abs(lv.things(i).at.y - cint(p.at.y) + 13) >= 16 then exit sub
    lv.things(i).state = 0
    g.bones = g.bones + 1
    g.score = g.score + 10
    Burst se, pt(), csng(lv.things(i).at.x), csng(lv.things(i).at.y), 4, 50, CGOLD + 4
    Sfx mu, se, SBONE
end sub

' A golden bone not found before: found, for good.
sub MeetGold ( _
    lv as Level, _
    zn as Realm, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    pt() as Part _
)
    dim n as integer
    if abs(lv.things(i).at.x - cint(p.at.x)) >= 12 or abs(lv.things(i).at.y - cint(p.at.y) + 13) >= 16 then exit sub
    n = GoldNo%(lv, i)
    if Found%(se, zn.stage, n) then exit sub
    Gild se, zn.stage, n
    g.score = g.score + GOLDPTS
    Burst se, pt(), csng(lv.things(i).at.x), csng(lv.things(i).at.y), 10, 90, CGOLD + 4
    Sfx mu, se, SGOLD
end sub

' A cat of the Mob, or a squirrel: rolled or charged into it is beaten, and he bounces off it; else it scratches.
sub MeetCat ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose, _
    pt() as Part _
)
    if lv.things(i).state = 0 or abs(lv.things(i).at.x - cint(p.at.x)) >= 15 then exit sub
    if abs(lv.things(i).at.y - 6 - cint(p.at.y) + 13) >= 17 then exit sub
    Tackle lv, mu, se, g, p, i, 6, lo(), pt()
end sub

' A spring: come down on or stood on, it throws him up.
sub MeetSpring ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    p as Hero, _
    i as integer _
)
    dim py as integer
    py = cint(p.at.y)
    if abs(lv.things(i).at.x - cint(p.at.x)) >= 11 or py < lv.things(i).at.y - 12 or py > lv.things(i).at.y + 2 then exit sub
    if not p.ground and p.vel.y < 0 then exit sub
    if p.ground then p.vel.x = p.gsp
    p.ground = 0
    p.vel.y = -SPRINGV
    p.at.y = lv.things(i).at.y - 12
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
    lv.things(i).state = 8
    Sfx mu, se, SSPRING
end sub

' A boost pad: run over slower than it sends, he is sent the way he faces.
sub MeetBoost ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    pt() as Part _
)
    if not p.ground or abs(lv.things(i).at.x - cint(p.at.x)) >= 12 then exit sub
    if abs(cint(p.at.y) - lv.things(i).at.y) >= 4 or abs(p.gsp) >= BOOSTV then exit sub
    p.gsp = BOOSTV * (1 - 2 * p.face)
    g.shake = .2
    Burst se, pt(), p.at.x, p.at.y - 4, 8, 150, CGOLD + 4
    Sfx mu, se, SBOOST
end sub

' A marker post: passed, it is where he comes back to.
sub MeetMark ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    pt() as Part _
)
    if lv.things(i).state <> 0 or cint(p.at.x) < lv.things(i).at.x then exit sub
    lv.things(i).state = 1
    g.home.x = lv.things(i).at.x
    g.home.y = lv.things(i).at.y
    Burst se, pt(), csng(lv.things(i).at.x), csng(lv.things(i).at.y - 28), 8, 80, CGOLD + 4
    Sfx mu, se, SMARK
end sub

' Thirty times a second: cats near the view pace about their homes,
' turning at edges and walls; springs settle; loose bones bounce and
' fade; sparks fall.
' Cat i a pixel on the way it faces, where its beat, the ground and no wall let it: whether it went.
function Paced% (lv as Level, i as integer)
    dim x as integer, y as integer, s as integer
    Paced% = 0
    x = lv.things(i).at.x + lv.things(i).state
    y = lv.things(i).at.y
    s = Surf%(lv, x + lv.things(i).state * 8, y)
    if abs(x - lv.things(i).home) > BEAT or abs(s - y) > 6 or Solid%(lv, x + lv.things(i).state * 8, y - 6) then exit function
    lv.things(i).at.x = x
    s = Surf%(lv, x, y)
    if abs(s - y) <= 6 then lv.things(i).at.y = s
    Paced% = -1
end function

' A cat's step: startled, it stands and counts its state down to a prowl's; prowling, a pixel along its beat, turned
' at the beat's end, a drop or a wall.
sub Prowl (lv as Level, i as integer)
    if abs(lv.things(i).state) > 1 then
        lv.things(i).state = lv.things(i).state - sgn(lv.things(i).state)
        exit sub
    end if
    if not Paced%(lv, i) then lv.things(i).state = -lv.things(i).state
end sub

' A cat's picture by its state: startled, turned to what barked, or a stride of its prowl the way it goes.
function CatPic% (se as Session, s as integer)
    if abs(s) > 1 then
        CatPic% = ICATF - 2 * (s > 0)
    else
        CatPic% = ICAT + ((se.ticks \ 8) and 1) * 2 - 4 * (s < 0)
    end if
end function
