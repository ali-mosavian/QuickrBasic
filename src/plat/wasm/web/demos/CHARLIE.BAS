' CHARLIE BARKLEY: Charlie the Maltese tears through the Amber Downs.
' Arrows run. Space, Z or Up jumps, curled. Down while running rolls;
' Down while still crouches: jump to wind up, let Down go to launch.
' M mutes, N turns the tune off; Esc leaves. SCREEN 13 on two pages: one drawn, one shown.
' Held keys come from the keyboard's port (60h); where it reads FFh,
' from INKEY$'s repeats. Physics in pixels and seconds, 60 steps a
' second; the pictures are DATA, drawn by tools/charlieart.py.
DEFINT A-Z

' Charlie. x, y: the feet. gsp: speed along the ground, ang its rise in
' radians, uphill to the right > 0, right round on a loop. sg: the stretch
' of the ground's line it is on, or left last; slip: the seconds the keys
' are no help, slid off a slope too steep for its speed.
TYPE Hero
    x AS SINGLE
    y AS SINGLE
    vx AS SINGLE
    vy AS SINGLE
    gsp AS SINGLE
    ang AS SINGLE
    slip AS SINGLE
    lastx AS SINGLE
    charge AS SINGLE
    want AS SINGLE
    coyote AS SINGLE
    inv AS SINGLE
    anim AS SINGLE
    deadT AS SINGLE
    idleT AS SINGLE
    ground AS INTEGER
    roll AS INTEGER
    jumped AS INTEGER
    crouch AS INTEGER
    charging AS INTEGER
    stun AS INTEGER
    face AS INTEGER
    skid AS INTEGER
    sprung AS INTEGER
    dead AS INTEGER
    sg AS INTEGER
    win AS INTEGER
END TYPE

' The game's time against the screen's frames: lag, the time not yet stepped; span, a frame's length as learnt; late,
' what the game is behind the clock; n, the frames seen.
TYPE Clock
    lag AS SINGLE
    span AS SINGLE
    late AS SINGLE
    n AS SINGLE
END TYPE

' Held keys, and jump's press since the last step.
TYPE Pad
    l AS INTEGER
    r AS INTEGER
    d AS INTEGER
    jump AS INTEGER
    hit AS INTEGER
    was AS INTEGER
    quit AS INTEGER
    start AS INTEGER
    mute AS INTEGER
END TYPE

TYPE Game
    mode AS INTEGER
    bones AS INTEGER
    lives AS INTEGER
    score AS LONG
    bonus AS LONG
    sig AS LONG
    clock AS SINGLE
    camx AS SINGLE
    camy AS SINGLE
    modeT AS SINGLE
    shake AS SINGLE
    homeX AS INTEGER
    homeY AS INTEGER
    page AS INTEGER
    quitting AS INTEGER
END TYPE

' A bone knocked loose: t its time left.
TYPE Loose
    x AS SINGLE
    y AS SINGLE
    vx AS SINGLE
    vy AS SINGLE
    t AS SINGLE
END TYPE

TYPE Part
    x AS SINGLE
    y AS SINGLE
    vx AS SINGLE
    vy AS SINGLE
    t AS SINGLE
    c AS INTEGER
END TYPE

' The world: MW by MH tiles of TS pixels; LW columns used. A tile's kind
' is its profile, Prof: each column's solid height from its bottom.
CONST SW = 320, SH = 200, TS = 16, MW = 800, MH = 30, NONE = 30000
CONST STEPT = 1! / 60!, PI = 3.141593
CONST GRAV = 750!, JUMPV = 330!, JUMPCUT = 160!, TOP = 360!
CONST RUNACC = 280!, DEC = 1800!, FRIC = 280!, AIRACC = 500!, SLOPE = 380!
CONST RFRIC = 60!, RDEC = 450!, RUPS = 300!, RDNS = 1100!, MAXV = 900!
CONST SPRINGV = 560!, CHARGE0 = 480!, CHARGEMAX = 360!, BOUNCEV = 330!
CONST BOOSTV = 660!, LOOPR = 72, LD = 172, PIPER = 48
' A jump asked for this long before landing is made on it; one this long after running off an edge still is.
CONST JBUFFER = .1, COYOTE = .1
' The ground's line: its most points, a loop's and a quarter pipe's stretches, the curves there can be; under STALL
' Charlie cannot stay on a wall or a roof.
CONST NPTS = 1600, LOOPN = 48, PIPEN = 12, NZONE = 47, STALL = 150!
' The moving platforms there can be, and their width.
CONST NMOVE = 23, MOVEW = 48
CONST HW = 7, HH = 24, STRIPH = 192, MIDH = 80
CONST MAXOBJ = 600, NLOOSE = 24, NPART = 48
CONST TITLE = 0, PLAYING = 1, CLEARED = 2, OVER = 3
CONST FIDLE = 0, FBLINK = 1, FTAP = 2, FRUN = 4, FDASH = 10, FBALL = 12
CONST FSKID = 16, FCROUCH = 17, FHURT = 18, FUP = 19, FWIN = 20, FSHIVER = 22, NF = 23

' What stands in the level, by x. From KLOOP on: scenery, drawn behind
' the ground.
CONST KBONE = 1, KCAT = 2, KSPRING = 3, KMARK = 4, KBOOST = 5, KGOAL = 6, KPLAT = 7
CONST KLOOP = 8, KTREE = 9, KBUSH = 10, KFLOWER = 11, KROCK = 12, KTHORN = 13, KLAMP = 14, KFAM = 15, KPIPE = 16
CONST KMOVE = 17, KPOST = 18, KTANK = 19, KVENT = 20

' Images in Img from At(k); a masked one's mask at k + 1. As charlieart.py's.
CONST IBG = 0, IMID = 1, ISTRIP = 3, IDEEP = 4, ILOOP = 5, ITITLE = 7
CONST ITILE = 10, IEARTH = 30, IHERO = 34, IBONE = IHERO + NF * 4
CONST ICAT = IBONE + 8, ISPRING = ICAT + 8, IBOOST = ISPRING + 4, IMARK = IBOOST + 4
CONST IGOAL = IMARK + 4, ITREE = IGOAL + 8, IBUSH = ITREE + 2, IFLOWER = IBUSH + 2
CONST IROCK = IFLOWER + 4, ITHORN = IROCK + 2, ILAMP = ITHORN + 2, ITILT = ILAMP + 2
CONST IFAM = ITILT + 320, IDASH = IFAM + 4
CONST ICARD = IDASH + 2, IGLYPH = ICARD + 2, NGLYPH = 27, IHUD = IGLYPH + NGLYPH * 4, IPIPE = IHUD + 2
' The Chrome Works' own: built here, then charlieart.py's.
CONST IZ2 = IPIPE + 2, ITILE2 = IZ2 + 9, IMOVE = ITILE2 + 24, IPOST = IMOVE + 2, ITANK = IPOST + 2, IVENT = ITANK + 2
CONST ICARD2 = IVENT + 2, IBLD = ICARD2 + 2, IPLANT = IBLD + 12
CONST IDK = IPLANT + 8, IWT = IDK + 4, IWS = IWT + 32, NIMG = IWS + 4
CONST HUDW = 108, HUDH = 40
CONST IMGSZ = 340000

CONST SCLEFT = 75, SCRIGHT = 77, SCUP = 72, SCDOWN = 80, SCSPACE = 57, SCZ = 44
CONST SJUMP = 0, SBONE = 1, SHURT = 2, SSPRING = 3, SPOP = 4, SROLL = 5
CONST SCHARGE = 6, SLAUNCH = 7, SCLEAR = 8, SDIE = 9, SMARK = 10, SBOOST = 11, SHISS = 12

' The palette's ramps, dark to light, as charlieart.py's.
CONST CHUD = 0, CWHITE = 1, CINK = 2, CYELLOW = 3, CRED = 4
CONST CSCARF = 21, CGRASS = 25, CEARTH = 30, CSTONE = 38, CGOLD = 42
CONST CSKY = 96, NSKY = 32, CFAR = 128, CMID = 132, CCLOUD = 138, CSUN = 141, CWATER = 144
CONST CSTEEL = 150, CDAY = 156, CFART = 185, CCHROME = 198, CPANEL = 204

