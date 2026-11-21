' The scene drawn, back to front.
defint A-Z
option explicit

from engine.video import Blit, SH, Store, SW
from barkley.model.images import IAM, IAP, IBIN, IBONE, ICHAIR
from barkley.model.images import ICHIMNEY, IDASH, IFORT, IGEYSER
from barkley.model.images import IGOAL, IGOLD, IHEAP, IHUD, IICICLE
from barkley.model.images import IJELLY, IJET, IMOVE, ISLED, ISPIN
from barkley.model.images import ISPRING, ISQUIRREL, ISWEEP, ITITLE
from barkley.model.images import ITOAD, IWALL
from barkley.model.hero import Hero
from barkley.model.level import KBIN, KBONE, KCAT, KCHIMNEY, KCRAB
from barkley.model.level import KFAM, KFRONT, KGOAL, KGOAT, KGOLD
from barkley.model.level import KHEAP, KICICLE, KJELLY, KJET, KLEG
from barkley.model.level import KLOAD, KLOOP, KMORTAR, KMOVE, KPIPE
from barkley.model.level import KPLAT, KPOUNCE, KPRESS, KSLED, KSNATCH
from barkley.model.level import KSPAN, KSPARK, KSPIN, KSPOUT, KSPRING
from barkley.model.level import KSQUIRREL, KSWEEP, KTHROW, KTIPPER
from barkley.model.level import KVINE, KWALKER, KWALL, KWAVE, Level
from barkley.model.level import NGOLD, NONE, Realm, TS
from barkley.model.game import CLEARED, Found, Game, Loose, NLOOSE
from barkley.model.game import NPART, OVER, Part, PLAYING, Session
from barkley.model.game import TITLE
from barkley.model.world import World
from barkley.model.kinds import FPRESS, FSTATE, FSWAY, FTICK, Kind
from barkley.model.kinds import Kinds
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
from barkley.stage.zones.pass import DrawAvalanche, DrawGale, DrawGoat
from barkley.stage.zones.pass import DrawLoad, DrawStray
from barkley.stage.zones.quarter import DrawLines, DrawMortar
from barkley.stage.zones.quarter import DrawPouncer, DrawSmoke
from barkley.stage.zones.quarter import DrawTipper, SnatchPic
from barkley.stage.hero.modes import FLUEH
from barkley.stage.zones.chase import DrawChase
from barkley.stage.hud import Banner, Hud, Num, Panel, Say, Txt
from barkley.stage.things.wood import SQAIM
from barkley.stage.things.pass import Fallen, Risen, TREMBLE

private const LD = 188
private const CABLEH = 44
private const SEATH = 30

' Back to front, then the screen's words.
sub Render ( _
    wd as World, _
    g as Game, _
    p as Hero, _
    lo() as Loose, _
    pt() as Part _
)
    dim cx as integer, cy as integer, x as integer, s as string

    cx = int(g.cam.x)
    cy = int(g.cam.y)
    if g.shake > 0 then cy = cy + ((wd.se.ticks and 2) - 1) * 2
    if g.mode <> TITLE then Hud wd.art, g, GoldFound%(wd)
    DrawBack wd.zn, wd.art, cx, cy
    if wd.lv.tide <> NONE then DrawSea wd.lv, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.kinds, wd.se, cx, cy, -1
    DrawTiles wd.lv, wd.zn, wd.art, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.kinds, wd.se, cx, cy, 0
    if wd.lv.n_ropes then DrawLines wd.lv, wd.art, cx, cy
    if wd.lv.stray.st = 2 then DrawStray wd.lv, wd.art, wd.se, cx, cy
    if wd.lv.n_movers then DrawCables wd.lv, cx, cy
    if wd.se.loose_n then DrawLoose wd.art, wd.se, cx, cy, lo()
    DrawChase wd.lv, wd.zn, wd.art, wd.se, cx, cy
    DrawHero wd.gr, wd.art, wd.se, p, cx, cy

    if wd.lv.aval_x <> NONE then
        DrawAvalanche wd.lv, wd.art, wd.se, cx, cy
    end if

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
            ' the zone a game starts at, an arrow each way
            ' there is another to pick
            Panel 0, 165, SW - 1, SH - 1
            s = ZoneName$(wd.se.pick)
            if wd.se.pick > 0 then s = "< " + s else s = "  " + s

            if wd.se.pick < wd.se.reached then
                s = s + " >"
            else
                s = s + "  "
            end if

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

            if (wd.zn.stage and 1) and x = 0 then
                Txt 14, 17, "ACT 2", CYELLOW
            end if
        end if
    case CLEARED
        Blit wd.art, 6, 5, IHUD, HUDW, HUDH
        ' up and to the right: under it Charlie meets its
        ' family
        Panel 116, 4, 315, 36
        Txt 2, 27 - (len(ZoneName$(wd.zn.id)) + 8) \ 2, _
            ZoneName$(wd.zn.id) + " CLEARED", CYELLOW
        Txt 3, 22, "BONUS " + Num$(g.bonus, 6), CWHITE
        Txt 4, 22, "SCORE " + Num$(g.score, 6), CWHITE
        if p.win then Say p, cx, cy, "WOOF!"
    case OVER
        Blit wd.art, 6, 5, IHUD, HUDW, HUDH
        Panel 108, 68, 211, 92
        Banner 10, "GAME OVER", CYELLOW
    end select
