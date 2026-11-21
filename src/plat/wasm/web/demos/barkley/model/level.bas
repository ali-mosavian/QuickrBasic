' A level's records and measures: tiles, gravity, the kinds of what stands in it, its ground, its zone.
defint A-Z
option explicit

from barkley.engine.vec import Spot, Vec

' A swarm of bees: where it is, at; its home; and what it does: 0 at home, 1 after him, 2 on its way home.
type Swarm
    at as Vec
    home as Spot
    st as integer
end type

' A gull: where it is, its home, where its dive is aimed, the seconds of rest till it may dive again, what it is doing,
' the bones it carries, and whether it heads left.
type Bird
    at as Vec
    home as Spot
    aim as Vec
    rest as single
    st as integer
    loot as integer
    face as integer
end type

' What is thrown or dropped, on its arc: its picture, and the colour it bursts in. bedded: let go in a bough,
' which it falls out of before any ground stops it.
type Lob
    at as Vec
    vel as Vec
    live as integer
    pic as integer
    c as integer
    bedded as integer
end type

' Who flees with a keepsake at an act's end: x, y the foot of its cart, or the floor under its crane, v its speed,
' t the seconds its last bump or its breaking still tells, hits its bumps. st: 0 waiting, 1 fleeing, 2 broken and
' the keepsake lying, 3 the keepsake taken back. A crane's crate is one too: x, y its foot, v its fall, t the
' seconds to its next step. st: 0 on the hook once t is out, 1 held over Charlie, 2 falling.
type Runner
    at as Vec
    v as single
    t as single
    hits as integer
    st as integer
end type

' A clothesline: from at, w wide, its ends level; trod: where along it it was last trodden, t the steps since. A lantern
' line is one whose far end hangs fall lower: taut, and ground to slide down, its stretch sg. cat: one more than
' the thing that is the cat walking it, 0 with none.
type Rope
    at as Spot
    w as integer
    trod as integer
    t as integer
    fall as integer
    sg as integer
    cat as integer
end type

' A thing of the level: where it is, its kind and state, and its home x, which the level's order goes by.
type Thing
    at as Spot
    kind as integer
    state as integer
    home as integer
end type

' A moving platform: its thing, its stretch sg and curve cv; its middle at, its reach; kind 0 to and fro, 1 up and
' down, 2 a lift, t how far up it is; the edge before it; a bridge's button. 3 a basket down a cable, 7 a chair
' up one, 8 a tram along a street: t how far along.
type Mover
    thing as integer
    sg as integer
    cv as integer
    at as Spot
    reach as integer
    kind as integer
    edge as integer
    button as integer
    t as single
end type

' The goat out of its place: st 1 ridden, 2 on its way home from at, 0 none.
type Rover
    at as Vec
    goat as integer
    st as integer
end type

' The avalanche's front, and whether it is let go.
type Front
    x as single
    st as integer
end type

' The level as laid: w columns, wpx pixels, of the MW by MH map; a column's top, what shows of it, its faces, its
' glaze (the pass's bare ice; icing: those laid now are), the first thing at it. prof: a tile kind's solid height
' by column. The race from chaseX to chaseEnd and the avalanche's head avalX, NONE where there is none. The
' sea's surface tide, NONE without one, from tideLo at step tideAt to tideHi half a tide later. Who flees in its
' chase, a crane's crate, the goat out of its place, the avalanche. The great oak's foot oakX, oakY, NONE without.
' The quarter's clotheslines.
type Level
    w as integer
    wpx as integer
    goalX as integer
    famX as integer
    startX as integer
    startY as integer
    chaseX as integer
    chaseEnd as integer
    avalX as integer
    oakX as integer
    oakY as integer
    nGold as integer
    tide as integer
    tideLo as integer
    tideHi as integer
    tideAt as long
    icing as integer
    fallow as integer
    gale as integer
    murk as integer
    glow as integer
    glowAt as integer
    prof() as integer
    top() as integer
    vis() as integer
    faceL() as integer
    faceR() as integer
    glaze() as integer
    first() as integer
    nObj as integer
    ob() as Thing
    nMv as integer
    mv() as Mover
    nGulls as integer
    gulls() as Bird
    nSwarm as integer
    swarms() as Swarm
    nRope as integer
    ropes() as Rope
    lobN as integer
    lobs() as Lob
    map() as integer
    cart as Runner
    box as Runner
    stray as Rover
    aval as Front
end type

' A stretch of the ground's line, from its point x, y to the next: its kind k (0 none, 1 the tiles' top, 2 a
' curve), l long, u, v a pixel along it, its rise a.
type Stretch
    x as single
    y as single
    u as single
    v as single
    l as single
    a as single
    k as integer
end type

' A curve: stretches a to b, from m on a loop's far half, between x l and r.
type Bend
    a as integer
    b as integer
    m as integer
    l as integer
    r as integer
end type

' The ground's line, what Charlie runs on: n points, the platforms' from platA; nz curves; a column's stretch.
type Ground
    n as integer
    platA as integer
    nz as integer
    pt() as Stretch
    cv() as Bend
    col() as integer
end type

' The zone played, 0 the Amber Downs, and its stage; the one ResetWorld is to load; its pictures: tiles, backdrop,
' loop, pipe, card, ribbon. decked: its ground is a deck, the works' kind, else earth, the Downs'.
type Realm
    id as integer
    stage as integer
    nextUp as integer
    tile as integer
    back as integer
    ring as integer
    pipe as integer
    card as integer
    wave as integer
    decked as integer
end type

' The world: MW by MH tiles of TS pixels; lv.w columns used. A tile's kind
' is its profile, lv.prof: each column's solid height from its bottom.
const TS = 16, MW = 800, MH = 30, NONE = 30000
const PI = 3.141593
const GRAV = 750!

' What stands in the level, by x. From KLOOP on: scenery, drawn behind
' the ground.
const KBONE = 1, KCAT = 2, KSPRING = 3, KMARK = 4, KBOOST = 5, KGOAL = 6, KPLAT = 7
const KLOOP = 8, KTREE = 9, KBUSH = 10, KFLOWER = 11, KROCK = 12, KTHORN = 13, KLAMP = 14, KFAM = 15, KPIPE = 16
const KMOVE = 17, KPOST = 18, KTANK = 19, KVENT = 20, KJET = 21, KBUMP = 22, KSPIN = 23, KHOOP = 24
const KBUTTON = 27, KCRAB = 28, KJELLY = 29, KBUOY = 30

const KSPAN = 31
const KPRESS = 32, KLEG = 33, KSWEEP = 43, KSPARK = 44
const KDRIFT = 34, KSLED = 45
const KTHROW = 46
const KICICLE = 35
const KSEAT = 36
const KGOAT = 47
const KWALL = 37
const KLOAD = 48, KHEAP = 49

' From KFRONT on: drawn over the ground. The sands' flume: a column of its fall, its level end, the tank at its
' head; the speed its water carries Charlie at.
const KFRONT = 40, KFLUME = 40, KSPOUT = 41, KCISTERN = 42
const KWAVE = 25, KWAVE2 = 26

const KSAPLING = 38
const KVINE = 39
const KBRAMBLE = 50
const KLOG = 51
const KMOUND = 52
const KSQUIRREL = 53
' A golden bone: an act has NGOLD of them, each kept once found.
const KGOLD = 54, NGOLD = 3
const KAWNING = 55, KCHIMNEY = 56, KTRAM = 57, KBIN = 58, KPOUNCE = 59, KWALKER = 60, KSNATCH = 61, KMORTAR = 62
const KTIPPER = 63
const ZONES = 6, NSTAGE = ZONES * 2
