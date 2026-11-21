' Sparks, dust and the bones knocked loose.
defint A-Z
option explicit

from engine.vec import Vec
from barkley.model.level import GRAV, Level, MH, PI, TS
from barkley.model.game import Loose, NLOOSE, NPART, Part, Session
from barkley.assets.palette import CCLAY, CEARTH, CGOLD, CLEAF, CLOAM
from barkley.assets.palette import CNUT, CSEA, CSOOT, CSTONE, CWHITE
from barkley.assets.palette import CYELLOW
from barkley.world.terrain.ground import Solid, Surf

' The snow he rolled up, flown apart.
const PSNOWCOAT = 0

' A dash wound up.
const PWIND = 1

' A long fall landed.
const PLAND = 2

' Off the sled, into snow.
const PSLEDSNOW = 3

' Off the sled, the sled gone.
const PSLEDWOOD = 4

' The loam stirred over him.
const PDIG = 5

' Up out of a mound.
const PMOUND = 6

' Up out of a pot, in its soot.
const PFLUE = 7

' The sea shaken off.
const PDRY = 8

' A drop off him, wet.
const PDRIP = 9

' Into the sea.
const PSPLASH = 10

' A paddle.
const PPADDLE = 11

' A foe beaten.
const PBEATEN = 12

' A bone taken.
const PBONE = 13

' A golden bone taken.
const PGOLD = 14

' A boost.
const PBOOST = 15

' A mark or a button met.
const PMARK = 16

' A bumper.
const PBUMP = 17

' A hoop gone through.
const PHOOP = 18

' A drift ploughed.
const PDRIFT = 19

' A wall of snow broken.
const PWALL = 20

' An icicle shattered.
const PSHATTER = 21

' A thrower buried.
const PBURY = 22

' A bramble grown back.
const PLEAF = 23

' Down a chimney.
const PSOOT = 24

' A pot broken.
const PPOT = 25

' A bone snatched.
const PSNATCH = 26

' An acorn burst.
const PACORN = 27

' A snowball burst.
const PSNOWBALL = 28

' The cart bumped.
const PCARTBUMP = 29

' The cart broken.
const PCART = 30

' The keepsake taken.
const PKEEP = 31

' The box broken.
const PBOX = 32

' The bones knocked loose, dt on: each falls and bounces
' till its time is up or it is off the world's foot.
sub LooseBones ( _
    lv as Level, _
    se as Session, _
    lo() as Loose, _
    dt as single _
)
    dim i as integer, x as integer, y as integer, s as integer

    for i = 0 to NLOOSE - 1
        if lo(i).t <= 0 then continue for

        lo(i).t = lo(i).t - dt
        lo(i).vel.y = lo(i).vel.y + GRAV * .6 * dt
        lo(i).at.x = lo(i).at.x + lo(i).vel.x * dt

        if Solid%(lv, cint(lo(i).at.x), cint(lo(i).at.y) - 3) then
            lo(i).vel.x = -lo(i).vel.x
        end if

        lo(i).at.y = lo(i).at.y + lo(i).vel.y * dt
        x = cint(lo(i).at.x)
        y = cint(lo(i).at.y) + 4
        s = Surf%(lv, x, y)

        if s <= y and y - s < 14 and lo(i).vel.y > 0 then
            lo(i).at.y = s - 4
            lo(i).vel.y = -lo(i).vel.y * .7
        end if

        if lo(i).at.y > MH * TS + 16 then lo(i).t = 0
        if lo(i).t <= 0 then se.loose_n = se.loose_n - 1
    next
end sub

' n bones let fall at x, y, loose, to be picked up.
sub Strew ( _
    lo() as Loose, _
    se as Session, _
    x as single, _
    y as single, _
    n as integer _
)
    dim i as integer, m as integer

    m = n

    for i = 0 to NLOOSE - 1
        if m = 0 then exit for
        if lo(i).t > 0 then continue for

        lo(i).at.x = x
        lo(i).at.y = y
        lo(i).vel.x = (m * 37 mod 90) - 45
        lo(i).vel.y = -40
        lo(i).t = 4
        se.loose_n = se.loose_n + 1
        m = m - 1
    next
end sub

' Loose bone b taken for good.
sub Pick (lo() as Loose, se as Session, b as integer)
    lo(b).t = 0
    se.loose_n = se.loose_n - 1
end sub

' n bones knocked loose from x, y, fanned out over it.
sub Shed ( _
    lo() as Loose, _
    se as Session, _
    x as single, _
    y as single, _
    n as integer _
)
    dim i as integer, a as single, v as single

    for i = 0 to n - 1
        a = PI * (.15 + .7 * i / n)
        v = 240 - 70 * (i and 1)
        lo(i).at.x = x
        lo(i).at.y = y
        lo(i).vel.x = cos(a) * v * (1 - 2 * (i and 1)) * .8
        lo(i).vel.y = -sin(a) * v
        lo(i).t = 4
    next

    se.loose_n = n
end sub

