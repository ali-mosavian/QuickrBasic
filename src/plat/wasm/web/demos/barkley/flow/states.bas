' The game's states: the title, play, a stage cleared, the next stage, the tally.
defint A-Z
option explicit

from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.hero import Hero, Pad
from barkley.model.level import KBONE, KCAT, KCRAB, KGOAT, KJELLY, KMOUND, KSQUIRREL, KSWEEP, KTHROW, KWALL, MH, NONE
from barkley.model.level import NSTAGE, TS
from barkley.model.game import CLEARED, Game, Loose, NLOOSE, NPART, OVER, Part, PLAYING, Session, TITLE
from barkley.model.world import NLOB, NMOVE, World
from barkley.assets.palette import Cycle, Shade, Shimmer
from barkley.assets.tunes import CAUGHT, EndBeat, EndTune, SBONE, SCLEAR, Sfx
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
from barkley.flow.save import PickZone, Reach
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
    if wd.zn.id = 1 and (wd.se.ticks and 3) = 0 then Cycle wd.tn, wd.se
    if wd.lv.murk then Nightfall wd.lv, wd.zn, wd.tn
    if wd.lv.tideLo <> NONE then Flow wd.lv, wd.se
    if wd.lv.nMv then Movers wd.lv, wd.gr, wd.mu, wd.se, p
    g.modeT = g.modeT + STEPT
    if g.shake > 0 then g.shake = g.shake - STEPT
    MuteKey wd.mu, k
    select case g.mode
    case TITLE: TitleStep wd, g, p, k, lo(), pt()
    case PLAYING: PlayStep wd, g, p, k, lo(), pt()
    case CLEARED: ClearedStep wd, g, p, k, lo(), pt()
    case OVER
        if g.modeT > OVERT or (g.modeT > 1.5 and (k.hit or k.start)) or k.quit then ResetWorld wd, g, p, lo(), pt()
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
    elseif p.x >= wd.lv.goalX then
        if Met%(wd, g, p, k, pt()) then ResetWorld wd, g, p, lo(), pt()
    else
        Pilot wd.lv, wd.gr, wd.zn, wd.se, p, auto
        MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, auto, pt()
        Touch wd.lv, wd.mu, wd.se, g, p, lo(), pt()
        if p.y > MH * TS + 24 then Lose wd.mu, wd.se, p
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
    Touch wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    if wd.lv.stray.st = 2 then Homeward wd.lv, g
    if wd.lv.avalX <> NONE then Avalanche wd.lv, wd.mu, wd.se, g, p
    Flee wd.lv, wd.zn, wd.mu, wd.se, g, p, lo(), pt()
    if wd.lv.nGulls then Flock wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    if wd.lv.nSwarm then Swarms wd.lv, wd.mu, wd.se, g, p, lo()
    if wd.lv.lobN then Volley wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    if p.y > MH * TS + 24 then Lose wd.mu, wd.se, p
    if p.x >= wd.lv.goalX then StageClear wd.mu, wd.se, g
    Camera wd.lv, wd.zn, g, p, 0
end sub

