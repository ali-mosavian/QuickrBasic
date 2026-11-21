' What play tells the game: a sound, a song.
defint A-Z
option explicit

from engine.vec import Vec
from engine.events import Events, Post

' A sound effect, its number the amount.
const EVSOUND = 1

' The zone's slot to a song, its number the amount.
const EVTUNE = 2

' The chase's song in the zone's slot, the zone's lead on it.
const EVCHASETUNE = 3

' The level's end's slot to a song, its number the amount.
const EVSCENETUNE = 4

' An event that happened at no place.
sub Tell (ev as Events, what as integer, amount as integer)
    dim nowhere as Vec

    Post ev, what, nowhere, amount
end sub
