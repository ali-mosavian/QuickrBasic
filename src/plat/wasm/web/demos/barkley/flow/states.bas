' The game's states: the title, play, a stage cleared, the next stage, the tally.
defint A-Z
option explicit

from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.hero import Hero, Pad
from barkley.model.level import KBONE, KCAT, KCHIMNEY, KCRAB, KGOAT, KJELLY, KMOUND, KSQUIRREL, KSWEEP, KTHROW, KWALL
from barkley.model.level import KMORTAR, KSNATCH, KTIPPER, KWALKER, MH, NONE, NSTAGE, TS
from barkley.model.game import CLEARED, Game, Loose, NLOOSE, NPART, OVER, Part, PLAYING, Session, TITLE
from barkley.model.world import NLOB, NMOVE, World
from barkley.assets.palette import Cycle, Shade, Shimmer
from barkley.assets.tunes import CAUGHT, EndBeat, EndTune, KeepBeat, SBONE, SCLEAR, Sfx
from barkley.world.level import LoadLevel, ZoneOf
from barkley.world.movers import Movers
from barkley.stage.fx.damage import Lose
from barkley.stage.fx.lobs import Volley
from barkley.stage.hero.moves import MoveHero, THROWC
from barkley.stage.zones.sands import Flock, Flow, GHOVER
from barkley.stage.zones.wood import Nightfall, SQWAIT, Swarms
from barkley.stage.zones.pass import Avalanche, Homeward
from barkley.stage.zones.chase import Flee
from barkley.stage.camera import Camera
from barkley.stage.play import Dying, Spawn, Things, Touch
from barkley.control.keys import MuteKey, Spent
from barkley.control.pilot import Pilot
from barkley.flow.save import Keep, PickZone, Reach
from barkley.flow.intro import StartBump

' The level's end's songs, and game over's, by their numbers in CHARLIE.DAT; how long game over stays, its tune's
' length.
private const OVERT = 13

' The bonus is counted over the level's end's bars its lead plays, the last six but two: a step a beat.
const TALLYN = 16
private const TALLYBARS = 6

' The bonus counted into the score in time with the level's end: a step and a woof on each beat of the bars its
' lead plays, the bonus shared between the steps left, in tens; all of it at a key.
private sub Tally (mu as Deck, se as Session, g as Game, k as Pad)
    dim due as integer, take as long
    if g.bonus = 0 then exit sub
    due = int(EndBeat!(mu, se, g)) - (len(mu.song(CAUGHT).order) - TALLYBARS) * 4 + 1
    if due > TALLYN or k.hit or k.start then due = TALLYN
    if due <= g.tallied then exit sub
    do while g.tallied < due and g.bonus > 0
        take = ((g.bonus \ (TALLYN - g.tallied) + 9) \ 10) * 10
        g.tallied = g.tallied + 1
        if take > g.bonus then take = g.bonus
        g.bonus = g.bonus - take
        g.score = g.score + take
    loop
    Sfx mu, se, SBONE
end sub

