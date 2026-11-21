' Charlie hurt or tackled, a life lost, one of the Mob
' beaten.
defint A-Z
option explicit

from engine.events import Events
from barkley.model.hero import Hero
from barkley.model.level import Level
from barkley.model.game import Game, Loose, Session
from barkley.model.events import EVBURST, EVSCORE, EVSHAKE, EVSOUND
from barkley.model.events import Tell, TellAt
from barkley.assets.tunes import SHISS, SPOP
from barkley.stage.fx.parts import PBEATEN
from barkley.stage.things.react import Turn
from barkley.stage.hero.hurt import Hurt
from barkley.stage.hero.push import Recoil

const BOUNCEV = 330!

' One of the Mob run into: rolled or charged into it is
' beaten, dy up from its foot, and he bounces off it; else
' it hurts.
sub Tackle ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    i as integer, _
    dy as integer, _
    lo() as Loose _
)
    dim x as integer

    if p.roll = 0 and p.charging = 0 then
        x = lv.things(i).at.x
        Hurt ev, se, g, p, x, lo(), SHISS
        exit sub
    end if

    Beaten lv, ev, i, dy
    Recoil p, BOUNCEV
end sub

' Thing i beaten: gone, its points, a shake and a puff dy
' over its foot.
sub Beaten (lv as Level, ev as Events, i as integer, dy as integer)
    Turn lv, i, 0
    Tell ev, EVSCORE, 100
    Tell ev, EVSHAKE, 15
    TellAt ev, EVBURST, csng(lv.things(i).at.x), _
        csng(lv.things(i).at.y - dy), PBEATEN
    Tell ev, EVSOUND, SPOP
end sub
