' A stage's step: Charlie against the things of the level;
' his death and his return.
defint A-Z
option explicit

from engine.events import Events
from barkley.model.hero import Hero
from barkley.model.level import Ground, KAWNING, KBONE, KBOOST
from barkley.model.level import KBRAMBLE, KBUMP, KBUOY, KBUTTON, KCAT
from barkley.model.level import KCHIMNEY, KCRAB, KDRIFT, KFLUME, KGOAL
from barkley.model.level import KGOAT, KGOLD, KHEAP, KHOOP, KICICLE
from barkley.model.level import KJELLY, KJET, KLOG, KMARK, KMORTAR
from barkley.model.level import KMOUND, KPOUNCE, KPRESS, KSAPLING
from barkley.model.level import KSLED, KSNATCH, KSPARK, KSPIN, KSPOUT
from barkley.model.level import KSQUIRREL, KSWEEP, KTHORN, KTHROW
from barkley.model.level import KTIPPER, KVINE, KWALKER, KWALL, Level
from barkley.model.level import Realm, TS
from barkley.model.game import Game, Loose, NLOOSE, OVER, Part
from barkley.model.game import Session, TOONEW
from barkley.model.events import EVSCENETUNE, EVSOUND, Tell
from barkley.model.kinds import Kinds, TBOUNCE, TOWN
from barkley.assets.tunes import LEFTBEHIND, SBONE, SHURT, SSPRING
from barkley.world.terrain.ground import SegAt
from barkley.world.level import Mend
from barkley.stage.zones.downs import MeetBone, MeetBoost, MeetCat
from barkley.stage.zones.downs import MeetGold, MeetMark
from barkley.stage.zones.works import MeetBump, MeetButton, MeetHoop
from barkley.stage.zones.works import MeetJet, MeetPress, MeetSpark
from barkley.stage.zones.works import MeetSpin, MeetSweep
from barkley.stage.zones.sands import MeetCrab, MeetFlume, MeetJelly
from barkley.stage.zones.wood import MeetBramble, MeetLog, MeetMound
from barkley.stage.zones.wood import MeetSapling, MeetVine
from barkley.stage.zones.pass import MeetDrift, MeetGoat, MeetHeap
from barkley.stage.zones.pass import MeetIcicle, MeetSled, MeetThrow
from barkley.stage.zones.pass import MeetWall
from barkley.stage.zones.quarter import Lines, MeetAwning, MeetChimney
from barkley.stage.zones.quarter import MeetMortar, MeetPounce
from barkley.stage.zones.quarter import MeetTipper
from barkley.stage.camera import Follow
from barkley.stage.hero.push import Launch, Perch
from barkley.stage.things.react import Rest, Turn
from barkley.stage.things.chase import ResetChase
from barkley.stage.hero.hurt import Fall, Hurt, Stand
from barkley.stage.fx.parts import Pick

sub Spawn ( _
    lv as Level, _
    gr as Ground, _
    zn as Realm, _
    ev as Events, _
    g as Game, _
    p as Hero _
)
    Stand p, g.home.x, g.home.y, _
        SegAt%(lv, gr, g.home.x, csng(g.home.y))
    g.bones = 0
    ResetChase lv, ev
    Mend lv
    Follow lv, zn, g.cam, p, -1
end sub