end sub

' The things in view: back, the scenery behind the ground;
' else the rest. The level's golden bones found, a bit each;
' -1 where it has none.
private function GoldFound% (wd as World)
    dim n as integer, v as integer

    GoldFound% = -1
    if wd.lv.n_gold = 0 then exit function

    for n = NGOLD - 1 to 0 step -1
        v = v * 2 - Found%(wd.se, wd.zn.stage, n)
    next

    GoldFound% = v
end function

sub DrawThings ( _
    lv as Level, _
    zn as Realm, _
    art as Store, _
    kd as Kinds, _
    se as Session, _
    cx as integer, _
    cy as integer, _
    back as integer _
)
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

        if kd.k(k).frames then
            DrawRow kd.k(k), art, se, i, lv.things(i).state, x, y
            continue for
        end if

        select case k
        case KBONE
            if lv.things(i).state then
                Blit art, x - 5, y - 5, _
                    IBONE + ((se.ticks \ 6) and 3) * 2, 10, 10
            end if
        case KGOLD
            ' a golden bone, till it is found
            if Found%(se, zn.stage, GoldNo%(lv, i)) then continue for

            Blit art, x - 5, y - 5, _
                IGOLD + ((se.ticks \ 6) and 3) * 2, 10, 10
        case KCAT, KWALKER
            if lv.things(i).state then
                Blit art, x - 10, y - 13, _
                    CatPic%(se, lv.things(i).state), 20, 14
            end if
        case KPOUNCE
            DrawPouncer lv, art, se, i, x, y
        case KMORTAR
            DrawMortar lv, art, se, i, x, y
        case KTIPPER
            DrawTipper lv, art, se, i, x, y
        case KSNATCH
            if lv.things(i).state then
                Blit art, x - 10, y - 13, _
                    SnatchPic%(se, lv.things(i).state), 20, 14
            end if
        case KBIN
            ' eyes under its lid while its cat, the thing
            ' after it, is in it
            Blit art, x - 9, y - 24, _
                IBIN - 2 * (lv.things(i + 1).state <> 0), 18, 25
        case KCRAB
            if lv.things(i).state then
                Blit art, x - 12, y - 15, _
                    CrabPic%(se, lv.things(i).state), 24, 15
            end if
        case KSPRING
            ' the sands' is a deck chair, the wood's a
            ' toadstool
            if zn.id = 2 then
                Blit art, x - 15, y - 21, _
                    ICHAIR - 2 * (lv.things(i).state > 0), 30, 22
            elseif zn.id = 4 then
                Blit art, x - 8, y - 15, _
                    ITOAD - 2 * (lv.things(i).state > 0), 16, 16
            else
                Blit art, x - 8, y - 15, _
                    ISPRING - 2 * (lv.things(i).state > 0), 16, 16
            end if
        case KCHIMNEY
            ' the way out smokes
            Blit art, x - 9, y - 31, ICHIMNEY, 18, 32
            if lv.things(i).state = 0 then DrawSmoke se, x, y - FLUEH
        case KVINE
            DrawVine x, y, lv.things(i).state / 100
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
                k = IAP + (((lv.things(i).at.x \ TS + 1) and 3) * 3 _
                    + (lv.things(i).at.y \ TS) mod 3) * 2
                Blit art, x - PIPER, y - PIPER, k, PIPER, PIPER + TS
            end if
        case KSLED
            if lv.things(i).state = 0 then
                Blit art, x - 15, y - 25, ISLED + 4, 30, 42
            end if
        case KTHROW
            if lv.things(i).state then
                Blit art, x - 14, y - 25, _
                    IFORT - 2 * Risen%(lv, i) - 4 _
                    * (lv.things(i).state < 0), 28, 26
            end if
        case KICICLE
            ' loosed, it trembles
            k = lv.things(i).state > 0 _
                and lv.things(i).state <= TREMBLE _
                and (se.ticks and 2) <> 0

            if lv.things(i).state >= 0 then
                Blit art, x - 3 - k, y + Fallen%(lv, i), IICICLE, 7, _
                    16
            end if
        case KSWEEP
            k = ISWEEP _
                + ((se.ticks \ 6) and 1) * 2 - 4 _
                * (lv.things(i).state > 0)

            if lv.things(i).state then
                Blit art, x - 15, y - 19, k, 30, 20
            end if
        case KSPARK
            DrawSpark lv, art, se, i, x, y, cx
        case KPRESS
            DrawPress lv, art, se, i, x, y
        case KLEG
            DrawLeg art, x, y
        case KSPOUT
            DrawSpout lv, art, se, x, y, lv.things(i).at.x \ TS, cy
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
            if lv.things(i).state then
                Blit art, x - 30, y - 33, IHEAP, 60, 34
            end if
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
        case KSQUIRREL
            ' an acorn held out, it is about to drop it
            k = ISQUIRREL - 2 * (abs(lv.things(i).state) <= SQAIM) _
                - 4 * (lv.things(i).state > 0)

            if lv.things(i).state then
                Blit art, x - 10, y - 17, k, 20, 18
            end if
        case KFAM
            ' who meets Charlie: they cheer once it has come
            ' to them
            DrawFamily art, se, x, y, se.cheer
        case KJET
            ' in the pass a geyser
            k = IJET
            if zn.id = 3 then k = IGEYSER
            Blit art, x - 8, y - 6, k, 16, 6
            f = k + 2 + ((se.ticks \ 3) and 1) * 2

            if (se.ticks + lv.things(i).at.x) mod 180 < 80 then
                Blit art, x - 6, y - 70, f, 12, 64
            end if
        case KJELLY
            Blit art, x - 8, y - 18 + ((se.ticks \ 20) and 1), _
                IJELLY - 2 * (lv.things(i).state > 1), 16, 18
        case KSPIN
            line (x - 1, y - SPINH)-(x, y - 1), CINK, BF
            a = se.ticks * .06 + lv.things(i).at.x

            for f = 1 to 3
                pset (x + cint(SPINR * f / 4 * cos(a)), _
                    y - SPINH + cint(SPINR * f / 4 * sin(a))), CWHITE
            next

            Blit art, x - 4, y - SPINH - 4, ISPIN + 2, 8, 8
            Blit art, x + cint(SPINR * cos(a)) - 8, _
                y - SPINH + cint(SPINR * sin(a)) - 8, ISPIN, 16, 16
        case KWAVE
            ' a twist's far half; its near one is drawn over
            ' Charlie
            Blit art, x, y - WAVEA - 22, zn.wave, WAVEL, WAVEH
        end select
    next
