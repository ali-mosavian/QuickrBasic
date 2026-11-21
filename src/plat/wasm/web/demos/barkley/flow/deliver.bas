' What play posted in a step, each to its owner.
defint A-Z
option explicit

from engine.events import Event, Taken
from barkley.model.world import World
from barkley.model.events import EVBURST, EVCHASETUNE, EVSCENETUNE
from barkley.model.events import EVSCORE, EVSHAKE, EVSOUND, EVTUNE
from barkley.model.game import Game, Part
from barkley.assets.tunes import ChaseTune, SceneTune, Sfx, ZoneTune
from barkley.stage.fx.parts import Puff

' The step's events, in the order posted, each to its owner's
' modifier; the queue empty after.
sub Deliver (wd as World, g as Game, pt() as Part)
    dim e as Event

    do while Taken%(wd.ev, e)
        select case e.what
        case EVSOUND
            Sfx wd.mu, wd.se, e.amount
        case EVTUNE
            ZoneTune wd.mu, e.amount
        case EVCHASETUNE
            ChaseTune wd.mu
        case EVSCENETUNE
            SceneTune wd.mu, e.amount
        case EVSCORE
            g.score = g.score + e.amount
        case EVBURST
            Puff wd.se, pt(), e.at, e.amount
        case EVSHAKE
            g.shake = e.amount / 100
        end select
    loop
end sub
