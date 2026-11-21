' A stage's step: Charlie against the things of the level;
' his death and his return.
defint A-Z
option explicit

from engine.video import SW
from engine.clock import STEPT
from engine.events import Events
from barkley.model.hero import Hero
from barkley.model.level import GRAV, Ground, KAWNING, KBONE, KBOOST
from barkley.model.level import KBRAMBLE, KBUMP, KBUOY, KBUTTON, KCAT
from barkley.model.level import KCHIMNEY, KCRAB, KDRIFT, KFLUME, KGOAL
from barkley.model.level import KGOAT, KGOLD, KHEAP, KHOOP, KICICLE
from barkley.model.level import KJELLY, KJET, KLOAD, KLOG, KMARK
from barkley.model.level import KMORTAR, KMOUND, KPOUNCE, KPRESS
from barkley.model.level import KSAPLING, KSLED, KSNATCH, KSPARK
from barkley.model.level import KSPIN, KSPOUT, KSPRING, KSQUIRREL
from barkley.model.level import KSWEEP, KTHORN, KTHROW, KTIPPER, KVINE
from barkley.model.level import KWALKER, KWALL, Level, Realm, TS
from barkley.model.game import Game, Loose, NLOOSE, OVER, Part
from barkley.model.game import Session, TOONEW
from barkley.model.events import EVSCENETUNE, EVSOUND, Tell
from barkley.assets.tunes import LEFTBEHIND, SBONE, SHURT
from barkley.world.terrain.ground import SegAt
from barkley.world.level import Mend
from barkley.stage.fx.parts import LooseBones, Particles
from barkley.stage.fx.damage import Hurt
from barkley.stage.zones.downs import MeetBone, MeetBoost, MeetCat
from barkley.stage.zones.downs import MeetGold, MeetMark, MeetSpring
from barkley.stage.zones.downs import Prowl
from barkley.stage.zones.works import MeetBump, MeetButton, MeetHoop
from barkley.stage.zones.works import MeetJet, MeetPress, MeetSpark
from barkley.stage.zones.works import MeetSpin, MeetSweep
from barkley.stage.zones.sands import Bob, MeetCrab, MeetFlume
from barkley.stage.zones.sands import MeetJelly
from barkley.stage.zones.wood import MeetBramble, MeetLog, MeetMound
from barkley.stage.zones.wood import MeetSapling, MeetVine, Regrow
from barkley.stage.zones.wood import Scamper
from barkley.stage.zones.pass import Graze, MeetDrift, MeetGoat
from barkley.stage.zones.pass import MeetHeap, MeetIcicle, MeetSled
from barkley.stage.zones.pass import MeetThrow, MeetWall, TakeAim
from barkley.stage.zones.pass import Totter, Unload
from barkley.stage.zones.quarter import Lines, MeetAwning, MeetChimney
from barkley.stage.zones.quarter import MeetMortar, MeetPounce
from barkley.stage.zones.quarter import MeetTipper, Mortar, Pounce
from barkley.stage.zones.quarter import Settle, Snatch, Tipper
from barkley.stage.zones.chase import ResetChase
from barkley.stage.camera import Camera

const SPARKR = 40
const JELLYD = 10

sub Spawn ( _
    lv as Level, _
    gr as Ground, _
    zn as Realm, _
    ev as Events, _
    g as Game, _
    p as Hero _
)
    dim fresh as Hero

    p = fresh
    p.at.x = g.home.x
    p.at.y = g.home.y
    p.ground = -1
    p.sg = SegAt%(lv, gr, g.home.x, csng(g.home.y))
    g.bones = 0
    ResetChase lv, ev
    Mend lv
    Camera lv, zn, g, p, -1
end sub

