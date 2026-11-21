' The intro, and the bump between zones: a script of cues played by the beat.
defint A-Z
option explicit

from barkley.engine.video import Blit, SH, SW
from barkley.engine.clock import STEPT
from barkley.model.images import IBOXES, ICRATE, IHEDGE, IHOUSE
from barkley.model.hero import Hero, Pad
from barkley.model.level import Level
from barkley.model.game import Cue, Game, INTRO
from barkley.model.world import World
from barkley.assets.palette import Shade, Shimmer
from barkley.assets.cast import BaronPic, DrawCat, DrawEyes
from barkley.assets.cast import DrawFamily, DrawFloat, DrawKeep
from barkley.assets.cast import DrawVan
from barkley.assets.tunes import Sfx, ZoneTune
from barkley.world.level import LoadLevel, SINTRO, ZoneOf
from barkley.world.backdrop import DrawBack, DrawTiles
from barkley.stage.hero.moves import BARKT
from barkley.stage.hero.draw import DrawBark, DrawHero, Pose
from barkley.stage.scene import DrawThings
from barkley.control.keys import MuteKey, Spent

' each zone's tune, its number in CHARLIE.DAT: a digit a zone
' The intro: BEATS a second, ILEN of them, the ground HORIZON down the screen between two bars BARH high. Its
' cues' kinds, in QNAMES' order, six letters a name.
const BEATS = 2!
const ILEN = 112
const HORIZON = 150
private const BARH = 14

' Between zones the bump again: its cues' first beat and how many, its song's number in CHARLIE.DAT.
const BUMP0 = 120
private const BUMPLEN = 16
const BETWEEN = 12
private const QCAM = 1
private const QHOUSE = 2
private const QBOXES = 3
private const QCRATE = 4
private const QHEDGE = 5
private const QFAM = 6
const QVAN = 7
private const QFLOAT = 8
private const QBARON = 9
private const QCAT = 10
private const QBASKET = 11
private const QEYES = 12
const QHERO = 13
private const QWOOF = 14
const QSND = 15
private const QNAMES = "cam   house boxes crate hedge fam   van   float baron cat   basketeyes  hero  woof  snd   "

