' What play posted in a step, each to its owner.
defint A-Z
option explicit

from engine.events import Event, Taken
from barkley.model.world import World
from barkley.model.events import EVCHASETUNE, EVSCENETUNE, EVSCORE
from barkley.model.events import EVSOUND, EVTUNE
from barkley.assets.tunes import ChaseTune, SceneTune, Sfx, ZoneTune
from barkley.model.game import Game

' The step's events, in the order posted, each to its owner's
' modifier; the queue empty after.
sub Deliver (wd as World, g as Game)
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
        end select
    loop
end sub
