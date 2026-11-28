' The keys: held ones from the keyboard's port (60h);
' where it reads none, INKEY$'s, each held a moment after
' it is typed.
option explicit

from engine.input import Keys, Poll
from highvale.model.craft import Controls

' scan codes
const ESC_KEY = 1
const ENTER_KEY = 28
const A_KEY = 30
const Z_KEY = 44
const X_KEY = 45
const SPACE_KEY = 57
const UP_KEY = 72
const LEFT_KEY = 75
const RIGHT_KEY = 77
const DOWN_KEY = 80
' a typed key counts as held this long
const TYPED_HOLD = 0.3
' the port's bytes read a frame at most
const POLLS = 16

' The scan code of a key INKEY$ gave; 0 none we use.
function scan_of (k as string) as integer
    select case k
        case chr$(0) + "H"
            scan_of = UP_KEY
        case chr$(0) + "P"
            scan_of = DOWN_KEY
        case chr$(0) + "K"
            scan_of = LEFT_KEY
        case chr$(0) + "M"
            scan_of = RIGHT_KEY
        case "A", "a"
            scan_of = A_KEY
        case "Z", "z"
            scan_of = Z_KEY
        case "X", "x"
            scan_of = X_KEY
        case " "
            scan_of = SPACE_KEY
        case chr$(13)
            scan_of = ENTER_KEY
        case chr$(27)
            scan_of = ESC_KEY
        case else
            scan_of = 0
    end select
end function

' Key scan held: by the port, or typed lately.
function held ( _
    kb as Keys, _
    byval scan as integer, _
    byval now as single _
) as integer
    if kb.port then
        held = kb.held(scan) <> 0
    else
        held = kb.since(scan) > 0 and _
            now - kb.since(scan) < TYPED_HOLD
    end if
end function

' The keys read into what they ask; INKEY$'s typed keys
' kept by time where the port reads none, else let go.
sub read_keys (kb as Keys, ctl as Controls, byval now as single)
    dim i as integer
    dim k as string
    dim scan as integer

    for i = 1 to POLLS
        Poll kb
    next

    do
        k = inkey$
        if k = "" then exit do
        scan = scan_of(k)
        if scan then kb.since(scan) = now
    loop

    ctl.turn = held(kb, LEFT_KEY, now) - held(kb, RIGHT_KEY, now)
    ctl.pitch = held(kb, DOWN_KEY, now) - held(kb, UP_KEY, now)
    ctl.climb = held(kb, Z_KEY, now) - held(kb, A_KEY, now)
    ctl.guns = held(kb, SPACE_KEY, now)
    ctl.launch = held(kb, X_KEY, now)
    ctl.go = held(kb, ENTER_KEY, now)
    ctl.quit = held(kb, ESC_KEY, now)
end sub
