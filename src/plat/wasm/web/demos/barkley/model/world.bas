' The World: every part of the game, its arrays sized.
defint A-Z
option explicit

from engine.datafile import DataFile, NameData
from engine.video import Store
from engine.input import Keys
from engine.audio import Deck, NBAR, NDRUM, NSLOT, NVOICE
from engine.events import Events
from barkley.model.images import IMGSZ, NIMG
from barkley.model.hero import Autopilot
from barkley.model.level import Ground, Level, MH, MW, NSTAGE, Realm
from barkley.model.level import TS, ZONES
from barkley.model.game import Session, Tones

' All of it. What runs the game takes it whole; the rest
' take the parts they use.
type World
    lv as Level
    gr as Ground
    zn as Realm
    art as Store
    tn as Tones
    kb as Keys
    mu as Deck
    se as Session
    pilot as Autopilot
    dat as DataFile
    ev as Events
end type

private const NPTS = 1600
private const NZONE = 47

' The moving platforms there can be, and their width.
const NMOVE = 23

private const MAXOBJ = 600
const NLOB = 4

' Bees: the swarms a level may hold, and the bees drawn in
' one; how near he wakes it; its speed, under his run's; how
' near it stings; how far behind he has lost it; a bee's
' body and its wings.
private const NSWARM = 4

' A gull: how near Charlie passes under it for a dive, its
' dive's speed and its climb's, its rest between dives, the
' bones it takes, how far a bark reaches it; and what it is
' doing: gone is 0.
private const NGULL = 6

' The clotheslines a level may hold.
private const NROPE = 12

' The records' arrays, sized: a TYPE's own bounds must be
' numbers, and these are constants.
sub Allot (wd as World)
    dim i as integer

    ' where its tunes and its levels start, then its
    ' pictures
    NameData wd.dat, "CHARLIE.DAT", "CHARLIE.DAT, the game's " + _
        "pictures and tunes, must be beside CHARLIE.BAS."
    NameData wd.mu.dat, wd.dat.path, wd.dat.lack
    NameData wd.art.dat, wd.dat.path, wd.dat.lack
    redim wd.lv.map(MW - 1, MH - 1)
    redim wd.lv.prof(7, TS - 1)
    redim wd.lv.top(MW)
    redim wd.lv.vis(MW)
    redim wd.lv.face_l(MW)
    redim wd.lv.face_r(MW)
    redim wd.lv.glaze(MW)
    redim wd.lv.first(MW)
    redim wd.lv.things(MAXOBJ - 1)
    redim wd.lv.movers(NMOVE)
    redim wd.lv.gulls(NGULL - 1)
    redim wd.lv.swarms(NSWARM - 1)
    redim wd.lv.ropes(NROPE - 1)
    redim wd.lv.lobs(NLOB - 1)
    redim wd.gr.pt(NPTS)
    redim wd.gr.cv(NZONE)
    redim wd.gr.col(MW)
    redim wd.art.img(IMGSZ)
    redim wd.art.at(NIMG)
    redim wd.art.dat_at(ZONES - 1)
    redim wd.tn.n(NSTAGE - 1)
    redim wd.tn.which(NSTAGE - 1, 159)
    redim wd.tn.own(NSTAGE - 1, 159)
    redim wd.mu.song(NSLOT - 1)

    for i = 0 to NSLOT - 1
        redim wd.mu.song(i).half(NBAR * 2 - 1)
        redim wd.mu.song(i).vo_one(NVOICE - 1)
        redim wd.mu.song(i).vo_two(NVOICE - 1)
        redim wd.mu.song(i).vo_under(NVOICE - 1)
        redim wd.mu.song(i).dr_patch(NDRUM - 1)
        redim wd.mu.song(i).dr_note(NDRUM - 1)
        redim wd.mu.song(i).dr_vol(NDRUM - 1)
        redim wd.mu.song(i).dr_up(NDRUM - 1)
    next
end sub
