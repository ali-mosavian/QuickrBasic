' A stage's level, loaded from CHARLIE.DAT; the zone of a stage.
defint A-Z
option explicit

from barkley.engine.datafile import Chars, DataFile, OpenData, Word
from barkley.model.images import IBG, ICARD, ICARD2, ILOOP, ITILE
from barkley.model.images import ITILE2, IWAVE, IZ2
from barkley.model.level import KAWNING, KBIN, KBONE, KBOOST, KBRAMBLE
from barkley.model.level import KBUMP, KBUOY, KBUSH, KBUTTON, KCAT
from barkley.model.level import KCHIMNEY, KCISTERN, KCRAB, KDRIFT
from barkley.model.level import KFAM, KFLOWER, KFLUME, KGOAL, KGOAT
from barkley.model.level import KGOLD, KHEAP, KHOOP, KICICLE, KJELLY
from barkley.model.level import KJET, KLEG, KLOAD, KLOG, KLOOP, KMARK
from barkley.model.level import KMORTAR, KMOUND, KPIPE, KPLAT, KPOUNCE
from barkley.model.level import KPRESS, KROCK, KSAPLING, KSLED
from barkley.model.level import KSNATCH, KSPARK, KSPIN, KSPOUT
from barkley.model.level import KSPRING, KSQUIRREL, KSWEEP, KTANK
from barkley.model.level import KTHORN, KTHROW, KTIPPER, KTREE, KVENT
from barkley.model.level import KVINE, KWALKER, KWALL, KWAVE, Level
from barkley.model.level import MH, MW, NONE, NSTAGE, PI, TS
from barkley.model.game import Session
from barkley.model.world import World
from barkley.assets.palette import CWATER, Shade
from barkley.assets.store import LoadZoneArt
from barkley.assets.tunes import ZoneTune
from barkley.world.terrain.track import Faces, MOVEW, PIPER, Track
from barkley.world.terrain.track import WAVEA
from barkley.world.layout import AddMover, AddObj, Clearing, Column
from barkley.world.layout import Limbs, Pair, Pit, PITD, Rooted
from barkley.world.layout import Scatter, Shelf, SortObjs, Weed

' A clothesline's ends over the roofs it joins, and how far in from their edges its posts stand.
const LINEH = 24
private const POSTIN = 6

' A tram's roof over its street.
const TRAMH = 32

' A lantern line: its post over the roof it leaves, how far lower the roof it comes down to lies, and its end over
' that one.
const LANTH = 40
const LANTFALL = 64
private const LANTEND = 8

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
private sub SeekLevel (df as DataFile, st as integer)
    dim levels as long
    OpenData df
    seek #df.num, 5
    levels = Word&(df)
    seek #df.num, levels + 4 + st * 4
    seek #df.num, levels + Word&(df)
end sub

' A prop as the works have it, where the van's road is theirs: a tank for a tree, a vent for a bush; nothing grows
' there, and no rock lies.
private function Plated% (k as integer)
    select case k
    case KTREE
        Plated% = KTANK
    case KBUSH
        Plated% = KVENT
    case KFLOWER, KROCK
        Plated% = 0
    case else
        Plated% = k
    end select
end function