' The intro's script, back to front: a cue's kind, the beats it is on from and to, where it starts and where it
' ends, its ease, its hop and its own number. A van's: 1 its door open, 2 Charlie at its window, 4 looking back
' from it. A float's: 1 the Baron aboard. A cat's: 1 the basket on it, 2 stood still. Charlie's: Pose's. A sound's:
' Sfx's. A scene every 16 beats, as the tune has them. What goes into the van, or is in it, comes before it.
Cues:
' the last morning: the family at the door, the boxes thrown aboard, Charlie round them all and in
data "cam", 0, 16, 0, 0, 60, 0, 0, 0, 0
data "house", 0, 32, 40, 0, 40, 0, 0, 0, 0
data "fam", 0, 16, 120, 0, 120, 0, 0, 0, 1
data "boxes", 0, 10, 232, 0, 232, 0, 0, 0, 0
data "crate", 4, 5.5, 232, -36, 276, -22, 0, 22, 0
data "crate", 8, 9.5, 232, -36, 276, -22, 0, 22, 0
data "hero", 14.5, 15.5, 240, 0, 280, -13, 0, 18, 3
data "van", 0, 16, 330, 0, 330, 0, 0, 0, 1
data "hero", 0, 3, 70, 0, 250, 0, 0, 0, 1
data "hero", 3, 6, 250, 0, 90, 0, 0, 0, 1
data "hero", 6, 9, 90, 0, 250, 0, 0, 0, 1
data "hero", 9, 11.5, 250, 0, 200, 0, 0, 0, 1
data "hero", 11.5, 13, 200, 0, 200, 0, 0, 0, 6
data "woof", 12, 12.8, 200, 0, 200, 0, 0, 0, 0
data "snd", 12, 12.5, 0, 0, 0, 0, 0, 0, 13
data "hero", 13, 14.5, 200, 0, 240, 0, 0, 0, 1
' the van pulls out, Charlie's head in the wind
data "cam", 16, 17, 80, 0, 80, 0, 0, 0, 0
data "cam", 17, 32, 80, 0, 1120, 0, 1, 0, 0
data "van", 16, 17, 330, 0, 330, 0, 0, 0, 2
data "van", 17, 32, 330, 0, 1280, 0, 1, 0, 2
' a hedge it passes: eyes, then the Mob, the Baron last; he points, their float comes, and they are after it
data "cam", 32, 48, 1400, 0, 1400, 0, 0, 0, 0
data "cat", 37, 40, 1570, 0, 1660, 0, 0, 0, 0
data "cat", 40, 46, 1660, 0, 1660, 0, 0, 0, 2
data "cat", 38, 41, 1560, 0, 1690, 0, 0, 0, 0
data "cat", 41, 46, 1690, 0, 1690, 0, 0, 0, 2
data "baron", 39, 39.75, 1600, 0, 1622, 0, 0, 10, 0
data "hedge", 32, 48, 1540, 0, 1540, 0, 0, 0, 0
data "eyes", 33.5, 37, 1496, -22, 1496, -22, 0, 0, 3
data "eyes", 37, 38.9, 1548, -22, 1548, -22, 0, 0, 1
data "van", 32, 36, 1280, 0, 1800, 0, 0, 0, 2
data "baron", 39.75, 40.5, 1622, 0, 1644, 0, 0, 10, 0
data "baron", 40.5, 41.5, 1644, 0, 1644, 0, 0, 0, 0
data "baron", 41.5, 45, 1644, 0, 1644, 0, 0, 0, 1
data "float", 42.5, 45, 1340, 0, 1590, 0, 2, 0, 0
data "float", 45, 45.75, 1590, 0, 1590, 0, 0, 0, 0
data "baron", 45, 45.75, 1644, 0, 1568, -31, 0, 16, 0
data "float", 45.75, 48, 1590, 0, 1890, 0, 1, 0, 1
data "cat", 46, 48, 1660, 0, 1840, 0, 0, 0, 0
data "cat", 46, 48, 1690, 0, 1860, 0, 0, 0, 0
' a bump: the latch pops and the basket is out; eyes in a hedge, a leap, and Biscuit is away with it
data "cam", 48, 64, 2140, 0, 2140, 0, 0, 0, 0
data "hedge", 48, 64, 2180, 0, 2180, 0, 0, 0, 0
data "eyes", 54, 58, 2146, -22, 2146, -22, 0, 0, 1
data "van", 48, 50, 1998, 0, 2258, 0, 0, 0, 2
data "van", 50, 51, 2258, 0, 2388, 0, 0, 9, 3
data "van", 51, 55, 2388, 0, 2908, 0, 0, 0, 3
data "snd", 50, 50.5, 0, 0, 0, 0, 0, 0, 3
data "basket", 50.2, 51.5, 2216, -30, 2196, 0, 0, 22, 0
data "basket", 51.5, 52.3, 2196, 0, 2186, 0, 0, 8, 0
data "basket", 52.3, 59.25, 2186, 0, 2186, 0, 0, 0, 0
data "cat", 58, 58.75, 2150, -20, 2172, 0, 0, 12, 0
data "cat", 58.75, 59.25, 2172, 0, 2172, 0, 0, 0, 2
data "snd", 58.75, 59, 0, 0, 0, 0, 0, 0, 12
data "cat", 59.25, 63.5, 2186, 0, 2500, 0, 0, 0, 1
' Charlie has seen: he barks from the window and no one hears; out of the door, then, and down
data "cam", 64, 72, 2700, 0, 3740, 0, 0, 0, 0
data "cam", 72, 75, 3740, 0, 3900, 0, 2, 0, 0
data "cam", 75, 100, 3900, 0, 3900, 0, 0, 0, 0
data "hero", 70.5, 72, 3689, -13, 3864, -13, 0, 0, 20
data "van", 64, 70.5, 2900, 0, 3745, 0, 0, 0, 5
data "van", 70.5, 80, 3745, 0, 4980, 0, 0, 0, 1
data "woof", 66, 66.8, 3152, -20, 3256, -20, 0, 0, 1
data "snd", 66, 66.5, 0, 0, 0, 0, 0, 0, 13
data "woof", 67.5, 68.3, 3347, -20, 3451, -20, 0, 0, 1
data "snd", 67.5, 68, 0, 0, 0, 0, 0, 0, 13
data "woof", 69, 69.8, 3542, -20, 3646, -20, 0, 0, 1
data "snd", 69, 69.5, 0, 0, 0, 0, 0, 0, 13
data "hero", 72, 73.5, 3864, -13, 3960, 0, 0, 28, 3
data "hero", 73.5, 75.5, 3960, 0, 4060, 0, 2, 0, 3
data "hero", 75.5, 80, 4060, 0, 4060, 0, 0, 0, 5
' the cat goes by him with the basket, then the Mob on its float: he looks after them, and barks
data "float", 85.5, 89.5, 3830, 0, 4330, 0, 0, 0, 1
data "hero", 80, 81, 4060, 0, 4060, 0, 0, 0, 0
data "hero", 81, 82.6, 4060, 0, 4060, 0, 0, 0, 16
data "cat", 81, 85, 3860, 0, 4300, 0, 0, 0, 1
data "hero", 82.6, 83.4, 4060, 0, 4060, 0, 0, 10, 5
data "hero", 83.4, 85.2, 4060, 0, 4060, 0, 0, 0, 0
data "hero", 85.2, 87.1, 4060, 0, 4060, 0, 0, 0, 16
data "hero", 87.1, 87.9, 4060, 0, 4060, 0, 0, 10, 5
data "hero", 87.9, 96, 4060, 0, 4060, 0, 0, 0, 0
data "cat", 86.5, 90.5, 3860, 0, 4300, 0, 0, 0, 0
data "cat", 87, 91, 3860, 0, 4300, 0, 0, 0, 0
data "woof", 92, 92.8, 4060, 0, 4060, 0, 0, 0, 0
data "snd", 92, 92.5, 0, 0, 0, 0, 0, 0, 13
data "woof", 93.5, 94.3, 4060, 0, 4060, 0, 0, 0, 0
data "snd", 93.5, 94, 0, 0, 0, 0, 0, 0, 13
' he winds up, as the player will, and is after them
data "hero", 96, 97, 4060, 0, 4060, 0, 0, 0, 4
data "hero", 97, 100, 4060, 0, 4060, 0, 0, 0, 7
data "snd", 97, 97.5, 0, 0, 0, 0, 0, 0, 6
data "snd", 98, 98.5, 0, 0, 0, 0, 0, 0, 6
data "snd", 99, 99.5, 0, 0, 0, 0, 0, 0, 6
data "snd", 100, 100.5, 0, 0, 0, 0, 0, 0, 7
data "hero", 100, 102, 4060, 0, 4260, 0, 1, 0, 2
data "hero", 102, 112, 4260, 0, 6160, 0, 0, 0, 2
data "cam", 100, 102, 3900, 0, 4020, 0, 1, 0, 0
data "cam", 102, 112, 4020, 0, 5940, 0, 0, 0, 0
' between zones, the bump again on the next zone's road: its keepsake out, and the cat that was following away
' with it
data "cam", 120, 136, 2140, 0, 2140, 0, 0, 0, 0
data "van", 120, 122, 1998, 0, 2258, 0, 0, 0, 2
data "van", 122, 123, 2258, 0, 2388, 0, 0, 9, 3
data "van", 123, 127, 2388, 0, 2908, 0, 0, 0, 3
data "snd", 122, 122.5, 0, 0, 0, 0, 0, 0, 3
data "basket", 122.2, 123.5, 2216, -30, 2196, 0, 0, 22, 0
data "basket", 123.5, 124.3, 2196, 0, 2186, 0, 0, 8, 0
data "basket", 124.3, 130.25, 2186, 0, 2186, 0, 0, 0, 0
data "cat", 126.5, 129.75, 2120, 0, 2172, 0, 0, 0, 0
data "cat", 129.75, 130.25, 2172, 0, 2172, 0, 0, 0, 2
data "snd", 129.75, 130, 0, 0, 0, 0, 0, 0, 12
data "cat", 130.25, 134.5, 2186, 0, 2500, 0, 0, 0, 1
data "end"