' Charlie against the things near it, and the loose bones.
sub Touch ( _
    lv as Level, _
    zn as Realm, _
    kd as Kinds, _
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

    if lv.n_ropes then Lines lv, ev, se, p
    px = cint(p.at.x)
    py = cint(p.at.y)
    c = (px - 80) \ TS
    if c < 0 then c = 0
    i = lv.first(c)

    do while i < lv.n_things
        x = lv.things(i).at.x
        if x > px + 80 then exit do

        dx = abs(x - px)

        select case kd.k(lv.things(i).kind).touch
        case TBOUNCE
            Bounced lv, kd, ev, p, i
        case TOWN
            select case lv.things(i).kind
            case KBONE
                MeetBone lv, ev, g, p, i
            case KGOLD
                MeetGold lv, zn, ev, se, p, i
            case KCAT, KSQUIRREL, KWALKER, KSNATCH
                MeetCat lv, ev, se, g, p, i, lo()
            case KCRAB
                MeetCrab lv, ev, se, g, p, i, lo()
            case KSWEEP
                MeetSweep lv, ev, se, g, p, i, lo()
            case KTHROW
                MeetThrow lv, ev, se, g, p, i, lo()
            case KGOAT
                MeetGoat lv, ev, se, g, p, i, lo()
            case KWALL
                MeetWall lv, ev, p, i
            case KHEAP
                MeetHeap lv, p, i
            case KICICLE
                MeetIcicle lv, ev, se, g, p, i, lo()
            case KSLED
                MeetSled lv, ev, se, p, i
            case KDRIFT
                MeetDrift lv, ev, p, i
            case KSPARK
                MeetSpark lv, ev, se, g, p, i, lo()
            case KPRESS
                MeetPress lv, ev, se, g, p, i, lo()
            case KJELLY
                MeetJelly lv, kd, ev, se, g, p, i, lo(), pt()
            case KSAPLING
                MeetSapling lv, kd, ev, se, p, i
            case KAWNING
                MeetAwning lv, kd, ev, se, p, i
            case KCHIMNEY
                MeetChimney lv, ev, p, i
            case KPOUNCE
                MeetPounce lv, ev, se, g, p, i, lo()
            case KMORTAR
                MeetMortar lv, ev, se, g, p, i, lo()
            case KTIPPER
                MeetTipper lv, ev, se, g, p, i, lo()
            case KVINE
                MeetVine lv, ev, se, p, i
            case KBRAMBLE
                MeetBramble lv, kd, ev, se, g, p, i, lo()
            case KLOG
                MeetLog lv, ev, se, p, i
            case KMOUND
                MeetMound lv, ev, p, i
            case KBOOST
                MeetBoost lv, ev, p, i
            case KFLUME, KSPOUT
                MeetFlume lv, ev, se, p, i
            case KMARK
                MeetMark lv, ev, g, p, i
            case KGOAL
                if px >= x then Turn lv, i, 1
            case KTHORN
                if dx < 12 and abs(py - lv.things(i).at.y) < 8 then
                    Hurt ev, se, g, p, x, lo(), SHURT
                end if
            case KBUTTON
                MeetButton lv, ev, p, i
            case KJET
                MeetJet lv, ev, se, p, i
            case KBUMP, KBUOY
                MeetBump lv, kd, ev, p, i
            case KSPIN
                MeetSpin lv, ev, se, g, p, i, lo()
            case KHOOP
                MeetHoop lv, kd, ev, p, i
            end select
        end select

        i = i + 1
    loop

    if se.loose_n = 0 then exit sub

    for i = 0 to NLOOSE - 1
        if lo(i).t <= 0 or lo(i).t >= TOONEW then continue for
        if abs(lo(i).at.x - px) >= 12 _
            or abs(lo(i).at.y - py + 13) >= 16 then continue for

        Pick lo(), se, i
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
    Fall p
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

' A thing that throws him up, met: within its row's reach,
' between its top and its bottom, and not as he rises. He
' is sprung up from its top at its row's speed; it rests.
private sub Bounced ( _
    lv as Level, _
    kd as Kinds, _
    ev as Events, _
    p as Hero, _
    i as integer _
)
    dim py as integer, k as integer

    k = lv.things(i).kind
    py = cint(p.at.y)
    if abs(lv.things(i).at.x - cint(p.at.x)) >= kd.k(k).reach _
        or py < lv.things(i).at.y - kd.k(k).top _
        or py > lv.things(i).at.y + kd.k(k).bottom then exit sub
    if not p.ground and p.vel.y < 0 then exit sub

    Launch p, csng(kd.k(k).up), 0, csng(kd.k(k).up)
    Perch p, lv.things(i).at.y - kd.k(k).top
    Rest lv, kd, i
    Tell ev, EVSOUND, SSPRING
end sub
