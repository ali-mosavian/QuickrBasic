' A stage's step: Charlie against the things of the level; his death and his return.
defint A-Z
option explicit

from barkley.engine.video import SW
from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.hero import Hero
from barkley.model.level import GRAV, Ground, KBONE, KBOOST, KBRAMBLE, KBUMP, KBUOY, KBUTTON, KCAT, KCRAB, KDRIFT
from barkley.model.level import KFLUME, KGOAL, KGOAT, KHEAP, KHOOP, KICICLE, KJELLY, KJET, KLOAD, KLOG, KMARK, KMOUND
from barkley.model.level import KPRESS, KSAPLING, KSLED, KSPARK, KSPIN, KSPOUT, KSPRING, KSQUIRREL, KSWEEP, KTHORN
from barkley.model.level import KAWNING, KCHIMNEY, KGOLD, KPOUNCE, KSNATCH, KTHROW, KVINE, KWALKER, KWALL, Level
from barkley.model.level import KMORTAR, KTIPPER, Realm, TS
from barkley.model.game import Game, Loose, NLOOSE, OVER, Part, Session, TOONEW
from barkley.assets.tunes import LEFTBEHIND, SBONE, SceneTune, Sfx, SHURT
from barkley.world.terrain.ground import SegAt
from barkley.world.level import Mend
from barkley.stage.fx.parts import LooseBones, Particles
from barkley.stage.fx.damage import Hurt
from barkley.stage.zones.downs import MeetBone, MeetBoost, MeetCat, MeetGold, MeetMark, MeetSpring, Prowl
from barkley.stage.zones.works import MeetBump, MeetButton, MeetHoop, MeetJet, MeetPress, MeetSpark, MeetSpin, MeetSweep
from barkley.stage.zones.sands import Bob, MeetCrab, MeetFlume, MeetJelly
from barkley.stage.zones.wood import MeetBramble, MeetLog, MeetMound, MeetSapling, MeetVine, Regrow, Scamper
from barkley.stage.zones.pass import Graze, MeetDrift, MeetGoat, MeetHeap, MeetIcicle, MeetSled, MeetThrow, MeetWall
from barkley.stage.zones.pass import TakeAim, Totter, Unload
from barkley.stage.zones.quarter import Lines, MeetAwning, MeetChimney, MeetMortar, MeetPounce, Mortar, Pounce, Settle
from barkley.stage.zones.quarter import MeetTipper, Snatch, Tipper
from barkley.stage.zones.chase import ResetChase
from barkley.stage.camera import Camera

const SPARKR = 40
const JELLYD = 10

sub Spawn (lv as Level, gr as Ground, zn as Realm, mu as Deck, g as Game, p as Hero)
    dim fresh as Hero
    p = fresh
    p.at.x = g.home.x
    p.at.y = g.home.y
    p.ground = -1
    p.sg = SegAt%(lv, gr, g.home.x, csng(g.home.y))
    g.bones = 0
    ResetChase lv, mu
    Mend lv
    Camera lv, zn, g, p, -1
end sub

