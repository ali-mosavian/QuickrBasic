' The game's states: the title, play, a stage cleared, the
' next stage, the tally.
defint A-Z
option explicit

from engine.clock import STEPT
from engine.audio import Deck
from engine.events import Events
from barkley.model.hero import Hero, Pad
from barkley.model.level import KBONE, KCAT, KCHIMNEY, KCRAB, KGOAT
from barkley.model.level import KJELLY, KMORTAR, KMOUND, KSNATCH
from barkley.model.level import KSQUIRREL, KSWEEP, KTHROW, KTIPPER
from barkley.model.level import KWALKER, KWALL, MH, NONE, NSTAGE, TS
from barkley.model.game import CLEARED, Game, Loose, NLOOSE, NPART
from barkley.model.game import OVER, Part, PLAYING, Session, TITLE
from barkley.model.world import NLOB, NMOVE, World
from barkley.model.events import EVSOUND, Tell
from barkley.assets.palette import Cycle, Shade, Shimmer
from barkley.assets.tunes import CAUGHT, EndBeat, EndTune, KeepBeat
from barkley.assets.tunes import SBONE, SCLEAR
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
from barkley.flow.deliver import Deliver

' The level's end's songs, and game over's, by their numbers
' in CHARLIE.DAT; how long game over stays, its tune's
' length.
private const OVERT = 13

' The bonus is counted over the level's end's bars its lead
' plays, the last six but two: a step a beat.
const TALLYN = 16
private const TALLYBARS = 6

' The bonus counted into the score in time with the level's
' end: a step and a woof on each beat of the bars its lead
' plays, the bonus shared between the steps left, in tens;
' all of it at a key.
private sub Tally ( _
    mu as Deck, _
    ev as Events, _
    se as Session, _
    g as Game, _
    k as Pad _
)
    dim due as integer, take as long

    if g.bonus = 0 then exit sub

    due = int(EndBeat!(mu, se, g)) _
        - (len(mu.song(CAUGHT).order) - TALLYBARS) * 4 + 1
    if due > TALLYN or k.hit or k.start then due = TALLYN
    if due <= g.tallied then exit sub

    do while g.tallied < due and g.bonus > 0
        take = ((g.bonus \ (TALLYN - g.tallied) + 9) \ 10) * 10
        g.tallied = g.tallied + 1
        if take > g.bonus then take = g.bonus
        g.bonus = g.bonus - take
        g.score = g.score + take
    loop

    Tell ev, EVSOUND, SBONE
end sub

' A step. The title plays itself: Charlie run by the pilot.
sub Tick ( _
    wd as World, _
    g as Game, _
    p as Hero, _
    k as Pad, _
    lo() as Loose, _
    pt() as Part _
)
    wd.se.ticks = wd.se.ticks + 1
    KeepBeat wd.mu, wd.se

    if wd.zn.id = 1 and (wd.se.ticks and 3) = 0 then
        Cycle wd.tn, wd.se
    end if

    if wd.lv.murk then Nightfall wd.lv, wd.zn, wd.tn
    if wd.lv.tide_lo <> NONE then Flow wd.lv, wd.se
    if wd.lv.n_movers then Movers wd.lv, wd.gr, wd.ev, wd.se, p
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
        if g.mode_t > OVERT _
            or (g.mode_t > 1.5 and (k.hit or k.start)) or k.quit then
            ResetWorld wd, g, p, lo(), pt()
        end if
    end select

    if (wd.se.ticks and 1) = 0 then
        Things wd.lv, wd.ev, wd.se, g, p, lo(), pt()
    end if

    Deliver wd

    Shimmer wd.zn, wd.tn, wd.se
    Spent k
end sub

' The title's step: behind it the game plays itself, from
' the start again when that ends; a key starts a game.
private sub TitleStep ( _
    wd as World, _
    g as Game, _
    p as Hero, _
    k as Pad, _
    lo() as Loose, _
    pt() as Part _
)
    dim auto as Pad

    if k.quit then g.quitting = -1

    if p.dead then
        ResetWorld wd, g, p, lo(), pt()
    elseif p.at.x >= wd.lv.goal_x then
        if Met%(wd, g, p, k, pt()) then
            ResetWorld wd, g, p, lo(), pt()
        end if
    else
        Pilot wd.lv, wd.gr, wd.zn, wd.se, p, auto, wd.pilot
        MoveHero wd.lv, wd.gr, wd.ev, wd.se, p, auto, pt()
        Touch wd.lv, wd.zn, wd.ev, wd.se, g, p, lo(), pt()
        if p.at.y > MH * TS + 24 then Lose wd.ev, wd.se, p
        Camera wd.lv, wd.zn, g, p, 0
    end if

    PickZone wd.se, k
    if k.hit or k.start then StartGame wd, g, p, lo(), pt()
end sub

' A game's step: Charlie moved, against the things and what
' chases, flies and is thrown; past the goal, the stage is
' cleared.
private sub PlayStep ( _
    wd as World, _
    g as Game, _
    p as Hero, _
    k as Pad, _
    lo() as Loose, _
    pt() as Part _
)
    if k.quit then ResetWorld wd, g, p, lo(), pt()

    if p.dead then
        Dying wd.lv, wd.gr, wd.zn, wd.ev, g, p
        exit sub
    end if

    g.clock = g.clock + STEPT
    MoveHero wd.lv, wd.gr, wd.ev, wd.se, p, k, pt()
    Touch wd.lv, wd.zn, wd.ev, wd.se, g, p, lo(), pt()
    if wd.lv.stray.st = 2 then Homeward wd.lv, g
    if wd.lv.aval_x <> NONE then Avalanche wd.lv, wd.ev, wd.se, g, p
    Flee wd.lv, wd.zn, wd.ev, wd.se, g, p, lo(), pt()
    if wd.lv.n_gulls then Flock wd.lv, wd.ev, wd.se, g, p, lo(), pt()
    if wd.lv.n_swarms then Swarms wd.lv, wd.ev, wd.se, g, p, lo()
    if wd.lv.n_lobs then Volley wd.lv, wd.ev, wd.se, g, p, lo(), pt()
    if p.at.y > MH * TS + 24 then Lose wd.ev, wd.se, p
    if p.at.x >= wd.lv.goal_x then StageClear wd.ev, wd.se, g
    Camera wd.lv, wd.zn, g, p, 0
