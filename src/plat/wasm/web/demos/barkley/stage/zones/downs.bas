' The Amber Downs: the Mob's cats, bones, springs, boost pads and marker posts.
defint A-Z
option explicit

from barkley.engine.audio import Deck
from barkley.model.images import ICAT, ICATF
from barkley.model.hero import Hero
from barkley.model.level import Level, Realm
from barkley.model.game import Found, Game, Gild, Loose, Part, Session
from barkley.assets.palette import CGOLD
from barkley.assets.tunes import SBONE, SBOOST, Sfx, SGOLD, SMARK, SSPRING
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
sub MeetBone (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    if lv.ob(i).s = 0 or abs(lv.ob(i).x - cint(p.x)) >= 12 then exit sub
    if abs(lv.ob(i).y - cint(p.y) + 13) >= 16 then exit sub
    lv.ob(i).s = 0
    g.bones = g.bones + 1
    g.score = g.score + 10
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y), 4, 50, CGOLD + 4
    Sfx mu, se, SBONE
end sub

' A golden bone not found before: found, for good.
sub MeetGold (lv as Level, zn as Realm, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    dim n as integer
    if abs(lv.ob(i).x - cint(p.x)) >= 12 or abs(lv.ob(i).y - cint(p.y) + 13) >= 16 then exit sub
    n = GoldNo%(lv, i)
    if Found%(se, zn.stage, n) then exit sub
    Gild se, zn.stage, n
    g.score = g.score + GOLDPTS
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y), 10, 90, CGOLD + 4
    Sfx mu, se, SGOLD
end sub

' A cat of the Mob, or a squirrel: rolled or charged into it is beaten, and he bounces off it; else it scratches.
sub MeetCat (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    if lv.ob(i).s = 0 or abs(lv.ob(i).x - cint(p.x)) >= 15 then exit sub
    if abs(lv.ob(i).y - 6 - cint(p.y) + 13) >= 17 then exit sub
    Tackle lv, mu, se, g, p, i, 6, lo(), pt()
end sub

' A spring: come down on or stood on, it throws him up.
sub MeetSpring (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    dim py as integer
    py = cint(p.y)
    if abs(lv.ob(i).x - cint(p.x)) >= 11 or py < lv.ob(i).y - 12 or py > lv.ob(i).y + 2 then exit sub
    if not p.ground and p.vy < 0 then exit sub
    if p.ground then p.vx = p.gsp
    p.ground = 0
    p.vy = -SPRINGV
    p.y = lv.ob(i).y - 12
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
    lv.ob(i).s = 8
    Sfx mu, se, SSPRING
end sub

' A boost pad: run over slower than it sends, he is sent the way he faces.
sub MeetBoost (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    if not p.ground or abs(lv.ob(i).x - cint(p.x)) >= 12 then exit sub
    if abs(cint(p.y) - lv.ob(i).y) >= 4 or abs(p.gsp) >= BOOSTV then exit sub
    p.gsp = BOOSTV * (1 - 2 * p.face)
    g.shake = .2
    Burst se, pt(), p.x, p.y - 4, 8, 150, CGOLD + 4
    Sfx mu, se, SBOOST
end sub

' A marker post: passed, it is where he comes back to.
sub MeetMark (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    if lv.ob(i).s <> 0 or cint(p.x) < lv.ob(i).x then exit sub
    lv.ob(i).s = 1
    g.homeX = lv.ob(i).x
    g.homeY = lv.ob(i).y
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y - 28), 8, 80, CGOLD + 4
    Sfx mu, se, SMARK
end sub

' Thirty times a second: cats near the view pace about their homes,
' turning at edges and walls; springs settle; loose bones bounce and
' fade; sparks fall.
' Cat i a pixel on the way it faces, where its beat, the ground and no wall let it: whether it went.
function Paced% (lv as Level, i as integer)
    dim x as integer, y as integer, s as integer
    Paced% = 0
    x = lv.ob(i).x + lv.ob(i).s
    y = lv.ob(i).y
    s = Surf%(lv, x + lv.ob(i).s * 8, y)
    if abs(x - lv.ob(i).a) > BEAT or abs(s - y) > 6 or Solid%(lv, x + lv.ob(i).s * 8, y - 6) then exit function
    lv.ob(i).x = x
    s = Surf%(lv, x, y)
    if abs(s - y) <= 6 then lv.ob(i).y = s
    Paced% = -1
end function

' A cat's step: startled, it stands and counts its state down to a prowl's; prowling, a pixel along its beat, turned
' at the beat's end, a drop or a wall.
sub Prowl (lv as Level, i as integer)
    if abs(lv.ob(i).s) > 1 then
        lv.ob(i).s = lv.ob(i).s - sgn(lv.ob(i).s)
        exit sub
    end if
    if not Paced%(lv, i) then lv.ob(i).s = -lv.ob(i).s
end sub

' A cat's picture by its state: startled, turned to what barked, or a stride of its prowl the way it goes.
function CatPic% (se as Session, s as integer)
    if abs(s) > 1 then
        CatPic% = ICATF - 2 * (s > 0)
    else
        CatPic% = ICAT + ((se.ticks \ 8) and 1) * 2 - 4 * (s < 0)
    end if
end function