' Charlie against the things near it, and the loose bones.
sub Touch (lv as Level, zn as Realm, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer, c as integer, px as integer, py as integer, x as integer, dx as integer
    ' under the ground nothing is met
    if p.dig then exit sub
    if lv.n_ropes then Lines lv, mu, se, g, p, pt()
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
        case KBONE: MeetBone lv, mu, se, g, p, i, pt()
        case KGOLD: MeetGold lv, zn, mu, se, g, p, i, pt()
        case KCAT, KSQUIRREL, KWALKER, KSNATCH: MeetCat lv, mu, se, g, p, i, lo(), pt()
        case KCRAB: MeetCrab lv, mu, se, g, p, i, lo(), pt()
        case KSWEEP: MeetSweep lv, mu, se, g, p, i, lo(), pt()
        case KTHROW: MeetThrow lv, mu, se, g, p, i, lo(), pt()
        case KGOAT: MeetGoat lv, mu, se, g, p, i, lo()
        case KWALL: MeetWall lv, mu, se, g, p, i, pt()
        case KHEAP: MeetHeap lv, p, i
        case KICICLE: MeetIcicle lv, mu, se, g, p, i, lo(), pt()
        case KSLED: MeetSled lv, mu, se, p, i
        case KDRIFT: MeetDrift lv, mu, se, p, i, pt()
        case KSPARK: MeetSpark lv, mu, se, g, p, i, lo()
        case KPRESS: MeetPress lv, mu, se, g, p, i, lo()
        case KJELLY: MeetJelly lv, mu, se, g, p, i, lo(), pt()
        case KSPRING: MeetSpring lv, mu, se, p, i
        case KSAPLING: MeetSapling lv, mu, se, p, i
        case KAWNING: MeetAwning lv, mu, se, p, i
        case KCHIMNEY: MeetChimney lv, mu, se, p, i, pt()
        case KPOUNCE: MeetPounce lv, mu, se, g, p, i, lo(), pt()
        case KMORTAR: MeetMortar lv, mu, se, g, p, i, lo()
        case KTIPPER: MeetTipper lv, mu, se, g, p, i, lo()
        case KVINE: MeetVine lv, mu, se, p, i
        case KBRAMBLE: MeetBramble lv, mu, se, g, p, i, lo()
        case KLOG: MeetLog lv, mu, se, p, i
        case KMOUND: MeetMound lv, mu, se, p, i, pt()
        case KBOOST: MeetBoost lv, mu, se, g, p, i, pt()
        case KFLUME, KSPOUT: MeetFlume lv, mu, se, p, i
        case KMARK: MeetMark lv, mu, se, g, p, i, pt()
        case KGOAL: if px >= x then lv.things(i).state = 1
        case KTHORN: if dx < 12 and abs(py - lv.things(i).at.y) < 8 then Hurt mu, se, g, p, x, lo(), SHURT
        case KBUTTON: MeetButton lv, mu, se, p, i, pt()
        case KJET: MeetJet lv, mu, se, p, i
        case KBUMP, KBUOY: MeetBump lv, mu, se, g, p, i, pt()
        case KSPIN: MeetSpin lv, mu, se, g, p, i, lo()
        case KHOOP: MeetHoop lv, mu, se, p, i, pt()
        end select
        i = i + 1
    loop
    if se.loose_n = 0 then exit sub
    for i = 0 to NLOOSE - 1
        if lo(i).t <= 0 or lo(i).t >= TOONEW then continue for
        if abs(lo(i).at.x - px) >= 12 or abs(lo(i).at.y - py + 13) >= 16 then continue for
        lo(i).t = 0
        se.loose_n = se.loose_n - 1
        g.bones = g.bones + 1
        Sfx mu, se, SBONE
    next
end sub

' Out: Charlie rises and falls through everything; then the next life.
sub Dying (lv as Level, gr as Ground, zn as Realm, mu as Deck, g as Game, p as Hero)
    p.dead_t = p.dead_t + STEPT
    p.vel.y = p.vel.y + GRAV * STEPT
    p.at.y = p.at.y + p.vel.y * STEPT
    if p.dead_t < 2 then exit sub
    g.lives = g.lives - 1
    if g.lives <= 0 then
        g.mode = OVER
        g.mode_t = 0
        SceneTune mu, LEFTBEHIND
    else
        Spawn lv, gr, zn, mu, g, p
    end if
end sub

sub Things (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer, c as integer
    ' typed: a const made from a const and left typeless is rounded to a whole number, 0 here
    const DT! = STEPT * 2
    c = (cint(g.cam.x) - 100) \ TS
    if c < 0 then c = 0
    for i = lv.first(c) to lv.n_things - 1
        if lv.things(i).home > g.cam.x + SW + 100 then exit for
        select case lv.things(i).kind
        case KCAT, KCRAB, KSWEEP: if lv.things(i).state <> 0 then Prowl lv, i
        case KSPARK: lv.things(i).at.x = lv.things(i).home + cint(SPARKR * sin((se.ticks + lv.things(i).home) * .03))
        case KICICLE: Totter lv, mu, se, g, pt(), i
        case KLOAD: Unload lv, mu, se, g, p, lo(), pt(), i
        case KGOAT: if lv.things(i).state <> 0 then Graze lv, se, i, p
        case KTHROW: TakeAim lv, i, p
        case KSQUIRREL: if lv.things(i).state <> 0 then Scamper lv, i, p
        case KPOUNCE: Pounce lv, i, p
        case KSNATCH: if lv.things(i).state <> 0 then Snatch lv, mu, se, i, lo(), pt()
        case KMORTAR: Mortar lv, mu, se, i
        case KTIPPER: Tipper lv, mu, se, i, pt()
        case KSPRING, KBUMP, KHOOP, KSAPLING, KAWNING: if lv.things(i).state > 0 then lv.things(i).state = lv.things(i).state - 1
        case KBRAMBLE: Regrow lv, se, pt(), i
        case KVINE: if p.vine <> i + 1 then lv.things(i).state = lv.things(i).state * 7 \ 8
        case KBUOY
            lv.things(i).at.y = Bob%(lv, lv.things(i).at.x, -4, 9)
            if lv.things(i).state > 0 then lv.things(i).state = lv.things(i).state - 1
        case KJELLY
            lv.things(i).at.y = Bob%(lv, lv.things(i).at.x, JELLYD, 0)
            if lv.things(i).state > 1 then lv.things(i).state = lv.things(i).state - 1
        end select
    next
    if lv.n_ropes then Settle lv
    if se.loose_n then LooseBones lv, se, lo(), DT
    if se.part_n then Particles se, pt(), DT
end sub
