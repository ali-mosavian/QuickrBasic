' The scene drawn, back to front.
defint A-Z
option explicit

from barkley.engine.video import Blit, SH, Store, SW
from barkley.model.images import IAM, IAP, IAWNING, IBIN, IBONE
from barkley.model.images import IBOOST, IBRAMBLE, IBTN, IBUMP, IBUOY
from barkley.model.images import IBUSH, ICHAIR, ICHIMNEY, ICISTERN
from barkley.model.images import IDASH, IDRIFT, IFLOWER, IFLUME, IFORT
from barkley.model.images import IGEYSER, IGOAL, IGOLD, IHEAP, IHOOP
from barkley.model.images import IHUD, IICICLE, IJELLY, IJET, ILAMP
from barkley.model.images import IMARK, IMOUND, IMOVE, IPOST, IROCK
from barkley.model.images import ISAPLING, ISEAT, ISLED, ISPIN
from barkley.model.images import ISPRING, ISQUIRREL, ISWEEP, ITANK
from barkley.model.images import ITHORN, ITITLE, ITOAD, ITRAM, ITREE
from barkley.model.images import IVENT, IWALL
from barkley.model.hero import Hero
from barkley.model.level import KAWNING, KBIN, KBONE, KBOOST, KBRAMBLE
from barkley.model.level import KBUMP, KBUOY, KBUSH, KBUTTON, KCAT
from barkley.model.level import KCHIMNEY, KCISTERN, KCRAB, KDRIFT
from barkley.model.level import KFAM, KFLOWER, KFLUME, KFRONT, KGOAL
from barkley.model.level import KGOAT, KGOLD, KHEAP, KHOOP, KICICLE
from barkley.model.level import KJELLY, KJET, KLAMP, KLEG, KLOAD
from barkley.model.level import KLOOP, KMARK, KMORTAR, KMOUND, KMOVE
from barkley.model.level import KPIPE, KPLAT, KPOST, KPOUNCE, KPRESS
from barkley.model.level import KROCK, KSAPLING, KSEAT, KSLED, KSNATCH
from barkley.model.level import KSPAN, KSPARK, KSPIN, KSPOUT, KSPRING
from barkley.model.level import KSQUIRREL, KSWEEP, KTANK, KTHORN
from barkley.model.level import KTHROW, KTIPPER, KTRAM, KTREE, KVENT
from barkley.model.level import KVINE, KWALKER, KWALL, KWAVE, Level
from barkley.model.level import NGOLD, NONE, Realm, TS
from barkley.model.game import CLEARED, Found, Game, Loose, NLOOSE
from barkley.model.game import NPART, OVER, Part, PLAYING, Session
from barkley.model.game import TITLE
from barkley.model.world import World
from barkley.assets.palette import CGOLD, CINK, CSCARF, CWHITE
from barkley.assets.palette import CYELLOW
from barkley.assets.store import HUDH, HUDW
from barkley.assets.cast import DrawFamily
from barkley.world.terrain.ground import LOOPR, WAVEL
from barkley.world.terrain.track import MOVEW, PIPER, WAVEA
from barkley.world.level import GoldNo, ZoneName
from barkley.world.movers import Dip
from barkley.world.backdrop import DrawBack, DrawTiles
from barkley.stage.fx.lobs import DrawLobs
from barkley.stage.hero.moves import BARKT
from barkley.stage.hero.draw import DrawBark, DrawHero, DrawOver
from barkley.stage.hero.draw import WAVEH
from barkley.stage.zones.downs import CatPic
from barkley.stage.zones.works import DrawLeg, DrawPress, DrawSpark
from barkley.stage.zones.works import SPINH, SPINR
from barkley.stage.zones.sands import CrabPic, DrawGulls, DrawSea
from barkley.stage.zones.sands import DrawSpout, DrawSurf
from barkley.stage.zones.wood import DrawBees, DrawFlies, DrawVine
from barkley.stage.zones.wood import SQAIM
from barkley.stage.zones.pass import DrawAvalanche, DrawGale, DrawGoat
from barkley.stage.zones.pass import DrawLoad, DrawStray, Fallen
from barkley.stage.zones.pass import Risen, TREMBLE
from barkley.stage.zones.quarter import DrawLines, DrawMortar
from barkley.stage.zones.quarter import DrawPouncer, DrawSmoke
from barkley.stage.zones.quarter import DrawTipper, SnatchPic
from barkley.stage.hero.modes import FLUEH
from barkley.stage.zones.chase import DrawChase
from barkley.stage.hud import Banner, Hud, Num, Panel, Say, Txt

