' OPTION DIALECT QUICKR
' time: the clock, TIMER's, and the date
' seconds since midnight
public native function ticks() as double
native function date_text() as string
native function time_text() as string

' the date and time now: 2026-11-24 09:30:00
public function now() as string
    let d = date_text()
    return f"{d[6..]}-{d[..5]} {time_text()}"
end function

' waits seconds, ticks() watched
public function sleep(seconds as double)
    let start = ticks()
    while since(start) < seconds
    wend
end function

' seconds since start, a ticks(); past midnight too
function since(start as double) as double
    let d = ticks() - start
    if d < 0 then return d + 86400
    return d
end function

' a frame clock: tick() gives the seconds since the last tick, 0 the first
public type Clock
    last as double = -1

    fn tick(mut self) as double
        var d = 0.0
        if self.last >= 0 then d = since(self.last)
        self.last = ticks()
        return d
    end fn
end type