' The intro from its first beat: its road, its script, the view on the road's start.
sub StartIntro (wd as World, g as Game, cu() as Cue)
    wd.se.road = 0
    wd.se.scene_next = 0
    wd.se.scene_end = ILEN
    LoadLevel wd, SINTRO
    LoadCues cu()
    g.mode = INTRO
    g.mode_t = 0
    g.cam.x = 0
    g.cam.y = wd.lv.start.y - HORIZON
end sub

' Between zones the van's bump again, on the road of stage nxt's zone: its look, its keepsake and a cat, to four
' bars of their own; then that stage.
sub StartBump (wd as World, g as Game, nxt as integer)
    wd.se.road = ZoneOf%(wd.se, nxt)
    wd.se.scene_next = nxt
    wd.se.scene_end = BUMP0 + BUMPLEN
    LoadLevel wd, SINTRO
    ZoneTune wd.mu, BETWEEN
    wd.tn.dusk = 16
    Shade wd.tn
    g.mode = INTRO
    g.mode_t = BUMP0 / BEATS
    g.cam.y = wd.lv.start.y - HORIZON
end sub

' The script from its DATA: a row a cue, "end" after the last, its kind 0.
private sub LoadCues (cu() as Cue)
    dim n as integer, kind as string
    restore Cues
    do
        read kind
        cu(n).kind = (instr(QNAMES, left$(kind + "     ", 6)) + 5) \ 6
        if cu(n).kind = 0 then exit do
        read cu(n).t0, cu(n).t1, cu(n).start.x, cu(n).start.y, cu(n).finish.x, cu(n).finish.y, cu(n).ease, cu(n).hop, cu(n).own
        n = n + 1
    loop
end sub