private const LD = 188
private const CABLEH = 44
private const SEATH = 30

' Back to front, then the screen's words.
sub Render (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim cx as integer, cy as integer, x as integer, s as string
    cx = int(g.cam.x)
    cy = int(g.cam.y)
    if g.shake > 0 then cy = cy + ((wd.se.ticks and 2) - 1) * 2
    if g.mode <> TITLE then Hud wd.art, g, GoldFound%(wd)
    DrawBack wd.zn, wd.art, cx, cy
    if wd.lv.tide <> NONE then DrawSea wd.lv, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.se, cx, cy, -1
    DrawTiles wd.lv, wd.zn, wd.art, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.se, cx, cy, 0
    if wd.lv.n_ropes then DrawLines wd.lv, wd.art, cx, cy
    if wd.lv.stray.st = 2 then DrawStray wd.lv, wd.art, wd.se, cx, cy
    if wd.lv.n_movers then DrawCables wd.lv, cx, cy
    if wd.se.loose_n then DrawLoose wd.art, wd.se, cx, cy, lo()
    DrawChase wd.lv, wd.zn, wd.art, wd.se, cx, cy
    DrawHero wd.gr, wd.art, wd.se, p, cx, cy
    if wd.lv.aval_x <> NONE then DrawAvalanche wd.lv, wd.art, wd.se, cx, cy
    if wd.lv.gale then DrawGale wd.lv, wd.se
    if wd.lv.murk then DrawFlies wd.lv, wd.se, cx
    if wd.lv.n_gulls then DrawGulls wd.lv, wd.art, wd.se, cx, cy
    if wd.lv.n_swarms then DrawBees wd.lv, wd.se, cx, cy
    if wd.lv.n_lobs then DrawLobs wd.lv, wd.art, cx, cy
    if wd.lv.tide <> NONE then DrawSurf wd.lv, wd.art, cx, cy
    DrawOver wd.lv, wd.zn, wd.art, cx, cy
    if p.bark_t > BARKT - .2 then DrawBark p, cx, cy
    if wd.se.part_n then DrawParts pt(), cx, cy
    select case g.mode
    case TITLE
        Blit wd.art, (SW - 170) \ 2, 22, ITITLE, 170, 26
        Blit wd.art, (SW - 170) \ 2, 52, IDASH, 170, 26
        if wd.se.reached then
            ' the zone a game starts at, an arrow each way there is another to pick
            Panel 0, 165, SW - 1, SH - 1
            s = ZoneName$(wd.se.pick)
            if wd.se.pick > 0 then s = "< " + s else s = "  " + s
            if wd.se.pick < wd.se.reached then s = s + " >" else s = s + "  "
            Banner 22, s, CWHITE
        else
            Panel 0, 181, SW - 1, SH - 1
        end if
        if (wd.se.ticks \ 30) and 1 then
            Banner 24, "PRESS SPACE", CYELLOW
        else
            Banner 24, "SPACE JUMPS  DOWN ROLLS  X BARKS", CWHITE
        end if
    case PLAYING
        Blit wd.art, 6, 5, IHUD, HUDW, HUDH
        if p.stun then
            Say p, cx, cy, "HEY!"
        elseif p.idle_t > 5 then
            Say p, cx, cy, "C'MON!"
        elseif p.idle_t > 3 then
            Say p, cx, cy, "..."
        end if
        if g.mode_t < 2.6 then
            x = cint(g.mode_t * 900) - SW
            if x > 0 then x = 0
            if g.mode_t > 2.2 then x = cint((g.mode_t - 2.2) * 900)
            line (x, 84)-(x + SW - 1, 110), CSCARF, BF
            line (x, 84)-(x + SW - 1, 85), CGOLD + 3, BF
            line (x, 109)-(x + SW - 1, 110), CGOLD + 3, BF
            Blit wd.art, x + (SW - 178) \ 2, 89, wd.zn.card, 178, 18
            if (wd.zn.stage and 1) and x = 0 then Txt 14, 17, "ACT 2", CYELLOW
        end if
    case CLEARED
        Blit wd.art, 6, 5, IHUD, HUDW, HUDH
        ' up and to the right: under it Charlie meets its family
        Panel 116, 4, 315, 36
        Txt 2, 27 - (len(ZoneName$(wd.zn.id)) + 8) \ 2, ZoneName$(wd.zn.id) + " CLEARED", CYELLOW
        Txt 3, 22, "BONUS " + Num$(g.bonus, 6), CWHITE
        Txt 4, 22, "SCORE " + Num$(g.score, 6), CWHITE
        if p.win then Say p, cx, cy, "WOOF!"
    case OVER
        Blit wd.art, 6, 5, IHUD, HUDW, HUDH
        Panel 108, 68, 211, 92
        Banner 10, "GAME OVER", CYELLOW
    end select
end sub

' The things in view: back, the scenery behind the ground; else the rest.
' The level's golden bones found, a bit each; -1 where it has none.
private function GoldFound% (wd as World)
    dim n as integer, v as integer
    GoldFound% = -1
    if wd.lv.n_gold = 0 then exit function
    for n = NGOLD - 1 to 0 step -1
        v = v * 2 - Found%(wd.se, wd.zn.stage, n)
    next
    GoldFound% = v
end function

sub DrawThings (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer, back as integer)
    dim i as integer, c as integer, x as integer, y as integer
    dim k as integer, f as integer, a as single
    c = (cx - WAVEL) \ TS
    if c < 0 then c = 0
    for i = lv.first(c) to lv.n_things - 1
        x = lv.things(i).at.x - cx
        if x > SW + 96 then exit for
        k = lv.things(i).kind
        if (k >= KLOOP and k < KFRONT) <> back then continue for
        y = lv.things(i).at.y - cy
        select case k
        case KBONE
            if lv.things(i).state then Blit art, x - 5, y - 5, IBONE + ((se.ticks \ 6) and 3) * 2, 10, 10
        case KGOLD
            ' a golden bone, till it is found
            if Found%(se, zn.stage, GoldNo%(lv, i)) then continue for
            Blit art, x - 5, y - 5, IGOLD + ((se.ticks \ 6) and 3) * 2, 10, 10
        case KCAT, KWALKER
            if lv.things(i).state then Blit art, x - 10, y - 13, CatPic%(se, lv.things(i).state), 20, 14
        case KPOUNCE
            DrawPouncer lv, art, se, i, x, y
        case KMORTAR
            DrawMortar lv, art, se, i, x, y
        case KTIPPER
            DrawTipper lv, art, se, i, x, y
        case KSNATCH
            if lv.things(i).state then Blit art, x - 10, y - 13, SnatchPic%(se, lv.things(i).state), 20, 14
        case KBIN
            ' eyes under its lid while its cat, the thing after it, is in it
            Blit art, x - 9, y - 24, IBIN - 2 * (lv.things(i + 1).state <> 0), 18, 25
        case KCRAB
            if lv.things(i).state then Blit art, x - 12, y - 15, CrabPic%(se, lv.things(i).state), 24, 15
        case KSPRING
            ' the sands' is a deck chair, the wood's a toadstool
            if zn.id = 2 then
                Blit art, x - 15, y - 21, ICHAIR - 2 * (lv.things(i).state > 0), 30, 22
            elseif zn.id = 4 then
                Blit art, x - 8, y - 15, ITOAD - 2 * (lv.things(i).state > 0), 16, 16
            else
                Blit art, x - 8, y - 15, ISPRING - 2 * (lv.things(i).state > 0), 16, 16
            end if
        case KSAPLING
            Blit art, x - 6, y - 49, ISAPLING - 2 * (lv.things(i).state > 0), 60, 50
        case KAWNING
            Blit art, x - 23, y - 3, IAWNING - 2 * (lv.things(i).state > 0), 46, 22
        case KCHIMNEY
            ' the way out smokes
            Blit art, x - 9, y - 31, ICHIMNEY, 18, 32
            if lv.things(i).state = 0 then DrawSmoke se, x, y - FLUEH
        case KVINE
            DrawVine x, y, lv.things(i).state / 100
        case KBOOST
            Blit art, x - 12, y - 9, IBOOST + ((se.ticks \ 4) and 1) * 2, 24, 10
        case KMARK
            Blit art, x - 6, y - 31, IMARK + 2 * lv.things(i).state, 12, 32
        case KGOAL
            f = 0
            if lv.things(i).state then f = (se.ticks \ 4) and 3
            Blit art, x - 12, y - 47, IGOAL + f * 2, 24, 48
        case KPLAT
            Blit art, x, y, zn.tile + 18, TS, TS
        case KLOOP
            Blit art, x - LD \ 2, y - LOOPR - LD \ 2, zn.ring, LD, LD
        case KPIPE
            if zn.decked then
                Blit art, x - PIPER, y - PIPER, zn.pipe, PIPER, PIPER
            else
                k = IAP + (((lv.things(i).at.x \ TS + 1) and 3) * 3 + (lv.things(i).at.y \ TS) mod 3) * 2
                Blit art, x - PIPER, y - PIPER, k, PIPER, PIPER + TS
            end if
        case KSLED
            if lv.things(i).state = 0 then Blit art, x - 15, y - 25, ISLED + 4, 30, 42
        case KTHROW
            if lv.things(i).state then Blit art, x - 14, y - 25, IFORT - 2 * Risen%(lv, i) - 4 * (lv.things(i).state < 0), 28, 26
        case KDRIFT
            Blit art, x - 28, y - 11, IDRIFT, 56, 12
        case KICICLE
            ' loosed, it trembles
            k = lv.things(i).state > 0 and lv.things(i).state <= TREMBLE and (se.ticks and 2) <> 0
            if lv.things(i).state >= 0 then Blit art, x - 3 - k, y + Fallen%(lv, i), IICICLE, 7, 16
        case KSWEEP
            k = ISWEEP + ((se.ticks \ 6) and 1) * 2 - 4 * (lv.things(i).state > 0)
            if lv.things(i).state then Blit art, x - 15, y - 19, k, 30, 20
        case KSPARK
            DrawSpark lv, art, se, i, x, y, cx
        case KPRESS
            DrawPress lv, art, se, i, x, y
        case KLEG
            DrawLeg art, x, y
        case KFLUME
            Blit art, x, y - 8, IFLUME + ((se.ticks \ 3) and 3) * 2, TS, 29
        case KSPOUT
            DrawSpout lv, art, se, x, y, lv.things(i).at.x \ TS, cy
        case KCISTERN
            Blit art, x - 30, y - 25, ICISTERN, 30, 30
        case KSEAT
            Blit art, x, y - SEATH, ISEAT, MOVEW, SEATH + 8
        case KWALL
            if lv.things(i).state then
                Blit art, x - 9, y - 81, IWALL, 18, 82
            else
                Blit art, x - 9, y - 21, IWALL + 2, 18, 22
            end if
        case KGOAT
            DrawGoat lv, art, se, i, x, y
        case KLOAD
            DrawLoad lv, art, se, i, x, y
        case KHEAP
            if lv.things(i).state then Blit art, x - 30, y - 33, IHEAP, 60, 34
        case KSPAN
            ' three planks; tottering, it shakes
            if lv.things(i).state then y = y + ((se.ticks \ 2) and 1)
            for f = 0 to 2
                Blit art, x + f * TS, y, zn.tile + 18, TS, TS
            next
        case KMOVE
            if zn.decked then
                Blit art, x, y, IMOVE, MOVEW, 12
            else
                Blit art, x, y, IAM, MOVEW, 12
            end if
        case KTRAM
            Blit art, x - 9, y - 1, ITRAM, 66, 33
        case KPOST
            Blit art, x - 7, y - 47, IPOST, 16, 48
        case KTANK
            Blit art, x - 16, y - 39, ITANK, 32, 40
        case KVENT
            Blit art, x - 8, y - 27, IVENT, 16, 28
        case KTREE
            Blit art, x - 32, y - 78, ITREE, 64, 80
        case KBUSH
            Blit art, x - 20, y - 18, IBUSH, 40, 20
        case KFLOWER
            Blit art, x - 6, y - 14, IFLOWER + ((se.ticks \ 30 + i) and 1) * 2, 12, 16
        case KROCK
            Blit art, x - 10, y - 10, IROCK, 20, 12
        case KTHORN
            Blit art, x - 8, y - 10, ITHORN, 16, 12
        case KBRAMBLE
            Blit art, x - 19, y - 25, IBRAMBLE - 2 * (lv.things(i).state > 0), 38, 26
        case KMOUND
            Blit art, x - 17, y - 12, IMOUND, 34, 14
        case KSQUIRREL
            ' an acorn held out, it is about to drop it
            k = ISQUIRREL - 2 * (abs(lv.things(i).state) <= SQAIM) - 4 * (lv.things(i).state > 0)
            if lv.things(i).state then Blit art, x - 10, y - 17, k, 20, 18
        case KFAM
            ' who meets Charlie: they cheer once it has come to them
            DrawFamily art, se, x, y, se.cheer
        case KLAMP
            Blit art, x - 10, y - 42, ILAMP, 20, 44
        case KJET
            ' in the pass a geyser
            k = IJET
            if zn.id = 3 then k = IGEYSER
            Blit art, x - 8, y - 6, k, 16, 6
            f = k + 2 + ((se.ticks \ 3) and 1) * 2
            if (se.ticks + lv.things(i).at.x) mod 180 < 80 then Blit art, x - 6, y - 70, f, 12, 64
        case KBUMP
            Blit art, x - 12, y - 12, IBUMP - 2 * (lv.things(i).state > 0), 24, 24
        case KBUOY
            Blit art, x - 9, y - 9, IBUOY - 2 * (lv.things(i).state > 0), 18, 18
        case KJELLY
            Blit art, x - 8, y - 18 + ((se.ticks \ 20) and 1), IJELLY - 2 * (lv.things(i).state > 1), 16, 18
        case KSPIN
            line (x - 1, y - SPINH)-(x, y - 1), CINK, BF
            a = se.ticks * .06 + lv.things(i).at.x
            for f = 1 to 3
                pset (x + cint(SPINR * f / 4 * cos(a)), y - SPINH + cint(SPINR * f / 4 * sin(a))), CWHITE
            next
            Blit art, x - 4, y - SPINH - 4, ISPIN + 2, 8, 8
            Blit art, x + cint(SPINR * cos(a)) - 8, y - SPINH + cint(SPINR * sin(a)) - 8, ISPIN, 16, 16
        case KBUTTON
            Blit art, x - 8, y - 8, IBTN + 2 * lv.things(i).state, 16, 8
        case KWAVE
            ' a twist's far half; its near one is drawn over Charlie
            Blit art, x, y - WAVEA - 22, zn.wave, WAVEL, WAVEH
        case KHOOP
            Blit art, x - 6, y - 16, IHOOP - 2 * (lv.things(i).state > 0), 12, 32
        end select
    next
end sub

' The cables, their posts, and what hangs from them: a basket by its two lines, a chair by its hanger and grip.
private sub DrawCables (lv as Level, cx as integer, cy as integer)
    dim m as integer, x as integer, y as integer, x1 as integer
    dim bx as integer, by as integer, i as integer
    dim d as integer
    for m = 0 to lv.n_movers - 1
        if lv.movers(m).kind <> 3 and lv.movers(m).kind <> 7 then continue for
        x = lv.movers(m).edge - 8 - cx
        y = lv.movers(m).at.y - CABLEH - cy
        x1 = x + lv.movers(m).reach + MOVEW
        d = Dip%(lv, m)
        if x >= SW or x1 < 0 then continue for
        line (x, y)-(x1, y + d), CINK
        line (x, y + 1)-(x1, y + d + 1), CYELLOW
        line (x - 1, y - 2)-(x, y + CABLEH), CINK, BF
        line (x1, y + d - 2)-(x1 + 1, y + d + CABLEH), CINK, BF
        bx = lv.things(lv.movers(m).thing).at.x - cx
        by = lv.things(lv.movers(m).thing).at.y - cy
        if lv.movers(m).kind = 3 then
            for i = 4 to MOVEW - 4 step MOVEW - 8
                line (bx + i, by)-(bx + i, y + 1 + clng(bx + i - x) * d \ (x1 - x)), CINK
            next
        else
            bx = bx + MOVEW \ 2
            i = y + clng(bx - x) * d \ (x1 - x)
            line (bx - 1, i)-(bx, by - SEATH), CINK, BF
            line (bx - 3, i - 1)-(bx + 2, i + 2), CINK, BF
        end if
    next
end sub

private sub DrawLoose (art as Store, se as Session, cx as integer, cy as integer, lo() as Loose)
    dim i as integer, k as integer
    k = IBONE + ((se.ticks \ 6) and 3) * 2
    for i = 0 to NLOOSE - 1
        if lo(i).t <= 1 and (lo(i).t <= 0 or (se.ticks and 4) = 0) then continue for
        Blit art, cint(lo(i).at.x) - 5 - cx, cint(lo(i).at.y) - 5 - cy, k, 10, 10
    next
end sub

sub DrawParts (pt() as Part, cx as integer, cy as integer)
    dim i as integer, x as integer, y as integer
    for i = 0 to NPART - 1
        if pt(i).t <= 0 then continue for
        x = cint(pt(i).at.x) - cx
        y = cint(pt(i).at.y) - cy
        if x < 0 or x >= SW - 1 or y < 0 or y >= SH - 1 then continue for
        pset (x, y), pt(i).c
        if pt(i).t > .25 then pset (x + 1, y), pt(i).c
    next
end sub