' The particles, dt on: each on its arc till its time is up.
sub Particles (se as Session, pt() as Part, dt as single)
    dim i as integer

    for i = 0 to NPART - 1
        if pt(i).t <= 0 then continue for

        pt(i).t = pt(i).t - dt
        pt(i).at.x = pt(i).at.x + pt(i).vel.x * dt
        pt(i).at.y = pt(i).at.y + pt(i).vel.y * dt
        pt(i).vel.y = pt(i).vel.y + 300 * dt
        if pt(i).t <= 0 then se.part_n = se.part_n - 1
    next
end sub

sub Emit ( _
    se as Session, _
    pt() as Part, _
    x as single, _
    y as single, _
    vx as single, _
    vy as single, _
    t as single, _
    c as integer _
)
    dim i as integer

    if se.part_n >= NPART then exit sub

    for i = 0 to NPART - 1
        if pt(i).t > 0 then continue for

        pt(i).at.x = x
        pt(i).at.y = y
        pt(i).vel.x = vx
        pt(i).vel.y = vy
        pt(i).t = t
        pt(i).c = c
        se.part_n = se.part_n + 1
        exit sub
    next
end sub

' n sparks out from x, y at speed v.
sub Burst ( _
    se as Session, _
    pt() as Part, _
    x as single, _
    y as single, _
    n as integer, _
    v as single, _
    c as integer _
)
    dim i as integer, a as single

    for i = 0 to n - 1
        a = 2 * PI * (i + rnd * .5) / n
        Emit se, pt(), x, y, cos(a) * v, sin(a) * v - v * .5, .5, c
    next
end sub

' The puff id at at: its sparks, or its drop.
sub Puff (se as Session, pt() as Part, at as Vec, id as integer)
    select case id
    case PSNOWCOAT
        Burst se, pt(), at.x, at.y, 12, 140, CWHITE
    case PWIND
        Burst se, pt(), at.x, at.y, 3, 70, CSTONE + 3
    case PLAND
        Burst se, pt(), at.x, at.y, 5, 60, CSTONE + 3
    case PSLEDSNOW
        Burst se, pt(), at.x, at.y, 10, 120, CWHITE
    case PSLEDWOOD
        Burst se, pt(), at.x, at.y, 10, 120, CEARTH + 4
    case PDIG
        Burst se, pt(), at.x, at.y, 2, 50, CLOAM
    case PMOUND
        Burst se, pt(), at.x, at.y, 10, 140, CLOAM
    case PFLUE
        Burst se, pt(), at.x, at.y, 10, 140, CSOOT
    case PDRY
        Burst se, pt(), at.x, at.y, 10, 130, CSEA + 1
    case PDRIP
        Emit se, pt(), at.x, at.y, 0, 30, .3, CSEA + 2
    case PSPLASH
        Burst se, pt(), at.x, at.y, 8, 110, CSEA
    case PPADDLE
        Burst se, pt(), at.x, at.y, 5, 80, CSEA
    case PBEATEN
        Burst se, pt(), at.x, at.y, 10, 130, CWHITE
    case PBONE
        Burst se, pt(), at.x, at.y, 4, 50, CGOLD + 4
    case PGOLD
        Burst se, pt(), at.x, at.y, 10, 90, CGOLD + 4
    case PBOOST
        Burst se, pt(), at.x, at.y, 8, 150, CGOLD + 4
    case PMARK
        Burst se, pt(), at.x, at.y, 8, 80, CGOLD + 4
    case PBUMP
        Burst se, pt(), at.x, at.y, 6, 90, CGOLD + 4
    case PHOOP
        Burst se, pt(), at.x, at.y, 8, 120, CGOLD + 4
    case PDRIFT
        Burst se, pt(), at.x, at.y, 8, 100, CWHITE
    case PWALL
        Burst se, pt(), at.x, at.y, 16, 180, CWHITE
    case PSHATTER
        Burst se, pt(), at.x, at.y, 8, 110, CWHITE
    case PBURY
        Burst se, pt(), at.x, at.y, 16, 160, CWHITE
    case PLEAF
        Burst se, pt(), at.x, at.y, 10, 140, CLEAF
    case PSOOT
        Burst se, pt(), at.x, at.y, 8, 90, CSOOT
    case PPOT
        Burst se, pt(), at.x, at.y, 8, 120, CCLAY
    case PSNATCH
        Burst se, pt(), at.x, at.y, 4, 60, CWHITE
    case PACORN
        Burst se, pt(), at.x, at.y, 6, 90, CNUT
    case PSNOWBALL
        Burst se, pt(), at.x, at.y, 6, 90, CWHITE
    case PCARTBUMP
        Burst se, pt(), at.x, at.y, 12, 150, CYELLOW
    case PCART
        Burst se, pt(), at.x, at.y, 16, 200, CEARTH + 4
    case PKEEP
        Burst se, pt(), at.x, at.y, 12, 120, CYELLOW
    case PBOX
        Burst se, pt(), at.x, at.y, 10, 140, CEARTH + 4
    end select
end sub

' No bones loose, no specks.
sub Sweep (lo() as Loose, pt() as Part)
    dim i as integer

    for i = 0 to NLOOSE - 1
        lo(i).t = 0
    next

    for i = 0 to NPART - 1
        pt(i).t = 0
    next
end sub
