' The World: every part of the game.
defint A-Z
option explicit

from engine.datafile import DataFile
from engine.video import Store
from engine.input import Keys
from engine.audio import Deck
from engine.events import Events
from barkley.model.hero import Autopilot
from barkley.model.level import Ground, Level, Realm
from barkley.model.game import Session, Tones
from barkley.model.kinds import Kinds

' All of it. What runs the game takes it whole; the rest
' take the parts they use.
type World
    lv as Level
    gr as Ground
    zn as Realm
    art as Store
    tn as Tones
    kb as Keys
    mu as Deck
    se as Session
    pilot as Autopilot
    dat as DataFile
    ev as Events
    kinds as Kinds
end type
