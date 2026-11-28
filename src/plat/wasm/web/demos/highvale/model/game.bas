' The whole game: the state it is in, the mission, and
' everything flying over the vale.
option explicit

from highvale.model.land import Land
from highvale.model.craft import Craft, Controls
from highvale.model.foes import Foe, Shot, Blast
from highvale.model.foes import MOST_FOES, MOST_SHOTS, MOST_BLASTS

' the states
const TITLE = 0
const BRIEFING = 1
const FLYING = 2
const CLEARED = 3
const DOWNED = 4
const VICTORY = 5
const MISSIONS = 3

type Game
    state as integer
    mission as integer
    since as single
    heli as Craft
    asked as Controls
    vale as Land
    foes(MOST_FOES - 1) as Foe
    shots(MOST_SHOTS - 1) as Shot
    blasts(MOST_BLASTS - 1) as Blast
    note as string
    note_left as single
end type
