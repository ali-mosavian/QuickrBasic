' A game's records: its score and mode, the session, loose bones, sparks, cues, the palette's state.
defint A-Z
option explicit

type Game
    mode as integer
    bones as integer
    lives as integer
    score as long
    bonus as long
    tallied as integer
    sig as long
    clock as single
    camx as single
    camy as single
    modeT as single
    shake as single
    homeX as integer
    homeY as integer
    page as integer
    quitting as integer
end type

' A bone knocked loose: t its time left.
type Loose
    x as single
    y as single
    vx as single
    vy as single
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
    metT as single
    pastT as single
    looseN as integer
    partN as integer
    sceneEnd as integer
    sceneNext as integer
    road as integer
    reached as integer
    pick as integer
end type

const NLOOSE = 24, NPART = 48
const TITLE = 0, PLAYING = 1, CLEARED = 2, OVER = 3, INTRO = 4
const NCUE = 112
