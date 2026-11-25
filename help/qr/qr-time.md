# time Module
keys: ~quickr time, ~quickr time.ticks, ~quickr time.now, ~quickr time.sleep, ~quickr time.Clock

The clock, TIMER's, and the date.

    ticks() as double              seconds since midnight
    now() as string                the date and time: 2026-11-24 09:30:00
    sleep(seconds as double)       waits
    Clock, c.tick() as double      seconds since the clock's last tick

See also [Quickr](quickr index), [keys](quickr keys)

## Details
A Clock's first tick() gives 0. A game's loop ticks one each frame to
move things by the time that passed, past midnight too.

## Example
    ' OPTION DIALECT QUICKR
    import time
    var c as time.Clock
    c.tick()
    time.sleep(0.5)
    print c.tick()
