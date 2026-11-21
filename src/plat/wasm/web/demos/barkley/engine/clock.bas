' The game's time in fixed steps against the screen's frames.
defint A-Z
option explicit

' The game's time against the screen's frames: lag, the time not yet stepped; span, a frame's length as learnt; late,
' what the game is behind the clock; frames, the frames seen.
type Clock
    lag as single
    span as single
    late as single
    frames as single
end type

const STEPT = 1! / 60!

' A frame has been shown, d by the clock after the last: the time the game is behind by, c.lag, moved on. The game
' runs by the clock, the same at any frame rate; but the clock is too coarse to time one frame (TIMER moves 4 to 8 ms
' at a time, 55 on DOS), and stepped by it a frame showed two steps or none. So a frame is taken to last what frames
' have lasted (c.span, learnt from the clock): exactly a step, or half of one, where that is within a few percent, the
' lag then kept off the edge between a step and none; otherwise that long, with a little of what the game is behind
' the clock (c.late) made up each frame. A stall is made up at once.
sub Pace (d as single, c as Clock)
    dim a as single, r as single, ph as single
    c.frames = c.frames + 1
    if d < 3 * c.span or c.frames < 3 then
        a = 1 / c.frames
        if a < .005 then a = .005
        c.span = c.span + (d - c.span) * a
    end if
    r = c.span / STEPT
    a = 0
    if abs(r - 1) < .05 then a = STEPT
    if abs(r - .5) < .03 then a = STEPT / 2
    if a = 0 then
        ByClock d, c
        exit sub
    end if
    c.late = 0
    ph = c.lag - a * int(c.lag / a)
    c.lag = c.lag + (a / 2 - ph) * .05
    if d > 2.5 * a then a = a * cint(d / a)
    c.lag = c.lag + a
end sub

' A frame of no step's length: that long, with a little of what the game is behind the clock made up; a stall is
' made up at once.
private sub ByClock (d as single, c as Clock)
    dim a as single
    a = c.span + c.late * .02
    c.late = c.late + d - a
    if abs(c.late) > .1 then
        a = a + c.late
        c.late = 0
    end if
    if a < 0 then a = 0
    c.lag = c.lag + a
end sub

' The steps the game is behind by, taken off its lag.
function Steps% (c as Clock)
    dim n as integer
    if c.lag > .1 then c.lag = .1
    do while c.lag >= STEPT
        c.lag = c.lag - STEPT
        n = n + 1
    loop
    Steps% = n
end function

function Elapsed! (a as single, b as single)
    dim d as single
    d = b - a
    if d < 0 or d > .1 then d = .1
    Elapsed! = d
end function