' The ground's columns, then what stands on them, from the stage's level: a command and its count, to its end.
sub LoadLevel (wd as World, st as integer)
    dim cmd as string, n as integer, c as integer, g as integer
    dim bx as integer
    dim i as integer, s as integer, k as integer, z as integer
    dim light as integer, fall as integer
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
    wd.lv.goal_x = NONE
    wd.lv.fam_x = NONE
    wd.lv.chase_x = NONE
    wd.lv.tide_lo = NONE
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
    wd.lv.glow_at = GLOWN
    wd.lv.aval_x = NONE
    wd.lv.oak.x = NONE
    wd.lv.fallow = 0
    wd.lv.n_things = 0
    wd.lv.n_gold = 0
    wd.lv.n_movers = 0
    wd.lv.n_gulls = 0
    wd.lv.n_swarms = 0
    wd.lv.n_ropes = 0
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
    SeekLevel wd.dat, st
    do
        cmd = Chars$(wd.dat)
        n = Word&(wd.dat)
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
        case "gap"
            c = c + n
        case "ice"
            ' the ground from here bare ice, or with 0 under snow again
            wd.lv.icing = n <> 0
        case "wall"
            g = g + n
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
        case "bump"
            AddObj wd.lv, KBUMP, c * TS + 8, s - n * TS
        case "hoop"
            AddObj wd.lv, KHOOP, c * TS + 8, s - n * TS
        case "pipe"
            ' a wall with a quarter pipe up to it, over the three columns before: nothing grows there
            for i = 0 to wd.lv.n_things - 1
                if Rooted%(wd.lv, i, KTREE) and wd.lv.things(i).at.x >= c * TS - PIPER then wd.lv.things(i).kind = 0
            next
            AddObj wd.lv, KPIPE, c * TS, s
            ' Amber Downs' pipe is drawn with the ground under it
            if z = 0 then
                wd.lv.vis(c - 1) = 0
                wd.lv.vis(c - 2) = 0
                wd.lv.vis(c - 3) = 0
            end if
            g = g + n
        case "drop"
            g = g - n
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
                wd.lv.movers(wd.lv.n_movers - 1).button = bx
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
            wd.lv.n_gold = wd.lv.n_gold + 1
        case "arc"
            for i = 0 to n - 1
                AddObj wd.lv, KBONE, (c + i) * TS + 8, s - 20 - cint(50 * sin(PI * (i + .5) / n))
            next
        case "hb"
            for i = 1 to n mod 100
                AddObj wd.lv, KBONE, (c + i) * TS + 8, s - (n \ 100) * TS - 14
            next
        case "cat"
            k = KCAT
        case "snatcher"
            k = KSNATCH
        case "crab"
            k = KCRAB
        case "sweeper"
            k = KSWEEP
        case "sled"
            k = KSLED
        case "thrower"
            k = KTHROW
        case "gale"
            wd.lv.gale = -1
        case "night"
            wd.lv.murk = -1
        case "avalanche"
            wd.lv.aval_x = c * TS + 8
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
        case "drift"
            k = KDRIFT
        case "spark"
            k = KSPARK
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
        case "buoy"
            AddObj wd.lv, KBUOY, (c + n) * TS + 8, s
        case "gull"
            ' a gull, its home n tiles over the ground here
            wd.lv.gulls(wd.lv.n_gulls).home.x = c * TS + 8
            wd.lv.gulls(wd.lv.n_gulls).home.y = s - n * TS
            wd.lv.n_gulls = wd.lv.n_gulls + 1
        case "line"
            ' a street n wide and a clothesline over it, post to post, LINEH over the roof here
            Hang wd.lv, c, n, s - LINEH, 0
            Pit wd.lv, c, g, n
            c = c + n
        case "walker"
            ' a cat walking the line just hung, its home n columns along it
            i = wd.lv.n_ropes - 1
            AddObj wd.lv, KWALKER, wd.lv.ropes(i).at.x + POSTIN + n * TS + 8, wd.lv.ropes(i).at.y
        case "slide"
            ' a street n wide and a lantern line down over it, from a post LANTH over the roof here to the roof
            ' past it, LANTFALL lower; a bone over the line every other column
            fall = LANTH + LANTFALL - LANTEND
            Hang wd.lv, c, n, s - LANTH, fall
            for i = 1 to n - 1 step 2
                AddObj wd.lv, KBONE, (c + i) * TS, s - LANTH - 14 + clng(i * TS + POSTIN) * fall \ (n * TS + POSTIN * 2)
            next
            Pit wd.lv, c, g - LANTFALL \ TS, n
            c = c + n
            g = g - LANTFALL \ TS
        case "awning"
            ' an awning out from the wall that begins here, n tiles over the roof
            AddObj wd.lv, KAWNING, c * TS - AWNOUT, s - n * TS
        case "chimney"
            ' a chimney: of the two of one n the first is the way in, the second the way out; till they are told
            ' each other, -n - 1
            Clearing wd.lv, c
            AddObj wd.lv, KCHIMNEY, c * TS + 8, s
            wd.lv.things(wd.lv.n_things - 1).state = -n - 1
        case "mortar"
            ' a mortar, firing on beat n of every bar
            Clearing wd.lv, c
            AddObj wd.lv, KMORTAR, c * TS + 8, s
            wd.lv.things(wd.lv.n_things - 1).state = n
        case "tipper"
            ' a house behind the way, a cat at its window: its pot goes on beat n of every bar
            AddObj wd.lv, KTIPPER, c * TS + 8, s
            wd.lv.things(wd.lv.n_things - 1).state = n
        case "tram"
            ' a tram on the street here, its way the n columns on: they are laid after it
            AddMover wd.lv, c * TS + 8, s - TRAMH, n * TS - MOVEW - 16, 8, c * TS
        case "pouncer"
            ' a dustbin, and the cat in it
            Clearing wd.lv, c
            AddObj wd.lv, KBIN, c * TS + 8, s
            k = KPOUNCE
        case "spring"
            k = KSPRING
        case "sapling"
            k = KSAPLING
        case "bramble"
            k = KBRAMBLE
        case "log"
            k = KLOG
        case "vine"
            AddObj wd.lv, KVINE, c * TS + 8, s - n * TS
        case "squirrel"
            AddObj wd.lv, KSQUIRREL, c * TS + 8, s - n * TS
        case "bees"
            ' a swarm, its home n tiles over the ground here
            wd.lv.swarms(wd.lv.n_swarms).home.x = c * TS + 8
            wd.lv.swarms(wd.lv.n_swarms).home.y = s - n * TS
            wd.lv.n_swarms = wd.lv.n_swarms + 1
        case "burrow"
            ' a burrow's mound, its other end the mound of the same n: till they are told each other, -n - 1
            AddObj wd.lv, KMOUND, c * TS + 8, s
            wd.lv.things(wd.lv.n_things - 1).state = -n - 1
        case "oak"
            ' the great oak here, its limbs' flats over the ground either side of it
            wd.lv.oak.x = c * TS + 8
            wd.lv.oak.y = s
            Limbs wd.lv, g
        case "mark"
            k = KMARK
        case "boost"
            k = KBOOST
        case "loop"
            k = KLOOP
        case "tree"
            k = KTREE
        case "bush"
            k = KBUSH
        case "flower"
            k = KFLOWER
        case "rock"
            k = KROCK
        case "thorn"
            k = KTHORN
        case "jet"
            Clearing wd.lv, c
            k = KJET
        case "spin"
            k = KSPIN
        case "goal"
            k = KGOAL
            wd.lv.goal_x = c * TS + 8
        case "fam"
            k = KFAM
            wd.lv.fam_x = c * TS + 8
        case "chase"
            wd.lv.chase_x = c * TS
            wd.lv.chase_end = (c + n) * TS
        case "tide"
            ' the sea: its height at low water * 100 + at high, in the ground's tiles
            wd.lv.tide_lo = (MH - n \ 100) * TS
            wd.lv.tide_hi = (MH - n mod 100) * TS
            wd.lv.tide = wd.lv.tide_lo
            wd.lv.tide_at = wd.se.ticks
        case "tune"
            wd.mu.stage_tune = n
        case "dog"
            wd.lv.start.x = c * TS + 8
            wd.lv.start.y = s
        end select
        if wd.zn.decked then k = Plated%(k)
        if k then AddObj wd.lv, k, c * TS + 8, s
    loop until cmd = "end"
    close #wd.dat.num
    wd.lv.w = c
    wd.lv.wpx = c * TS
    ' nothing grows in front of the family
    for i = 0 to wd.lv.n_things - 1
        if Rooted%(wd.lv, i, KBUSH) and abs(wd.lv.things(i).at.x - wd.lv.fam_x) < 112 then wd.lv.things(i).kind = 0
    next
    SortObjs wd.lv
    Pair wd.lv, KMOUND, -1
    Pair wd.lv, KCHIMNEY, 0
    Walkers wd.lv
    Track wd.lv, wd.gr
    Faces wd.lv
    ZoneTune wd.mu, wd.mu.stage_tune