DECLARE SUB Main ()
DECLARE SUB Tint (k AS INTEGER, c AS LONG)
DECLARE SUB Shade ()
DECLARE FUNCTION Met% (g AS Game, p AS Hero, k AS Pad, pt() AS Part)
DECLARE SUB OpenScreen ()
DECLARE SUB Flip (page AS INTEGER)
DECLARE SUB LoadTables ()
DECLARE SUB LoadLevel (z AS INTEGER)
DECLARE SUB AddMover (x AS INTEGER, y AS INTEGER, a AS INTEGER, k AS INTEGER, e AS INTEGER)
DECLARE SUB Movers (p AS Hero)
DECLARE FUNCTION Swing! (m AS INTEGER, quarter AS INTEGER)
DECLARE SUB NextZone (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
DECLARE SUB BuildDay ()
DECLARE SUB BuildWorks ()
DECLARE SUB Slab (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, c AS INTEGER)
DECLARE SUB Stamp (x AS INTEGER, y AS INTEGER, k AS INTEGER, band AS INTEGER)
DECLARE SUB Column (c AS INTEGER, g AS INTEGER, kind AS INTEGER)
DECLARE SUB AddObj (k AS INTEGER, x AS INTEGER, y AS INTEGER)
DECLARE SUB SortObjs ()
DECLARE SUB BuildArt ()
DECLARE SUB LoadArt ()
DECLARE SUB Unpack ()
DECLARE FUNCTION Word& ()
DECLARE FUNCTION Chars$ ()
DECLARE SUB OpenData ()
DECLARE SUB Mirror (k AS INTEGER, k2 AS INTEGER, w AS INTEGER, h AS INTEGER)
DECLARE SUB Flop (k AS INTEGER, k2 AS INTEGER, w AS INTEGER, h AS INTEGER)
DECLARE SUB BuildStrips (e AS INTEGER, k AS INTEGER)
DECLARE SUB BuildLoop (k AS INTEGER, ce AS INTEGER, cr AS INTEGER)
DECLARE SUB BuildPipe (k AS INTEGER, ce AS INTEGER, cr AS INTEGER)
DECLARE SUB Track ()
DECLARE SUB Knot (x AS SINGLE, y AS SINGLE, k AS INTEGER)
DECLARE FUNCTION SegAt% (x AS INTEGER, y AS SINGLE)
DECLARE SUB Along (p AS Hero, d AS SINGLE)
DECLARE SUB Drop (p AS Hero)
DECLARE SUB Settle (p AS Hero, s AS INTEGER, d AS INTEGER)
DECLARE FUNCTION Curve% (p AS Hero, x0 AS SINGLE, y0 AS SINGLE, d AS INTEGER)
DECLARE SUB BuildMid ()
DECLARE SUB BuildBack ()
DECLARE SUB Cloud (x AS INTEGER, y AS INTEGER)
DECLARE SUB FillEll (cx AS INTEGER, cy AS INTEGER, rx AS INTEGER, ry AS INTEGER, c AS INTEGER)
DECLARE SUB Grab (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, k AS INTEGER)
DECLARE SUB MaskGrab (x AS INTEGER, y AS INTEGER, w AS INTEGER, h AS INTEGER, mx AS INTEGER, my AS INTEGER, k AS INTEGER)
DECLARE SUB Blit (x AS INTEGER, y AS INTEGER, k AS INTEGER, w AS INTEGER, h AS INTEGER)
DECLARE SUB Poll ()
DECLARE SUB ReadPad (k AS Pad)
DECLARE FUNCTION Held2% (s AS INTEGER, t AS SINGLE)
DECLARE SUB Pace (d AS SINGLE, c AS Clock)
DECLARE FUNCTION Steps% (c AS Clock)
DECLARE FUNCTION Elapsed! (a AS SINGLE, b AS SINGLE)
DECLARE SUB Sfx (id AS INTEGER)
DECLARE SUB LoadSong ()
DECLARE SUB FeedSong ()
DECLARE SUB PickSong (n AS INTEGER, late AS INTEGER)
DECLARE FUNCTION Beat$ (x AS STRING, loud AS INTEGER)
DECLARE FUNCTION Riff$ (p AS STRING, h AS INTEGER, low AS INTEGER)
DECLARE FUNCTION Surf% (sx AS INTEGER, y AS INTEGER)
DECLARE FUNCTION Solid% (x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION Wall% (p AS Hero, dir AS INTEGER)
DECLARE FUNCTION Land% (p AS Hero, d AS INTEGER)
DECLARE SUB Tick (g AS Game, p AS Hero, k AS Pad, lo() AS Loose, pt() AS Part)
DECLARE SUB Pilot (p AS Hero, k AS Pad)
DECLARE SUB ResetWorld (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
DECLARE SUB StartGame (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
DECLARE SUB Spawn (g AS Game, p AS Hero)
DECLARE SUB MoveHero (p AS Hero, k AS Pad, pt() AS Part)
DECLARE SUB Walk (p AS Hero, l AS INTEGER, r AS INTEGER, d AS INTEGER, hit AS INTEGER, pt() AS Part)
DECLARE SUB Airborne (p AS Hero, l AS INTEGER, r AS INTEGER, d AS INTEGER, hold AS INTEGER, pt() AS Part)
DECLARE SUB Slow (v AS SINGLE, f AS SINGLE)
DECLARE SUB Touch (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
DECLARE SUB Hurt (g AS Game, p AS Hero, fromX AS INTEGER, lo() AS Loose, snd AS INTEGER)
DECLARE SUB Lose (p AS Hero)
DECLARE SUB Dying (g AS Game, p AS Hero)
DECLARE SUB Things (g AS Game, lo() AS Loose, pt() AS Part)
DECLARE SUB Emit (pt() AS Part, x AS SINGLE, y AS SINGLE, vx AS SINGLE, vy AS SINGLE, t AS SINGLE, c AS INTEGER)
DECLARE SUB Burst (pt() AS Part, x AS SINGLE, y AS SINGLE, n AS INTEGER, v AS SINGLE, c AS INTEGER)
DECLARE SUB Camera (g AS Game, p AS Hero, snap AS INTEGER)
DECLARE SUB Render (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
DECLARE SUB DrawBack (cx AS INTEGER, cy AS INTEGER)
DECLARE SUB DrawTiles (cx AS INTEGER, cy AS INTEGER)
DECLARE SUB DrawThings (cx AS INTEGER, cy AS INTEGER, back AS INTEGER)
DECLARE SUB DrawLoose (cx AS INTEGER, cy AS INTEGER, lo() AS Loose)
DECLARE SUB DrawHero (p AS Hero, cx AS INTEGER, cy AS INTEGER)
DECLARE FUNCTION HeroFrame% (p AS Hero)
DECLARE SUB DrawParts (pt() AS Part, cx AS INTEGER, cy AS INTEGER)
DECLARE SUB Hud (g AS Game)
DECLARE SUB HudLine (row AS INTEGER, s AS STRING, n AS INTEGER)
DECLARE SUB BuildGlyphs ()
DECLARE SUB Scatter (c AS INTEGER, s AS INTEGER)
DECLARE SUB Say (p AS Hero, cx AS INTEGER, cy AS INTEGER, s AS STRING)
DECLARE SUB Panel (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER)
DECLARE SUB Txt (row AS INTEGER, col AS INTEGER, s AS STRING, c AS INTEGER)
DECLARE SUB Banner (row AS INTEGER, s AS STRING, c AS INTEGER)
DECLARE FUNCTION Num$ (v AS LONG, w AS INTEGER)

DIM SHARED Map(MW - 1, MH - 1) AS INTEGER, Prof(7, TS - 1) AS INTEGER
DIM SHARED ColTop(MW) AS INTEGER, ColVis(MW) AS INTEGER
' The ground's line, what Charlie runs on: points GX, GY, GN of them; from each to the next a stretch of kind GK (0
' none, 1 the tiles' top, 2 a curve), GL long, GU, GV a pixel along it, its rise GA. ColSeg: a column's stretch;
' PlatA: the platforms' first. A curve z: stretches ZA to ZB, from ZM on a loop's far half, between x ZL and ZR.
DIM SHARED GX(NPTS) AS SINGLE, GY(NPTS) AS SINGLE, GU(NPTS) AS SINGLE, GV(NPTS) AS SINGLE, GL(NPTS) AS SINGLE
DIM SHARED GA(NPTS) AS SINGLE, GK(NPTS) AS INTEGER, GN AS INTEGER, ColSeg(MW) AS INTEGER, PlatA AS INTEGER
DIM SHARED ZA(NZONE) AS INTEGER, ZB(NZONE) AS INTEGER, ZM(NZONE) AS INTEGER, ZL(NZONE) AS INTEGER
DIM SHARED ZR(NZONE) AS INTEGER, NZ AS INTEGER
' The zone played, 0 the Amber Downs, 1 the Chrome Works, and the one ResetWorld is to load; its pictures: tiles,
' strips, backdrop, loop, pipe, card.
DIM SHARED Zone AS INTEGER, NextUp AS INTEGER, ZTile AS INTEGER, ZStrip AS INTEGER, ZBack AS INTEGER
DIM SHARED ZLoop AS INTEGER, ZPipe AS INTEGER, ZCard AS INTEGER
' A moving platform m: its thing MvO, its stretch MvS and curve MvZ; its middle MvX, MvY and reach MvA; MvK 0 to
' and fro, 1 up and down, 2 a lift, MvT how far up it is; MvE the edge before it.
DIM SHARED MvO(NMOVE) AS INTEGER, MvS(NMOVE) AS INTEGER, MvZ(NMOVE) AS INTEGER, MvX(NMOVE) AS INTEGER
DIM SHARED MvY(NMOVE) AS INTEGER, MvA(NMOVE) AS INTEGER, MvK(NMOVE) AS INTEGER, MvE(NMOVE) AS INTEGER
DIM SHARED MvT(NMOVE) AS SINGLE, NMv AS INTEGER
DIM SHARED Img(IMGSZ) AS LONG, At(NIMG) AS LONG, NextAt AS LONG
DIM SHARED ObX(MAXOBJ - 1) AS INTEGER, ObY(MAXOBJ - 1) AS INTEGER, ObK(MAXOBJ - 1) AS INTEGER
DIM SHARED ObS(MAXOBJ - 1) AS INTEGER, ObA(MAXOBJ - 1) AS INTEGER
DIM SHARED First(MW) AS INTEGER, NObj AS INTEGER
DIM SHARED Held(127) AS INTEGER, KeyT(127) AS SINGLE, LastScan AS INTEGER, PortOK AS INTEGER
DIM SHARED LW AS INTEGER, LWPX AS INTEGER, GoalX AS INTEGER, FamX AS INTEGER, Cheer AS INTEGER
DIM SHARED StartX AS INTEGER, StartY AS INTEGER
DIM SHARED Tune AS INTEGER, BarkAt AS LONG
DIM SHARED Ticks AS LONG, Muted AS INTEGER, LooseN AS INTEGER, PartN AS INTEGER
DIM SHARED Glint(2) AS LONG
' The colours as loaded, Dusk sixteenths of them shown; MetT: the seconds Charlie has been with its family.
DIM SHARED Pal(255) AS LONG, Dusk AS INTEGER, MetT AS SINGLE, PastT AS SINGLE
' A song's most bars.
CONST NBAR = 30
DIM SHARED Trk(NBAR * 20) AS STRING, Kit(NBAR * 4) AS STRING, Org(NBAR * 4) AS STRING, Third(NBAR * 4) AS STRING, Horn(1) AS STRING, Keys(1) AS STRING
DIM SHARED Order(1) AS STRING, Voicing(1) AS STRING, Tempo(1) AS INTEGER, BassIn(1) AS INTEGER
DIM SHARED Lvl(1, 4) AS INTEGER, Solo(1) AS STRING, Fade(1) AS INTEGER, Entry(1) AS INTEGER
DIM SHARED Sng AS INTEGER, Bar AS INTEGER, HalfBar AS INTEGER
DIM SHARED Coming AS INTEGER, ComingBar AS INTEGER, FadeOut AS INTEGER, FadeIn AS INTEGER

Main
END

' The Amber Downs, left to right: a command and its count. g, the ground's
' height in tiles, starts at 10. plat and hb: height * 100 + length.
Level:
DATA flat,4, dog,0, flat,4, flower,0, flat,4, bones,6, flat,8, tree,0, flat,4
DATA upg,2, flat,3, cat,0, flat,6, bush,0, dng,3, bones,5, flat,8, flower,0
DATA up45,2, flat,2, arc,6, gap,6, flat,5, cat,0, flat,6, tree,0, flat,3
DATA spring,0, flat,1, wall,6, flat,4, rock,0, bones,6, flat,8, plat,305, hb,504, flat,8
DATA mark,0, flat,4, dn45,4, dng,4, boost,0, bones,10, flat,8, flower,0, flat,6
DATA loop,0, flat,7, tree,0, flat,5, bones,6, flat,6, upg,2, up45,3, arc,9, gap,10, flat,8
DATA cat,0, flat,5, bush,0, cat,0, flat,6, tree,0, flat,6, drop,3, flat,6
DATA spring,0, flat,2, plat,608, hb,706, flat,12, upg,4, flat,4, flower,0, flat,4
DATA mark,0, flat,4, dn45,6, dng,6, boost,0, flat,5, rock,0, flat,7, loop,0, flat,8
DATA boost,0, bones,8, flat,6, tree,0, flat,8, loop,0, flat,10, up45,4, arc,10, gap,12, flat,10
DATA gap,3, flat,3, gap,3, flat,3, gap,3, flat,6, cat,0, flat,6, bush,0, cat,0, flat,6
DATA boost,0, flat,6, pipe,7, flat,6, tree,0, bones,8, flat,10, dng,5, boost,0, bones,10, flat,16
DATA flower,0, upg,3, flat,4, cat,0, dng,3, flat,6, tree,0, flat,4
DATA mark,0, flat,4, spring,0, flat,2, plat,722, hb,820, flat,6, thorn,0, flat,5, cat,0, flat,5
DATA thorn,0, flat,6, rock,0, flat,4, dn45,3, dng,3, boost,0, flat,8, loop,0, flat,8
DATA up45,3, arc,8, gap,9, flat,6, thorn,0, flat,5, upg,3, flat,3, tree,0, flat,3
DATA dng,4, boost,0, bones,8, flat,10, loop,0, flat,7, boost,0, flat,6, up45,4, arc,12, gap,14
DATA flat,10, bush,0, flat,4, spring,0, flat,2, plat,618, hb,716, flat,7, thorn,0, flat,6
DATA thorn,0, flat,8, flower,0, flat,4
DATA goal,0, flat,9, fam,0, flat,6, bush,0, flat,5, "end",0

' The Chrome Works: mover: a gap that wide with a platform to and fro over it. hover: a gap of 4 with one up and
' down, the ground past it that much higher. lift: a gap of 3 with a lift, the ground past it that much higher.
Level2:
DATA flat,4, dog,0, flat,8, bones,6, flat,8, cat,0, flat,12, bones,4, flat,8
DATA mover,8, flat,8, bones,5, flat,6, upg,2, flat,3, boost,0, bones,8, flat,8, loop,0, flat,10
DATA lift,6, flat,6, bones,6, flat,8, cat,0, flat,12, mark,0, flat,8
DATA mover,10, flat,6, dng,3, flat,4, drop,5, flat,5, boost,0, flat,6, pipe,6, flat,8, bones,8, flat,6
DATA drop,4, flat,8, cat,0, flat,16, hover,5, flat,8, bones,6, flat,6, mover,7, flat,6, mover,7, flat,8
DATA mark,0, flat,4, dn45,4, dng,3, boost,0, bones,10, flat,8, loop,0, flat,8, up45,3, arc,8, gap,9, flat,8
DATA cat,0, flat,14, lift,6, flat,6, bones,8, flat,8, drop,5, flat,6, spring,0, flat,2, plat,608, hb,706, flat,10
DATA boost,0, flat,8, loop,0, flat,10, drop,3, flat,6, cat,0, flat,8
DATA goal,0, flat,9, fam,0, flat,6, flat,5, "end",0

' The tunes are in CHARLIE.DAT with the pictures, from tools/charliesong.txt.

' Set up, then read input, step in fixed ticks and draw, till Esc on the
' title.
SUB Main
    DIM g AS Game, p AS Hero, k AS Pad
    DIM lo(NLOOSE - 1) AS Loose, pt(NPART - 1) AS Part
    DIM now AS SINGLE, last AS SINGLE, c AS Clock, n AS INTEGER

    OpenScreen
    LoadTables
    BuildArt
    LoadLevel 0
    LoadSong
    g.page = 1
    ResetWorld g, p, lo(), pt()

    last = TIMER
    c.span = STEPT
    DO
        ' the next frame on the screen (port 3DAh's retrace; with no display, the next look), the keys read meanwhile
        DO: Poll: LOOP UNTIL INP(&H3DA) AND 8
        now = TIMER
        Pace Elapsed!(last, now), c
        last = now
        ReadPad k
        ' a picture for each step, none for a frame between two
        n = Steps%(c)
        IF n THEN
            DO WHILE n
                Tick g, p, k, lo(), pt()
                n = n - 1
            LOOP
            PickSong -(g.mode = TITLE OR g.mode = CLEARED), g.mode = CLEARED
            FeedSong
            Render g, p, lo(), pt()
            Flip g.page
        END IF
    LOOP UNTIL g.quitting

    SCREEN 0
    WIDTH 80
END SUB

SUB OpenScreen
    SCREEN 13, , 1, 0
END SUB

' Shows the page just drawn, draws on the other.
SUB Flip (page AS INTEGER)
    SCREEN , , 1 - page, page
    page = 1 - page
END SUB

' Each tile kind's column heights: 1 solid; 2, 3 rise and fall at 45
' degrees; 4 and 5, 6 and 7 rise and fall half as steep over two tiles.
SUB LoadTables
    DIM i AS INTEGER
    FOR i = 0 TO TS - 1
        Prof(1, i) = TS
        Prof(2, i) = i + 1
        Prof(3, i) = TS - i
        Prof(4, i) = (i + 2) \ 2
        Prof(5, i) = 8 + (i + 2) \ 2
        Prof(6, i) = TS - i \ 2
        Prof(7, i) = 8 - i \ 2
    NEXT
END SUB

' Image k upside down as k2.
SUB Flop (k AS INTEGER, k2 AS INTEGER, w AS INTEGER, h AS INTEGER)
    DIM y AS INTEGER, strip(40) AS LONG
    PUT (0, 0), Img(At(k)), PSET
    FOR y = 0 TO h - 1
        GET (0, y)-(w - 1, y), strip
        PUT (100, h - 1 - y), strip, PSET
    NEXT
    Grab 100, 0, 100 + w - 1, h - 1, k2
END SUB

' The ground's columns, then what stands on them, from the Level DATA.
SUB LoadLevel (z AS INTEGER)
    DIM cmd AS STRING, n AS INTEGER, c AS INTEGER, g AS INTEGER
    DIM i AS INTEGER, s AS INTEGER, k AS INTEGER
    Zone = z
    ERASE Map
    NObj = 0: NMv = 0
    FOR i = 0 TO MW: ColTop(i) = MH: NEXT
    g = 10
    IF z THEN
        ZTile = ITILE2: ZStrip = IZ2 + 3: ZBack = IZ2: ZLoop = IZ2 + 5: ZPipe = IZ2 + 7: ZCard = ICARD2
        RESTORE Level2
    ELSE
        ZTile = ITILE: ZStrip = ISTRIP: ZBack = IBG: ZLoop = ILOOP: ZPipe = IPIPE: ZCard = ICARD
        RESTORE Level
    END IF
    DO
        READ cmd, n
        s = (MH - g) * TS
        k = 0
        SELECT CASE cmd
        CASE "flat"
            FOR i = 1 TO n: Column c, g, 1: Scatter c, s: c = c + 1: NEXT
        CASE "up45"
            FOR i = 1 TO n: Column c, g, 2: g = g + 1: c = c + 1: NEXT
        CASE "dn45"
            FOR i = 1 TO n: g = g - 1: Column c, g, 3: c = c + 1: NEXT
        CASE "upg"
            FOR i = 1 TO n
                Column c, g, 4: Column c + 1, g, 5
                g = g + 1: c = c + 2
            NEXT
        CASE "dng"
            FOR i = 1 TO n
                g = g - 1
                Column c, g, 6: Column c + 1, g, 7
                c = c + 2
            NEXT
        CASE "gap"
            c = c + n
        CASE "wall"
            g = g + n
        CASE "pipe"
            ' a wall with a quarter pipe up to it, over the three columns before: nothing grows there
            FOR i = 0 TO NObj - 1
                IF ((ObK(i) >= KTREE AND ObK(i) <= KLAMP) OR ObK(i) >= KPOST) AND ObX(i) >= c * TS - PIPER THEN ObK(i) = 0
            NEXT
            AddObj KPIPE, c * TS, s
            g = g + n
        CASE "drop"
            g = g - n
        CASE "mover"
            ' at each end of its way it lies 8 over the ground
            AddMover c * TS + (n * TS - MOVEW) \ 2, s, (n * TS - MOVEW) \ 2 + 8, 0, c * TS
            c = c + n
        CASE "hover"
            AddMover c * TS + 8, s - n * TS \ 2, n * TS \ 2, 1, c * TS
            c = c + 4: g = g + n
        CASE "lift"
            AddMover c * TS, s, n * TS, 2, c * TS
            c = c + 3: g = g + n
        CASE "plat"
            FOR i = 1 TO n MOD 100
                Map(c + i, MH - g - n \ 100 - 1) = 1
                AddObj KPLAT, (c + i) * TS, s - (n \ 100 + 1) * TS
            NEXT
        CASE "bones"
            FOR i = 0 TO n - 1: AddObj KBONE, (c + i) * TS + 8, s - 14: NEXT
        CASE "arc"
            FOR i = 0 TO n - 1
                AddObj KBONE, (c + i) * TS + 8, s - 20 - CINT(50 * SIN(PI * (i + .5) / n))
            NEXT
        CASE "hb"
            FOR i = 1 TO n MOD 100: AddObj KBONE, (c + i) * TS + 8, s - (n \ 100) * TS - 14: NEXT
        CASE "cat": k = KCAT
        CASE "spring": k = KSPRING
        CASE "mark": k = KMARK
        CASE "boost": k = KBOOST
        CASE "loop": k = KLOOP
        CASE "tree": k = KTREE
        CASE "bush": k = KBUSH
        CASE "flower": k = KFLOWER
        CASE "rock": k = KROCK
        CASE "thorn": k = KTHORN
        CASE "goal": k = KGOAL: GoalX = c * TS + 8
        CASE "fam": k = KFAM: FamX = c * TS + 8
        CASE "dog"
            StartX = c * TS + 8: StartY = s
        END SELECT
        IF k THEN AddObj k, c * TS + 8, s
    LOOP UNTIL cmd = "end"
    LW = c: LWPX = c * TS
    ' nothing grows in front of the family
    FOR i = 0 TO NObj - 1
        IF ((ObK(i) >= KBUSH AND ObK(i) <= KLAMP) OR ObK(i) >= KPOST) AND ABS(ObX(i) - FamX) < 112 THEN ObK(i) = 0
    NEXT
    SortObjs
    Track
END SUB

' The ground's line from the columns' tops, left to right, broken where they do not meet; a loop spliced in where
' it stands, a quarter pipe and its wall over the three columns before the wall; then the platforms' tops.
SUB Track
    DIM c AS INTEGER, i AS INTEGER, j AS INTEGER, k AS INTEGER, x AS INTEGER, y AS INTEGER
    DIM yl AS INTEGER, yr AS INTEGER, x1 AS INTEGER, a AS SINGLE
    GN = 0: NZ = 0
    FOR c = 0 TO MW: ColSeg(c) = -1: NEXT
    FOR i = 0 TO NObj - 1
        IF ObK(i) = KPIPE THEN ColSeg((ObX(i) - PIPER) \ TS) = -2 - i
    NEXT
    FOR c = 0 TO LW - 1
        x = c * TS
        IF ColSeg(c) < -1 THEN
            y = ObY(-2 - ColSeg(c))
            ColSeg(c) = -1
            IF GX(GN - 1) <> x OR GY(GN - 1) <> y THEN Knot CSNG(x), CSNG(y), 0
            ZA(NZ) = GN - 1: ZM(NZ) = -1: ZL(NZ) = x: ZR(NZ) = x + PIPER
            FOR j = 1 TO PIPEN - 1
                a = j * PI / 2 / PIPEN
                Knot x + PIPER * SIN(a), y - PIPER + PIPER * COS(a), 2
            NEXT
            Knot CSNG(x + PIPER), CSNG(y - PIPER), 2
            j = ColTop(c + PIPER \ TS) * TS
            IF j < y - PIPER THEN Knot CSNG(x + PIPER), CSNG(j), 2
            ZB(NZ) = GN - 2: NZ = NZ + 1
            c = c + PIPER \ TS - 1
        ELSEIF ColTop(c) < MH THEN
            k = Map(c, ColTop(c))
            y = (ColTop(c) + 1) * TS
            ' a tile's top at its edges: its profile's, to the even pixel
            yl = y - (Prof(k, 0) AND 30): yr = y - (Prof(k, TS - 1) AND 30)
            j = GN = 0
            IF NOT j THEN j = GX(GN - 1) <> x OR GY(GN - 1) <> yl
            IF j THEN Knot CSNG(x), CSNG(yl), 0
            ColSeg(c) = GN - 1
            i = First(c)
            DO WHILE i < NObj
                IF ObX(i) >= x + TS THEN EXIT DO
                IF ObK(i) = KLOOP THEN
                    Knot CSNG(ObX(i)), CSNG(yl), 1
                    ZA(NZ) = GN - 1: ZM(NZ) = GN - 1 + LOOPN \ 2
                    ZL(NZ) = ObX(i) - LOOPR: ZR(NZ) = ObX(i) + LOOPR
                    FOR j = 1 TO LOOPN - 1
                        a = j * 2 * PI / LOOPN
                        Knot ObX(i) + LOOPR * SIN(a), yl - LOOPR + LOOPR * COS(a), 2
                    NEXT
                    Knot CSNG(ObX(i)), CSNG(yl), 2
                    ZB(NZ) = GN - 2: NZ = NZ + 1
                END IF
                i = i + 1
            LOOP
            Knot CSNG(x + TS), CSNG(yr), 1
        END IF
    NEXT
    PlatA = GN
    x = NONE
    FOR i = 0 TO NObj - 1
        IF ObK(i) = KPLAT THEN
            IF x <> NONE AND ObX(i) = x1 AND ObY(i) = y THEN
                x1 = x1 + TS
            ELSE
                IF x <> NONE THEN Knot CSNG(x), CSNG(y), 0: Knot CSNG(x1), CSNG(y), 1
                x = ObX(i): x1 = x + TS: y = ObY(i)
            END IF
        END IF
    NEXT
    IF x <> NONE THEN Knot CSNG(x), CSNG(y), 0: Knot CSNG(x1), CSNG(y), 1
    ' the moving platforms: a stretch each, a curve of one, in the order they were made
    k = 0
    FOR i = 0 TO NObj - 1
        IF ObK(i) = KMOVE THEN
            Knot CSNG(ObX(i)), CSNG(ObY(i)), 0
            Knot CSNG(ObX(i) + MOVEW), CSNG(ObY(i)), 3
            MvO(k) = i: MvS(k) = GN - 2: MvZ(k) = NZ: MvT(k) = 0
            ZA(NZ) = GN - 2: ZB(NZ) = GN - 2: ZM(NZ) = -1: ZL(NZ) = ObX(i): ZR(NZ) = ObX(i) + MOVEW
            NZ = NZ + 1: k = k + 1
        END IF
    NEXT
END SUB

' A moving platform, its middle's left end at x, y; reach a, kind k; e the edge before it.
SUB AddMover (x AS INTEGER, y AS INTEGER, a AS INTEGER, k AS INTEGER, e AS INTEGER)
    AddObj KMOVE, x, y
    MvX(NMv) = x: MvY(NMv) = y: MvA(NMv) = a: MvK(NMv) = k: MvE(NMv) = e
    NMv = NMv + 1
END SUB

' Platform m's swing now, -1 to 1, held a while at each end when it goes up and down; quarter: a quarter of its
' way on, which tells where it is going.
FUNCTION Swing! (m AS INTEGER, quarter AS INTEGER)
    DIM q AS SINGLE, v AS SINGLE
    q = Ticks * STEPT * (1.1 - .2 * MvK(m)) + m + quarter * PI / 2
    v = SIN(q)
    IF MvK(m) = 1 THEN
        v = v * 1.3
        IF v > 1 THEN v = 1
        IF v < -1 THEN v = -1
    END IF
    Swing! = v
END FUNCTION

' The platforms a step on: to and fro, up and down, or a lift, up while Charlie stands on it and down again
' when not. Charlie goes with the one it stands on.
SUB Movers (p AS Hero)
    DIM m AS INTEGER, s AS INTEGER, o AS INTEGER, x AS SINGLE, y AS SINGLE, dx AS SINGLE, dy AS SINGLE
    FOR m = 0 TO NMv - 1
        s = MvS(m)
        x = MvX(m): y = MvY(m)
        SELECT CASE MvK(m)
        CASE 0
            x = x + MvA(m) * Swing!(m, 0)
        CASE 1
            y = y + MvA(m) * Swing!(m, 0)
        CASE 2
            IF p.ground AND p.sg = s THEN MvT(m) = MvT(m) + 90 * STEPT ELSE MvT(m) = MvT(m) - 60 * STEPT
            IF MvT(m) < 0 THEN MvT(m) = 0
            IF MvT(m) > MvA(m) THEN MvT(m) = MvA(m)
            y = y - MvT(m)
        END SELECT
        dx = x - GX(s): dy = y - GY(s)
        IF dx <> 0 OR dy <> 0 THEN
            GX(s) = x: GX(s + 1) = x + MOVEW: GY(s) = y: GY(s + 1) = y
            ZL(MvZ(m)) = INT(x): ZR(MvZ(m)) = INT(x) + MOVEW + 1
            o = MvO(m): ObX(o) = CINT(x): ObY(o) = CINT(y)
            IF p.ground AND p.sg = s THEN p.x = p.x + dx: p.y = p.y + dy
        END IF
    NEXT
END SUB

' The line's next point, x, y, a stretch of kind k to it from the last: 0, none.
SUB Knot (x AS SINGLE, y AS SINGLE, k AS INTEGER)
    DIM i AS INTEGER, u AS SINGLE, v AS SINGLE, d AS SINGLE
    GX(GN) = x: GY(GN) = y: GK(GN) = 0
    IF k <> 0 THEN
        i = GN - 1
        u = x - GX(i): v = y - GY(i)
        d = SQR(u * u + v * v)
        u = u / d: v = v / d
        GU(i) = u: GV(i) = v: GL(i) = d: GK(i) = k
        IF u > 0 THEN
            GA(i) = ATN(-v / u)
        ELSEIF u < 0 THEN
            GA(i) = ATN(-v / u) + PI
        ELSE
            GA(i) = -SGN(v) * PI / 2
        END IF
    END IF
    GN = GN + 1
END SUB

' The tiles' stretch of the line under x, its top within 4 of y; -1 when there is none.
FUNCTION SegAt% (x AS INTEGER, y AS SINGLE)
    DIM s AS INTEGER
    SegAt% = -1
    IF x < 0 OR x >= LWPX THEN EXIT FUNCTION
    s = ColSeg(x \ TS)
    IF s >= 0 THEN
        ' past a loop that stands in the column: the stretch after it
        IF x > GX(s + 1) THEN s = s + LOOPN + 1
        IF ABS(GY(s) + (x - GX(s)) * GV(s) / GU(s) - y) < 4 THEN SegAt% = s: EXIT FUNCTION
    END IF
    FOR s = PlatA TO GN - 2
        IF GK(s) = 1 AND x >= GX(s) AND x <= GX(s + 1) AND ABS(GY(s) - y) < 4 THEN SegAt% = s: EXIT FUNCTION
    NEXT
END FUNCTION

' Flowers, bushes and lantern plants along the flats, by the column.
SUB Scatter (c AS INTEGER, s AS INTEGER)
    IF c < 8 THEN EXIT SUB
    IF Zone THEN
        ' lamp posts, tanks and valve stacks along the decks
        SELECT CASE (c * 37 + 11) MOD 23
        CASE 0, 12: AddObj KPOST, c * TS + 8, s
        CASE 5: AddObj KTANK, c * TS + 8, s
        CASE 17: AddObj KVENT, c * TS + 8, s
        END SELECT
        EXIT SUB
    END IF
    SELECT CASE (c * 37 + 11) MOD 19
    CASE 0, 7: AddObj KFLOWER, c * TS + 8, s
    CASE 3: AddObj KBUSH, c * TS + 8, s
    CASE 11: AddObj KLAMP, c * TS + 8, s
    END SELECT
END SUB

' Column c solid g tiles up from the bottom; kind over 1 a slope on top.
SUB Column (c AS INTEGER, g AS INTEGER, kind AS INTEGER)
    DIM r AS INTEGER
    FOR r = MH - g TO MH - 1
        Map(c, r) = 1
    NEXT
    IF kind > 1 THEN
        Map(c, MH - g - 1) = kind
        ColTop(c) = MH - g - 1: ColVis(c) = kind + 2
    ELSE
        ColTop(c) = MH - g: ColVis(c) = 1 + (c AND 1)
    END IF
END SUB

SUB AddObj (k AS INTEGER, x AS INTEGER, y AS INTEGER)
    ObK(NObj) = k: ObX(NObj) = x: ObY(NObj) = y: ObA(NObj) = x
    NObj = NObj + 1
END SUB

' The things by x, and First(c) the first at or past column c.
SUB SortObjs
    DIM i AS INTEGER, j AS INTEGER, x AS INTEGER, y AS INTEGER, k AS INTEGER, c AS INTEGER
    FOR i = 1 TO NObj - 1
        x = ObX(i): y = ObY(i): k = ObK(i): j = i - 1
        DO WHILE j >= 0
            IF ObX(j) <= x THEN EXIT DO
            ObX(j + 1) = ObX(j): ObY(j + 1) = ObY(j): ObK(j + 1) = ObK(j): j = j - 1
        LOOP
        ObX(j + 1) = x: ObY(j + 1) = y: ObK(j + 1) = k
    NEXT
    i = 0
    FOR c = 0 TO MW
        DO WHILE i < NObj
            IF ObX(i) >= c * TS THEN EXIT DO
            i = i + 1
        LOOP
        First(c) = i
    NEXT
    FOR i = 0 TO NObj - 1: ObA(i) = ObX(i): NEXT
END SUB

' Every picture: the DATA's, then those built from them or drawn here on
' the hidden page; the backdrop last, as it fills the page.
SUB BuildArt
    DIM i AS INTEGER
    CLS
    LoadArt
    ' the turned run, as drawn and mirrored: each upside down too, for the far side of a loop
    FOR i = 0 TO 159: Flop ITILT + i, ITILT + 160 + i, 32, 32: NEXT
    BuildStrips IEARTH, ISTRIP
    BuildLoop ILOOP, CEARTH, CGRASS
    BuildPipe IPIPE, CEARTH, CGRASS
    BuildLoop IZ2 + 5, CPANEL, CCHROME + 1
    BuildPipe IZ2 + 7, CPANEL, CCHROME + 1
    BuildGlyphs
    BuildMid
    BuildWorks
    BuildDay
    BuildBack
END SUB

' Colour k is c, shown Dusk sixteenths as bright.
SUB Tint (k AS INTEGER, c AS LONG)
    Pal(k) = c
    PALETTE k, (c AND 63) * Dusk \ 16 + 256& * ((c \ 256 AND 63) * Dusk \ 16) + 65536 * ((c \ 65536) * Dusk \ 16)
END SUB

SUB Shade
    DIM k AS INTEGER
    FOR k = 0 TO 255: Tint k, Pal(k): NEXT
END SUB

' The palette and the pictures from CHARLIE.DAT; a mirrored one's mirror made.
SUB LoadArt
    DIM n AS INTEGER, i AS INTEGER, k AS INTEGER, w AS INTEGER, h AS INTEGER
    DIM masked AS INTEGER, k2 AS INTEGER, c AS LONG
    Dusk = 16
    OpenData
    c = Word&
    n = Word&
    FOR i = 1 TO n
        k = Word&
        Tint k, Word&
    NEXT
    n = Word&
    FOR i = 1 TO n
        k = Word&: w = Word&: h = Word&: masked = Word&: k2 = Word&
        At(k) = NextAt
        Unpack
        IF masked THEN At(k + 1) = NextAt: Unpack
        IF k2 >= 0 THEN Mirror k, k2, w, h: Mirror k + 1, k2 + 1, w, h
    NEXT
    CLOSE #1
END SUB

' CHARLIE.DAT open as #1: where its tunes start, then its pictures.
SUB OpenData
    OPEN "CHARLIE.DAT" FOR BINARY AS #1
    IF LOF(1) = 0 THEN
        CLOSE #1
        KILL "CHARLIE.DAT"
        SCREEN 0
        PRINT "CHARLIE.DAT, the game's pictures and tunes, must be beside CHARLIE.BAS."
        END
    END IF
END SUB

' CHARLIE.DAT's next LONG.
FUNCTION Word&
    DIM v AS LONG
    GET #1, , v
    Word& = v
END FUNCTION

' CHARLIE.DAT's next string: its length, then its characters.
FUNCTION Chars$
    DIM s AS STRING
    s = SPACE$(Word&)
    IF LEN(s) THEN GET #1, , s
    Chars$ = s
END FUNCTION

' A run-length stream of the file's LONGs to Img at NextAt: n then n values, -n then
' one value n times, 0 the end.
SUB Unpack
    DIM n AS INTEGER, e AS LONG, v AS LONG
    n = Word&
    DO WHILE n
        IF n > 0 THEN
            FOR e = NextAt TO NextAt + n - 1: GET #1, , Img(e): NEXT
        ELSE
            n = -n
            v = Word&
            FOR e = NextAt TO NextAt + n - 1: Img(e) = v: NEXT
        END IF
        NextAt = NextAt + n
        n = Word&
    LOOP
END SUB

' Image k, w by h, mirrored as k2: a column at a time.
SUB Mirror (k AS INTEGER, k2 AS INTEGER, w AS INTEGER, h AS INTEGER)
    DIM x AS INTEGER, strip(40) AS LONG
    PUT (0, 0), Img(At(k)), PSET
    FOR x = 0 TO w - 1
        GET (x, 0)-(x, h - 1), strip
        PUT (100 + w - 1 - x, 0), strip, PSET
    NEXT
    Grab 100, 0, 100 + w - 1, h - 1, k2
END SUB

' A column's earth under its top tile, lighter near the top; and deep
' earth for under that.
' Earth e's four tiles as two strips, k and k + 1: the ground under a column's top, and what repeats below it.
SUB BuildStrips (e AS INTEGER, k AS INTEGER)
    DIM i AS INTEGER, n AS INTEGER
    FOR i = 0 TO STRIPH \ TS - 1
        n = i
        IF n > 3 THEN n = 3
        PUT (0, i * TS), Img(At(e + n)), PSET
        PUT (20, i * TS), Img(At(e + 3)), PSET
    NEXT
    Grab 0, 0, TS - 1, STRIPH - 1, k
    Grab 20, 0, 20 + TS - 1, STRIPH - 1, k + 1
END SUB

' A loop: a ring of earth, grass on its inside, and its mask.
SUB BuildLoop (k AS INTEGER, ce AS INTEGER, cr AS INTEGER)
    DIM r AS INTEGER, i AS INTEGER, m AS INTEGER
    m = LD \ 2
    LINE (0, 0)-(LD - 1, LD - 1), 0, BF
    CIRCLE (m, m), LOOPR + 13, ce, , , 1
    CIRCLE (m, m), LOOPR - 1, ce, , , 1
    PAINT (m, m - LOOPR - 6), ce + 4, ce
    FOR r = LOOPR + 7 TO LOOPR + 12 STEP 2
        CIRCLE (m, m), r, ce + 3 - (r AND 2), , , 1
    NEXT
    FOR i = 0 TO 4
        CIRCLE (m, m), LOOPR + i, cr + 4 - i, , , 1
    NEXT
    LINE (0, 0)-(LD - 1, 2), 0, BF
    Grab 0, 0, LD - 1, LD - 1, k
    LINE (0, 0)-(LD - 1, LD - 1), 255, BF
    CIRCLE (m, m), LOOPR + 13, 0, , , 1
    CIRCLE (m, m), LOOPR - 1, 0, , , 1
    PAINT (m, m - LOOPR - 6), 0, 0
    LINE (0, 0)-(LD - 1, 2), 255, BF
    Grab 0, 0, LD - 1, LD - 1, k + 1
    LINE (0, 0)-(SW - 1, LD), 0, BF
END SUB

' A quarter pipe: the earth under its curve, grass along it, and its mask.
SUB BuildPipe (k AS INTEGER, ce AS INTEGER, cr AS INTEGER)
    DIM i AS INTEGER
    LINE (0, 0)-(PIPER - 1, PIPER - 1), ce + 4, BF
    CIRCLE (0, 0), PIPER, cr, , , 1
    PAINT (1, 1), 0, cr
    FOR i = 0 TO 4
        CIRCLE (0, 0), PIPER + i, cr + 4 - i, , , 1
    NEXT
    LINE (130, 0)-(130 + PIPER - 1, PIPER - 1), 0, BF
    CIRCLE (130, 0), PIPER, 1, , , 1
    PAINT (131, 1), 255, 1
    CIRCLE (130, 0), PIPER, 0, , , 1
    Grab 0, 0, PIPER - 1, PIPER - 1, k
    Grab 130, 0, 130 + PIPER - 1, PIPER - 1, k + 1
    LINE (0, 0)-(SW - 1, PIPER), 0, BF
END SUB

' The near hills, a masked band of period SW that scrolls on its own.
SUB BuildMid
    DIM x AS INTEGER, h AS INTEGER, t AS SINGLE
    LINE (0, 0)-(SW - 1, MIDH - 1), 0, BF
    LINE (0, 100)-(SW - 1, 100 + MIDH - 1), 255, BF
    FOR x = 0 TO SW - 1
        t = 2 * PI * x / SW
        h = CINT(26 - 12 * SIN(3 * t + .3) - 7 * SIN(7 * t + 1.3) - 3 * SIN(17 * t))
        LINE (x, h)-(x, MIDH - 1), CMID + 3
        LINE (x, h)-(x, h + 1 + (x AND 1)), CMID + 5
        LINE (x, h + 9 + CINT(3 * SIN(9 * t)))-(x, MIDH - 1), CMID + 2
        LINE (x, h + 24 + CINT(4 * SIN(5 * t + 1)))-(x, MIDH - 1), CMID + 1
        LINE (x, 100 + h)-(x, 100 + MIDH - 1), 0
    NEXT
    FOR x = 6 TO SW - 12 STEP 23
        FillEll x + (x * 7) MOD 9, 40 + (x * 5) MOD 22, 5, 3, CMID + 4
    NEXT
    Grab 0, 0, SW - 1, MIDH - 1, IMID
    Grab 0, 100, SW - 1, 100 + MIDH - 1, IMID + 1
END SUB

' The Chrome Works' near band: the plant, its pipe walls, machine houses, ball tanks and stacks, of period SW.
SUB BuildWorks
    DIM x AS INTEGER
    LINE (0, 0)-(SW - 1, MIDH - 1), 0, BF
    LINE (0, 100)-(SW - 1, 100 + MIDH - 1), 255, BF
    Slab 0, 74, SW - 1, MIDH - 1, CSTEEL + 1
    Stamp 138, 0, IPLANT + 2, -1
    Stamp 160, 5, IPLANT + 2, -1
    Stamp 62, 3, IPLANT + 2, -1
    Stamp 8, 2, IPLANT + 6, -1
    Stamp 196, 2, IPLANT + 6, -1
    Stamp 84, 0, IPLANT, -1
    Stamp 268, 0, IPLANT, -1
    FOR x = 0 TO SW - 1 STEP 64: Stamp x, 38, IPLANT + 4, -1: NEXT
    Slab 0, 36, SW - 1, 37, CCHROME + 4
    Slab 0, 38, SW - 1, 38, CCHROME + 1
    Grab 0, 0, SW - 1, MIDH - 1, IZ2 + 1
    Grab 0, 100, SW - 1, 100 + MIDH - 1, IZ2 + 2
END SUB

' A box of the band being built, and of its mask 100 below.
SUB Slab (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, c AS INTEGER)
    LINE (x1, y1)-(x2, y2), c, BF
    LINE (x1, y1 + 100)-(x2, y2 + 100), 0, BF
END SUB

' Picture k onto what is being built, at x, y; band: its shape onto the band's mask 100 below too.
SUB Stamp (x AS INTEGER, y AS INTEGER, k AS INTEGER, band AS INTEGER)
    PUT (x, y), Img(At(k + 1)), AND
    PUT (x, y), Img(At(k)), OR
    IF band THEN PUT (x, y + 100), Img(At(k + 1)), AND
END SUB

' The Chrome Works' far backdrop: a bright sky, streaks of cloud, a city of glass towers in two rows, of period SW.
SUB BuildDay
    DIM x AS INTEGER, y AS INTEGER, k AS INTEGER, i AS INTEGER
    FOR y = 0 TO SH - 1
        k = y * 16 \ 150
        IF k > 15 THEN k = 15
        LINE (0, y)-(SW - 1, y), CDAY + k
        IF (y + 3) * 16 \ 150 > k AND k < 15 THEN LINE (0, y)-(SW - 1, y), CDAY + k + 1, , &H5555 * (1 + (y AND 1))
    NEXT
    FOR i = 0 TO 8
        y = 12 + i * 11: x = (i * 83) MOD 230: k = 40 + (i * 37) MOD 70
        LINE (x, y)-(x + k, y + 1), CDAY + 15, BF
        LINE (x + 6, y - 1)-(x + k - 8, y - 1), CDAY + 15, , &HAAAA
        LINE (x + 10, y + 2)-(x + k - 4, y + 2), CDAY + 15, , &H5555
    NEXT
    Stamp 4, 28, IBLD, 0
    Stamp 72, 64, IBLD + 2, 0
    Stamp 122, 82, IBLD + 4, 0
    Stamp 198, 28, IBLD, 0
    Stamp 250, 64, IBLD + 2, 0
    Stamp 30, 52, IBLD + 6, 0
    Stamp 96, 78, IBLD + 8, 0
    Stamp 146, 96, IBLD + 10, 0
    Stamp 222, 52, IBLD + 6, 0
    Stamp 284, 78, IBLD + 8, 0
    LINE (0, 152)-(SW - 1, SH - 1), CSTEEL + 1, BF
    Grab 0, 0, SW - 1, SH - 1, IZ2
END SUB

' The sky, sun, clouds and far peaks, of period SW so the backdrop wraps.
SUB BuildBack
    DIM x AS INTEGER, y AS INTEGER, m AS INTEGER, k AS INTEGER, was AS INTEGER
    DIM t AS SINGLE
    FOR y = 0 TO SH - 1
        k = y * NSKY \ 132
        IF k > NSKY - 1 THEN k = NSKY - 1
        LINE (0, y)-(SW - 1, y), CSKY + k
        IF (y + 2) * NSKY \ 132 > k AND k < NSKY - 1 THEN LINE (0, y)-(SW - 1, y), CSKY + k + 1, , &H5555 * (1 + (y AND 1))
    NEXT
    FillEll 236, 52, 22, 22, CSKY + NSKY - 6
    FillEll 236, 52, 17, 17, CSUN
    FillEll 236, 52, 14, 14, CSUN + 1
    FillEll 234, 50, 10, 10, CSUN + 2
    FOR k = 0 TO 5
        x = k * 53 + 20: y = 16 + (k * 37) MOD 46
        Cloud x, y: Cloud x - SW, y: Cloud x + SW, y
    NEXT
    was = 100
    FOR x = 0 TO SW - 1
        t = 2 * PI * x / SW
        m = CINT(96 - 18 * SIN(2 * t + .7) - 10 * SIN(5 * t + 2.1) - 4 * SIN(11 * t))
        LINE (x, m)-(x, SH - 1), CFAR + 1 - (m > was)
        IF m < 84 THEN LINE (x, m)-(x, m + (84 - m) \ 3), CFAR + 3
        PSET (x, m), CFAR + 2
        was = m
    NEXT
    Tint CWATER, 12 + 256& * 26 + 65536 * 46
    Glint(0) = 18 + 256& * 34 + 65536 * 52
    Glint(1) = 28 + 256& * 44 + 65536 * 58
    Glint(2) = 44 + 256& * 56 + 65536 * 62
    LINE (0, 122)-(SW - 1, 141), CWATER, BF
    FOR y = 123 TO 140 STEP 2
        FOR x = (y * 37) MOD 23 TO SW - 12 STEP 19 + y MOD 7
            LINE (x, y)-(x + 4 + (x + y) MOD 7, y), CWATER + 1 + (x + y) MOD 3
        NEXT
    NEXT
    LINE (0, 142)-(SW - 1, SH - 1), CMID + 1, BF
    Grab 0, 0, SW - 1, SH - 1, IBG
END SUB

SUB Cloud (x AS INTEGER, y AS INTEGER)
    FillEll x, y + 1, 15, 5, CCLOUD
    FillEll x - 9, y + 2, 9, 4, CCLOUD
    FillEll x + 9, y + 3, 8, 3, CCLOUD
    FillEll x, y - 1, 12, 4, CCLOUD + 2
    FillEll x - 8, y, 6, 3, CCLOUD + 2
    FillEll x + 8, y + 1, 6, 2, CCLOUD + 1
END SUB

' The HUD's characters, each 9 by 9 over a shadow: yellow, then white.
' And room for the HUD's picture and mask.
SUB BuildGlyphs
    DIM i AS INTEGER, x AS INTEGER, y AS INTEGER, c AS INTEGER, ink AS INTEGER
    FOR c = 0 TO 1
        ink = CYELLOW
        IF c THEN ink = CWHITE
        FOR i = 0 TO NGLYPH - 1
            LINE (0, 0)-(8, 8), 0, BF
            COLOR CWHITE
            LOCATE 24, 1: PRINT MID$("0123456789:xABCDEHILMNOPRST", i + 1, 1);
            FOR y = 0 TO 7
                FOR x = 0 TO 7
                    IF POINT(x, 184 + y) THEN PSET (x + 1, y + 1), CINK
                NEXT
            NEXT
            FOR y = 0 TO 7
                FOR x = 0 TO 7
                    IF POINT(x, 184 + y) THEN PSET (x, y), ink
                NEXT
            NEXT
            MaskGrab 0, 0, 9, 9, 20, 0, IGLYPH + (c * NGLYPH + i) * 2
        NEXT
    NEXT
    LINE (0, 184)-(8, 191), 0, BF
    Grab 0, 0, HUDW - 1, HUDH - 1, IHUD
    Grab 0, 0, HUDW - 1, HUDH - 1, IHUD + 1
END SUB

SUB FillEll (cx AS INTEGER, cy AS INTEGER, rx AS INTEGER, ry AS INTEGER, c AS INTEGER)
    DIM y AS INTEGER, w AS INTEGER, d AS SINGLE
    FOR y = -ry TO ry
        d = y / (ry + .5)
        w = CINT(rx * SQR(1 - d * d))
        LINE (cx - w, cy + y)-(cx + w, cy + y), c
    NEXT
END SUB

SUB Grab (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, k AS INTEGER)
    At(k) = NextAt
    GET (x1, y1)-(x2, y2), Img(NextAt)
    NextAt = NextAt + (4 + CLNG(x2 - x1 + 1) * (y2 - y1 + 1) + 3) \ 4
END SUB

' The w by h at x, y kept as k, and its mask, drawn at mx, my, as k + 1:
' 255 where it is clear, 0 where drawn.
SUB MaskGrab (x AS INTEGER, y AS INTEGER, w AS INTEGER, h AS INTEGER, mx AS INTEGER, my AS INTEGER, k AS INTEGER)
    DIM i AS INTEGER, j AS INTEGER
    FOR j = 0 TO h - 1
        FOR i = 0 TO w - 1
            IF POINT(x + i, y + j) THEN PSET (mx + i, my + j), 0 ELSE PSET (mx + i, my + j), 255
        NEXT
    NEXT
    Grab x, y, x + w - 1, y + h - 1, k
    Grab mx, my, mx + w - 1, my + h - 1, k + 1
END SUB

' Masked image k, w by h, at x, y; nothing when wholly off the screen.
SUB Blit (x AS INTEGER, y AS INTEGER, k AS INTEGER, w AS INTEGER, h AS INTEGER)
    IF x + w <= 0 OR x >= SW OR y + h <= 0 OR y >= SH THEN EXIT SUB
    PUT (x, y), Img(At(k + 1)), AND
    PUT (x, y), Img(At(k)), OR
END SUB

' The keyboard port's last scan code: a press sets Held, a release clears.
SUB Poll
    DIM s AS INTEGER
    s = INP(&H60)
    IF s = LastScan THEN EXIT SUB
    LastScan = s
    IF s = 255 OR s = 224 THEN EXIT SUB
    PortOK = -1
    IF s AND 128 THEN Held(s AND 127) = 0 ELSE Held(s) = -1
END SUB

SUB ReadPad (k AS Pad)
    DIM c AS STRING, t AS SINGLE
    Poll
    t = TIMER
    DO
        c = INKEY$
        SELECT CASE c
        CASE CHR$(27): k.quit = -1
        CASE "m", "M": k.mute = -1
        CASE "n", "N"
            Tune = NOT Tune
            IF NOT Tune THEN PLAY "P64", "P64", "P64", , "P64", "P64", "P64", "P64"
        CASE CHR$(13): k.start = -1
        CASE " ": k.start = -1: KeyT(SCSPACE) = t
        CASE "z", "Z": KeyT(SCZ) = t
        CASE CHR$(0) + "K": KeyT(SCLEFT) = t
        CASE CHR$(0) + "M": KeyT(SCRIGHT) = t
        CASE CHR$(0) + "H": KeyT(SCUP) = t
        CASE CHR$(0) + "P": KeyT(SCDOWN) = t
        END SELECT
    LOOP WHILE LEN(c)
    k.l = Held2%(SCLEFT, t)
    k.r = Held2%(SCRIGHT, t)
    k.d = Held2%(SCDOWN, t)
    k.jump = Held2%(SCSPACE, t) OR Held2%(SCZ, t) OR Held2%(SCUP, t)
    IF k.jump AND NOT k.was THEN k.hit = -1
    k.was = k.jump
END SUB

' Key s held: by the port, or, without it, typed within a repeat's wait.
FUNCTION Held2% (s AS INTEGER, t AS SINGLE)
    IF PortOK THEN
        Held2% = Held(s)
    ELSE
        Held2% = t - KeyT(s) < .55 AND KeyT(s) > 0
    END IF
END FUNCTION

' A frame has been shown, d by the clock after the last: the time the game is behind by, c.lag, moved on. The game
' runs by the clock, the same at any frame rate; but the clock is too coarse to time one frame (TIMER moves 4 to 8 ms
' at a time, 55 on DOS), and stepped by it a frame showed two steps or none. So a frame is taken to last what frames
' have lasted (c.span, learnt from the clock): exactly a step, or half of one, where that is within a few percent, the
' lag then kept off the edge between a step and none; otherwise that long, with a little of what the game is behind
' the clock (c.late) made up each frame. A stall is made up at once.
SUB Pace (d AS SINGLE, c AS Clock)
    DIM a AS SINGLE, r AS SINGLE, ph AS SINGLE
    c.n = c.n + 1
    IF d < 3 * c.span OR c.n < 3 THEN
        a = 1 / c.n
        IF a < .005 THEN a = .005
        c.span = c.span + (d - c.span) * a
    END IF
    r = c.span / STEPT
    IF ABS(r - 1) < .05 THEN
        a = STEPT
    ELSEIF ABS(r - .5) < .03 THEN
        a = STEPT / 2
    ELSE
        a = c.span + c.late * .02
        c.late = c.late + d - a
        IF ABS(c.late) > .1 THEN a = a + c.late: c.late = 0
        IF a < 0 THEN a = 0
        c.lag = c.lag + a
        EXIT SUB
    END IF
    c.late = 0
    ph = c.lag - a * INT(c.lag / a)
    c.lag = c.lag + (a / 2 - ph) * .05
    IF d > 2.5 * a THEN a = a * CINT(d / a)
    c.lag = c.lag + a
END SUB

' The steps the game is behind by, taken off its lag.
FUNCTION Steps% (c AS Clock)
    DIM n AS INTEGER
    IF c.lag > .1 THEN c.lag = .1
    DO WHILE c.lag >= STEPT
        c.lag = c.lag - STEPT
        n = n + 1
    LOOP
    Steps% = n
END FUNCTION

FUNCTION Elapsed! (a AS SINGLE, b AS SINGLE)
    DIM d AS SINGLE
    d = b - a
    IF d < 0 OR d > .1 THEN d = .1
    Elapsed! = d
END FUNCTION

' A sound on the fourth voice, the tune's five left playing.
SUB Sfx (id AS INTEGER)
    DIM s AS STRING
    IF Muted THEN EXIT SUB
    SELECT CASE id
    CASE SJUMP: s = "L64O4CEGO5C"
    CASE SBONE
        ' a bark: a low woof slid up into, two voices beating, through a mouth; one at a time
        IF ABS(Ticks - BarkAt) < 8 THEN EXIT SUB
        BarkAt = Ticks
        s = "W2I15Y2,1000,6H0,6,2,3K4,0,6L8O3D"
    CASE SHURT: s = "L32O3GECO2G"
    CASE SSPRING: s = "L64O3CGO4EO5CG"
    CASE SPOP: s = "L64O5GO4CO5E"
    CASE SROLL: s = "L64O3CFA"
    CASE SCHARGE: s = "L64O4CDEFG"
    CASE SLAUNCH: s = "L64O3GO4CGO5CG"
    CASE SCLEAR: s = "L16O5CEGO6CL8E"
    CASE SDIE: s = "L16O4CO3BAGL8F"
    CASE SMARK: s = "L32O5CEGO6C"
    CASE SBOOST: s = "L64O4CEGO5CEGO6C"
    CASE SHISS: s = "W3Y3,3500,0H3,8,0,3L4O6B"
    END SELECT
    PLAY , , , "T255W1V13Y0K0I0H0,0,15,0" + s
END SUB

SUB LoadSong
    DIM i AS INTEGER, j AS INTEGER, k AS INTEGER, root AS INTEGER, nb AS INTEGER, nd AS INTEGER
    DIM bas(4) AS STRING, hits(2) AS STRING, pat(6) AS STRING, low AS INTEGER
    DIM st AS STRING, br AS STRING, sol AS STRING
    DIM w AS INTEGER, n AS INTEGER, bars AS INTEGER, mid AS STRING
    OpenData
    SEEK #1, Word&
    FOR w = 0 TO 1
    Order(w) = Chars$: Voicing(w) = Chars$: bars = Word&: Tempo(w) = Word&: BassIn(w) = Word&
    FOR i = 0 TO 4: Lvl(w, i) = Word&: NEXT
    Fade(w) = Word&: Entry(w) = Word&
    FOR i = 0 TO 4: bas(i) = Chars$: NEXT
    FOR i = 0 TO 2: hits(i) = Chars$: NEXT
    FOR i = 0 TO 6: pat(i) = Chars$: NEXT
    FOR n = 0 TO bars - 1
        i = w * NBAR + n
        root = Word&: nb = Word&: nd = Word&: st = Chars$: br = Chars$: sol = Chars$
        ' Charlie's theme has a third brass a bar
        mid = ""
        IF w THEN mid = Chars$
        low = 27
        IF LEN(st) = 2 THEN
            low = 15
            st = hits(VAL(LEFT$(st, 1))) + RIGHT$(st, 1)
            DO WHILE INSTR(st, "x") AND INSTR(st, "x") < 17: MID$(st, INSTR(st, "x"), 1) = MID$(st, 17, 1): LOOP
        END IF
        FOR j = 0 TO 1
            k = (i * 2 + j) * 5
            Kit(i * 2 + j) = MID$(pat(nd), j * 8 + 1, 8)
            Trk(k) = Beat$(Kit(i * 2 + j), 8)
            Trk(k + 1) = Riff$(bas(nb - 1), j, 15 + root)
            Trk(k + 2) = Riff$(br, j, 27)
            Trk(k + 3) = Riff$(sol, j, 27)
            Trk(k + 4) = Riff$(st, j, low)
            Org(i * 2 + j) = Riff$(br, j, 15)
            Third(i * 2 + j) = Riff$(mid, j, 27)
        NEXT
    NEXT
    NEXT
    CLOSE #1
    ' the kit; FM, as the consoles of the day: a round bass; a reedy pulse lead; two brasses that waver, their
    ' harmonics a saw's by a modulator that bends itself, for the intro; then an organ that wavers as it is held, the
    ' same an octave down on the other side, and a clavinet. Notes stop when they stop, and the room answers little.
    ' In stereo: the brass and the organ to the left, the second brass, the clavinet and the low organ to the right,
    ' a third organ between them
    PLAY "MBT140Z1,0L16", "T140W4Z1,6,3,3,2H0,5,5,1", "T140R2S4", , "T140R2W0Q42H1,4,10,1U5I4", "T140R2S12", "T140R2S12W4Z1,4,3,13,4H0,4,11,1U9", "T140R2W4Z1,4,3,13,4H0,4,11,1U9"
    Horn(0) = "W4Z1,8,3,11,4H0,4,12,1U11I2": Horn(1) = "W4Z1,4,3,13,5H1,4,11,2U9I7"
    Keys(0) = "W4Z1,4,3,13,4H0,4,11,1U9I0": Keys(1) = "W4Z3,9,3,1,3H0,4,1,2U0I0"
    ' the lead: a reedy pulse, or, in Charlie's theme, a brass
    Solo(0) = "W0Q42H1,4,10,1U5I4": Solo(1) = "W4Z1,9,3,15,3H5,4,10,1U5I4"
    Sng = 1: Bar = 0: HalfBar = 0: Coming = -1
    Tune = -1
END SUB

' Half a bar of the kit, `loud` eighths as loud: a kick, a sine that falls to its pitch, a thump and then a tail; a
' tom; a snare, all the noise, and hats, the noise at its highest over 2.5 kHz: struck, a ghost of one (h), or left
' open (O).
FUNCTION Beat$ (x AS STRING, loud AS INTEGER)
    DIM q AS INTEGER, d AS STRING
    FOR q = 1 TO 8
        SELECT CASE MID$(x, q, 1)
        CASE "K": d = d + "W4H0,4,3,3Y0K1,8V" + LTRIM$(STR$(12 * loud \ 8)) + "O1C"
        CASE "S": d = d + "W3H0,5,0,1K0Y0V" + LTRIM$(STR$(13 * loud \ 8)) + "O6C"
        CASE "T": d = d + "W4H0,5,0,3Y0K5,6V" + LTRIM$(STR$(11 * loud \ 8)) + "O2G"
        CASE "H": d = d + "W3H0,3,0,1K0Y3,2500,0V" + LTRIM$(STR$((12 * loud + 7) \ 8)) + "O6B"
        CASE "h": d = d + "W3H0,2,0,1K0Y3,2500,0V" + LTRIM$(STR$(5 * loud \ 8)) + "O6B"
        CASE "O": d = d + "W3H0,7,0,4K0Y3,2500,0V" + LTRIM$(STR$(11 * loud \ 8)) + "O6B"
        CASE ELSE: d = d + "P16"
        END SELECT
    NEXT
    Beat$ = d
END FUNCTION

' Half h of a bar's riff as PLAY's notes, its lowest note `low`: at most eight, as the drums' always are.
FUNCTION Riff$ (p AS STRING, h AS INTEGER, low AS INTEGER)
    DIM i AS INTEGER, j AS INTEGER, r AS INTEGER, last AS INTEGER, note AS INTEGER
    DIM c AS STRING, s AS STRING, ln AS STRING
    IF LEN(p) < 16 THEN Riff$ = "P2": EXIT FUNCTION
    i = h * 8 + 1: last = h * 8 + 8
    DO WHILE i <= last
        c = MID$(p, i, 1): j = i
        ' a note held over the half bar is struck again
        DO WHILE c = "-" AND j > 1: j = j - 1: c = MID$(p, j, 1): LOOP
        r = 1
        IF c = "." OR c = "-" THEN
            note = 0
            DO WHILE i + r <= last AND MID$(p, i + r, 1) = ".": r = r + 1: LOOP
        ELSE
            note = low + INSTR("0123456789abcdefghijklmnopqrstuvwxyz", LCASE$(c)) - 1
            DO WHILE i + r <= last AND MID$(p, i + r, 1) = "-": r = r + 1: LOOP
        END IF
        i = i + r
        ' five and seven sixteenths: four and six, then a rest
        j = 0
        IF r = 5 OR r = 7 THEN r = r - 1: j = 1
        SELECT CASE r
        CASE 1: ln = "16"
        CASE 2, 3: ln = "8"
        CASE 4, 6: ln = "4"
        CASE ELSE: ln = "2"
        END SELECT
        IF note THEN ln = "L" + ln + "N" + LTRIM$(STR$(note)) ELSE ln = "P" + ln
        IF r = 3 OR r = 6 THEN ln = ln + "."
        ' a capital slides from the note before
        IF note > 0 AND c <> LCASE$(c) THEN ln = "K6" + ln + "K0"
        s = s + ln
        IF j THEN s = s + "P16"
    LOOP
    Riff$ = s
END FUNCTION

' The next half bar, once the last is nearly played: the drums are the first voice, eight sixteenths every time,
' so PLAY(0) counts the tune's own clock. The last four bars fade, an eighth a half bar.
SUB FeedSong
    DIM k AS INTEGER, b AS INTEGER, loud AS INTEGER, d AS STRING, bass AS STRING, one AS STRING, two AS STRING, under AS STRING
    DIM t AS STRING, mid AS STRING
    IF Muted OR NOT Tune THEN EXIT SUB
    IF PLAY(0) > 2 THEN EXIT SUB
    ' the song picked comes in once this one has faded
    IF Coming >= 0 AND FadeOut = 0 THEN Sng = Coming: Bar = ComingBar: HalfBar = 0: Coming = -1
    t = "T" + LTRIM$(STR$(Tempo(Sng)))
    b = ASC(MID$(Order(Sng), Bar + 1, 1)) - 97
    k = (Sng * NBAR + b) * 2 + HalfBar
    loud = (LEN(Order(Sng)) - Bar) * 2 - HalfBar
    IF loud > 8 OR Bar < LEN(Order(Sng)) - Fade(Sng) THEN loud = 8
    IF FadeOut > 0 THEN
        loud = loud * FadeOut \ 3: FadeOut = FadeOut - 1
    ELSEIF FadeIn > 0 THEN
        loud = loud * (3 - FadeIn) \ 3: FadeIn = FadeIn - 1
    END IF
    d = Trk(k * 5)
    IF loud < 8 THEN d = Beat$(Kit(k), loud)
    ' the bass waits its bars; a bar's brasses are the horns or the organ, by the song
    bass = "V" + LTRIM$(STR$(Lvl(Sng, 4) * loud \ 8)) + Trk(k * 5 + 1)
    IF Bar < BassIn(Sng) THEN bass = "P2"
    ' the organ's low double: with the organ, not the intro's brass
    one = Keys(0): two = Keys(1): under = "V" + LTRIM$(STR$(Lvl(Sng, 3) * loud \ 8)) + Org(k)
    SELECT CASE MID$(Voicing(Sng), b + 1, 1)
    CASE "h": one = Horn(0): two = Horn(1): under = "P2"
    CASE "o": two = Keys(0)
    END SELECT
    mid = t + "V" + LTRIM$(STR$(Lvl(Sng, 2) * loud \ 8)) + Third(k)
    k = k * 5
    one = t + one + "V" + LTRIM$(STR$(Lvl(Sng, 0) * loud \ 8)) + Trk(k + 2)
    two = t + two + "V" + LTRIM$(STR$(Lvl(Sng, 2) * loud \ 8)) + Trk(k + 4)
    PLAY t + d, t + bass, one, , t + Solo(Sng) + "V" + LTRIM$(STR$((Lvl(Sng, 1) * loud + 7) \ 8)) + Trk(k + 3), two, t + under, mid
    HalfBar = 1 - HalfBar
    IF HalfBar = 0 THEN Bar = (Bar + 1) MOD LEN(Order(Sng))
END SUB

' Song n, when it is not the one playing, from its start or, late, from the bar it comes in at: Charlie's theme (1)
' for the title and, from its hits, the level's end; the Downs' tune (0) for the run. The one playing fades over a
' bar (half of one, late) and the next fades in over one; late, it comes in full.
SUB PickSong (n AS INTEGER, late AS INTEGER)
    IF n = Sng THEN Coming = -1: FadeOut = 0: EXIT SUB
    IF n = Coming THEN EXIT SUB
    Coming = n: ComingBar = 0: FadeOut = 2: FadeIn = 2
    IF late THEN ComingBar = Entry(n): FadeOut = 1: FadeIn = 0
END SUB

' The ground's top at column sx near y: in y's tile, the one above when
' that is full, the one below when it is empty; NONE when both are, or
' off the level.
FUNCTION Surf% (sx AS INTEGER, y AS INTEGER)
    DIM tx AS INTEGER, ty AS INTEGER, i AS INTEGER, h AS INTEGER
    Surf% = NONE
    IF sx < 0 OR sx >= LWPX OR y < TS THEN EXIT FUNCTION
    tx = sx \ TS: i = sx AND 15: ty = y \ TS
    IF ty >= MH - 1 THEN EXIT FUNCTION
    h = Prof(Map(tx, ty), i)
    IF h = TS THEN
        Surf% = ty * TS - Prof(Map(tx, ty - 1), i)
    ELSEIF h = 0 THEN
        h = Prof(Map(tx, ty + 1), i)
        IF h THEN Surf% = (ty + 2) * TS - h
    ELSE
        Surf% = (ty + 1) * TS - h
    END IF
END FUNCTION

' Past the level's ends a wall; above and below it air.
FUNCTION Solid% (x AS INTEGER, y AS INTEGER)
    IF x < 0 OR x >= LWPX THEN Solid% = -1: EXIT FUNCTION
    IF y < 0 OR y >= MH * TS THEN Solid% = 0: EXIT FUNCTION
    Solid% = (y AND 15) >= TS - Prof(Map(x \ TS, y \ TS), x AND 15)
END FUNCTION

' Charlie's side toward dir in a wall: pushed out, and true.
FUNCTION Wall% (p AS Hero, dir AS INTEGER)
    DIM n AS INTEGER, y AS INTEGER
    y = CINT(p.y)
    DO WHILE Solid%(CINT(p.x) + dir * HW, y - 11) OR Solid%(CINT(p.x) + dir * HW, y - HH + 4)
        p.x = p.x - dir
        n = n + 1
        IF n > TS THEN EXIT DO
    LOOP
    Wall% = n > 0
END FUNCTION

' Falling Charlie's feet in the tiles' ground: on the line's stretch there, its speed along it kept.
FUNCTION Land% (p AS Hero, d AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER, l AS INTEGER, c AS INTEGER, r AS INTEGER, s AS INTEGER
    x = CINT(p.x): y = CINT(p.y)
    l = Surf%(x - 6, y): c = Surf%(x, y): r = Surf%(x + 6, y)
    IF l > y OR l < y - 14 THEN l = NONE
    IF c > y OR c < y - 14 THEN c = NONE
    IF r > y OR r < y - 14 THEN r = NONE
    IF l = NONE AND c = NONE AND r = NONE THEN Land% = 0: EXIT FUNCTION
    ' by a foot on an edge: onto the edge
    IF c <> NONE THEN
        s = SegAt%(x, CSNG(c))
    ELSEIF l < r THEN
        s = SegAt%(x - 6, CSNG(l))
        ' going off that edge, not onto it: no ground
        IF p.vx > 0 THEN s = -1
    ELSE
        s = SegAt%(x + 6, CSNG(r))
        IF p.vx < 0 THEN s = -1
    END IF
    IF s < 0 THEN Land% = 0: EXIT FUNCTION
    IF p.x < GX(s) THEN p.x = GX(s)
    IF p.x > GX(s + 1) THEN p.x = GX(s + 1)
    p.y = GY(s) + (p.x - GX(s)) * GV(s) / GU(s)
    Settle p, s, d
    Land% = -1
END FUNCTION

' Down on stretch s: the speed along it kept.
SUB Settle (p AS Hero, s AS INTEGER, d AS INTEGER)
    p.sg = s: p.ang = GA(s)
    p.gsp = p.vx * GU(s) + p.vy * GV(s)
    IF p.jumped AND NOT d THEN p.roll = 0
    ' hit, it lands where it is and stands afraid a while
    IF p.stun THEN p.gsp = 0
    p.ground = -1: p.jumped = 0: p.sprung = 0: p.coyote = 0
END SUB

' Charlie in the air, come from x0, y0, through a curve's stretch from its open side: down on it where it can be
' stood on, or is met fast enough to run along; else off it, sliding. A loop's far half is not there for one that
' has not run its near half.
FUNCTION Curve% (p AS Hero, x0 AS SINGLE, y0 AS SINGLE, d AS INTEGER)
    DIM z AS INTEGER, s AS INTEGER, a AS INTEGER, b AS INTEGER, far AS INTEGER
    DIM d0 AS SINGLE, d1 AS SINGLE, t AS SINGLE, f AS SINGLE
    Curve% = 0
    FOR z = 0 TO NZ - 1
        IF p.x >= ZL(z) - 8 AND p.x <= ZR(z) + 8 THEN
            a = ZA(z): b = ZB(z)
            IF ZM(z) >= 0 THEN
                far = p.sg >= ZM(z)
                IF p.sg >= PlatA THEN far = x0 > ZL(z) + LOOPR
                IF far THEN a = ZM(z) ELSE b = ZM(z) - 1
            END IF
            FOR s = a TO b
                d1 = (p.x - GX(s)) * GV(s) - (p.y - GY(s)) * GU(s)
                IF d1 < 0 AND p.vx * GV(s) - p.vy * GU(s) < -1 THEN
                    d0 = (x0 - GX(s)) * GV(s) - (y0 - GY(s)) * GU(s)
                    t = (p.x - GX(s)) * GU(s) + (p.y - GY(s)) * GV(s)
                    ' from just behind it too, at a join; not behind a wall
                    ' and from further behind a platform, which moves
                    IF d0 > -1 + 3 * (GK(s) = 3) AND (d0 >= 0 OR GU(s) >= .35) AND t >= -1 AND t <= GL(s) + 1 THEN
                        IF t < 0 THEN t = 0
                        IF t > GL(s) THEN t = GL(s)
                        p.x = GX(s) + GU(s) * t: p.y = GY(s) + GV(s) * t
                        f = p.vx * GU(s) + p.vy * GV(s)
                        IF GU(s) >= .35 OR ABS(f) >= STALL THEN
                            Settle p, s, d
                            Curve% = -1
                        ELSE
                            p.x = p.x + GV(s): p.y = p.y - GU(s)
                            p.vx = f * GU(s): p.vy = f * GV(s)
                        END IF
                        EXIT FUNCTION
                    END IF
                END IF
            NEXT
        END IF
    NEXT
END FUNCTION

' Charlie d pixels along the line, over its joins: slowed in a sharp hollow, into the air over a sharp crest or
' off the line's end.
SUB Along (p AS Hero, d AS SINGLE)
    DIM i AS INTEGER, j AS INTEGER, a AS INTEGER, t AS SINGLE, cr AS SINGLE, dt AS SINGLE
    i = p.sg
    t = (p.x - GX(i)) * GU(i) + (p.y - GY(i)) * GV(i) + d
    DO
        IF t > GL(i) THEN
            j = i + 1: a = i
        ELSEIF t < 0 THEN
            j = i - 1: a = j
        ELSE
            EXIT DO
        END IF
        IF j >= 0 THEN IF GK(j) = 0 THEN j = -1
        IF j >= 0 THEN
            cr = GU(a) * GV(a + 1) - GV(a) * GU(a + 1)
            dt = GU(a) * GU(a + 1) + GV(a) * GV(a + 1)
            IF cr > 0 AND dt < .64 THEN j = -1
        END IF
        IF j < 0 THEN
            IF t < 0 THEN t = 0 ELSE t = GL(i)
            p.x = GX(i) + GU(i) * t: p.y = GY(i) + GV(i) * t
            Drop p
            EXIT SUB
        END IF
        IF t < 0 THEN t = t + GL(j) ELSE t = t - GL(i)
        IF cr < 0 AND dt < .7 THEN
            IF dt < 0 THEN dt = 0
            p.gsp = p.gsp * dt
            IF j > i THEN t = t * dt ELSE t = GL(j) - (GL(j) - t) * dt
        END IF
        i = j
    LOOP
    p.sg = i: p.ang = GA(i)
    p.x = GX(i) + GU(i) * t: p.y = GY(i) + GV(i) * t
END SUB

' Off the ground, its speed kept: from a wall or a roof, clear of it.
SUB Drop (p AS Hero)
    DIM i AS INTEGER
    i = p.sg
    p.ground = 0: p.vx = p.gsp * GU(i): p.vy = p.gsp * GV(i)
    IF GU(i) > .7 THEN
        p.coyote = COYOTE
    ELSEIF GU(i) < .35 THEN
        p.x = p.x + 2 * GV(i): p.y = p.y - 2 * GU(i)
    END IF
END SUB

' A step. The title plays itself: Charlie run by the pilot.
SUB Tick (g AS Game, p AS Hero, k AS Pad, lo() AS Loose, pt() AS Part)
    DIM idle AS Pad, auto AS Pad, i AS INTEGER
    Ticks = Ticks + 1
    IF NMv THEN Movers p
    g.modeT = g.modeT + STEPT
    IF g.shake > 0 THEN g.shake = g.shake - STEPT
    IF k.mute THEN
        Muted = NOT Muted: k.mute = 0
        IF Muted THEN PLAY "P64", "P64", "P64", , "P64", "P64", "P64", "P64"
    END IF
    SELECT CASE g.mode
    CASE TITLE
        IF k.quit THEN g.quitting = -1
        IF p.dead THEN
            ResetWorld g, p, lo(), pt()
        ELSEIF p.x >= GoalX THEN
            IF Met%(g, p, k, pt()) THEN ResetWorld g, p, lo(), pt()
        ELSE
            Pilot p, auto
            MoveHero p, auto, pt()
            Touch g, p, lo(), pt()
            IF p.y > MH * TS + 24 THEN Lose p
            Camera g, p, 0
        END IF
        IF k.hit OR k.start THEN StartGame g, p, lo(), pt()
    CASE PLAYING
        IF k.quit THEN ResetWorld g, p, lo(), pt()
        IF p.dead THEN
            Dying g, p
        ELSE
            g.clock = g.clock + STEPT
            MoveHero p, k, pt()
            Touch g, p, lo(), pt()
            IF p.y > MH * TS + 24 THEN Lose p
            IF p.x >= GoalX THEN
                g.mode = CLEARED: g.modeT = 0
                g.bonus = g.bones * 50& + 10& * (240 - INT(g.clock))
                IF g.bonus < g.bones * 50& THEN g.bonus = g.bones * 50&
                Sfx SCLEAR
            END IF
            Camera g, p, 0
        END IF
    CASE CLEARED
        IF g.modeT > 1.5 AND g.bonus > 0 THEN
            g.bonus = g.bonus - 50: g.score = g.score + 50
            IF g.bonus < 0 THEN g.score = g.score + g.bonus: g.bonus = 0
            IF (Ticks AND 3) = 0 THEN Sfx SBONE
        END IF
        ' met, a while and the fade over: on to the next zone, or after the last, the title
        IF k.quit THEN
            ResetWorld g, p, lo(), pt()
        ELSEIF Met%(g, p, k, pt()) THEN
            IF Zone = 0 THEN NextZone g, p, lo(), pt() ELSE ResetWorld g, p, lo(), pt()
        END IF
    CASE OVER
        IF g.modeT > 5 OR (g.modeT > 1.5 AND (k.hit OR k.start)) OR k.quit THEN ResetWorld g, p, lo(), pt()
    END SELECT
    IF (Ticks AND 1) = 0 THEN Things g, lo(), pt()
    IF (Ticks AND 15) = 0 THEN
        i = Ticks \ 16
        Tint CWATER + 1, Glint(i MOD 3)
        Tint CWATER + 2, Glint((i + 1) MOD 3)
        Tint CWATER + 3, Glint((i + 2) MOD 3)
    END IF
    k.hit = 0: k.start = 0: k.quit = 0
END SUB

' The pad of a Charlie that holds right and jumps at pits, walls and cats.
SUB Pilot (p AS Hero, k AS Pad)
    STATIC hold AS INTEGER
    DIM x AS INTEGER, y AS INTEGER, i AS INTEGER, want AS INTEGER, m AS INTEGER, d AS INTEGER, ready AS INTEGER
    k.r = -1: k.l = 0
    IF p.ground AND GK(p.sg) = 3 THEN
        ' on a platform: still, till there is ground ahead to jump to
        x = CINT(p.x): y = CINT(p.y)
        k.r = 0
        IF p.gsp > 20 THEN k.l = -1
        i = CINT(GX(p.sg)) + MOVEW + 10
        IF Surf%(i, y) <> NONE AND NOT Solid%(i, y - 12) THEN k.r = -1: k.l = 0: want = -1
    ELSEIF p.ground AND GK(p.sg) = 1 THEN
        x = CINT(p.x): y = CINT(p.y)
        i = x + 22 + CINT(ABS(p.gsp)) \ 40
        IF Surf%(i, y) = NONE AND Surf%(i, y + 2 * TS) = NONE THEN want = -1
        ' a wall, not a ramp's foot
        IF Solid%(x + 13, y - 20) THEN want = -1
        i = First(x \ TS)
        DO WHILE i < NObj
            IF ObX(i) > x + 50 THEN EXIT DO
            IF ((ObK(i) = KCAT AND ObS(i) <> 0) OR ObK(i) = KTHORN) AND ObX(i) > x THEN want = -1
            i = i + 1
        LOOP
        ' a platform's gap ahead: slowly to its edge, there till the platform is at it and staying, then on
        FOR m = 0 TO NMv - 1
            d = MvE(m) - x
            IF d > -4 AND d < 300 AND ABS(MvY(m) + MvA(m) * (MvK(m) = 1) - y) < 200 THEN
                IF d < 50 THEN want = 0
                IF p.gsp > 70 + d THEN k.r = 0: k.l = -1 ELSE IF p.gsp > 30 + d THEN k.r = 0
                IF d < 10 THEN
                    SELECT CASE MvK(m)
                    CASE 0: ready = Swing!(m, 0) < -.85 AND Swing!(m, 1) < 0
                    CASE 1: ready = Swing!(m, 0) >= 1 AND Swing!(m, 1) > .2
                    CASE 2: ready = MvT(m) = 0
                    END SELECT
                    IF ready THEN
                        k.r = -1: k.l = 0
                    ELSE
                        k.r = 0
                        IF p.gsp > 5 THEN k.l = -1
                    END IF
                END IF
            END IF
        NEXT
    ELSEIF NOT p.ground THEN
        ' in the air toward a platform's gap: slowed, to come down before it
        FOR m = 0 TO NMv - 1
            d = MvE(m) - CINT(p.x)
            IF d > -40 AND d < 260 AND p.coyote <= 0 AND p.vx > d \ 3 AND p.sg <> MvS(m) THEN k.r = 0: k.l = -1
        NEXT
    END IF
    IF want AND hold = 0 THEN k.hit = -1: hold = 24
    IF hold > 0 THEN k.jump = -1: hold = hold - 1 ELSE k.jump = 0
END SUB

' The level as it starts, the title up, Charlie at the start for the pilot.
SUB ResetWorld (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
    DIM i AS INTEGER
    IF Zone <> NextUp THEN LoadLevel NextUp
    g.mode = TITLE: g.modeT = 0: Cheer = 0: MetT = 0: PastT = 0
    IF Dusk < 16 THEN Dusk = 16: Shade
    g.lives = 3: g.score = 0: g.clock = 0
    g.homeX = StartX: g.homeY = StartY
    FOR i = 0 TO NObj - 1
        ObX(i) = ObA(i)
        ObS(i) = 0
        IF ObK(i) = KBONE THEN ObS(i) = 1
        IF ObK(i) = KCAT THEN ObS(i) = -1
    NEXT
    FOR i = 0 TO NMOVE: MvT(i) = 0: NEXT
    FOR i = 0 TO NLOOSE - 1: lo(i).t = 0: NEXT
    FOR i = 0 TO NPART - 1: pt(i).t = 0: NEXT
    LooseN = 0: PartN = 0
    Spawn g, p
END SUB

' The Amber Downs cleared: the Chrome Works, the score and lives kept.
SUB NextZone (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
    DIM sc AS LONG, lv AS INTEGER
    sc = g.score: lv = g.lives
    NextUp = 1
    ResetWorld g, p, lo(), pt()
    NextUp = 0
    g.mode = PLAYING: g.score = sc: g.lives = lv
END SUB

' Past the goal, a game's or the title's: on to the family and no further, the view sliding on to hold them all
' with Charlie at its left; a while with them, then the picture fades. Whether it has.
FUNCTION Met% (g AS Game, p AS Hero, k AS Pad, pt() AS Part)
    DIM idle AS Pad
    idle.r = p.x < FamX - 108 AND NOT p.win
    MoveHero p, idle, pt()
    IF p.x > FamX - 106 THEN p.x = FamX - 106: p.gsp = 0: p.vx = 0: p.roll = 0
    IF p.ground AND ABS(p.gsp) < 5 AND p.x >= FamX - 108 THEN p.win = -1: p.face = 0
    Cheer = p.win
    g.camx = g.camx + (FamX - 166 - g.camx) * 4 * STEPT
    PastT = PastT + STEPT
    IF p.win AND g.bonus = 0 THEN MetT = MetT + STEPT
    IF Dusk = 16 AND (MetT > 6 OR PastT > 40 OR (MetT > 2 AND (k.hit OR k.start))) THEN Dusk = 15
    IF Dusk < 16 AND (Ticks AND 3) = 0 THEN Dusk = Dusk - 1: Shade
    Met% = Dusk <= 0
END FUNCTION

SUB StartGame (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
    ResetWorld g, p, lo(), pt()
    g.mode = PLAYING
END SUB

SUB Spawn (g AS Game, p AS Hero)
    DIM fresh AS Hero
    p = fresh
    p.x = g.homeX: p.y = g.homeY: p.ground = -1
    p.sg = SegAt%(g.homeX, CSNG(g.homeY))
    g.bones = 0
    Camera g, p, -1
END SUB

SUB MoveHero (p AS Hero, k AS Pad, pt() AS Part)
    DIM l AS INTEGER, r AS INTEGER, d AS INTEGER, hit AS INTEGER, hold AS INTEGER
    DIM v AS SINGLE
    ' a jump held through the hit is a jump as it ends: it was forgotten
    IF p.stun AND p.ground AND p.inv < 1 THEN p.stun = 0: IF k.jump THEN k.hit = -1
    IF p.stun = 0 THEN l = k.l: r = k.r: d = k.d: hit = k.hit: hold = k.jump
    ' a jump pressed just before landing, or just after the ground ran out, was lost: it is kept a moment
    IF hit THEN p.want = JBUFFER
    IF p.want > 0 THEN
        p.want = p.want - STEPT
        IF p.ground THEN
            hit = -1: p.want = 0
        ELSEIF p.coyote > 0 THEN
            p.vy = -JUMPV: p.jumped = -1: p.roll = -1: p.coyote = 0: p.want = 0
            Sfx SJUMP
        END IF
    END IF
    IF p.coyote > 0 THEN p.coyote = p.coyote - STEPT
    IF p.inv > 0 THEN p.inv = p.inv - STEPT
    p.lastx = p.x
    ' slid off a slope too steep for its speed, the keys are no help a while
    IF p.slip > 0 THEN
        p.slip = p.slip - STEPT
        IF p.ground THEN l = 0: r = 0
    END IF
    IF p.ground THEN
        Walk p, l, r, d, hit, pt()
    ELSE
        Airborne p, l, r, d, hold, pt()
    END IF
    IF p.x < 8 THEN p.x = 8: p.gsp = 0: p.vx = 0
    IF p.x > LWPX - 8 THEN p.x = LWPX - 8: p.gsp = 0: p.vx = 0
    IF p.ground THEN v = ABS(p.gsp) ELSE v = ABS(p.vx)
    IF v < 5 AND p.ground THEN p.idleT = p.idleT + STEPT ELSE p.idleT = 0
    IF p.roll OR p.charging THEN
        IF p.charging THEN v = 300 + p.charge
        p.anim = p.anim + (v + 120) * STEPT / 9
    ELSEIF v >= TOP - 5 THEN
        p.anim = p.anim + 20 * STEPT
    ELSE
        p.anim = p.anim + v * STEPT / 7
    END IF
END SUB

' On the ground: wind up, jump, roll, run; then along it, a few pixels a
' move, into walls and onto the ground's curve.
SUB Walk (p AS Hero, l AS INTEGER, r AS INTEGER, d AS INTEGER, hit AS INTEGER, pt() AS Part)
    DIM sn AS SINGLE, cs AS SINGLE, f AS SINGLE, n AS INTEGER, s AS INTEGER, dir AS INTEGER, i AS INTEGER
    sn = SIN(p.ang): cs = COS(p.ang)
    dir = 1 - 2 * p.face
    IF p.charging THEN
        p.charge = p.charge * (1 - 2 * STEPT)
        IF hit THEN
            p.charge = p.charge + 130
            IF p.charge > CHARGEMAX THEN p.charge = CHARGEMAX
            Sfx SCHARGE
            Burst pt(), p.x - dir * 10, p.y - 2, 3, 70, CSTONE + 3
        END IF
        IF d THEN EXIT SUB
        p.charging = 0: p.roll = -1
        p.gsp = (CHARGE0 + p.charge) * dir
        Sfx SLAUNCH
    ELSEIF hit THEN
        IF p.crouch THEN
            p.charging = -1: p.charge = 0: Sfx SCHARGE
            EXIT SUB
        END IF
        p.vx = p.gsp * cs - JUMPV * sn
        p.vy = -p.gsp * sn - JUMPV * cs
        p.ground = 0: p.roll = -1: p.jumped = -1: p.crouch = 0
        Sfx SJUMP
        EXIT SUB
    END IF

    p.crouch = d AND ABS(p.gsp) < 20 AND NOT p.roll
    p.skid = 0
    IF p.roll THEN
        IF (p.gsp > 0) = (sn > 0) THEN f = RUPS ELSE f = RDNS
        p.gsp = p.gsp - f * sn * STEPT
        IF l AND p.gsp > 0 THEN p.gsp = p.gsp - RDEC * STEPT
        IF r AND p.gsp < 0 THEN p.gsp = p.gsp + RDEC * STEPT
        Slow p.gsp, RFRIC
        IF ABS(p.gsp) < 30 AND ABS(sn) < .2 THEN p.roll = 0
    ELSE
        IF p.gsp <> 0 OR ABS(sn) > .75 OR p.slip > 0 THEN p.gsp = p.gsp - SLOPE * sn * STEPT
        IF p.crouch THEN
            Slow p.gsp, FRIC * 4
        ELSEIF r THEN
            IF p.gsp < 0 THEN
                p.gsp = p.gsp + DEC * STEPT
                p.skid = p.gsp < -80
            ELSEIF p.gsp < TOP THEN
                p.gsp = p.gsp + RUNACC * STEPT
                IF p.gsp > TOP THEN p.gsp = TOP
            END IF
            IF p.gsp >= 0 THEN p.face = 0
        ELSEIF l THEN
            IF p.gsp > 0 THEN
                p.gsp = p.gsp - DEC * STEPT
                p.skid = p.gsp > 80
            ELSEIF p.gsp > -TOP THEN
                p.gsp = p.gsp - RUNACC * STEPT
                IF p.gsp < -TOP THEN p.gsp = -TOP
            END IF
            IF p.gsp <= 0 THEN p.face = 1
        ELSEIF p.slip <= 0 AND cs >= .7 THEN
            ' on steep ground nothing but the slope slows it
            Slow p.gsp, FRIC
        END IF
        IF d AND ABS(p.gsp) >= 60 THEN p.roll = -1: p.crouch = 0: Sfx SROLL
        IF p.skid AND (Ticks AND 3) = 0 THEN Emit pt(), p.x, p.y - 1, -p.gsp * .1, -30, .4, CSTONE + 3
    END IF
    IF p.gsp > MAXV THEN p.gsp = MAXV
    IF p.gsp < -MAXV THEN p.gsp = -MAXV

    n = 1 + INT(ABS(p.gsp) * STEPT / 8)
    FOR s = 1 TO n
        Along p, p.gsp * STEPT / n
        IF NOT p.ground THEN EXIT SUB
        ' a wall stops it on the tiles' ground; a curve is run up
        i = p.sg
        IF GK(i) <> 2 AND p.gsp <> 0 THEN
            IF Wall%(p, SGN(p.gsp)) THEN
                p.gsp = 0
                p.y = GY(i) + (p.x - GX(i)) * GV(i) / GU(i)
                Along p, 0
                IF NOT p.ground THEN EXIT SUB
            END IF
        END IF
    NEXT
    ' too slow for a wall or a roof, off it; for a steep slope, down it
    i = p.sg
    IF ABS(p.gsp) < STALL THEN
        IF GU(i) < .35 THEN
            Drop p
        ELSEIF GU(i) < .7 THEN
            p.slip = .5
        END IF
    END IF
END SUB

' In the air: gravity, steering, a jump cut short when let go; then a few
' pixels a move, into walls, ceilings and onto the ground.
SUB Airborne (p AS Hero, l AS INTEGER, r AS INTEGER, d AS INTEGER, hold AS INTEGER, pt() AS Part)
    DIM n AS INTEGER, s AS INTEGER, m AS SINGLE, y AS INTEGER, fell AS SINGLE, x0 AS SINGLE, y0 AS SINGLE
    p.crouch = 0: p.charging = 0: p.skid = 0
    p.vy = p.vy + GRAV * STEPT
    IF p.vy > 900 THEN p.vy = 900
    IF r THEN
        IF p.vx < TOP THEN p.vx = p.vx + AIRACC * STEPT: IF p.vx > TOP THEN p.vx = TOP
        p.face = 0
    ELSEIF l THEN
        IF p.vx > -TOP THEN p.vx = p.vx - AIRACC * STEPT: IF p.vx < -TOP THEN p.vx = -TOP
        p.face = 1
    END IF
    IF p.jumped AND NOT hold AND p.vy < -JUMPCUT THEN p.vy = -JUMPCUT
    m = ABS(p.vx)
    IF ABS(p.vy) > m THEN m = ABS(p.vy)
    n = 1 + INT(m * STEPT / 8)
    FOR s = 1 TO n
        x0 = p.x: y0 = p.y
        p.x = p.x + p.vx * STEPT / n
        IF p.vx <> 0 THEN IF Wall%(p, SGN(p.vx)) THEN p.vx = 0
        p.y = p.y + p.vy * STEPT / n
        fell = p.vy
        IF Curve%(p, x0, y0, d) THEN
            IF fell > 420 THEN Burst pt(), p.x, p.y - 2, 5, 60, CSTONE + 3
            EXIT FOR
        END IF
        IF p.vy < 0 THEN
            y = CINT(p.y) - HH
            IF Solid%(CINT(p.x), y) OR Solid%(CINT(p.x) - 4, y) OR Solid%(CINT(p.x) + 4, y) THEN
                p.y = (y \ TS + 1) * TS + HH
                p.vy = 0
            END IF
        ELSE
            fell = p.vy
            IF Land%(p, d) THEN
                IF fell > 420 THEN Burst pt(), p.x, p.y - 2, 5, 60, CSTONE + 3
                EXIT FOR
            END IF
        END IF
    NEXT
END SUB

' v toward 0 by f a second, not past it.
SUB Slow (v AS SINGLE, f AS SINGLE)
    IF v > 0 THEN
        v = v - f * STEPT: IF v < 0 THEN v = 0
    ELSEIF v < 0 THEN
        v = v + f * STEPT: IF v > 0 THEN v = 0
    END IF
END SUB

' Charlie against the things near it, and the loose bones.
SUB Touch (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
    DIM i AS INTEGER, c AS INTEGER, px AS INTEGER, py AS INTEGER, x AS INTEGER, dx AS INTEGER
    px = CINT(p.x): py = CINT(p.y)
    c = (px - 80) \ TS
    IF c < 0 THEN c = 0
    i = First(c)
    DO WHILE i < NObj
        x = ObX(i)
        IF x > px + 80 THEN EXIT DO
        dx = ABS(x - px)
        SELECT CASE ObK(i)
        CASE KBONE
            IF ObS(i) AND dx < 12 THEN
                IF ABS(ObY(i) - py + 13) < 16 THEN
                    ObS(i) = 0: g.bones = g.bones + 1: g.score = g.score + 10
                    Burst pt(), CSNG(x), CSNG(ObY(i)), 4, 50, CGOLD + 4
                    Sfx SBONE
                END IF
            END IF
        CASE KCAT
            IF ObS(i) <> 0 AND dx < 15 THEN
                IF ABS(ObY(i) - 6 - py + 13) < 17 THEN
                    IF p.roll OR p.charging THEN
                        ObS(i) = 0: g.score = g.score + 100: g.shake = .15
                        Burst pt(), CSNG(x), CSNG(ObY(i) - 6), 10, 130, CWHITE
                        Sfx SPOP
                        IF NOT p.ground AND p.vy > 0 THEN p.vy = -BOUNCEV
                    ELSE
                        Hurt g, p, x, lo(), SHISS
                    END IF
                END IF
            END IF
        CASE KSPRING
            IF dx < 11 AND py >= ObY(i) - 12 AND py <= ObY(i) + 2 THEN
                IF p.ground OR p.vy >= 0 THEN
                    IF p.ground THEN p.vx = p.gsp
                    p.ground = 0: p.vy = -SPRINGV: p.y = ObY(i) - 12
                    p.roll = 0: p.jumped = 0: p.sprung = -1
                    ObS(i) = 8
                    Sfx SSPRING
                END IF
            END IF
        CASE KBOOST
            IF dx < 12 AND p.ground AND ABS(py - ObY(i)) < 4 AND ABS(p.gsp) < BOOSTV THEN
                p.gsp = BOOSTV * (1 - 2 * p.face)
                g.shake = .2
                Burst pt(), p.x, p.y - 4, 8, 150, CGOLD + 4
                Sfx SBOOST
            END IF
        CASE KMARK
            IF ObS(i) = 0 AND px >= x THEN
                ObS(i) = 1: g.homeX = x: g.homeY = ObY(i)
                Burst pt(), CSNG(x), CSNG(ObY(i) - 28), 8, 80, CGOLD + 4
                Sfx SMARK
            END IF
        CASE KGOAL
            IF px >= x THEN ObS(i) = 1
        CASE KTHORN
            IF dx < 12 AND ABS(py - ObY(i)) < 8 THEN Hurt g, p, x, lo(), SHURT
        END SELECT
        i = i + 1
    LOOP
    IF LooseN = 0 THEN EXIT SUB
    FOR i = 0 TO NLOOSE - 1
        IF lo(i).t > 0 AND lo(i).t < 3.3 THEN
            IF ABS(lo(i).x - px) < 12 AND ABS(lo(i).y - py + 13) < 16 THEN
                lo(i).t = 0: LooseN = LooseN - 1: g.bones = g.bones + 1
                Sfx SBONE
            END IF
        END IF
    NEXT
END SUB

' Hit: the bones fly loose and Charlie is knocked back; with none, it is out.
SUB Hurt (g AS Game, p AS Hero, fromX AS INTEGER, lo() AS Loose, snd AS INTEGER)
    DIM i AS INTEGER, n AS INTEGER, a AS SINGLE, v AS SINGLE
    IF p.inv > 0 THEN EXIT SUB
    IF snd = SHISS THEN Sfx snd
    IF g.bones = 0 THEN Lose p: EXIT SUB
    n = g.bones
    IF n > NLOOSE THEN n = NLOOSE
    g.bones = 0
    FOR i = 0 TO n - 1
        a = PI * (.15 + .7 * i / n)
        v = 240 - 70 * (i AND 1)
        lo(i).x = p.x: lo(i).y = p.y - 13
        lo(i).vx = COS(a) * v * (1 - 2 * (i AND 1)) * .8
        lo(i).vy = -SIN(a) * v
        lo(i).t = 4
    NEXT
    LooseN = n
    p.ground = 0: p.roll = 0: p.charging = 0: p.crouch = 0: p.jumped = 0
    p.stun = -1: p.inv = 2
    p.vy = -260
    IF p.x < fromX THEN p.vx = -140 ELSE p.vx = 140
    g.shake = .25
    IF snd = SHURT THEN Sfx snd
END SUB

SUB Lose (p AS Hero)
    IF p.dead THEN EXIT SUB
    p.dead = -1: p.deadT = 0: p.vy = -340: p.ground = 0
    Sfx SDIE
END SUB

' Out: Charlie rises and falls through everything; then the next life.
SUB Dying (g AS Game, p AS Hero)
    p.deadT = p.deadT + STEPT
    p.vy = p.vy + GRAV * STEPT
    p.y = p.y + p.vy * STEPT
    IF p.deadT < 2 THEN EXIT SUB
    g.lives = g.lives - 1
    IF g.lives <= 0 THEN
        g.mode = OVER: g.modeT = 0
    ELSE
        Spawn g, p
    END IF
END SUB

' Thirty times a second: cats near the view pace about their homes,
' turning at edges and walls; springs settle; loose bones bounce and
' fade; sparks fall.
SUB Things (g AS Game, lo() AS Loose, pt() AS Part)
    DIM i AS INTEGER, x AS INTEGER, y AS INTEGER, s AS INTEGER, c AS INTEGER
    CONST DT = STEPT * 2
    c = (CINT(g.camx) - 100) \ TS
    IF c < 0 THEN c = 0
    i = First(c)
    DO WHILE i < NObj
        IF ObA(i) > g.camx + SW + 100 THEN EXIT DO
        IF ObK(i) = KCAT AND ObS(i) <> 0 THEN
            x = ObX(i) + ObS(i): y = ObY(i)
            s = Surf%(x + ObS(i) * 8, y)
            IF ABS(x - ObA(i)) > 40 OR ABS(s - y) > 6 OR Solid%(x + ObS(i) * 8, y - 6) THEN
                ObS(i) = -ObS(i)
            ELSE
                ObX(i) = x
                s = Surf%(x, y)
                IF ABS(s - y) <= 6 THEN ObY(i) = s
            END IF
        ELSEIF ObK(i) = KSPRING AND ObS(i) > 0 THEN
            ObS(i) = ObS(i) - 1
        END IF
        i = i + 1
    LOOP
    IF LooseN THEN
        FOR i = 0 TO NLOOSE - 1
            IF lo(i).t > 0 THEN
                lo(i).t = lo(i).t - DT
                lo(i).vy = lo(i).vy + GRAV * .6 * DT
                lo(i).x = lo(i).x + lo(i).vx * DT
                IF Solid%(CINT(lo(i).x), CINT(lo(i).y) - 3) THEN lo(i).vx = -lo(i).vx
                lo(i).y = lo(i).y + lo(i).vy * DT
                x = CINT(lo(i).x): y = CINT(lo(i).y) + 4
                s = Surf%(x, y)
                IF s <= y AND y - s < 14 AND lo(i).vy > 0 THEN
                    lo(i).y = s - 4: lo(i).vy = -lo(i).vy * .7
                END IF
                IF lo(i).y > MH * TS + 16 THEN lo(i).t = 0
                IF lo(i).t <= 0 THEN LooseN = LooseN - 1
            END IF
        NEXT
    END IF
    IF PartN THEN
        FOR i = 0 TO NPART - 1
            IF pt(i).t > 0 THEN
                pt(i).t = pt(i).t - DT
                pt(i).x = pt(i).x + pt(i).vx * DT
                pt(i).y = pt(i).y + pt(i).vy * DT
                pt(i).vy = pt(i).vy + 300 * DT
                IF pt(i).t <= 0 THEN PartN = PartN - 1
            END IF
        NEXT
    END IF
END SUB

SUB Emit (pt() AS Part, x AS SINGLE, y AS SINGLE, vx AS SINGLE, vy AS SINGLE, t AS SINGLE, c AS INTEGER)
    DIM i AS INTEGER
    IF PartN >= NPART THEN EXIT SUB
    FOR i = 0 TO NPART - 1
        IF pt(i).t <= 0 THEN
            pt(i).x = x: pt(i).y = y: pt(i).vx = vx: pt(i).vy = vy
            pt(i).t = t: pt(i).c = c
            PartN = PartN + 1
            EXIT SUB
        END IF
    NEXT
END SUB

' n sparks out from x, y at speed v.
SUB Burst (pt() AS Part, x AS SINGLE, y AS SINGLE, n AS INTEGER, v AS SINGLE, c AS INTEGER)
    DIM i AS INTEGER, a AS SINGLE
    FOR i = 0 TO n - 1
        a = 2 * PI * (i + RND * .5) / n
        Emit pt(), x, y, COS(a) * v, SIN(a) * v - v * .5, .5, c
    NEXT
END SUB

' The view ahead of Charlie's run, kept in the level; snap puts it there.
SUB Camera (g AS Game, p AS Hero, snap AS INTEGER)
    DIM tx AS SINGLE, ty AS SINGLE, v AS SINGLE
    IF p.ground THEN v = p.gsp ELSE v = p.vx
    v = v * .2
    IF v > 70 THEN v = 70
    IF v < -70 THEN v = -70
    tx = p.x - SW \ 2 + v
    ty = p.y - 124
    IF snap THEN
        g.camx = tx: g.camy = ty
    ELSE
        g.camx = g.camx + (tx - g.camx) * 7 * STEPT
        g.camy = g.camy + (ty - g.camy) * 6 * STEPT
        IF p.x - g.camx > 250 THEN g.camx = p.x - 250
        IF p.x - g.camx < 70 THEN g.camx = p.x - 70
        IF p.y - g.camy > 172 THEN g.camy = p.y - 172
        IF p.y - g.camy < 48 THEN g.camy = p.y - 48
    END IF
    IF g.camx < 0 THEN g.camx = 0
    IF g.camx > LWPX - SW THEN g.camx = LWPX - SW
    IF g.camy < 0 THEN g.camy = 0
    IF g.camy > MH * TS - SH THEN g.camy = MH * TS - SH
END SUB

' Back to front, then the screen's words.
SUB Render (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
    DIM cx AS INTEGER, cy AS INTEGER, x AS INTEGER
    cx = INT(g.camx): cy = INT(g.camy)
    IF g.shake > 0 THEN cy = cy + ((Ticks AND 2) - 1) * 2
    IF g.mode <> TITLE THEN Hud g
    DrawBack cx, cy
    DrawThings cx, cy, -1
    DrawTiles cx, cy
    DrawThings cx, cy, 0
    IF LooseN THEN DrawLoose cx, cy, lo()
    DrawHero p, cx, cy
    IF PartN THEN DrawParts pt(), cx, cy
    SELECT CASE g.mode
    CASE TITLE
        Blit (SW - 170) \ 2, 22, ITITLE, 170, 26
        Blit (SW - 170) \ 2, 52, IDASH, 170, 26
        Panel 0, 181, SW - 1, SH - 1
        IF (Ticks \ 30) AND 1 THEN
            Banner 24, "PRESS SPACE", CYELLOW
        ELSE
            Banner 24, "ARROWS RUN  SPACE JUMPS  DOWN ROLLS", CWHITE
        END IF
    CASE PLAYING
        Blit 6, 5, IHUD, HUDW, HUDH
        IF p.stun THEN
            Say p, cx, cy, "HEY!"
        ELSEIF p.idleT > 5 THEN
            Say p, cx, cy, "C'MON!"
        ELSEIF p.idleT > 3 THEN
            Say p, cx, cy, "..."
        END IF
        IF g.modeT < 2.6 THEN
            x = CINT(g.modeT * 900) - SW
            IF x > 0 THEN x = 0
            IF g.modeT > 2.2 THEN x = CINT((g.modeT - 2.2) * 900)
            LINE (x, 84)-(x + SW - 1, 110), CSCARF, BF
            LINE (x, 84)-(x + SW - 1, 85), CGOLD + 3, BF
            LINE (x, 109)-(x + SW - 1, 110), CGOLD + 3, BF
            Blit x + (SW - 178) \ 2, 89, ZCard, 178, 18
        END IF
    CASE CLEARED
        Blit 6, 5, IHUD, HUDW, HUDH
        ' up and to the right: under it Charlie meets its family
        Panel 124, 4, 315, 36
        IF Zone THEN Txt 2, 17, "CHROME WORKS CLEARED", CYELLOW ELSE Txt 2, 18, "AMBER DOWNS CLEARED", CYELLOW
        Txt 3, 22, "BONUS " + Num$(g.bonus, 6), CWHITE
        Txt 4, 22, "SCORE " + Num$(g.score, 6), CWHITE
        IF p.win THEN Say p, cx, cy, "WOOF!"
    CASE OVER
        Blit 6, 5, IHUD, HUDW, HUDH
        Panel 108, 68, 211, 92
        Banner 10, "GAME OVER", CYELLOW
    END SELECT
END SUB

' The far backdrop an eighth of the view's scroll, the near hills half,
' both wrapping; the hills sink as the view rises.
SUB DrawBack (cx AS INTEGER, cy AS INTEGER)
    DIM o AS INTEGER, y AS INTEGER
    o = (cx \ 8) MOD SW
    PUT (-o, 0), Img(At(ZBack)), PSET
    IF o THEN PUT (SW - o, 0), Img(At(ZBack)), PSET
    o = (cx \ 2) MOD SW
    y = 132 - cy \ 6
    IF y >= SH THEN EXIT SUB
    PUT (-o, y), Img(At(ZBack + 2)), AND
    PUT (-o, y), Img(At(ZBack + 1)), OR
    IF o THEN
        PUT (SW - o, y), Img(At(ZBack + 2)), AND
        PUT (SW - o, y), Img(At(ZBack + 1)), OR
    END IF
END SUB

' Each column in view: its top tile, then its earth in strips below.
SUB DrawTiles (cx AS INTEGER, cy AS INTEGER)
    DIM tx AS INTEGER, tx1 AS INTEGER, x AS INTEGER, y AS INTEGER, v AS INTEGER, k AS INTEGER, r AS INTEGER
    tx1 = cx \ TS + SW \ TS
    IF tx1 > LW - 1 THEN tx1 = LW - 1
    FOR tx = cx \ TS TO tx1
        y = ColTop(tx) * TS - cy
        IF y < SH THEN
            x = tx * TS - cx
            IF y > -TS THEN
                v = ColVis(tx)
                k = ZTile + (v - 1) * 2
                IF v <= 2 AND Zone THEN
                    PUT (x, y), Img(At(IDK + (tx AND 3))), PSET
                ELSEIF v <= 2 THEN
                    PUT (x, y), Img(At(k)), PSET
                ELSE
                    PUT (x, y), Img(At(k + 1)), AND
                    PUT (x, y), Img(At(k)), OR
                END IF
            END IF
            y = y + TS
            IF Zone THEN
                ' The works' wall hangs from the world, not the deck: single tiles down to its next 128, then strips.
                k = (tx AND 3) * 8
                r = ColTop(tx) + 1
                DO WHILE (r AND 7) <> 0 AND y < SH
                    IF y > -TS THEN PUT (x, y), Img(At(IWT + k + (r AND 7))), PSET
                    r = r + 1
                    y = y + TS
                LOOP
                DO WHILE y < SH
                    IF y > -128 THEN PUT (x, y), Img(At(IWS + (tx AND 3))), PSET
                    y = y + 128
                LOOP
            END IF
            IF y < SH AND y > -STRIPH THEN PUT (x, y), Img(At(ZStrip)), PSET
            y = y + STRIPH
            DO WHILE y < SH
                IF y > -STRIPH THEN PUT (x, y), Img(At(ZStrip + 1)), PSET
                y = y + STRIPH
            LOOP
        END IF
    NEXT
END SUB

' The things in view: back, the scenery behind the ground; else the rest.
SUB DrawThings (cx AS INTEGER, cy AS INTEGER, back AS INTEGER)
    DIM i AS INTEGER, c AS INTEGER, x AS INTEGER, y AS INTEGER, k AS INTEGER, f AS INTEGER
    c = (cx - 96) \ TS
    IF c < 0 THEN c = 0
    i = First(c)
    DO WHILE i < NObj
        x = ObX(i) - cx
        IF x > SW + 96 THEN EXIT DO
        k = ObK(i)
        IF (k >= KLOOP) = back THEN
            y = ObY(i) - cy
            SELECT CASE k
            CASE KBONE
                IF ObS(i) THEN Blit x - 5, y - 5, IBONE + ((Ticks \ 6) AND 3) * 2, 10, 10
            CASE KCAT
                IF ObS(i) THEN Blit x - 10, y - 13, ICAT + ((Ticks \ 8) AND 1) * 2 - 4 * (ObS(i) < 0), 20, 14
            CASE KSPRING
                Blit x - 8, y - 15, ISPRING - 2 * (ObS(i) > 0), 16, 16
            CASE KBOOST
                Blit x - 12, y - 9, IBOOST + ((Ticks \ 4) AND 1) * 2, 24, 10
            CASE KMARK
                Blit x - 6, y - 31, IMARK + 2 * ObS(i), 12, 32
            CASE KGOAL
                f = 0
                IF ObS(i) THEN f = (Ticks \ 4) AND 3
                Blit x - 12, y - 47, IGOAL + f * 2, 24, 48
            CASE KPLAT
                Blit x, y, ZTile + 18, TS, TS
            CASE KLOOP
                Blit x - LD \ 2, y - LOOPR - LD \ 2, ZLoop, LD, LD
            CASE KPIPE
                Blit x - PIPER, y - PIPER, ZPipe, PIPER, PIPER
            CASE KMOVE
                Blit x, y, IMOVE, MOVEW, 12
            CASE KPOST
                Blit x - 7, y - 47, IPOST, 16, 48
            CASE KTANK
                Blit x - 16, y - 39, ITANK, 32, 40
            CASE KVENT
                Blit x - 8, y - 27, IVENT, 16, 28
            CASE KTREE
                Blit x - 24, y - 70, ITREE, 48, 72
            CASE KBUSH
                Blit x - 12, y - 12, IBUSH, 24, 14
            CASE KFLOWER
                Blit x - 6, y - 14, IFLOWER + ((Ticks \ 30 + i) AND 1) * 2, 12, 16
            CASE KROCK
                Blit x - 10, y - 10, IROCK, 20, 12
            CASE KTHORN
                Blit x - 8, y - 10, ITHORN, 16, 12
            CASE KFAM
                ' who meets Charlie: they cheer once it has come to them
                f = 0
                IF Cheer THEN f = (Ticks \ 14) AND 1
                Blit x - 90, y - 81, IFAM + f * 2, 180, 82
            CASE KLAMP
                Blit x - 10, y - 42, ILAMP, 20, 44
            END SELECT
        END IF
        i = i + 1
    LOOP
END SUB

SUB DrawLoose (cx AS INTEGER, cy AS INTEGER, lo() AS Loose)
    DIM i AS INTEGER, k AS INTEGER
    k = IBONE + ((Ticks \ 6) AND 3) * 2
    FOR i = 0 TO NLOOSE - 1
        IF lo(i).t > 1 OR (lo(i).t > 0 AND (Ticks AND 4)) THEN
            Blit CINT(lo(i).x) - 5 - cx, CINT(lo(i).y) - 5 - cy, k, 10, 10
        END IF
    NEXT
END SUB

' Charlie, and streaks of wind behind it at speed; running on a slope, the
' frame turned to it.
SUB DrawHero (p AS Hero, cx AS INTEGER, cy AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER, j AS INTEGER, d AS INTEGER, v AS SINGLE, sy AS INTEGER, n AS INTEGER
    DIM f AS INTEGER, t AS INTEGER, sx AS INTEGER
    IF p.inv > 0 AND NOT p.stun AND (Ticks AND 4) THEN EXIT SUB
    x = CINT(p.x) - cx: y = CINT(p.y) - cy
    IF p.ground THEN v = p.gsp ELSE v = p.vx
    IF ABS(v) > 400 AND NOT p.dead THEN
        d = SGN(v)
        FOR j = 0 TO 3
            sy = y - 26 + (Ticks * 7 + j * 11) MOD 22
            n = 12 + (Ticks * 5 + j * 9) MOD 18
            LINE (x - d * 14, sy)-(x - d * (14 + n), sy), CWHITE
        NEXT
    END IF
    f = HeroFrame%(p)
    ' running, turned to the ground: t sixteenths of a turn, nose up
    IF p.ground AND f >= FRUN AND f < FBALL THEN
        v = p.ang
        IF p.face THEN v = -v
        t = CINT(v * 8 / PI)
        IF t < 0 THEN t = t + 16
        t = t AND 15
    END IF
    IF t = 0 THEN
        ' curled on the ground: its middle 13 off it, whichever way that is
        IF p.ground AND f >= FBALL AND f < FBALL + 4 THEN
            x = x + CINT(13 * GV(p.sg)): y = y + 13 - CINT(13 * GU(p.sg))
        END IF
        ' afraid, it shakes
        IF f = FHURT OR f = FSHIVER THEN x = x + ((Ticks \ 2) AND 1) * 2 - 1
        Blit x - 16, y - 31, IHERO + (p.face * NF + f) * 2, 32, 32
        EXIT SUB
    END IF
    ' turned about its middle, 12 off the ground: the picture as drawn (0) or mirrored to face left (1); past
    ' half a turn the one half a turn back upside down, mirrored (3) or not (2)
    IF f >= FDASH THEN f = 3 + f - FDASH ELSE f = (f - FRUN) \ 2
    j = p.face
    IF t >= 8 THEN t = t - 8: j = 3 - j
    x = x + CINT(12 * GV(p.sg)) - 16 + (j AND 1)
    y = y - CINT(12 * GU(p.sg)) - 18 - 5 * (j > 1)
    Blit x, y, ITILT + (j * 40 + t * 5 + f) * 2, 32, 32
END SUB

FUNCTION HeroFrame% (p AS Hero)
    IF p.dead OR p.stun THEN
        HeroFrame% = FHURT
        IF (Ticks \ 3) AND 1 THEN HeroFrame% = FSHIVER
    ELSEIF p.roll OR p.charging THEN
        HeroFrame% = FBALL + (INT(p.anim) AND 3)
    ELSEIF NOT p.ground THEN
        IF p.sprung THEN HeroFrame% = FUP ELSE HeroFrame% = FRUN + INT(p.anim) MOD 6
    ELSEIF p.skid THEN
        HeroFrame% = FSKID
    ELSEIF p.crouch THEN
        HeroFrame% = FCROUCH
    ELSEIF p.win THEN
        HeroFrame% = FWIN + ((Ticks \ 20) AND 1)
    ELSEIF ABS(p.gsp) < 5 THEN
        IF p.idleT > 3 THEN
            HeroFrame% = FTAP + ((Ticks \ 12) AND 1)
        ELSEIF Ticks MOD 180 < 8 THEN
            HeroFrame% = FBLINK
        ELSE
            HeroFrame% = FIDLE
        END IF
    ELSEIF ABS(p.gsp) >= TOP - 5 THEN
        HeroFrame% = FDASH + (INT(p.anim) AND 1)
    ELSE
        HeroFrame% = FRUN + INT(p.anim) MOD 6
    END IF
END FUNCTION

SUB DrawParts (pt() AS Part, cx AS INTEGER, cy AS INTEGER)
    DIM i AS INTEGER, x AS INTEGER, y AS INTEGER
    FOR i = 0 TO NPART - 1
        IF pt(i).t > 0 THEN
            x = CINT(pt(i).x) - cx: y = CINT(pt(i).y) - cy
            IF x >= 0 AND x < SW - 1 AND y >= 0 AND y < SH - 1 THEN
                PSET (x, y), pt(i).c
                IF pt(i).t > .25 THEN PSET (x + 1, y), pt(i).c
            END IF
        END IF
    NEXT
END SUB

' The score, time, bones and lives, top left over the scene: a picture
' and its mask, put together from the characters when a number moves.
' Called before the frame is drawn: it draws on the page's corner.
SUB Hud (g AS Game)
    DIM s AS INTEGER, sig AS LONG
    s = INT(g.clock)
    sig = g.score * 7 + g.bones * 131& + g.lives * 100003 + s * 13& + 1
    IF sig = g.sig THEN EXIT SUB
    g.sig = sig
    LINE (0, 0)-(HUDW - 1, HUDH - 1), 0, BF
    LINE (0, 100)-(HUDW - 1, 100 + HUDH - 1), 255, BF
    HudLine 0, "SCORE " + Num$(g.score, 6), 6
    HudLine 1, "TIME  " + LTRIM$(STR$(s \ 60)) + ":" + Num$(CLNG(s MOD 60), 2), 6
    HudLine 2, "BONES " + Num$(CLNG(g.bones), 3), 6
    HudLine 3, "CHARLIE x" + LTRIM$(STR$(g.lives)), 7
    GET (0, 0)-(HUDW - 1, HUDH - 1), Img(At(IHUD))
    GET (0, 100)-(HUDW - 1, 100 + HUDH - 1), Img(At(IHUD + 1))
END SUB

' A row of the HUD's picture and mask: s, its first n characters yellow.
SUB HudLine (row AS INTEGER, s AS STRING, n AS INTEGER)
    DIM i AS INTEGER, k AS INTEGER
    FOR i = 1 TO LEN(s)
        k = INSTR("0123456789:xABCDEHILMNOPRST", MID$(s, i, 1)) - 1
        IF k >= 0 THEN
            k = IGLYPH + (k - NGLYPH * (i > n)) * 2
            PUT (i * 9 - 9, row * 10), Img(At(k)), PSET
            PUT (i * 9 - 9, 100 + row * 10), Img(At(k + 1)), PSET
        END IF
    NEXT
END SUB

' What Charlie says, in a box over its head.
SUB Say (p AS Hero, cx AS INTEGER, cy AS INTEGER, s AS STRING)
    DIM row AS INTEGER, col AS INTEGER
    col = (CINT(p.x) - cx) \ 8 - LEN(s) \ 2 + 1
    row = (CINT(p.y) - cy - 42) \ 8
    IF col < 2 THEN col = 2
    IF col > 39 - LEN(s) THEN col = 39 - LEN(s)
    IF row < 3 THEN row = 3
    IF row > 23 THEN row = 23
    Panel col * 8 - 12, row * 8 - 12, (col + LEN(s)) * 8 - 5, row * 8 + 3
    Txt row, col, s, CWHITE
END SUB

' A dark box with a gold edge, for words: every other pixel of it, the scene through the rest.
SUB Panel (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER)
    DIM y AS INTEGER
    FOR y = y1 TO y2
        IF (x1 + y) AND 1 THEN LINE (x1, y)-(x2, y), CHUD, , &H5555 ELSE LINE (x1, y)-(x2, y), CHUD, , &HAAAA
    NEXT
    LINE (x1, y1)-(x2, y2), CGOLD + 2, B
END SUB

SUB Txt (row AS INTEGER, col AS INTEGER, s AS STRING, c AS INTEGER)
    COLOR c
    LOCATE row, col
    PRINT s;
END SUB

' s centred on row.
SUB Banner (row AS INTEGER, s AS STRING, c AS INTEGER)
    Txt row, 21 - LEN(s) \ 2, s, c
END SUB

' v in w digits, 0s first.
FUNCTION Num$ (v AS LONG, w AS INTEGER)
    Num$ = RIGHT$(STRING$(w, "0") + LTRIM$(STR$(v)), w)
END FUNCTION
