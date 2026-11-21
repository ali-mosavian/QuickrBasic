' What play posted in a step, each to its owner.
defint A-Z
option explicit

from engine.events import Event, Taken
from barkley.model.world import World
from barkley.model.events import EVCHASETUNE, EVSCENETUNE, EVSOUND
from barkley.model.events import EVTUNE
from barkley.assets.tunes import ChaseTune, SceneTune, Sfx, ZoneTune

' The step's events, in the order posted, each to its owner's
' modifier; the queue empty after.
sub Deliver (wd as World)
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
        end select
    loop
end sub
