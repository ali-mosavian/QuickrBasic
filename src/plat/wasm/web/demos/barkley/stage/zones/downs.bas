' The Amber Downs: the Mob's cats, bones, springs, boost
' pads and marker posts.
defint A-Z
option explicit

from engine.events import Events
from barkley.model.images import ICAT, ICATF
from barkley.model.hero import Hero
from barkley.model.level import Level, Realm
from barkley.model.game import Found, Game, Gild, Loose, Session
from barkley.model.events import EVBURST, EVSCORE, EVSHAKE, EVSOUND
from barkley.model.events import Tell, TellAt
from barkley.assets.tunes import SBONE, SBOOST, SGOLD, SMARK
from barkley.world.level import GoldNo
from barkley.stage.fx.parts import PBONE, PBOOST, PGOLD, PMARK
from barkley.stage.fx.damage import Tackle
from barkley.stage.hero.push import Boost
from barkley.stage.things.react import Turn

const GOLDPTS = 1000
private const BOOSTV = 660!

' A bone where the level put it: taken.
sub MeetBone ( _
    lv as Level, _
    ev as Events, _
    g as Game, _
    p as Hero, _
    i as integer _
)
    if lv.things(i).state = 0 _
        or abs(lv.things(i).at.x - cint(p.at.x)) >= 12 then exit sub
    if abs(lv.things(i).at.y - cint(p.at.y) + 13) >= 16 then exit sub

    Turn lv, i, 0
    g.bones = g.bones + 1
    Tell ev, EVSCORE, 10
    TellAt ev, EVBURST, csng(lv.things(i).at.x), _
        csng(lv.things(i).at.y), PBONE
    Tell ev, EVSOUND, SBONE
end sub

' A golden bone not found before: found, for good.
sub MeetGold ( _
    lv as Level, _
    zn as Realm, _
    ev as Events, _
    se as Session, _
    p as Hero, _
    i as integer _
)
    dim n as integer

    if abs(lv.things(i).at.x - cint(p.at.x)) >= 12 _
        or abs(lv.things(i).at.y - cint(p.at.y) _
        + 13) >= 16 then exit sub

    n = GoldNo%(lv, i)
    if Found%(se, zn.stage, n) then exit sub

    Gild se, zn.stage, n
    Tell ev, EVSCORE, GOLDPTS
    TellAt ev, EVBURST, csng(lv.things(i).at.x), _
        csng(lv.things(i).at.y), PGOLD
    Tell ev, EVSOUND, SGOLD
end sub

' A cat of the Mob, or a squirrel: rolled or charged into it
' is beaten, and he bounces off it; else it scratches.
sub MeetCat ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    lo() as Loose _
)
    if lv.things(i).state = 0 _
        or abs(lv.things(i).at.x - cint(p.at.x)) >= 15 then exit sub
    if abs(lv.things(i).at.y - 6 - cint(p.at.y) _
        + 13) >= 17 then exit sub

    Tackle lv, ev, se, g, p, i, 6, lo()
end sub

' A boost pad: run over slower than it sends, he is sent the
' way he faces.
sub MeetBoost (lv as Level, ev as Events, p as Hero, i as integer)
    if not p.ground _
        or abs(lv.things(i).at.x - cint(p.at.x)) >= 12 then exit sub
    if abs(cint(p.at.y) - lv.things(i).at.y) >= 4 _
        or abs(p.gsp) >= BOOSTV then exit sub

    Boost p, BOOSTV
    Tell ev, EVSHAKE, 20
    TellAt ev, EVBURST, p.at.x, p.at.y - 4, PBOOST
    Tell ev, EVSOUND, SBOOST
end sub

' A marker post: passed, it is where he comes back to.
sub MeetMark ( _
    lv as Level, _
    ev as Events, _
    g as Game, _
    p as Hero, _
    i as integer _
)
    if lv.things(i).state <> 0 _
        or cint(p.at.x) < lv.things(i).at.x then exit sub

    Turn lv, i, 1
    g.home.x = lv.things(i).at.x
    g.home.y = lv.things(i).at.y
    TellAt ev, EVBURST, csng(lv.things(i).at.x), _
        csng(lv.things(i).at.y - 28), PMARK
    Tell ev, EVSOUND, SMARK
end sub

' A cat's picture by its state: startled, turned to what
' barked, or a stride of its prowl the way it goes.
function CatPic% (se as Session, s as integer)
    if abs(s) > 1 then
        CatPic% = ICATF - 2 * (s > 0)
    else
        CatPic% = ICAT + ((se.ticks \ 8) and 1) * 2 - 4 * (s < 0)
    end if
end function
