' A stage's level, loaded from CHARLIE.DAT; the zone of a stage.
defint A-Z
option explicit

from barkley.engine.datafile import Chars, OpenData, Word
from barkley.model.images import IBG, ICARD, ICARD2, ILOOP, ITILE, ITILE2, IWAVE, IZ2
from barkley.model.level import KBONE, KBOOST, KBRAMBLE, KBUMP, KBUOY, KBUSH, KBUTTON, KCAT, KCISTERN, KCRAB, KDRIFT
from barkley.model.level import KAWNING, KCHIMNEY, KGOLD
from barkley.model.level import KFAM, KFLOWER, KFLUME, KGOAL, KGOAT, KHEAP, KHOOP, KICICLE, KJELLY, KJET, KLEG, KLOAD
from barkley.model.level import KLOG, KLOOP, KMARK, KMOUND, KPIPE, KPLAT, KPRESS, KROCK, KSAPLING, KSLED, KSPARK, KSPIN
from barkley.model.level import KSPOUT, KSPRING, KSQUIRREL, KSWEEP, KTANK, KTHORN, KTHROW, KTREE, KVENT, KVINE, KWALL
from barkley.model.level import KWAVE, Level, MH, MW, NONE, NSTAGE, PI, TS
from barkley.model.game import Session
from barkley.model.world import World
from barkley.assets.palette import CWATER, Shade
from barkley.assets.store import LoadZoneArt
from barkley.assets.tunes import ZoneTune
from barkley.world.terrain.track import Faces, MOVEW, PIPER, Track, WAVEA
from barkley.world.layout import AddMover, AddObj, Clearing, Column, Limbs, Pair, Pit, PITD, Rooted, Scatter, Shelf
from barkley.world.layout import SortObjs, Weed

' A clothesline's ends over the roofs it joins, and how far in from their edges its posts stand.
const LINEH = 24
private const POSTIN = 6

' An awning's middle out from its wall.
private const AWNOUT = 23

private const HULL = 11
const CABLED = 64
const LOADFAR = 104
const GLOWN = 8

' The levels: a zone's two acts, the second at another hour in other colours, zone by zone.
' the stages played, and after them the intro's road
const SINTRO = NSTAGE

' the zones' names, 16 letters each; what marks CHARLIE.SAV as the game's: its zone is added to it
private const ZNAMES = "AMBER DOWNS     CHROME WORKS    SALTWICK SANDS  WHITECAP PASS   RUSSET WOOD     LANTERN QUARTER"

' A stage's zone: two acts a zone; the intro's road is the Downs.
function ZoneOf% (se as Session, st as integer)
    ZoneOf% = st \ 2
    if st = SINTRO then ZoneOf% = se.road
end function

' CHARLIE.DAT open at a stage's level: a zone's two acts, zone by zone, then the intro's road. They are written
' in tools/charlielevels.txt.
private sub SeekLevel (st as integer)
    dim levels as long
    OpenData
    seek #1, 5
    levels = Word&
    seek #1, levels + 4 + st * 4
    seek #1, levels + Word&
end sub

' A prop as the works have it, where the van's road is theirs: a tank for a tree, a vent for a bush; nothing grows
' there, and no rock lies.
private function Plated% (k as integer)
    select case k
    case KTREE: Plated% = KTANK
    case KBUSH: Plated% = KVENT
    case KFLOWER, KROCK: Plated% = 0
    case else: Plated% = k
    end select
end function