' Charlie against the things near it, and the loose bones.
sub Touch ( _
    lv as Level, _
    zn as Realm, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose, _
    pt() as Part _
)
    dim i as integer, c as integer, px as integer, py as integer
    dim x as integer, dx as integer

    ' under the ground nothing is met
    if p.dig then exit sub

    if lv.n_ropes then Lines lv, ev, se, g, p, pt()
    px = cint(p.at.x)
    py = cint(p.at.y)
    c = (px - 80) \ TS
    if c < 0 then c = 0
    i = lv.first(c)

    do while i < lv.n_things
        x = lv.things(i).at.x
        if x > px + 80 then exit do

        dx = abs(x - px)

        select case lv.things(i).kind
        case KBONE
            MeetBone lv, ev, se, g, p, i, pt()
        case KGOLD
            MeetGold lv, zn, ev, se, g, p, i, pt()
        case KCAT, KSQUIRREL, KWALKER, KSNATCH
            MeetCat lv, ev, se, g, p, i, lo(), pt()
        case KCRAB
            MeetCrab lv, ev, se, g, p, i, lo(), pt()
        case KSWEEP
            MeetSweep lv, ev, se, g, p, i, lo(), pt()
        case KTHROW
            MeetThrow lv, ev, se, g, p, i, lo(), pt()
        case KGOAT
            MeetGoat lv, ev, se, g, p, i, lo()
        case KWALL
            MeetWall lv, ev, se, g, p, i, pt()
        case KHEAP
            MeetHeap lv, p, i
        case KICICLE
            MeetIcicle lv, ev, se, g, p, i, lo(), pt()
        case KSLED
            MeetSled lv, ev, se, p, i
        case KDRIFT
            MeetDrift lv, ev, se, p, i, pt()
        case KSPARK
            MeetSpark lv, ev, se, g, p, i, lo()
        case KPRESS
            MeetPress lv, ev, se, g, p, i, lo()
        case KJELLY
            MeetJelly lv, ev, se, g, p, i, lo(), pt()
        case KSPRING
            MeetSpring lv, ev, se, p, i
        case KSAPLING
            MeetSapling lv, ev, se, p, i
        case KAWNING
            MeetAwning lv, ev, se, p, i
        case KCHIMNEY
            MeetChimney lv, ev, se, p, i, pt()
        case KPOUNCE
            MeetPounce lv, ev, se, g, p, i, lo(), pt()
        case KMORTAR
            MeetMortar lv, ev, se, g, p, i, lo()
        case KTIPPER
            MeetTipper lv, ev, se, g, p, i, lo()
        case KVINE
            MeetVine lv, ev, se, p, i
        case KBRAMBLE
            MeetBramble lv, ev, se, g, p, i, lo()
        case KLOG
            MeetLog lv, ev, se, p, i
        case KMOUND
            MeetMound lv, ev, se, p, i, pt()
        case KBOOST
            MeetBoost lv, ev, se, g, p, i, pt()
        case KFLUME, KSPOUT
            MeetFlume lv, ev, se, p, i
        case KMARK
            MeetMark lv, ev, se, g, p, i, pt()
        case KGOAL
            if px >= x then lv.things(i).state = 1
        case KTHORN
            if dx < 12 and abs(py - lv.things(i).at.y) < 8 then
                Hurt ev, se, g, p, x, lo(), SHURT
            end if
        case KBUTTON
            MeetButton lv, ev, se, p, i, pt()
        case KJET
            MeetJet lv, ev, se, p, i
        case KBUMP, KBUOY
            MeetBump lv, ev, se, g, p, i, pt()
        case KSPIN
            MeetSpin lv, ev, se, g, p, i, lo()
        case KHOOP
            MeetHoop lv, ev, se, p, i, pt()
        end select

        i = i + 1
    loop

    if se.loose_n = 0 then exit sub

    for i = 0 to NLOOSE - 1
        if lo(i).t <= 0 or lo(i).t >= TOONEW then continue for
        if abs(lo(i).at.x - px) >= 12 _
            or abs(lo(i).at.y - py + 13) >= 16 then continue for

        lo(i).t = 0
        se.loose_n = se.loose_n - 1
        g.bones = g.bones + 1
        Tell ev, EVSOUND, SBONE
    next
end sub

' Out: Charlie rises and falls through everything; then the
' next life.
sub Dying ( _
    lv as Level, _
    gr as Ground, _
    zn as Realm, _
    ev as Events, _
    g as Game, _
    p as Hero _
)
    p.dead_t = p.dead_t + STEPT
    p.vel.y = p.vel.y + GRAV * STEPT
    p.at.y = p.at.y + p.vel.y * STEPT
    if p.dead_t < 2 then exit sub

    g.lives = g.lives - 1

    if g.lives <= 0 then
        g.mode = OVER
        g.mode_t = 0
        Tell ev, EVSCENETUNE, LEFTBEHIND
    else
        Spawn lv, gr, zn, ev, g, p
    end if
end sub

sub Things ( _
    lv as Level, _
    ev as Events, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose, _
    pt() as Part _
)
    dim i as integer, c as integer
    ' typed: a const made from a const and left typeless is
    ' rounded to a whole number, 0 here
    const DT! = STEPT * 2

    c = (cint(g.cam.x) - 100) \ TS
    if c < 0 then c = 0

    for i = lv.first(c) to lv.n_things - 1
        if lv.things(i).home > g.cam.x + SW + 100 then exit for

        select case lv.things(i).kind
        case KCAT, KCRAB, KSWEEP
            if lv.things(i).state <> 0 then Prowl lv, i
        case KSPARK
            lv.things(i).at.x = lv.things(i).home _
                + cint(SPARKR _
                * sin((se.ticks + lv.things(i).home) * .03))
        case KICICLE
            Totter lv, ev, se, g, pt(), i
        case KLOAD
            Unload lv, ev, se, g, p, lo(), pt(), i
        case KGOAT
            if lv.things(i).state <> 0 then Graze lv, se, i, p
        case KTHROW
            TakeAim lv, i, p
        case KSQUIRREL
            if lv.things(i).state <> 0 then Scamper lv, i, p
        case KPOUNCE
            Pounce lv, i, p
        case KSNATCH
            if lv.things(i).state <> 0 then
                Snatch lv, ev, se, i, lo(), pt()
            end if
        case KMORTAR
            Mortar lv, ev, se, i
        case KTIPPER
            Tipper lv, ev, se, i, pt()
        case KSPRING, KBUMP, KHOOP, KSAPLING, KAWNING
            if lv.things(i).state > 0 then
                lv.things(i).state = lv.things(i).state - 1
            end if
        case KBRAMBLE
            Regrow lv, se, pt(), i
        case KVINE
            if p.vine <> i + 1 then
                lv.things(i).state = lv.things(i).state * 7 \ 8
            end if
        case KBUOY
            lv.things(i).at.y = Bob%(lv, lv.things(i).at.x, -4, 9)

            if lv.things(i).state > 0 then
                lv.things(i).state = lv.things(i).state - 1
            end if
        case KJELLY
            lv.things(i).at.y = Bob%(lv, lv.things(i).at.x, JELLYD, 0)

            if lv.things(i).state > 1 then
                lv.things(i).state = lv.things(i).state - 1
            end if
        end select
    next

    if lv.n_ropes then Settle lv
    if se.loose_n then LooseBones lv, se, lo(), DT
    if se.part_n then Particles se, pt(), DT
end sub
