' The game's main loop.
defint A-Z
option explicit

from engine.video import Flip, OpenScreen
from engine.input import Poll
from engine.clock import Clock, Elapsed, Pace, Steps, STEPT
from engine.audio import FeedSong
from barkley.model.hero import Hero, Pad
from barkley.model.game import Cue, Game, INTRO, Loose, NCUE, NLOOSE
from barkley.model.game import NPART, Part
from barkley.model.world import World
from barkley.model.kinds import LoadKinds
from barkley.assets.store import BuildArt
from barkley.assets.tunes import LoadSongs, PickSong, SongFor, Sudden
from barkley.world.terrain.ground import LoadTables
from barkley.stage.scene import Render
from barkley.control.keys import ReadPad
from barkley.flow.save import LoadSave
from barkley.flow.intro import IntroDraw, IntroOver, StartIntro
from barkley.flow.states import GoStage, ResetWorld, Tick

' Set up, then read input, step in fixed ticks and draw,
' till Esc on the title.
sub Main (wd as World)
    dim g as Game, p as Hero, k as Pad
    dim lo(NLOOSE - 1) as Loose, pt(NPART - 1) as Part
    dim cu(NCUE) as Cue
    dim now as single, last as single, c as Clock, n as integer

    OpenScreen
    LoadTables wd.lv
    BuildArt wd.art, wd.tn
    LoadKinds wd.kinds, wd.dat
    g.page = 1
    StartIntro wd, g, cu()
    LoadSongs wd.mu
    LoadSave wd.se

    last = timer
    c.span = STEPT

    do
        ' the next frame on the screen (port 3DAh's retrace;
        ' with no display, the next look), the keys read
        ' meanwhile
        do
            Poll wd.kb
        loop until inp(&H3DA) and 8

        now = timer
        Pace Elapsed!(last, now), c
        last = now
        ReadPad wd.kb, wd.mu, k
        ' a picture for each step, none for a frame between
        ' two
        n = Steps%(c)
        if n = 0 then continue do

        do while n
            Advance wd, g, p, k, lo(), pt(), cu()
            n = n - 1
        loop

        PickSong wd.mu, SongFor%(g), Sudden%(wd.se, g)
        FeedSong wd.mu, wd.se.ticks

        if g.mode = INTRO then
            IntroDraw wd, g, p, cu()
        else
            Render wd, g, p, lo(), pt()
        end if

        Flip g.page
    loop until g.quitting

    screen 0
    width 80
end sub

' A step: of the intro, and on to the title once it is over;
' else of the game.
sub Advance ( _
    wd as World, _
    g as Game, _
    p as Hero, _
    k as Pad, _
    lo() as Loose, _
    pt() as Part, _
    cu() as Cue _
)
    if g.mode <> INTRO then
        Tick wd, g, p, k, lo(), pt()
        exit sub
    end if

    if IntroOver%(wd, g, k, cu()) = 0 then exit sub

    if wd.se.scene_next = 0 then
        ResetWorld wd, g, p, lo(), pt()
        exit sub
    end if

    GoStage wd, wd.se.scene_next, g, p, lo(), pt()
    wd.se.scene_next = 0
end sub