' The ground's columns, then what stands on them, from the stage's level: a command and its count, to its end.
sub LoadLevel (wd as World, st as integer)
    dim cmd as string, n as integer, c as integer, g as integer, bx as integer
    dim i as integer, s as integer, k as integer, z as integer, light as integer
    wd.zn.stage = st
    z = ZoneOf%(wd.se, st)
    wd.zn.id = z
    LoadZoneArt wd.art, z
    ' the intro's road is the Downs at dawn, lit as at evening; a later zone's is lit as its first act
    light = st
    if st = SINTRO then light = wd.se.road * 2
    if light = 0 and st = SINTRO then light = 1
    for i = 0 to 255
        wd.tn.now(i) = wd.tn.day(i)
    next
    for i = 0 to wd.tn.n(light) - 1
        wd.tn.now(wd.tn.which(light, i)) = wd.tn.own(light, i)
    next
    wd.lv.goalX = NONE
    wd.lv.famX = NONE
    wd.lv.chaseX = NONE
    wd.lv.tideLo = NONE
    wd.lv.tide = NONE
    wd.lv.cart.st = 0
    for i = 0 to 2
        wd.tn.glint(i) = wd.tn.now(CWATER + 1 + i)
    next
    Shade wd.tn
    redim wd.lv.map(MW - 1, MH - 1)
    redim wd.lv.glaze(MW)
    wd.lv.icing = 0
    wd.lv.gale = 0
    wd.lv.murk = 0
    wd.lv.glowAt = GLOWN
    wd.lv.avalX = NONE
    wd.lv.oakX = NONE
    wd.lv.fallow = 0
    wd.lv.nObj = 0
    wd.lv.nGold = 0
    wd.lv.nMv = 0
    wd.lv.nGulls = 0
    wd.lv.nSwarm = 0
    wd.lv.nRope = 0
    for i = 0 to MW
        wd.lv.top(i) = MH
    next
    g = 10
    wd.zn.decked = z = 1
    if wd.zn.decked then
        wd.zn.tile = ITILE2
        wd.zn.back = IZ2
        wd.zn.ring = IZ2 + 5
        wd.zn.pipe = IZ2 + 7
        wd.zn.card = ICARD2
        wd.zn.wave = IWAVE + 4
    else
        wd.zn.tile = ITILE
        wd.zn.back = IBG
        wd.zn.ring = ILOOP
        wd.zn.card = ICARD
        wd.zn.wave = IWAVE
    end if
    SeekLevel st
    do
        cmd = Chars$
        n = Word&
        s = (MH - g) * TS
        k = 0
        select case cmd
        case "flat"
            for i = 1 to n
                Column wd.lv, c, g, 1
                Scatter wd.lv, wd.zn, c, s
                c = c + 1
            next
        case "up45"
            for i = 1 to n
                Column wd.lv, c, g, 2
                g = g + 1
                c = c + 1
            next
        case "dn45"
            for i = 1 to n
                g = g - 1
                Column wd.lv, c, g, 3
                c = c + 1
            next
        case "flume"
            ' a 45 degree fall of n and a flume down it, from the tank at its head to its level end a column on;
            ' nothing grows by the tank
            Weed wd.lv, c * TS - 44
            AddObj wd.lv, KCISTERN, c * TS, s
            for i = 1 to n
                g = g - 1
                Column wd.lv, c, g, 3
                AddObj wd.lv, KFLUME, c * TS, (MH - g - 1) * TS
                c = c + 1
            next
            Column wd.lv, c, g, 1
            AddObj wd.lv, KSPOUT, c * TS, (MH - g) * TS
            c = c + 1
        case "upg"
            for i = 1 to n
                Column wd.lv, c, g, 4
                Column wd.lv, c + 1, g, 5
                g = g + 1
                c = c + 2
            next
        case "dng"
            for i = 1 to n
                g = g - 1
                Column wd.lv, c, g, 6
                Column wd.lv, c + 1, g, 7
                c = c + 2
            next
        case "gap": c = c + n
        case "ice"
            ' the ground from here bare ice, or with 0 under snow again
            wd.lv.icing = n <> 0
        case "wall": g = g + n
        case "pit"
            ' not a fall to nothing: a floor PITD down, a spring at its far wall
            Pit wd.lv, c, g, n
            c = c + n
        case "wave"
            ' a ribbon of n twists over a pit, each two halves of WAVEL, bones up its far side
            for i = 0 to n * 2 - 1
                AddObj wd.lv, KWAVE + (i and 1), (c + i * 12) * TS, s
                if (i and 1) <> 0 then continue for
                for k = 3 to 9 step 3
                    AddObj wd.lv, KBONE, (c + i * 12 + k) * TS, s - cint(WAVEA * (1 - cos(PI * k / 12)) / 2) - 6
                next
            next
            k = 0
            Pit wd.lv, c, g, n * 24
            c = c + n * 24
        case "bump": AddObj wd.lv, KBUMP, c * TS + 8, s - n * TS
        case "hoop": AddObj wd.lv, KHOOP, c * TS + 8, s - n * TS
        case "pipe"
            ' a wall with a quarter pipe up to it, over the three columns before: nothing grows there
            for i = 0 to wd.lv.nObj - 1
                if Rooted%(wd.lv, i, KTREE) and wd.lv.ob(i).x >= c * TS - PIPER then wd.lv.ob(i).k = 0
            next
            AddObj wd.lv, KPIPE, c * TS, s
            ' Amber Downs' pipe is drawn with the ground under it
            if z = 0 then
                wd.lv.vis(c - 1) = 0
                wd.lv.vis(c - 2) = 0
                wd.lv.vis(c - 3) = 0
            end if
            g = g + n
        case "drop": g = g - n
        case "mover"
            ' at each end of its way it lies 8 over the ground
            AddMover wd.lv, c * TS + (n * TS - MOVEW) \ 2, s, (n * TS - MOVEW) \ 2 + 8, 0, c * TS
            Pit wd.lv, c, g, n
            c = c + n
        case "boat"
            ' a pool n wide and a boat adrift in it, wall to wall: up and down with the tide, aground at low water
            AddMover wd.lv, c * TS + (n * TS - MOVEW) \ 2, s + PITD * TS - HULL, (n * TS - MOVEW) \ 2, 5, c * TS
            Pit wd.lv, c, g, n
            c = c + n
        case "pier"
            ' a pool n wide under an old pier, a span every three columns: each falls once it is trodden
            for i = 0 to n \ 3 - 1
                AddMover wd.lv, (c + i * 3) * TS, s, 0, 6, c * TS
            next
            Pit wd.lv, c, g, n
            c = c + n
        case "hover"
            AddMover wd.lv, c * TS + 8, s - n * TS \ 2, n * TS \ 2, 1, c * TS
            Pit wd.lv, c, g, 4
            c = c + 4
            g = g + n
        case "lift"
            AddMover wd.lv, c * TS, s, n * TS, 2, c * TS
            Pit wd.lv, c, g, 3
            c = c + 3
            g = g + n
        case "button"
            k = KBUTTON
            bx = c * TS + 8
        case "bridge"
            ' a pit n wide, platforms lying on its floor that rise to bridge it once the last button is trodden
            for i = 0 to n \ 3 - 1
                AddMover wd.lv, (c + i * 3) * TS, s + PITD * TS - 6, PITD * TS - 6, 4, c * TS
                wd.lv.mv(wd.lv.nMv - 1).b = bx
            next
            Pit wd.lv, c, g, n
            c = c + n
        case "cable"
            ' a basket down a cable over a pit n wide, the ground past it CABLED lower
            AddMover wd.lv, c * TS - 8, s, n * TS - MOVEW + 16, 3, c * TS
            Pit wd.lv, c, g - CABLED \ TS, n
            c = c + n
            g = g - CABLED \ TS
        case "chair"
            ' a ski lift's chair up a cable over a pit n wide, the ground past it CABLED higher
            AddMover wd.lv, c * TS - 8, s, n * TS - MOVEW + 16, 7, c * TS
            Pit wd.lv, c, g, n
            c = c + n
            g = g + CABLED \ TS
        case "plat"
            for i = 1 to n mod 100
                wd.lv.map(c + i, MH - g - n \ 100 - 1) = 1
                AddObj wd.lv, KPLAT, (c + i) * TS, s - (n \ 100 + 1) * TS
            next
        case "bones"
            for i = 0 to n - 1
                AddObj wd.lv, KBONE, (c + i) * TS + 8, s - 14
            next
        case "gold"
            ' a golden bone that many tiles over the ground
            AddObj wd.lv, KGOLD, c * TS + 8, s - n * TS - 14
            wd.lv.nGold = wd.lv.nGold + 1
        case "arc"
            for i = 0 to n - 1
                AddObj wd.lv, KBONE, (c + i) * TS + 8, s - 20 - cint(50 * sin(PI * (i + .5) / n))
            next
        case "hb"
            for i = 1 to n mod 100
                AddObj wd.lv, KBONE, (c + i) * TS + 8, s - (n \ 100) * TS - 14
            next
        case "cat": k = KCAT
        case "crab": k = KCRAB
        case "sweeper": k = KSWEEP
        case "sled": k = KSLED
        case "thrower": k = KTHROW
        case "gale": wd.lv.gale = -1
        case "night": wd.lv.murk = -1
        case "avalanche": wd.lv.avalX = c * TS + 8
        case "goat"
            Clearing wd.lv, c
            k = KGOAT
        case "icewall"
            Clearing wd.lv, c
            k = KWALL
        case "icicle"
            ' a shelf of three, n tiles over the ground here, and an icicle under its middle
            Shelf wd.lv, c, g, s, n, 3
            AddObj wd.lv, KICICLE, c * TS + 24, s - n * TS - 6
        case "load"
            ' a shelf of four, n tiles over the ground here, a load of snow on it, and where its heap will lie
            Shelf wd.lv, c, g, s, n, 4
            AddObj wd.lv, KLOAD, c * TS + 32, s - (n + 1) * TS
            AddObj wd.lv, KHEAP, c * TS + 32 + LOADFAR, s
        case "drift": k = KDRIFT
        case "spark": k = KSPARK
        case "press"
            ' n presses on a gantry over level ground, four columns each, a leg at either end
            AddObj wd.lv, KLEG, c * TS, s
            for i = 0 to n * 4 - 1
                Column wd.lv, c + i, g, 1
                if (i and 3) = 1 then AddObj wd.lv, KPRESS, (c + i + 1) * TS, s
            next
            c = c + n * 4
            AddObj wd.lv, KLEG, c * TS - 8, s
        case "jelly"
            ' a jellyfish n columns on, a buoy: afloat where the sea is over the ground there, aground where not
            AddObj wd.lv, KJELLY, (c + n) * TS + 8, s
        case "buoy": AddObj wd.lv, KBUOY, (c + n) * TS + 8, s
        case "gull"
            ' a gull, its home n tiles over the ground here
            wd.lv.gulls(wd.lv.nGulls).hx = c * TS + 8
            wd.lv.gulls(wd.lv.nGulls).hy = s - n * TS
            wd.lv.nGulls = wd.lv.nGulls + 1
        case "line"
            ' a street n wide and a clothesline over it, post to post, LINEH over the roof here
            wd.lv.ropes(wd.lv.nRope).x = c * TS - POSTIN
            wd.lv.ropes(wd.lv.nRope).w = n * TS + POSTIN * 2
            wd.lv.ropes(wd.lv.nRope).y = s - LINEH
            wd.lv.ropes(wd.lv.nRope).t = 0
            wd.lv.nRope = wd.lv.nRope + 1
            Weed wd.lv, c * TS - 20
            wd.lv.fallow = c + n + 2
            Pit wd.lv, c, g, n
            c = c + n
        case "awning"
            ' an awning out from the wall that begins here, n tiles over the roof
            AddObj wd.lv, KAWNING, c * TS - AWNOUT, s - n * TS
        case "chimney"
            ' a chimney: of the two of one n the first is the way in, the second the way out; till they are told
            ' each other, -n - 1
            Clearing wd.lv, c
            AddObj wd.lv, KCHIMNEY, c * TS + 8, s
            wd.lv.ob(wd.lv.nObj - 1).s = -n - 1
        case "spring": k = KSPRING
        case "sapling": k = KSAPLING
        case "bramble": k = KBRAMBLE
        case "log": k = KLOG
        case "vine": AddObj wd.lv, KVINE, c * TS + 8, s - n * TS
        case "squirrel": AddObj wd.lv, KSQUIRREL, c * TS + 8, s - n * TS
        case "bees"
            ' a swarm, its home n tiles over the ground here
            wd.lv.swarms(wd.lv.nSwarm).hx = c * TS + 8
            wd.lv.swarms(wd.lv.nSwarm).hy = s - n * TS
            wd.lv.nSwarm = wd.lv.nSwarm + 1
        case "burrow"
            ' a burrow's mound, its other end the mound of the same n: till they are told each other, -n - 1
            AddObj wd.lv, KMOUND, c * TS + 8, s
            wd.lv.ob(wd.lv.nObj - 1).s = -n - 1
        case "oak"
            ' the great oak here, its limbs' flats over the ground either side of it
            wd.lv.oakX = c * TS + 8
            wd.lv.oakY = s
            Limbs wd.lv, g
        case "mark": k = KMARK
        case "boost": k = KBOOST
        case "loop": k = KLOOP
        case "tree": k = KTREE
        case "bush": k = KBUSH
        case "flower": k = KFLOWER
        case "rock": k = KROCK
        case "thorn": k = KTHORN
        case "jet"
            Clearing wd.lv, c
            k = KJET
        case "spin": k = KSPIN
        case "goal"
            k = KGOAL
            wd.lv.goalX = c * TS + 8
        case "fam"
            k = KFAM
            wd.lv.famX = c * TS + 8
        case "chase"
            wd.lv.chaseX = c * TS
            wd.lv.chaseEnd = (c + n) * TS
        case "tide"
            ' the sea: its height at low water * 100 + at high, in the ground's tiles
            wd.lv.tideLo = (MH - n \ 100) * TS
            wd.lv.tideHi = (MH - n mod 100) * TS
            wd.lv.tide = wd.lv.tideLo
            wd.lv.tideAt = wd.se.ticks
        case "tune": wd.mu.stageTune = n
        case "dog"
            wd.lv.startX = c * TS + 8
            wd.lv.startY = s
        end select
        if wd.zn.decked then k = Plated%(k)
        if k then AddObj wd.lv, k, c * TS + 8, s
    loop until cmd = "end"
    close #1
    wd.lv.w = c
    wd.lv.wpx = c * TS
    ' nothing grows in front of the family
    for i = 0 to wd.lv.nObj - 1
        if Rooted%(wd.lv, i, KBUSH) and abs(wd.lv.ob(i).x - wd.lv.famX) < 112 then wd.lv.ob(i).k = 0
    next
    SortObjs wd.lv
    Pair wd.lv, KMOUND, -1
    Pair wd.lv, KCHIMNEY, 0
    Track wd.lv, wd.gr
    Faces wd.lv
    ZoneTune wd.mu, wd.mu.stageTune
end sub

' The pier's fallen spans back in their places, the swarms at home: Charlie starts again before them.
sub Mend (lv as Level)
    dim m as integer
    for m = 0 to lv.nSwarm - 1
        lv.swarms(m).x = lv.swarms(m).hx
        lv.swarms(m).y = lv.swarms(m).hy
        lv.swarms(m).st = 0
    next
    for m = 0 to lv.nMv - 1
        if lv.mv(m).k = 6 then lv.mv(m).t = 0
    next
    for m = 0 to lv.nObj - 1
        if lv.ob(m).k = KSLED then lv.ob(m).s = 0
        if lv.ob(m).k <> KGOAT then continue for
        lv.ob(m).s = 1
        lv.ob(m).x = lv.ob(m).a
    next
    lv.stray.st = 0
    lv.aval.st = 0
end sub

function ZoneName$ (z as integer)
    ZoneName$ = rtrim$(mid$(ZNAMES, z * 16 + 1, 16))
end function

' Which of its level's golden bones thing i is, from 0, by x.
function GoldNo% (lv as Level, i as integer)
    dim j as integer, n as integer
    for j = 0 to i - 1
        if lv.ob(j).k = KGOLD then n = n + 1
    next
    GoldNo% = n
end function