end sub

' The goal passed: the stage is cleared, its bonus by its
' bones and its time.
private sub StageClear (ev as Events, se as Session, g as Game)
    g.mode = CLEARED
    g.mode_t = 0
    g.tallied = 0
    g.bonus = g.bones * 50& + 10& * (240 - int(g.clock))
    if g.bonus < g.bones * 50& then g.bonus = g.bones * 50&
    Tell ev, EVSOUND, SCLEAR
end sub

' A cleared stage's step: its tally; then, met, a while and
' the fade over, on to the next stage, or after the last,
' the title.
private sub ClearedStep ( _
    wd as World, _
    g as Game, _
    p as Hero, _
    k as Pad, _
    lo() as Loose, _
    pt() as Part _
)
    dim nxt as integer

    Tally wd.mu, wd.ev, wd.se, g, k

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

' The level as it starts, the title up, Charlie at the start
' for the pilot.
sub ResetWorld ( _
    wd as World, _
    g as Game, _
    p as Hero, _
    lo() as Loose, _
    pt() as Part _
)
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
        ' a burrow's mound keeps its other end, and a
        ' chimney; a mortar its beat, and a tipper
        if wd.lv.things(i).kind = KMOUND _
            or wd.lv.things(i).kind = KCHIMNEY _
            or wd.lv.things(i).kind = KMORTAR then continue for
        if wd.lv.things(i).kind = KTIPPER then continue for

        wd.lv.things(i).state = 0
        if wd.lv.things(i).kind = KBONE then wd.lv.things(i).state = 1

        if wd.lv.things(i).kind = KCAT _
            or wd.lv.things(i).kind = KCRAB _
            or wd.lv.things(i).kind = KSWEEP then
            wd.lv.things(i).state = -1
        end if

        if wd.lv.things(i).kind = KWALKER _
            or wd.lv.things(i).kind = KSNATCH then
            wd.lv.things(i).state = -1
        end if

        if wd.lv.things(i).kind = KJELLY then
            wd.lv.things(i).state = 1
        end if

        if wd.lv.things(i).kind = KTHROW then
            wd.lv.things(i).state = THROWC
        end if

        if wd.lv.things(i).kind = KSQUIRREL then
            wd.lv.things(i).state = SQWAIT
        end if

        if wd.lv.things(i).kind <> KGOAT _
            and wd.lv.things(i).kind <> KWALL then continue for

        wd.lv.things(i).state = 1
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
    Spawn wd.lv, wd.gr, wd.zn, wd.ev, g, p
    Deliver wd
end sub

' A level cleared: on to level n, the score and lives kept.
sub GoStage ( _
    wd as World, _
    n as integer, _
    g as Game, _
    p as Hero, _
    lo() as Loose, _
    pt() as Part _
)
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

' Past the goal, a game's or the title's: on to the family
' and no further, the view sliding on to hold them all with
' Charlie at its left; a while with them, then the picture
' fades. Whether it has.
private function Met% ( _
    wd as World, _
    g as Game, _
    p as Hero, _
    k as Pad, _
    pt() as Part _
)
    dim idle as Pad, parted as integer

    idle.right = p.at.x < wd.lv.fam_x - 108 and not p.win
    MoveHero wd.lv, wd.gr, wd.ev, wd.se, p, idle, pt()

    if p.at.x > wd.lv.fam_x - 106 then
        p.at.x = wd.lv.fam_x - 106
        p.gsp = 0
        p.vel.x = 0
        p.roll = 0
    end if

    if p.ground and abs(p.gsp) < 5 _
        and p.at.x >= wd.lv.fam_x - 108 then
        p.win = -1
        p.face = 0
    end if

    wd.se.cheer = p.win
    g.cam.x = g.cam.x + (wd.lv.fam_x - 166 - g.cam.x) * 4 * STEPT
    wd.se.past_t = wd.se.past_t + STEPT
    if p.win and g.bonus = 0 then wd.se.met_t = wd.se.met_t + STEPT
    parted = wd.se.met_t > 6 or wd.se.past_t > 40 _
        or (wd.se.met_t > 2 and (k.hit or k.start))
    if wd.tn.dusk = 16 and parted then wd.tn.dusk = 15

    if wd.tn.dusk < 16 and (wd.se.ticks and 3) = 0 then
        wd.tn.dusk = wd.tn.dusk - 1
        Shade wd.tn
    end if

    Met% = wd.tn.dusk <= 0
end function

' A game from its start, at the first act of the zone picked
' on the title.
sub StartGame ( _
    wd as World, _
    g as Game, _
    p as Hero, _
    lo() as Loose, _
    pt() as Part _
)
    wd.zn.next_up = wd.se.pick * 2
    ResetWorld wd, g, p, lo(), pt()
    wd.zn.next_up = 0
    g.mode = PLAYING
end sub
