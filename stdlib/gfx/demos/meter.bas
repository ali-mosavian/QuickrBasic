' A demo's frame rate: frames counted over each PERIOD, and
' the median of the last KEPT counts, so a burst of frames
' or one slow one moves it little.

' seconds a frame rate is counted over
const PERIOD = 0.25
' the counts the rate is the median of
const KEPT = 9

' :::::::::::
' A frame rate being counted.
'
'     Attributes:
'       since:
'         When the count began, in seconds.
'       frames:
'         Frames counted since.
'       taken:
'         Counts taken.
'       shown:
'         The rate, 0 until a count is taken.
' :::::::::::
type Meter
    since as double
    frames as long
    taken as long
    shown as single
end type

' :::::::::::
' Counts a frame.
'
'     Args:
'       m:
'         The meter; since set before the first.
'       rates:
'         Its counts, KEPT of them.
'       now:
'         The frame's time, in seconds.
' :::::::::::
sub count_frame ( _
    m as Meter, _
    rates() as single, _
    byval now as double _
)
    m.frames = m.frames + 1
    span# = now - m.since

    if span# < PERIOD then exit sub

    rates(m.taken mod KEPT) = m.frames / span#
    m.taken = m.taken + 1
    m.since = now
    m.frames = 0
    m.shown = median(rates(), m.taken)
end sub

' :::::::::::
' The median of the counts.
'
'     Args:
'       rates:
'         The counts, KEPT of them.
'       n:
'         Counts taken: the first n are, while under KEPT.
'
'     Returns:
'       Their median.
' :::::::::::
function median (rates() as single, byval n as long) as single
    held% = KEPT

    if n < held% then held% = n

    dim sorted(held% - 1) as single

    for i = 0 to held% - 1
        v! = rates(i)
        j = i

        do while j > 0
            if sorted(j - 1) <= v! then exit do

            sorted(j) = sorted(j - 1)
            j = j - 1
        loop

        sorted(j) = v!
    next

    median = sorted(held% \ 2)
end function

' :::::::::::
' The rate as text.
'
'     Args:
'       m:
'         The meter.
'
'     Returns:
'       Its rate and " fps"; "" until a count is taken.
' :::::::::::
function rate_text$ (m as Meter)
    rate_text$ = ""

    if m.shown <= 0 then exit function

    rate_text$ = str$(int(m.shown + 0.5)) + " fps"
end function
