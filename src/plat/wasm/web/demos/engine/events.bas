' What happened in a step, in the order it happened: posted
' by play, handed out to their owners after it.
defint A-Z
option explicit

from engine.vec import Vec

' What happened, where, and how much: its meaning is the
' poster's and its owner's.
type Event
    what as integer
    at as Vec
    amount as integer
end type

' The queue holds EVLAST + 1 events a step.
const EVLAST = 63

' The step's events: n posted, taken of them handed out;
' dropped, those posted to a full queue.
type Events
    q(EVLAST) as Event
    n as integer
    taken as integer
    dropped as integer
end type

' An event to the queue's end; dropped and counted when the
' queue is full.
sub Post ( _
    ev as Events, _
    what as integer, _
    at as Vec, _
    amount as integer _
)
    if ev.n > EVLAST then
        ev.dropped = ev.dropped + 1
        exit sub
    end if

    ev.q(ev.n).what = what
    ev.q(ev.n).at = at
    ev.q(ev.n).amount = amount
    ev.n = ev.n + 1
end sub

' The next event as e; false, and the queue empty, when none
' is left.
function Taken% (ev as Events, e as Event)
    Taken% = 0

    if ev.taken >= ev.n then
        ev.n = 0
        ev.taken = 0
        exit function
    end if

    e = ev.q(ev.taken)
    ev.taken = ev.taken + 1
    Taken% = -1
end function
