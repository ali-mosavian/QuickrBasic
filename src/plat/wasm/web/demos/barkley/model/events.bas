' What play tells the game: a sound, a song, points, a puff.
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

' Points to the score, the amount.
const EVSCORE = 5

' A puff of sparks at its place, its id the amount.
const EVBURST = 6

' An event that happened at no place.
sub Tell (ev as Events, what as integer, amount as integer)
    dim nowhere as Vec

    Post ev, what, nowhere, amount
end sub

' An event at x, y.
sub TellAt ( _
    ev as Events, _
    what as integer, _
    x as single, _
    y as single, _
    amount as integer _
)
    dim at as Vec

    at.x = x
    at.y = y
    Post ev, what, at, amount
end sub