end sub

' The cables, their posts, and what hangs from them: a
' basket by its two lines, a chair by its hanger and grip.
private sub DrawCables (lv as Level, cx as integer, cy as integer)
    dim m as integer, x as integer, y as integer, x1 as integer
    dim bx as integer, by as integer, i as integer
    dim d as integer

    for m = 0 to lv.n_movers - 1
        if lv.movers(m).kind <> 3 _
            and lv.movers(m).kind <> 7 then continue for

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
                line (bx + i, by)-(bx + i, _
                    y + 1 + clng(bx + i - x) * d \ (x1 - x)), CINK
            next
        else
            bx = bx + MOVEW \ 2
            i = y + clng(bx - x) * d \ (x1 - x)
            line (bx - 1, i)-(bx, by - SEATH), CINK, BF
            line (bx - 3, i - 1)-(bx + 2, i + 2), CINK, BF
        end if
    next
end sub

private sub DrawLoose ( _
    art as Store, _
    se as Session, _
    cx as integer, _
    cy as integer, _
    lo() as Loose _
)
    dim i as integer, k as integer

    k = IBONE + ((se.ticks \ 6) and 3) * 2

    for i = 0 to NLOOSE - 1
        if lo(i).t <= 1 _
            and (lo(i).t <= 0 _
            or (se.ticks and 4) = 0) then continue for

        Blit art, cint(lo(i).at.x) - 5 - cx, _
            cint(lo(i).at.y) - 5 - cy, k, 10, 10
    next
end sub

sub DrawParts (pt() as Part, cx as integer, cy as integer)
    dim i as integer, x as integer, y as integer

    for i = 0 to NPART - 1
        if pt(i).t <= 0 then continue for

        x = cint(pt(i).at.x) - cx
        y = cint(pt(i).at.y) - cy
        if x < 0 or x >= SW - 1 or y < 0 _
            or y >= SH - 1 then continue for

        pset (x, y), pt(i).c
        if pt(i).t > .25 then pset (x + 1, y), pt(i).c
    next
end sub

' A kind drawn by its row, thing i at x, y in its state: the
' frame its row picks.
private sub DrawRow ( _
    kd as Kind, _
    art as Store, _
    se as Session, _
    i as integer, _
    state as integer, _
    x as integer, _
    y as integer _
)
    dim f as integer

    select case kd.by
    case FTICK
        f = (se.ticks \ kd.rate) and (kd.frames - 1)
    case FSWAY
        f = (se.ticks \ kd.rate + i) and (kd.frames - 1)
    case FSTATE
        f = state
    case FPRESS
        f = -(state > 0)
    end select

    Blit art, x + kd.dx, y + kd.dy, kd.pic + 2 * f, kd.w, kd.h
end sub