' A step. The title plays itself: Charlie run by the pilot.
sub Tick (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
    wd.se.ticks = wd.se.ticks + 1
    KeepBeat wd.mu, wd.se
    if wd.zn.id = 1 and (wd.se.ticks and 3) = 0 then Cycle wd.tn, wd.se
    if wd.lv.murk then Nightfall wd.lv, wd.zn, wd.tn
    if wd.lv.tide_lo <> NONE then Flow wd.lv, wd.se
    if wd.lv.n_movers then Movers wd.lv, wd.gr, wd.mu, wd.se, p
    g.mode_t = g.mode_t + STEPT
    if g.shake > 0 then g.shake = g.shake - STEPT
    if wd.se.unkept then Keep wd.se
    MuteKey wd.mu, k
    select case g.mode
    case TITLE
        TitleStep wd, g, p, k, lo(), pt()
    case PLAYING
        PlayStep wd, g, p, k, lo(), pt()
    case CLEARED
        ClearedStep wd, g, p, k, lo(), pt()
    case OVER
        if g.mode_t > OVERT or (g.mode_t > 1.5 and (k.hit or k.start)) or k.quit then ResetWorld wd, g, p, lo(), pt()
    end select
    if (wd.se.ticks and 1) = 0 then Things wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    Shimmer wd.zn, wd.tn, wd.se
    Spent k
end sub

' The title's step: behind it the game plays itself, from the start again when that ends; a key starts a game.
private sub TitleStep (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
    dim auto as Pad
    if k.quit then g.quitting = -1
    if p.dead then
        ResetWorld wd, g, p, lo(), pt()
    elseif p.at.x >= wd.lv.goal_x then
        if Met%(wd, g, p, k, pt()) then ResetWorld wd, g, p, lo(), pt()
    else
        Pilot wd.lv, wd.gr, wd.zn, wd.se, p, auto, wd.pilot
        MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, auto, pt()
        Touch wd.lv, wd.zn, wd.mu, wd.se, g, p, lo(), pt()
        if p.at.y > MH * TS + 24 then Lose wd.mu, wd.se, p
        Camera wd.lv, wd.zn, g, p, 0
    end if
    PickZone wd.se, k
    if k.hit or k.start then StartGame wd, g, p, lo(), pt()
end sub

' A game's step: Charlie moved, against the things and what chases, flies and is thrown; past the goal, the stage
' is cleared.
private sub PlayStep (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
    if k.quit then ResetWorld wd, g, p, lo(), pt()
    if p.dead then
        Dying wd.lv, wd.gr, wd.zn, wd.mu, g, p
        exit sub
    end if
    g.clock = g.clock + STEPT
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    Touch wd.lv, wd.zn, wd.mu, wd.se, g, p, lo(), pt()
    if wd.lv.stray.st = 2 then Homeward wd.lv, g
    if wd.lv.aval_x <> NONE then Avalanche wd.lv, wd.mu, wd.se, g, p
    Flee wd.lv, wd.zn, wd.mu, wd.se, g, p, lo(), pt()
    if wd.lv.n_gulls then Flock wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    if wd.lv.n_swarms then Swarms wd.lv, wd.mu, wd.se, g, p, lo()
    if wd.lv.n_lobs then Volley wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    if p.at.y > MH * TS + 24 then Lose wd.mu, wd.se, p
    if p.at.x >= wd.lv.goal_x then StageClear wd.mu, wd.se, g
    Camera wd.lv, wd.zn, g, p, 0
end sub

' The goal passed: the stage is cleared, its bonus by its bones and its time.
private sub StageClear (mu as Deck, se as Session, g as Game)
    g.mode = CLEARED
    g.mode_t = 0
    g.tallied = 0
    g.bonus = g.bones * 50& + 10& * (240 - int(g.clock))
    if g.bonus < g.bones * 50& then g.bonus = g.bones * 50&
    Sfx mu, se, SCLEAR
end sub

' A cleared stage's step: its tally; then, met, a while and the fade over, on to the next stage, or after the
' last, the title.
private sub ClearedStep (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
    dim nxt as integer
    Tally wd.mu, wd.se, g, k
    if k.quit then
        ResetWorld wd, g, p, lo(), pt()
        exit sub
    end if
    if Met%(wd, g, p, k, pt()) = 0 then exit sub
    nxt = wd.zn.stage + 1
    if nxt > NSTAGE - 1 then
        ResetWorld wd, g, p, lo(), pt()
        exit sub
    end if
    Reach wd.se, ZoneOf%(wd.se, nxt)
    if ZoneOf%(wd.se, nxt) = wd.zn.id then
        GoStage wd, nxt, g, p, lo(), pt()
    else
        StartBump wd, g, nxt
    end if
end sub

' The level as it starts, the title up, Charlie at the start for the pilot.
sub ResetWorld (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer
    if wd.zn.stage <> wd.zn.next_up then LoadLevel wd, wd.zn.next_up
    EndTune wd.lv, wd.zn, wd.mu
    wd.lv.glow = 0
    g.mode = TITLE
    g.mode_t = 0
    wd.se.cheer = 0
    wd.se.met_t = 0
    wd.se.past_t = 0
    if wd.tn.dusk < 16 then
        wd.tn.dusk = 16
        Shade wd.tn
    end if
    g.lives = 3
    g.score = 0
    g.clock = 0
    g.home.x = wd.lv.start.x
    g.home.y = wd.lv.start.y
    for i = 0 to wd.lv.n_things - 1
        wd.lv.things(i).at.x = wd.lv.things(i).home
        ' a burrow's mound keeps its other end, and a chimney; a mortar its beat, and a tipper
        if wd.lv.things(i).kind = KMOUND or wd.lv.things(i).kind = KCHIMNEY or wd.lv.things(i).kind = KMORTAR then continue for
        if wd.lv.things(i).kind = KTIPPER then continue for
        wd.lv.things(i).state = 0
        if wd.lv.things(i).kind = KBONE then wd.lv.things(i).state = 1
        if wd.lv.things(i).kind = KCAT or wd.lv.things(i).kind = KCRAB or wd.lv.things(i).kind = KSWEEP then wd.lv.things(i).state = -1
        if wd.lv.things(i).kind = KWALKER or wd.lv.things(i).kind = KSNATCH then wd.lv.things(i).state = -1
        if wd.lv.things(i).kind = KJELLY then wd.lv.things(i).state = 1
        if wd.lv.things(i).kind = KTHROW then wd.lv.things(i).state = THROWC
        if wd.lv.things(i).kind = KSQUIRREL then wd.lv.things(i).state = SQWAIT
        if wd.lv.things(i).kind = KGOAT or wd.lv.things(i).kind = KWALL then wd.lv.things(i).state = 1
    next
    for i = 0 to NLOB - 1
        wd.lv.lobs(i).live = 0
    next
    wd.lv.n_lobs = 0
    for i = 0 to NMOVE
        wd.lv.movers(i).t = 0
    next
    for i = 0 to wd.lv.n_gulls - 1
        wd.lv.gulls(i).at.x = wd.lv.gulls(i).home.x
        wd.lv.gulls(i).at.y = wd.lv.gulls(i).home.y
        wd.lv.gulls(i).rest = 0
        wd.lv.gulls(i).st = GHOVER
        wd.lv.gulls(i).loot = 0
    next
    for i = 0 to NLOOSE - 1
        lo(i).t = 0
    next
    for i = 0 to NPART - 1
        pt(i).t = 0
    next
    wd.se.loose_n = 0
    wd.se.part_n = 0
    Spawn wd.lv, wd.gr, wd.zn, wd.mu, g, p
end sub

' A level cleared: on to level n, the score and lives kept.
sub GoStage (wd as World, n as integer, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim sc as long, left0 as integer
    sc = g.score
    left0 = g.lives
    wd.zn.next_up = n
    ResetWorld wd, g, p, lo(), pt()
    wd.zn.next_up = 0
    g.mode = PLAYING
    g.score = sc
    g.lives = left0
end sub

' Past the goal, a game's or the title's: on to the family and no further, the view sliding on to hold them all
' with Charlie at its left; a while with them, then the picture fades. Whether it has.
private function Met% (wd as World, g as Game, p as Hero, k as Pad, pt() as Part)
    dim idle as Pad, parted as integer
    idle.right = p.at.x < wd.lv.fam_x - 108 and not p.win
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, idle, pt()
    if p.at.x > wd.lv.fam_x - 106 then
        p.at.x = wd.lv.fam_x - 106
        p.gsp = 0
        p.vel.x = 0
        p.roll = 0
    end if
    if p.ground and abs(p.gsp) < 5 and p.at.x >= wd.lv.fam_x - 108 then
        p.win = -1
        p.face = 0
    end if
    wd.se.cheer = p.win
    g.cam.x = g.cam.x + (wd.lv.fam_x - 166 - g.cam.x) * 4 * STEPT
    wd.se.past_t = wd.se.past_t + STEPT
    if p.win and g.bonus = 0 then wd.se.met_t = wd.se.met_t + STEPT
    parted = wd.se.met_t > 6 or wd.se.past_t > 40 or (wd.se.met_t > 2 and (k.hit or k.start))
    if wd.tn.dusk = 16 and parted then wd.tn.dusk = 15
    if wd.tn.dusk < 16 and (wd.se.ticks and 3) = 0 then
        wd.tn.dusk = wd.tn.dusk - 1
        Shade wd.tn
    end if
    Met% = wd.tn.dusk <= 0
end function

' A game from its start, at the first act of the zone picked on the title.
sub StartGame (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
    wd.zn.next_up = wd.se.pick * 2
    ResetWorld wd, g, p, lo(), pt()
    wd.zn.next_up = 0
    g.mode = PLAYING
end sub