end sub

' A line hung over the street of n columns from column c: its near end at height y, its far end fall lower.
' Nothing grows by its posts.
private sub Hang (lv as Level, c as integer, n as integer, y as integer, fall as integer)
    lv.ropes(lv.n_ropes).at.x = c * TS - POSTIN
    lv.ropes(lv.n_ropes).w = n * TS + POSTIN * 2
    lv.ropes(lv.n_ropes).at.y = y
    lv.ropes(lv.n_ropes).fall = fall
    lv.ropes(lv.n_ropes).t = 0
    lv.ropes(lv.n_ropes).cat = 0
    lv.n_ropes = lv.n_ropes + 1
    Weed lv, c * TS - 20
    lv.fallow = c + n + 2
end sub

' Each line told the cat that walks it, once the things are in their order: the walker over its street.
private sub Walkers (lv as Level)
    dim i as integer, r as integer
    for i = 0 to lv.n_things - 1
        if lv.things(i).kind <> KWALKER then continue for
        for r = 0 to lv.n_ropes - 1
            if lv.things(i).at.x <= lv.ropes(r).at.x or lv.things(i).at.x >= lv.ropes(r).at.x + lv.ropes(r).w then continue for
            lv.ropes(r).cat = i + 1
        next
    next
end sub

' The pier's fallen spans back in their places, the swarms at home: Charlie starts again before them.
sub Mend (lv as Level)
    dim m as integer
    for m = 0 to lv.n_swarms - 1
        lv.swarms(m).at.x = lv.swarms(m).home.x
        lv.swarms(m).at.y = lv.swarms(m).home.y
        lv.swarms(m).st = 0
    next
    for m = 0 to lv.n_movers - 1
        if lv.movers(m).kind = 6 then lv.movers(m).t = 0
    next
    for m = 0 to lv.n_things - 1
        if lv.things(m).kind = KSLED then lv.things(m).state = 0
        if lv.things(m).kind <> KGOAT then continue for
        lv.things(m).state = 1
        lv.things(m).at.x = lv.things(m).home
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
        if lv.things(j).kind = KGOLD then n = n + 1
    next
    GoldNo% = n
end function