' A step of the intro: its view and its sounds by the script, its last beat fading. Whether it is over: played
' out, or a key pressed.
function IntroOver% (wd as World, g as Game, k as Pad, cu() as Cue)
    dim i as integer, x as integer, y as integer, was as single
    dim now as single
    wd.se.ticks = wd.se.ticks + 1
    MuteKey wd.mu, k
    was = g.mode_t * BEATS
    g.mode_t = g.mode_t + STEPT
    now = g.mode_t * BEATS
    do while cu(i).kind
        if now >= cu(i).t0 and now < cu(i).t1 then
            select case cu(i).kind
            case QCAM
                CueAt wd.lv, cu(), i, now, x, y
                g.cam.x = x
            case QSND
                if was < cu(i).t0 then Sfx wd.mu, wd.se, cu(i).own
            end select
        end if
        i = i + 1
    loop
    Shimmer wd.zn, wd.tn, wd.se
    if now > wd.se.scene_end - 1 and wd.tn.dusk > 0 and (wd.se.ticks and 1) = 0 then
        wd.tn.dusk = wd.tn.dusk - 1
        Shade wd.tn
    end if
    IntroOver% = now >= wd.se.scene_end or k.hit or k.start or k.quit
    Spent k
end function

' Where cue i is at beat t, in the world: along its way, eased, a hop's arc over it.
sub CueAt (lv as Level, cu() as Cue, i as integer, t as single, x as integer, y as integer)
    dim u as single, v as single
    if cu(i).t1 > cu(i).t0 then
        u = (t - cu(i).t0) / (cu(i).t1 - cu(i).t0)
    end if
    v = u
    if cu(i).ease = 1 then v = u * u
    if cu(i).ease = 2 then v = u * (2 - u)
    x = cint(cu(i).start.x + (cu(i).finish.x - cu(i).start.x) * v)
    y = lv.start.y + cint(cu(i).start.y + (cu(i).finish.y - cu(i).start.y) * v - 4 * cu(i).hop * u * (1 - u))
end sub

' The intro's picture: the Downs, then each cue that is on, in the script's order, between two black bars.
sub IntroDraw (wd as World, g as Game, p as Hero, cu() as Cue)
    dim i as integer, cx as integer, cy as integer, now as single
    cx = int(g.cam.x)
    cy = int(g.cam.y)
    now = g.mode_t * BEATS
    DrawBack wd.zn, wd.art, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.se, cx, cy, -1
    DrawTiles wd.lv, wd.zn, wd.art, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.se, cx, cy, 0
    do while cu(i).kind
        if now >= cu(i).t0 and now < cu(i).t1 then
            DrawCue wd, cu(), i, now, p, cx, cy
        end if
        i = i + 1
    loop
    line (0, 0)-(SW - 1, BARH - 1), 0, BF
    line (0, SH - BARH)-(SW - 1, SH - 1), 0, BF
end sub

' Cue i's picture at beat `now`, the view at cx, cy: its foot's middle where the cue is.
private sub DrawCue (wd as World, cu() as Cue, i as integer, now as single, p as Hero, cx as integer, cy as integer)
    dim wx as integer, wy as integer, x as integer, y as integer
    dim a as integer
    CueAt wd.lv, cu(), i, now, wx, wy
    x = wx - cx
    y = wy - cy
    a = cu(i).own
    select case cu(i).kind
    case QHOUSE
        Blit wd.art, x - 66, y - 123, IHOUSE, 132, 124
    case QBOXES
        Blit wd.art, x - 22, y - 39, IBOXES, 44, 40
    case QCRATE
        Blit wd.art, x - 10, y - 15, ICRATE, 20, 16
    case QHEDGE
        Blit wd.art, x - 80, y - 51, IHEDGE, 160, 52
    case QFAM
        DrawFamily wd.art, wd.se, x, y, a
    case QVAN
        DrawVan wd.art, x, y, wx, a
    case QFLOAT
        DrawFloat wd.art, wd.se, x, y, wx, a
    case QBARON
        Blit wd.art, x - 20, y - 43, BaronPic%(wd.se, a), 40, 44
    case QCAT
        DrawCat wd.zn, wd.art, wd.se, x, y, a, cu(i).finish.x < cu(i).start.x
    case QBASKET
        DrawKeep wd.zn, wd.art, x, y
    case QEYES
        DrawEyes wd.se, x, y, a
    case QHERO
        Pose wd.lv, wd.gr, wd.se, p, wx, wy, a, cu(i).finish.x < cu(i).start.x
        DrawHero wd.gr, wd.art, wd.se, p, cx, cy
    case QWOOF
        p.at.x = wx
        p.at.y = wy
        p.face = a
        p.bark_t = BARKT - (now - cu(i).t0) / BEATS
        DrawBark p, cx, cy
    end select
end sub
