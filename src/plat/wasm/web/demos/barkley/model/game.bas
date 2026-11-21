' A game's records: its score and mode, the session, loose bones, sparks, cues, the palette's state.
defint A-Z
option explicit

from barkley.engine.vec import Spot, Vec

type Game
    mode as integer
    bones as integer
    lives as integer
    score as long
    bonus as long
    tallied as integer
    sig as long
    clock as single
    cam as Vec
    mode_t as single
    shake as single
    home as Spot
    page as integer
    quitting as integer
end type

' A bone knocked loose: t its time left; with TOONEW of it or more left it is not yet to be taken.
const TOONEW = 3.3
type Loose
    at as Vec
    vel as Vec
    t as single
end type

type Part
    x as single
    y as single
    vx as single
    vy as single
    t as single
    c as integer
end type

' A cue of the intro's script: what, from beat t0 to t1, on its way from x0, y0 to x1, y1 in the world, y from the
' ground and up under 0. ease: 1 gathering speed, 2 losing it; hop: an arc that high over its way; a: its own.
type Cue
    kind as integer
    t0 as single
    t1 as single
    x0 as single
    y0 as single
    x1 as single
    y1 as single
    ease as integer
    hop as integer
    a as integer
end type

' The colours: the shared ones as built, each stage's own over them (how many, which, what), and those shown,
' dusk sixteenths of them.
type Tones
    day(255) as long
    now(255) as long
    n() as integer
    which() as integer
    own() as long
    glint(2) as long
    dusk as integer
end type

' The game's steps; metT: the seconds Charlie has been with its family. The scene playing: the beat it ends at, the
' stage it leads to (0 the title), the zone its road is in. The furthest zone a game has reached, kept in
' CHARLIE.SAV, and the zone picked on the title to start at.
type Session
    ticks as long
    cheer as integer
    met_t as single
    past_t as single
    loose_n as integer
    part_n as integer
    scene_end as integer
    scene_next as integer
    road as integer
    reached as integer
    pick as integer
    gold_lo as long
    gold_hi as long
    unkept as integer
    pulse as long
    tempo as integer
end type

' The zone tune's clock is the session's pulse, PULSES of it a beat: a tick is the tune's beats a minute of them,
' its tempo.
const PULSES = 3600

const NLOOSE = 24, NPART = 48
const TITLE = 0, PLAYING = 1, CLEARED = 2, OVER = 3, INTRO = 4
const NCUE = 112

' The golden bones found are a bit each, three a stage, ten stages to a LONG: goldLo's the first ten, goldHi's the
' rest. unkept: something of the save is not yet in its file.
private function GoldBit& (s as integer, n as integer)
    dim k as integer, v as long
    v = 1
    for k = 1 to (s mod 10) * 3 + n
        v = v * 2
    next
    GoldBit& = v
end function

' Whether stage s's golden bone n is found.
function Found% (se as Session, s as integer, n as integer)
    if s >= 10 then
        Found% = (se.gold_hi and GoldBit&(s, n)) <> 0
    else
        Found% = (se.gold_lo and GoldBit&(s, n)) <> 0
    end if
end function

' Stage s's golden bone n is found, for good.
sub Gild (se as Session, s as integer, n as integer)
    if s >= 10 then
        se.gold_hi = se.gold_hi or GoldBit&(s, n)
    else
        se.gold_lo = se.gold_lo or GoldBit&(s, n)
    end if
    se.unkept = -1
end sub