' The goal passed: the stage is cleared, its bonus by its bones and its time.
private sub StageClear (mu as Deck, se as Session, g as Game)
    g.mode = CLEARED
    g.modeT = 0
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
    if wd.zn.stage <> wd.zn.nextUp then LoadLevel wd, wd.zn.nextUp
    EndTune wd.lv, wd.mu
    wd.lv.glow = 0
    g.mode = TITLE
    g.modeT = 0
    wd.se.cheer = 0
    wd.se.metT = 0
    wd.se.pastT = 0
    if wd.tn.dusk < 16 then
        wd.tn.dusk = 16
        Shade wd.tn
    end if
    g.lives = 3
    g.score = 0
    g.clock = 0
    g.homeX = wd.lv.startX
    g.homeY = wd.lv.startY
    for i = 0 to wd.lv.nObj - 1
        wd.lv.ob(i).x = wd.lv.ob(i).a
        ' a burrow's mound keeps its other end
        if wd.lv.ob(i).k = KMOUND then continue for
        wd.lv.ob(i).s = 0
        if wd.lv.ob(i).k = KBONE then wd.lv.ob(i).s = 1
        if wd.lv.ob(i).k = KCAT or wd.lv.ob(i).k = KCRAB or wd.lv.ob(i).k = KSWEEP then wd.lv.ob(i).s = -1
        if wd.lv.ob(i).k = KJELLY then wd.lv.ob(i).s = 1
        if wd.lv.ob(i).k = KTHROW then wd.lv.ob(i).s = THROWC
        if wd.lv.ob(i).k = KSQUIRREL then wd.lv.ob(i).s = SQWAIT
        if wd.lv.ob(i).k = KGOAT or wd.lv.ob(i).k = KWALL then wd.lv.ob(i).s = 1
    next
    for i = 0 to NLOB - 1
        wd.lv.lobs(i).live = 0
    next
    wd.lv.lobN = 0
    for i = 0 to NMOVE
        wd.lv.mv(i).t = 0
    next
    for i = 0 to wd.lv.nGulls - 1
        wd.lv.gulls(i).x = wd.lv.gulls(i).hx
        wd.lv.gulls(i).y = wd.lv.gulls(i).hy
        wd.lv.gulls(i).t = 0
        wd.lv.gulls(i).st = GHOVER
        wd.lv.gulls(i).loot = 0
    next
    for i = 0 to NLOOSE - 1
        lo(i).t = 0
    next
    for i = 0 to NPART - 1
        pt(i).t = 0
    next
    wd.se.looseN = 0
    wd.se.partN = 0
    Spawn wd.lv, wd.gr, wd.zn, wd.mu, g, p
end sub

' A level cleared: on to level n, the score and lives kept.
sub GoStage (wd as World, n as integer, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim sc as long, left0 as integer
    sc = g.score
    left0 = g.lives
    wd.zn.nextUp = n
    ResetWorld wd, g, p, lo(), pt()
    wd.zn.nextUp = 0
    g.mode = PLAYING
    g.score = sc
    g.lives = left0
end sub

' Past the goal, a game's or the title's: on to the family and no further, the view sliding on to hold them all
' with Charlie at its left; a while with them, then the picture fades. Whether it has.
private function Met% (wd as World, g as Game, p as Hero, k as Pad, pt() as Part)
    dim idle as Pad, parted as integer
    idle.r = p.x < wd.lv.famX - 108 and not p.win
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, idle, pt()
    if p.x > wd.lv.famX - 106 then
        p.x = wd.lv.famX - 106
        p.gsp = 0
        p.vx = 0
        p.roll = 0
    end if
    if p.ground and abs(p.gsp) < 5 and p.x >= wd.lv.famX - 108 then
        p.win = -1
        p.face = 0
    end if
    wd.se.cheer = p.win
    g.camx = g.camx + (wd.lv.famX - 166 - g.camx) * 4 * STEPT
    wd.se.pastT = wd.se.pastT + STEPT
    if p.win and g.bonus = 0 then wd.se.metT = wd.se.metT + STEPT
    parted = wd.se.metT > 6 or wd.se.pastT > 40 or (wd.se.metT > 2 and (k.hit or k.start))
    if wd.tn.dusk = 16 and parted then wd.tn.dusk = 15
    if wd.tn.dusk < 16 and (wd.se.ticks and 3) = 0 then
        wd.tn.dusk = wd.tn.dusk - 1
        Shade wd.tn
    end if
    Met% = wd.tn.dusk <= 0
end function

' A game from its start, at the first act of the zone picked on the title.
sub StartGame (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
    wd.zn.nextUp = wd.se.pick * 2
    ResetWorld wd, g, p, lo(), pt()
    wd.zn.nextUp = 0
    g.mode = PLAYING
end sub
