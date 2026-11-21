' The World: every part of the game, its arrays sized.
defint A-Z
option explicit

from barkley.engine.video import Store
from barkley.engine.input import Keys
from barkley.engine.audio import Deck, NBAR, NDRUM, NSLOT, NVOICE
from barkley.model.images import IMGSZ, NIMG
from barkley.model.level import Ground, Level, MH, MW, NSTAGE, Realm, TS, ZONES
from barkley.model.game import Session, Tones

' All of it. What runs the game takes it whole; the rest take the parts they use.
type World
    lv as Level
    gr as Ground
    zn as Realm
    art as Store
    tn as Tones
    kb as Keys
    mu as Deck
    se as Session
end type

private const NPTS = 1600, NZONE = 47

' The moving platforms there can be, and their width.
const NMOVE = 23

private const MAXOBJ = 600
const NLOB = 4

' Bees: the swarms a level may hold, and the bees drawn in one; how near he wakes it; its speed, under his
' run's; how near it stings; how far behind he has lost it; a bee's body and its wings.
private const NSWARM = 4

' A gull: how near Charlie passes under it for a dive, its dive's speed and its climb's, its rest between dives,
' the bones it takes, how far a bark reaches it; and what it is doing: gone is 0.
private const NGULL = 6

' The clotheslines a level may hold.
private const NROPE = 12

' The records' arrays, sized: a TYPE's own bounds must be numbers, and these are constants.
sub Allot (wd as World)
    dim i as integer
    redim wd.lv.map(MW - 1, MH - 1)
    redim wd.lv.prof(7, TS - 1)
    redim wd.lv.top(MW)
    redim wd.lv.vis(MW)
    redim wd.lv.faceL(MW)
    redim wd.lv.faceR(MW)
    redim wd.lv.glaze(MW)
    redim wd.lv.first(MW)
    redim wd.lv.ob(MAXOBJ - 1)
    redim wd.lv.mv(NMOVE)
    redim wd.lv.gulls(NGULL - 1)
    redim wd.lv.swarms(NSWARM - 1)
    redim wd.lv.ropes(NROPE - 1)
    redim wd.lv.lobs(NLOB - 1)
    redim wd.gr.pt(NPTS)
    redim wd.gr.cv(NZONE)
    redim wd.gr.col(MW)
    redim wd.art.img(IMGSZ)
    redim wd.art.at(NIMG)
    redim wd.art.datAt(ZONES - 1)
    redim wd.tn.n(NSTAGE - 1)
    redim wd.tn.which(NSTAGE - 1, 159)
    redim wd.tn.own(NSTAGE - 1, 159)
    redim wd.mu.song(NSLOT - 1)
    for i = 0 to NSLOT - 1
        redim wd.mu.song(i).half(NBAR * 2 - 1)
        redim wd.mu.song(i).voOne(NVOICE - 1)
        redim wd.mu.song(i).voTwo(NVOICE - 1)
        redim wd.mu.song(i).voUnder(NVOICE - 1)
        redim wd.mu.song(i).drPatch(NDRUM - 1)
        redim wd.mu.song(i).drNote(NDRUM - 1)
        redim wd.mu.song(i).drVol(NDRUM - 1)
        redim wd.mu.song(i).drUp(NDRUM - 1)
    next
end sub
