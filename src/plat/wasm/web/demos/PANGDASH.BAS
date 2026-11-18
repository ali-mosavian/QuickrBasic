' PANGOLIN DASH: Pip the pangolin rolls through the Amber Downs.
' Arrows run. Space, Z or Up jumps, curled. Down while running rolls;
' Down while still crouches: jump to wind up, let Down go to launch.
' M mutes; Esc leaves. SCREEN 13 on two pages: one drawn, one shown.
' Held keys come from the keyboard's port (60h); where it reads FFh,
' from INKEY$'s repeats. Physics in pixels and seconds, fixed steps.
DEFINT A-Z

TYPE Hero
    x AS SINGLE
    y AS SINGLE
    vx AS SINGLE
    vy AS SINGLE
    gsp AS SINGLE
    ang AS SINGLE
    charge AS SINGLE
    inv AS SINGLE
    anim AS SINGLE
    deadT AS SINGLE
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
    beads AS INTEGER
    lives AS INTEGER
    score AS LONG
    bonus AS LONG
    clock AS SINGLE
    camx AS SINGLE
    camy AS SINGLE
    modeT AS SINGLE
    homeX AS INTEGER
    homeY AS INTEGER
    page AS INTEGER
    quitting AS INTEGER
END TYPE

TYPE Bug
    x AS SINGLE
    y AS SINGLE
    vx AS SINGLE
    x0 AS INTEGER
    y0 AS INTEGER
    alive AS INTEGER
END TYPE

' A bead knocked loose: t its time left.
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
CONST SW = 320, SH = 200, TS = 16, MW = 320, MH = 20, NONE = 30000
CONST STEPT = 1! / 60!, PI = 3.141593
CONST GRAV = 650!, JUMPV = 290!, JUMPCUT = 150!, TOP = 240!
CONST RUNACC = 160!, DEC = 1300!, FRIC = 160!, AIRACC = 320!, SLOPE = 420!
CONST RFRIC = 70!, RDEC = 350!, RUPS = 260!, RDNS = 900!, MAXV = 720!
CONST SPRINGV = 470!, CHARGE0 = 380!, CHARGEMAX = 300!, BOUNCEV = 300!
CONST HW = 6, HH = 22
CONST NBEAD = 300, NLOOSE = 24, NBUG = 32, NSPRING = 16, NMARK = 4, NPART = 48
CONST TITLE = 0, PLAYING = 1, CLEARED = 2, OVER = 3
CONST FIDLE = 0, FRUN = 1, FBALL = 5, FHURT = 9, FSKID = 10, FUP = 11, FCROUCH = 12, NF = 13

' Images in Img from At(k); a masked one's mask at k + 1.
CONST IBG = 0, ITILE = 1, NVIS = 10, IHERO = ITILE + NVIS * 2
CONST IBEAD = IHERO + NF * 4, IBUG = IBEAD + 8, ISPRING = IBUG + 8
CONST ITITLE = ISPRING + 4, NIMG = ITITLE + 2, IMGSZ = 34000
CONST TITW = 200, TITH = 56

CONST SCLEFT = 75, SCRIGHT = 77, SCUP = 72, SCDOWN = 80, SCSPACE = 57, SCZ = 44
CONST SJUMP = 0, SBEAD = 1, SHURT = 2, SSPRING = 3, SPOP = 4, SROLL = 5
CONST SCHARGE = 6, SLAUNCH = 7, SCLEAR = 8, SDIE = 9, SMARK = 10

CONST CHUD = 0, CWHITE = 1, CINK = 2, CSKY = 16, NSKY = 32
CONST CFAR = 48, CFAR2 = 49, CHILL = 50, CHILL2 = 51, CHILL3 = 52, CCLOUD = 53, CCLOUD2 = 54
CONST CGRASS1 = 60, CGRASS2 = 61, CGRASS3 = 62
CONST CDIRT1 = 64, CDIRT2 = 65, CDIRT3 = 66, CDIRT4 = 67, CPEBBLE = 68
CONST CSCALE1 = 70, CSCALE2 = 71, CSCALE3 = 72, CSKIN = 73, CBELLY = 74
CONST CGOLD1 = 80, CGOLD2 = 81, CGOLD3 = 82, CGLINT = 83
CONST CSHELL = 84, CSHELL2 = 85, CSHELL3 = 86, CLEG = 87
CONST CRED = 88, CRED2 = 89, CMETAL = 90, CMETAL2 = 91
CONST CWOOD = 92, CWOOD2 = 93, CFLAME = 94, CYELLOW = 95

DECLARE SUB Main ()
DECLARE SUB OpenScreen ()
DECLARE SUB Flip (page AS INTEGER)
DECLARE SUB SetPal (n AS INTEGER, r AS INTEGER, g AS INTEGER, b AS INTEGER)
DECLARE SUB LoadPalette ()
DECLARE SUB LoadTables ()
DECLARE SUB LoadLevel ()
DECLARE SUB Column (c AS INTEGER, g AS INTEGER)
DECLARE SUB AddBead (x AS INTEGER, y AS INTEGER)
DECLARE SUB SortBeads ()
DECLARE FUNCTION VisOf% (tx AS INTEGER, ty AS INTEGER)
DECLARE SUB BuildArt ()
DECLARE SUB BuildBack ()
DECLARE SUB Cloud (x AS INTEGER, y AS INTEGER)
DECLARE SUB BuildTiles ()
DECLARE FUNCTION Earth% (i AS INTEGER, j AS INTEGER, v AS INTEGER)
DECLARE SUB BuildHero ()
DECLARE SUB DrawPip (f AS INTEGER)
DECLARE SUB PipBody (bob AS INTEGER, lb AS INTEGER, lf AS INTEGER, hurt AS INTEGER)
DECLARE SUB PipBall (r AS INTEGER)
DECLARE SUB BuildBits ()
DECLARE SUB BuildTitle ()
DECLARE SUB FillEll (cx AS INTEGER, cy AS INTEGER, rx AS INTEGER, ry AS INTEGER, c AS INTEGER)
DECLARE SUB Outline (w AS INTEGER, h AS INTEGER)
DECLARE SUB Grab (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, k AS INTEGER)
DECLARE SUB MaskGrab (x AS INTEGER, y AS INTEGER, w AS INTEGER, h AS INTEGER, mx AS INTEGER, my AS INTEGER, k AS INTEGER)
DECLARE SUB Sprite (w AS INTEGER, h AS INTEGER, k AS INTEGER, k2 AS INTEGER)
DECLARE SUB Blit (x AS INTEGER, y AS INTEGER, k AS INTEGER, w AS INTEGER, h AS INTEGER)
DECLARE SUB Poll ()
DECLARE SUB ReadPad (k AS Pad)
DECLARE FUNCTION Held2% (s AS INTEGER, t AS SINGLE)
DECLARE FUNCTION NextTime! (last AS SINGLE)
DECLARE FUNCTION Elapsed! (a AS SINGLE, b AS SINGLE)
DECLARE SUB Sfx (id AS INTEGER)
DECLARE FUNCTION Hgt% (tx AS INTEGER, ty AS INTEGER, i AS INTEGER)
DECLARE FUNCTION Surf% (sx AS INTEGER, y AS INTEGER)
DECLARE FUNCTION Solid% (x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION Wall% (p AS Hero, dir AS INTEGER)
DECLARE FUNCTION Hug% (p AS Hero)
DECLARE FUNCTION Land% (p AS Hero, d AS INTEGER)
DECLARE SUB Tick (g AS Game, p AS Hero, k AS Pad, lo() AS Loose, pt() AS Part)
DECLARE SUB StartGame (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
DECLARE SUB Spawn (g AS Game, p AS Hero)
DECLARE SUB MoveHero (p AS Hero, k AS Pad, pt() AS Part)
DECLARE SUB Walk (p AS Hero, l AS INTEGER, r AS INTEGER, d AS INTEGER, hit AS INTEGER, pt() AS Part)
DECLARE SUB Airborne (p AS Hero, l AS INTEGER, r AS INTEGER, d AS INTEGER, hold AS INTEGER)
DECLARE SUB Fric (v AS SINGLE, f AS SINGLE)
DECLARE SUB Touch (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
DECLARE SUB Hurt (g AS Game, p AS Hero, fromX AS SINGLE, lo() AS Loose)
DECLARE SUB Lose (p AS Hero)
DECLARE SUB Dying (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
DECLARE SUB MoveThings (g AS Game, lo() AS Loose, pt() AS Part)
DECLARE SUB Emit (pt() AS Part, x AS SINGLE, y AS SINGLE, vx AS SINGLE, vy AS SINGLE, t AS SINGLE, c AS INTEGER)
DECLARE SUB Burst (pt() AS Part, x AS SINGLE, y AS SINGLE, n AS INTEGER, v AS SINGLE, c AS INTEGER)
DECLARE SUB Camera (g AS Game, p AS Hero, snap AS INTEGER)
DECLARE FUNCTION GroundTop% (col AS INTEGER)
DECLARE SUB Render (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
DECLARE SUB DrawBack (cx AS INTEGER)
DECLARE SUB DrawTiles (cx AS INTEGER, cy AS INTEGER)
DECLARE SUB DrawProps (cx AS INTEGER, cy AS INTEGER)
DECLARE SUB DrawBeads (cx AS INTEGER, cy AS INTEGER, lo() AS Loose)
DECLARE SUB DrawBugs (cx AS INTEGER, cy AS INTEGER)
DECLARE SUB DrawHero (p AS Hero, cx AS INTEGER, cy AS INTEGER)
DECLARE FUNCTION HeroFrame% (p AS Hero)
DECLARE SUB DrawParts (pt() AS Part, cx AS INTEGER, cy AS INTEGER)
DECLARE SUB Hud (g AS Game)
DECLARE SUB Txt (row AS INTEGER, col AS INTEGER, s AS STRING, c AS INTEGER)
DECLARE SUB Banner (row AS INTEGER, s AS STRING, c AS INTEGER)
DECLARE FUNCTION Num$ (v AS LONG, w AS INTEGER)

DIM SHARED Map(MW - 1, MH - 1) AS INTEGER, Vis(MW - 1, MH - 1) AS INTEGER
DIM SHARED Prof(7, TS - 1) AS INTEGER
DIM SHARED Img(IMGSZ) AS LONG, At(NIMG) AS LONG, NextAt AS LONG
DIM SHARED BX(NBEAD - 1) AS INTEGER, BY(NBEAD - 1) AS INTEGER, BOn(NBEAD - 1) AS INTEGER
DIM SHARED First(MW) AS INTEGER, NBeads AS INTEGER
DIM SHARED Bugs(NBUG - 1) AS Bug, NBugs AS INTEGER
DIM SHARED SpX(NSPRING - 1) AS INTEGER, SpY(NSPRING - 1) AS INTEGER
DIM SHARED SpT(NSPRING - 1) AS SINGLE, NSprings AS INTEGER
DIM SHARED MkX(NMARK - 1) AS INTEGER, MkY(NMARK - 1) AS INTEGER
DIM SHARED MkOn(NMARK - 1) AS INTEGER, NMarks AS INTEGER
DIM SHARED Held(127) AS INTEGER, KeyT(127) AS SINGLE, LastScan AS INTEGER, PortOK AS INTEGER
DIM SHARED LW AS INTEGER, GoalX AS INTEGER, GoalY AS INTEGER
DIM SHARED StartX AS INTEGER, StartY AS INTEGER
DIM SHARED Ticks AS LONG, Muted AS INTEGER

Main
END

' The Amber Downs, left to right: a command and its count. g, the ground's
' height in tiles, starts at 4. plat and hb: height * 100 + length.
Level:
DATA flat,3, pip,0, flat,6, beads,5, flat,4, upg,2, flat,4, bug,0, flat,7
DATA dng,2, beads,4, flat,6, up45,2, flat,2, arc,5, gap,5, flat,4, bug,0, flat,6
DATA spring,0, flat,2, wall,5, flat,5, beads,5, flat,5, plat,304, flat,7
DATA mark,0, flat,3, dn45,3, dng,3, beads,8, flat,10, bug,0, flat,5, bug,0, flat,8
DATA up45,2, arc,7, gap,7, flat,6, drop,3, flat,6, bug,0, flat,5, plat,205, hb,403, flat,8
DATA upg,3, flat,3, spring,0, flat,2, wall,6, flat,4, dn45,4, dng,2, flat,3
DATA bug,0, beads,6, flat,9, mark,0, flat,4, gap,3, flat,3, gap,3, flat,3, bug,0, flat,6
DATA upg,2, up45,3, arc,8, gap,8, flat,5, dn45,5, dng,3, beads,10, flat,14
DATA bug,0, flat,3, bug,0, flat,3, bug,0, flat,6
DATA spring,0, flat,2, plat,506, hb,603, flat,10
DATA upg,1, flat,4, dng,1, beads,6, flat,8, goal,0, flat,14, "end",0

' Set up, then read input, step in fixed ticks and draw, till Esc on the
' title.
SUB Main
    DIM g AS Game, p AS Hero, k AS Pad
    DIM lo(NLOOSE - 1) AS Loose, pt(NPART - 1) AS Part
    DIM now AS SINGLE, last AS SINGLE, lag AS SINGLE

    OpenScreen
    LoadPalette
    LoadTables
    LoadLevel
    BuildArt
    g.page = 1
    g.mode = TITLE
    PLAY "MB"

    last = TIMER
    DO
        now = NextTime!(last)
        lag = lag + Elapsed!(last, now)
        last = now
        ReadPad k
        DO WHILE lag >= STEPT
            Tick g, p, k, lo(), pt()
            lag = lag - STEPT
        LOOP
        Render g, p, lo(), pt()
        Flip g.page
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

SUB SetPal (n AS INTEGER, r AS INTEGER, g AS INTEGER, b AS INTEGER)
    PALETTE n, r + 256& * g + 65536 * b
END SUB

SUB LoadPalette
    DIM i AS INTEGER, t AS SINGLE
    SetPal CHUD, 2, 2, 8
    SetPal CWHITE, 63, 63, 63
    SetPal CINK, 6, 4, 10
    FOR i = 0 TO NSKY - 1
        t = i / (NSKY - 1)
        SetPal CSKY + i, CINT(10 + 50 * t), CINT(20 + 32 * t), CINT(44 - 2 * t)
    NEXT
    SetPal CFAR, 30, 30, 44
    SetPal CFAR2, 40, 40, 52
    SetPal CHILL, 16, 34, 26
    SetPal CHILL2, 24, 44, 30
    SetPal CHILL3, 11, 25, 21
    SetPal CCLOUD, 62, 60, 58
    SetPal CCLOUD2, 50, 48, 52
    SetPal CGRASS1, 40, 60, 24
    SetPal CGRASS2, 24, 48, 14
    SetPal CGRASS3, 12, 33, 10
    SetPal CDIRT1, 44, 28, 14
    SetPal CDIRT2, 38, 23, 11
    SetPal CDIRT3, 30, 18, 9
    SetPal CDIRT4, 22, 13, 8
    SetPal CPEBBLE, 52, 42, 32
    SetPal CSCALE1, 52, 38, 21
    SetPal CSCALE2, 40, 27, 14
    SetPal CSCALE3, 25, 15, 8
    SetPal CSKIN, 58, 42, 36
    SetPal CBELLY, 61, 53, 42
    SetPal CGOLD1, 63, 54, 18
    SetPal CGOLD2, 59, 40, 6
    SetPal CGOLD3, 42, 24, 2
    SetPal CGLINT, 63, 63, 46
    SetPal CSHELL, 10, 40, 44
    SetPal CSHELL2, 4, 24, 30
    SetPal CSHELL3, 32, 58, 60
    SetPal CLEG, 6, 10, 12
    SetPal CRED, 58, 14, 10
    SetPal CRED2, 36, 6, 6
    SetPal CMETAL, 50, 52, 56
    SetPal CMETAL2, 28, 30, 36
    SetPal CWOOD, 38, 25, 12
    SetPal CWOOD2, 24, 14, 6
    SetPal CFLAME, 63, 42, 8
    SetPal CYELLOW, 63, 56, 20
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

' The ground's columns, then what stands on them, from the Level DATA.
SUB LoadLevel
    DIM cmd AS STRING, n AS INTEGER, c AS INTEGER, g AS INTEGER
    DIM i AS INTEGER, s AS INTEGER, tx AS INTEGER, ty AS INTEGER
    g = 4
    RESTORE Level
    DO
        READ cmd, n
        s = (MH - g) * TS
        SELECT CASE cmd
        CASE "flat"
            FOR i = 1 TO n: Column c, g: c = c + 1: NEXT
        CASE "up45"
            FOR i = 1 TO n
                Column c, g: Map(c, MH - g - 1) = 2: g = g + 1: c = c + 1
            NEXT
        CASE "dn45"
            FOR i = 1 TO n
                g = g - 1: Column c, g: Map(c, MH - g - 1) = 3: c = c + 1
            NEXT
        CASE "upg"
            FOR i = 1 TO n
                Column c, g: Map(c, MH - g - 1) = 4
                Column c + 1, g: Map(c + 1, MH - g - 1) = 5
                g = g + 1: c = c + 2
            NEXT
        CASE "dng"
            FOR i = 1 TO n
                g = g - 1
                Column c, g: Map(c, MH - g - 1) = 6
                Column c + 1, g: Map(c + 1, MH - g - 1) = 7
                c = c + 2
            NEXT
        CASE "gap"
            c = c + n
        CASE "wall"
            g = g + n
        CASE "drop"
            g = g - n
        CASE "plat"
            FOR i = 1 TO n MOD 100: Map(c + i, MH - g - n \ 100 - 1) = 1: NEXT
        CASE "beads"
            FOR i = 0 TO n - 1: AddBead (c + i) * TS + 8, s - 14: NEXT
        CASE "arc"
            FOR i = 0 TO n - 1
                AddBead (c + i) * TS + 8, s - 18 - CINT(44 * SIN(PI * (i + .5) / n))
            NEXT
        CASE "hb"
            FOR i = 1 TO n MOD 100: AddBead (c + i) * TS + 8, s - (n \ 100) * TS - 14: NEXT
        CASE "bug"
            Bugs(NBugs).x0 = c * TS + 8: Bugs(NBugs).y0 = s: NBugs = NBugs + 1
        CASE "spring"
            SpX(NSprings) = c * TS + 8: SpY(NSprings) = s: NSprings = NSprings + 1
        CASE "mark"
            MkX(NMarks) = c * TS + 8: MkY(NMarks) = s: NMarks = NMarks + 1
        CASE "pip"
            StartX = c * TS + 8: StartY = s
        CASE "goal"
            GoalX = c * TS + 8: GoalY = s
        END SELECT
    LOOP UNTIL cmd = "end"
    LW = c
    SortBeads
    FOR tx = 0 TO LW - 1
        FOR ty = 0 TO MH - 1
            Vis(tx, ty) = VisOf%(tx, ty)
        NEXT
    NEXT
END SUB

' Column c solid g tiles up from the bottom.
SUB Column (c AS INTEGER, g AS INTEGER)
    DIM r AS INTEGER
    FOR r = MH - g TO MH - 1
        Map(c, r) = 1
    NEXT
END SUB

SUB AddBead (x AS INTEGER, y AS INTEGER)
    BX(NBeads) = x: BY(NBeads) = y: BOn(NBeads) = -1
    NBeads = NBeads + 1
END SUB

' The beads by x, and First(c) the first at or past column c.
SUB SortBeads
    DIM i AS INTEGER, j AS INTEGER, x AS INTEGER, y AS INTEGER, c AS INTEGER
    FOR i = 1 TO NBeads - 1
        x = BX(i): y = BY(i): j = i - 1
        DO WHILE j >= 0
            IF BX(j) <= x THEN EXIT DO
            BX(j + 1) = BX(j): BY(j + 1) = BY(j): j = j - 1
        LOOP
        BX(j + 1) = x: BY(j + 1) = y
    NEXT
    i = 0
    FOR c = 0 TO MW
        DO WHILE i < NBeads
            IF BX(i) >= c * TS THEN EXIT DO
            i = i + 1
        LOOP
        First(c) = i
    NEXT
END SUB

' A tile's look: 0 none, 1 grass, 2 and 3 earth, 4 to 9 the slopes,
' 10 a ledge with air below.
FUNCTION VisOf% (tx AS INTEGER, ty AS INTEGER)
    DIM t AS INTEGER, above AS INTEGER, below AS INTEGER
    t = Map(tx, ty)
    IF t >= 2 THEN VisOf% = t + 2: EXIT FUNCTION
    IF t = 0 THEN VisOf% = 0: EXIT FUNCTION
    IF ty > 0 THEN above = Map(tx, ty - 1)
    below = 1
    IF ty < MH - 1 THEN below = Map(tx, ty + 1)
    IF above THEN
        VisOf% = 2 + ((tx + ty) AND 1)
    ELSEIF below THEN
        VisOf% = 1
    ELSE
        VisOf% = 10
    END IF
END FUNCTION

' Every picture, drawn on the hidden page and kept; the backdrop last, as
' it fills the page.
SUB BuildArt
    CLS
    BuildTiles
    BuildHero
    BuildBits
    BuildTitle
    BuildBack
END SUB

' The sky, clouds, far peaks and near hills, all of period SW so the
' backdrop wraps.
SUB BuildBack
    DIM x AS INTEGER, y AS INTEGER, m AS INTEGER, h AS INTEGER, k AS INTEGER
    DIM t AS SINGLE
    FOR y = 0 TO SH - 1
        k = y * NSKY \ 120
        IF k > NSKY - 1 THEN k = NSKY - 1
        LINE (0, y)-(SW - 1, y), CSKY + k
    NEXT
    FOR k = 0 TO 5
        x = k * 53 + 20: y = 18 + (k * 37) MOD 46
        Cloud x, y: Cloud x - SW, y: Cloud x + SW, y
    NEXT
    FOR x = 0 TO SW - 1
        t = 2 * PI * x / SW
        m = CINT(92 - 16 * SIN(2 * t + .7) - 9 * SIN(5 * t + 2.1) - 4 * SIN(11 * t))
        LINE (x, m)-(x, SH - 1), CFAR
        PSET (x, m), CFAR2
        h = CINT(122 - 10 * SIN(3 * t + .3) - 6 * SIN(7 * t + 1.3) - 2 * SIN(17 * t))
        LINE (x, h)-(x, SH - 1), CHILL
        LINE (x, h)-(x, h + 1), CHILL2
        LINE (x, h + 18 + CINT(3 * SIN(9 * t)))-(x, SH - 1), CHILL3
    NEXT
    Grab 0, 0, SW - 1, SH - 1, IBG
END SUB

SUB Cloud (x AS INTEGER, y AS INTEGER)
    FillEll x, y, 14, 5, CCLOUD
    FillEll x - 9, y + 1, 9, 4, CCLOUD
    FillEll x + 9, y + 2, 8, 3, CCLOUD
    LINE (x - 15, y + 5)-(x + 15, y + 5), CCLOUD2
END SUB

' Each look's 16 by 16: grass along a top's surface, earth below.
SUB BuildTiles
    DIM v AS INTEGER, t AS INTEGER, i AS INTEGER, j AS INTEGER
    DIM tp AS INTEGER, d AS INTEGER, c AS INTEGER, grass AS INTEGER
    FOR v = 1 TO NVIS
        LINE (0, 0)-(TS - 1, TS - 1), 0, BF
        t = 1
        IF v >= 4 AND v <= 9 THEN t = v - 2
        grass = v = 1 OR v >= 4
        FOR i = 0 TO TS - 1
            tp = TS - Prof(t, i)
            FOR j = tp TO TS - 1
                c = Earth%(i, j, v)
                d = j - tp
                IF grass THEN
                    IF d = 0 THEN
                        c = CGRASS1
                    ELSEIF d <= 2 THEN
                        c = CGRASS2
                    ELSEIF d = 3 OR (d = 4 AND i MOD 3) THEN
                        c = CGRASS3
                    END IF
                END IF
                IF v = 10 THEN
                    IF j >= 13 + (i * 5) MOD 3 THEN c = 0 ELSE IF j >= 11 THEN c = CDIRT4
                END IF
                PSET (i, j), c
            NEXT
        NEXT
        MaskGrab 0, 0, TS, TS, 80, 0, ITILE + (v - 1) * 2
    NEXT
END SUB

' Earth in wavy bands of period 8 down and TS across, so tiles meet.
FUNCTION Earth% (i AS INTEGER, j AS INTEGER, v AS INTEGER)
    DIM h AS INTEGER
    h = (i * 7 + j * 11 + v * 5) MOD 17
    IF h = 0 THEN
        Earth% = CPEBBLE
    ELSEIF h = 5 THEN
        Earth% = CDIRT3
    ELSE
        Earth% = CDIRT1 + ((j + CINT(1.5 * SIN(i * PI / 8))) \ 4) MOD 2
    END IF
END FUNCTION

' Pip's frames facing right, each kept with its mirror.
SUB BuildHero
    DIM f AS INTEGER
    FOR f = 0 TO NF - 1
        LINE (0, 0)-(23, 23), 0, BF
        DrawPip f
        Outline 24, 24
        Sprite 24, 24, IHERO + f * 2, IHERO + (NF + f) * 2
    NEXT
END SUB

SUB DrawPip (f AS INTEGER)
    DIM i AS INTEGER
    SELECT CASE f
    CASE FBALL TO FBALL + 3
        PipBall f - FBALL
    CASE FRUN TO FRUN + 3
        i = f - FRUN
        PipBody i MOD 2, 2 * (i = 0) - 2 * (i = 2), -2 * (i = 0) + 2 * (i = 2), 0
    CASE FHURT
        PipBody -1, 2, -2, -1
    CASE FSKID
        PipBody 1, 3, 3, 0
    CASE FUP
        PipBody -2, 0, 0, 0
    CASE FCROUCH
        PipBody 3, 0, 0, 0
    CASE ELSE
        PipBody 0, 0, 0, 0
    END SELECT
END SUB

' Pip standing: tail, legs, a scaled body, head and snout to the right.
' bob lifts or drops the body; lb, lf move the back and front feet.
SUB PipBody (bob AS INTEGER, lb AS INTEGER, lf AS INTEGER, hurt AS INTEGER)
    DIM i AS INTEGER, row AS INTEGER, k AS INTEGER, x AS INTEGER, y AS INTEGER
    DIM dx AS SINGLE, dy AS SINGLE
    FOR i = 0 TO 6
        LINE (2 + i, 20 + bob - i \ 2)-(2 + i, 20 + bob), CSCALE2
    NEXT
    LINE (7 + lb, 18 + bob)-(7 + lb, 21), CSCALE3
    LINE (6 + lb, 22)-(8 + lb, 22), CINK
    LINE (15 + lf, 18 + bob)-(15 + lf, 21), CSCALE3
    LINE (14 + lf, 22)-(16 + lf, 22), CINK
    FillEll 11, 14 + bob, 8, 6, CSCALE2
    FillEll 11, 12 + bob, 6, 3, CSCALE1
    FOR row = 0 TO 2
        FOR k = 0 TO 3
            x = 5 + k * 4 + (row AND 1) * 2: y = 10 + row * 3 + bob
            dx = (x + 1.5 - 11) / 8: dy = (y - 14 - bob) / 6
            IF dx * dx + dy * dy < .75 THEN
                PSET (x, y), CSCALE3: PSET (x + 1, y + 1), CSCALE3
                PSET (x + 2, y + 1), CSCALE3: PSET (x + 3, y), CSCALE3
            END IF
        NEXT
    NEXT
    LINE (6, 19 + bob)-(15, 19 + bob), CBELLY
    FillEll 18, 15 + bob, 3, 3, CSKIN
    LINE (20, 15 + bob)-(22, 16 + bob), CSKIN
    LINE (20, 17 + bob)-(21, 17 + bob), CSKIN
    PSET (22, 16 + bob), CINK
    LINE (16, 12 + bob)-(19, 12 + bob), CSCALE2
    IF hurt THEN
        PSET (18, 13 + bob), CINK: PSET (20, 15 + bob), CINK
        PSET (19, 14 + bob), CINK: PSET (18, 15 + bob), CINK: PSET (20, 13 + bob), CINK
    ELSE
        PSET (19, 14 + bob), CINK
        PSET (18, 13 + bob), CWHITE
    END IF
END SUB

' Pip curled, its plates turned r steps of 15 degrees.
SUB PipBall (r AS INTEGER)
    DIM k AS INTEGER, a AS SINGLE
    FillEll 12, 14, 8, 8, CSCALE2
    FillEll 10, 11, 5, 4, CSCALE1
    FOR k = 0 TO 5
        a = r * PI / 12 + k * PI / 3
        LINE (12 + CINT(3 * COS(a)), 14 + CINT(3 * SIN(a)))-(12 + CINT(8 * COS(a + .6)), 14 + CINT(8 * SIN(a + .6))), CSCALE3
    NEXT
    PSET (12, 14), CSKIN
END SUB

' Beads spinning, the beetle walking, the spring at rest and pressed.
SUB BuildBits
    DIM f AS INTEGER, k AS INTEGER, rx AS INTEGER, lx AS INTEGER, t AS INTEGER, y AS INTEGER
    FOR f = 0 TO 3
        LINE (0, 0)-(23, 23), 0, BF
        rx = 2 - f + 2 * (f = 3)
        FillEll 3, 3, rx, 2, CGOLD2
        PSET (3 + rx, 4), CGOLD3: PSET (3, 5), CGOLD3
        PSET (3 - rx \ 2, 2), CGLINT
        Outline 7, 7
        MaskGrab 0, 0, 7, 7, 80, 0, IBEAD + f * 2
    NEXT
    FOR f = 0 TO 1
        LINE (0, 0)-(23, 23), 0, BF
        FOR k = 0 TO 2
            lx = 4 + k * 3
            IF (k + f) AND 1 THEN LINE (lx, 9)-(lx - 1, 10), CLEG ELSE LINE (lx, 9)-(lx + 1, 10), CLEG
        NEXT
        FillEll 7, 5, 5, 4, CSHELL
        FillEll 6, 3, 3, 1, CSHELL3
        LINE (7, 2)-(7, 8), CSHELL2
        FillEll 12, 6, 2, 2, CSHELL2
        PSET (13, 5), CWHITE
        PSET (13, 3), CLEG: PSET (14, 2), CLEG
        Outline 16, 12
        Sprite 16, 12, IBUG + f * 2, IBUG + 4 + f * 2
    NEXT
    FOR f = 0 TO 1
        LINE (0, 0)-(23, 23), 0, BF
        t = 1 + 4 * f
        LINE (2, 8)-(13, 9), CMETAL2, BF
        FOR y = t + 2 TO 7 STEP 2
            LINE (4, y)-(11, y + 1), CMETAL
        NEXT
        LINE (1, t)-(14, t + 1), CRED, BF
        LINE (1, t + 1)-(14, t + 1), CRED2
        MaskGrab 0, 0, 16, 10, 80, 0, ISPRING + f * 2
    NEXT
END SUB

' The title, PRINTed small then drawn three times as large, in gold over
' a shadow.
SUB BuildTitle
    DIM x AS INTEGER, y AS INTEGER, pass AS INTEGER, c AS INTEGER, X0 AS INTEGER
    DIM Y0 AS INTEGER, line2 AS INTEGER
    LINE (0, 0)-(TITW - 1, TITH - 1), 0, BF
    COLOR CWHITE
    LOCATE 23, 1: PRINT "PANGOLIN";
    LOCATE 24, 1: PRINT "DASH";
    FOR pass = 0 TO 1
        FOR y = 0 TO 15
            line2 = y \ 8
            FOR x = 0 TO 63
                IF POINT(x, 176 + y) THEN
                    X0 = 4 + x * 3 + line2 * 48: Y0 = (y MOD 8) * 3 + line2 * 30
                    IF pass = 0 THEN
                        LINE (X0 + 2, Y0 + 2)-(X0 + 4, Y0 + 4), CINK, BF
                    ELSE
                        c = CGOLD2
                        IF y MOD 8 < 2 THEN c = CGOLD1
                        IF y MOD 8 > 4 THEN c = CGOLD3
                        LINE (X0, Y0)-(X0 + 2, Y0 + 2), c, BF
                    END IF
                END IF
            NEXT
        NEXT
    NEXT
    LINE (0, 176)-(63, 191), 0, BF
    MaskGrab 0, 0, TITW, TITH, 0, 100, ITITLE
END SUB

SUB FillEll (cx AS INTEGER, cy AS INTEGER, rx AS INTEGER, ry AS INTEGER, c AS INTEGER)
    DIM y AS INTEGER, w AS INTEGER, d AS SINGLE
    FOR y = -ry TO ry
        d = y / (ry + .5)
        w = CINT(rx * SQR(1 - d * d))
        LINE (cx - w, cy + y)-(cx + w, cy + y), c
    NEXT
END SUB

' Ink round what is drawn in the w by h at the page's corner.
SUB Outline (w AS INTEGER, h AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER, n AS INTEGER
    DIM o(w - 1, h - 1) AS INTEGER
    FOR y = 0 TO h - 1
        FOR x = 0 TO w - 1
            IF POINT(x, y) = 0 THEN
                n = 0
                IF x > 0 THEN IF POINT(x - 1, y) THEN n = -1
                IF x < w - 1 THEN IF POINT(x + 1, y) THEN n = -1
                IF y > 0 THEN IF POINT(x, y - 1) THEN n = -1
                IF y < h - 1 THEN IF POINT(x, y + 1) THEN n = -1
                o(x, y) = n
            END IF
        NEXT
    NEXT
    FOR y = 0 TO h - 1
        FOR x = 0 TO w - 1
            IF o(x, y) THEN PSET (x, y), CINK
        NEXT
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

' The page corner's w by h as k, and its mirror as k2.
SUB Sprite (w AS INTEGER, h AS INTEGER, k AS INTEGER, k2 AS INTEGER)
    DIM i AS INTEGER, j AS INTEGER
    MaskGrab 0, 0, w, h, 80, 0, k
    FOR j = 0 TO h - 1
        FOR i = 0 TO w - 1
            PSET (40 + w - 1 - i, j), POINT(i, j)
        NEXT
    NEXT
    MaskGrab 40, 0, w, h, 120, 0, k2
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

' The next TIMER value, waited for; the port read meanwhile.
FUNCTION NextTime! (last AS SINGLE)
    DIM t AS SINGLE
    DO
        Poll
        t = TIMER
    LOOP WHILE t = last
    NextTime! = t
END FUNCTION

FUNCTION Elapsed! (a AS SINGLE, b AS SINGLE)
    DIM d AS SINGLE
    d = b - a
    IF d < 0 OR d > .1 THEN d = .1
    Elapsed! = d
END FUNCTION

SUB Sfx (id AS INTEGER)
    IF Muted THEN EXIT SUB
    SELECT CASE id
    CASE SJUMP: PLAY "MBT255L64O3CEGO4C"
    CASE SBEAD: PLAY "MBT255L64O6EB"
    CASE SHURT: PLAY "MBT255L32O3GEC"
    CASE SSPRING: PLAY "MBT255L64O3CGO4EO5C"
    CASE SPOP: PLAY "MBT255L64O4GO3C"
    CASE SROLL: PLAY "MBT255L64O2CF"
    CASE SCHARGE: PLAY "MBT255L64O4CDE"
    CASE SLAUNCH: PLAY "MBT255L64O3GO4CGO5C"
    CASE SCLEAR: PLAY "MBT200L16O4CEGO5CL8C"
    CASE SDIE: PLAY "MBT200L16O3CO2BAGL8F"
    CASE SMARK: PLAY "MBT255L32O5CEG"
    END SELECT
END SUB

' Tile column i's solid height; past the ends a wall, below the level air.
FUNCTION Hgt% (tx AS INTEGER, ty AS INTEGER, i AS INTEGER)
    IF i < 0 OR tx < 0 OR tx >= LW THEN Hgt% = TS: EXIT FUNCTION
    IF ty < 0 OR ty >= MH THEN Hgt% = 0: EXIT FUNCTION
    Hgt% = Prof(Map(tx, ty), i)
END FUNCTION

' The ground's top at column sx near y: in y's tile, the one above when
' that is full, the one below when it is empty; NONE when both are.
FUNCTION Surf% (sx AS INTEGER, y AS INTEGER)
    DIM tx AS INTEGER, ty AS INTEGER, i AS INTEGER, h AS INTEGER
    tx = sx \ TS: i = sx MOD TS: ty = INT(y / TS)
    h = Hgt%(tx, ty, i)
    IF h = TS THEN
        Surf% = ty * TS - Hgt%(tx, ty - 1, i)
    ELSEIF h = 0 THEN
        h = Hgt%(tx, ty + 1, i)
        IF h = 0 THEN Surf% = NONE ELSE Surf% = (ty + 2) * TS - h
    ELSE
        Surf% = (ty + 1) * TS - h
    END IF
END FUNCTION

FUNCTION Solid% (x AS INTEGER, y AS INTEGER)
    DIM ty AS INTEGER
    ty = INT(y / TS)
    Solid% = y - ty * TS >= TS - Hgt%(x \ TS, ty, x MOD TS)
END FUNCTION

' Pip's side toward dir in a wall: pushed out, and true.
FUNCTION Wall% (p AS Hero, dir AS INTEGER)
    DIM n AS INTEGER, y AS INTEGER
    y = CINT(p.y)
    DO WHILE Solid%(CINT(p.x) + dir * HW, y - 9) OR Solid%(CINT(p.x) + dir * HW, y - HH + 4)
        p.x = p.x - dir
        n = n + 1
        IF n > TS THEN EXIT DO
    LOOP
    Wall% = n > 0
END FUNCTION

' Pip's feet onto the ground within reach, the ground's angle from the
' sensors either side; false when no ground is in reach.
FUNCTION Hug% (p AS Hero)
    DIM x AS INTEGER, y AS INTEGER, l AS INTEGER, c AS INTEGER, r AS INTEGER, lim AS INTEGER
    x = CINT(p.x): y = CINT(p.y)
    lim = 10 + CINT(ABS(p.gsp) * STEPT)
    l = Surf%(x - 5, y): c = Surf%(x, y): r = Surf%(x + 5, y)
    IF ABS(l - y) > lim THEN l = NONE
    IF ABS(c - y) > lim THEN c = NONE
    IF ABS(r - y) > lim THEN r = NONE
    IF l = NONE AND c = NONE AND r = NONE THEN Hug% = 0: EXIT FUNCTION
    IF c <> NONE THEN
        p.y = c
    ELSEIF l < r THEN
        p.y = l
    ELSE
        p.y = r
    END IF
    p.ang = 0
    IF l <> NONE AND r <> NONE THEN p.ang = ATN((l - r) / 10!)
    Hug% = -1
END FUNCTION

' Falling Pip's feet in the ground: on it, its speed along it kept.
FUNCTION Land% (p AS Hero, d AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER, l AS INTEGER, c AS INTEGER, r AS INTEGER
    x = CINT(p.x): y = CINT(p.y)
    l = Surf%(x - 5, y): c = Surf%(x, y): r = Surf%(x + 5, y)
    IF l > y OR l < y - 12 THEN l = NONE
    IF c > y OR c < y - 12 THEN c = NONE
    IF r > y OR r < y - 12 THEN r = NONE
    IF l = NONE AND c = NONE AND r = NONE THEN Land% = 0: EXIT FUNCTION
    IF c <> NONE THEN
        p.y = c
    ELSEIF l < r THEN
        p.y = l
    ELSE
        p.y = r
    END IF
    p.ang = 0
    IF l <> NONE AND r <> NONE THEN p.ang = ATN((l - r) / 10!)
    p.gsp = p.vx * COS(p.ang) - p.vy * SIN(p.ang)
    IF p.jumped AND NOT d THEN p.roll = 0
    p.ground = -1: p.jumped = 0: p.sprung = 0: p.stun = 0
    Land% = -1
END FUNCTION

SUB Tick (g AS Game, p AS Hero, k AS Pad, lo() AS Loose, pt() AS Part)
    DIM idle AS Pad, col AS INTEGER, t AS INTEGER
    Ticks = Ticks + 1
    g.modeT = g.modeT + STEPT
    IF k.mute THEN Muted = NOT Muted: k.mute = 0
    SELECT CASE g.mode
    CASE TITLE
        g.camx = g.camx + 40 * STEPT
        IF g.camx > LW * TS - SW THEN g.camx = 0
        col = (CINT(g.camx) + SW \ 2) \ TS
        t = GroundTop%(col) - 130
        IF t < 0 THEN t = 0
        IF t > MH * TS - SH THEN t = MH * TS - SH
        g.camy = g.camy + (t - g.camy) * 2 * STEPT
        IF k.quit THEN g.quitting = -1
        IF k.hit OR k.start THEN StartGame g, p, lo(), pt()
    CASE PLAYING
        IF k.quit THEN g.mode = TITLE: g.modeT = 0
        IF p.dead THEN
            Dying g, p, lo(), pt()
        ELSE
            g.clock = g.clock + STEPT
            MoveHero p, k, pt()
            Touch g, p, lo(), pt()
            IF p.y > MH * TS + 24 THEN Lose p
            IF p.x >= GoalX THEN
                g.mode = CLEARED: g.modeT = 0
                g.bonus = g.beads * 50& + 10& * (300 - INT(g.clock))
                IF g.bonus < g.beads * 50& THEN g.bonus = g.beads * 50&
                g.score = g.score + g.bonus
                Sfx SCLEAR
            END IF
            Camera g, p, 0
        END IF
        MoveThings g, lo(), pt()
    CASE CLEARED
        MoveHero p, idle, pt()
        MoveThings g, lo(), pt()
        Camera g, p, 0
        IF g.modeT > 6 OR (g.modeT > 1.5 AND (k.hit OR k.start)) OR k.quit THEN g.mode = TITLE: g.modeT = 0
    CASE OVER
        MoveThings g, lo(), pt()
        IF g.modeT > 5 OR (g.modeT > 1.5 AND (k.hit OR k.start)) OR k.quit THEN g.mode = TITLE: g.modeT = 0
    END SELECT
    k.hit = 0: k.start = 0: k.quit = 0
END SUB

SUB StartGame (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
    DIM i AS INTEGER
    g.mode = PLAYING: g.modeT = 0
    g.lives = 3: g.score = 0: g.beads = 0: g.clock = 0
    g.homeX = StartX: g.homeY = StartY
    FOR i = 0 TO NBeads - 1: BOn(i) = -1: NEXT
    FOR i = 0 TO NBugs - 1
        Bugs(i).x = Bugs(i).x0: Bugs(i).y = Bugs(i).y0
        Bugs(i).vx = -30: Bugs(i).alive = -1
    NEXT
    FOR i = 0 TO NMarks - 1: MkOn(i) = 0: NEXT
    FOR i = 0 TO NLOOSE - 1: lo(i).t = 0: NEXT
    FOR i = 0 TO NPART - 1: pt(i).t = 0: NEXT
    Spawn g, p
END SUB

SUB Spawn (g AS Game, p AS Hero)
    DIM fresh AS Hero
    p = fresh
    p.x = g.homeX: p.y = g.homeY: p.ground = -1
    g.beads = 0
    Camera g, p, -1
END SUB

SUB MoveHero (p AS Hero, k AS Pad, pt() AS Part)
    DIM l AS INTEGER, r AS INTEGER, d AS INTEGER, hit AS INTEGER, hold AS INTEGER
    DIM v AS SINGLE
    IF p.stun = 0 THEN l = k.l: r = k.r: d = k.d: hit = k.hit: hold = k.jump
    IF p.inv > 0 THEN p.inv = p.inv - STEPT
    IF p.ground THEN
        Walk p, l, r, d, hit, pt()
    ELSE
        Airborne p, l, r, d, hold
    END IF
    IF p.x < 8 THEN p.x = 8: p.gsp = 0: p.vx = 0
    IF p.x > LW * TS - 8 THEN p.x = LW * TS - 8: p.gsp = 0: p.vx = 0
    IF p.ground THEN v = ABS(p.gsp) ELSE v = ABS(p.vx)
    IF p.roll OR p.charging THEN
        IF p.charging THEN v = 300 + p.charge
        p.anim = p.anim + (v + 120) * STEPT / 7
    ELSE
        p.anim = p.anim + v * STEPT / 9
    END IF
END SUB

' On the ground: wind up, jump, roll, run; then along it, a few pixels a
' move, into walls and onto the ground's curve.
SUB Walk (p AS Hero, l AS INTEGER, r AS INTEGER, d AS INTEGER, hit AS INTEGER, pt() AS Part)
    DIM sn AS SINGLE, cs AS SINGLE, f AS SINGLE, n AS INTEGER, s AS INTEGER, dir AS INTEGER
    sn = SIN(p.ang): cs = COS(p.ang)
    dir = 1 - 2 * p.face
    IF p.charging THEN
        p.charge = p.charge * (1 - 2 * STEPT)
        IF hit THEN
            p.charge = p.charge + 110
            IF p.charge > CHARGEMAX THEN p.charge = CHARGEMAX
            Sfx SCHARGE
            Burst pt(), p.x - dir * 8, p.y - 2, 3, 60, CPEBBLE
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
        Fric p.gsp, RFRIC
        IF ABS(p.gsp) < 30 AND ABS(sn) < .2 THEN p.roll = 0
    ELSE
        IF p.gsp <> 0 OR ABS(sn) > .5 THEN p.gsp = p.gsp - SLOPE * sn * STEPT
        IF p.crouch THEN
            Fric p.gsp, FRIC * 4
        ELSEIF r THEN
            IF p.gsp < 0 THEN
                p.gsp = p.gsp + DEC * STEPT
                p.skid = p.gsp < -60
            ELSEIF p.gsp < TOP THEN
                p.gsp = p.gsp + RUNACC * STEPT
                IF p.gsp > TOP THEN p.gsp = TOP
            END IF
            IF p.gsp >= 0 THEN p.face = 0
        ELSEIF l THEN
            IF p.gsp > 0 THEN
                p.gsp = p.gsp - DEC * STEPT
                p.skid = p.gsp > 60
            ELSEIF p.gsp > -TOP THEN
                p.gsp = p.gsp - RUNACC * STEPT
                IF p.gsp < -TOP THEN p.gsp = -TOP
            END IF
            IF p.gsp <= 0 THEN p.face = 1
        ELSE
            Fric p.gsp, FRIC
        END IF
        IF d AND ABS(p.gsp) >= 60 THEN p.roll = -1: p.crouch = 0: Sfx SROLL
        IF p.skid AND Ticks MOD 4 = 0 THEN Emit pt(), p.x, p.y - 1, -p.gsp * .1, -30, .4, CPEBBLE
    END IF
    IF p.gsp > MAXV THEN p.gsp = MAXV
    IF p.gsp < -MAXV THEN p.gsp = -MAXV

    n = 1 + INT(ABS(p.gsp) * STEPT / 6)
    FOR s = 1 TO n
        cs = COS(p.ang): sn = SIN(p.ang)
        p.x = p.x + p.gsp * cs * STEPT / n
        p.y = p.y - p.gsp * sn * STEPT / n
        IF p.gsp <> 0 THEN IF Wall%(p, SGN(p.gsp)) THEN p.gsp = 0
        IF NOT Hug%(p) THEN
            p.ground = 0: p.vx = p.gsp * cs: p.vy = -p.gsp * sn
            EXIT FOR
        END IF
    NEXT
END SUB

' In the air: gravity, steering, a jump cut short when let go; then a few
' pixels a move, into walls, ceilings and onto the ground.
SUB Airborne (p AS Hero, l AS INTEGER, r AS INTEGER, d AS INTEGER, hold AS INTEGER)
    DIM n AS INTEGER, s AS INTEGER, m AS SINGLE, y AS INTEGER
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
    n = 1 + INT(m * STEPT / 6)
    FOR s = 1 TO n
        p.x = p.x + p.vx * STEPT / n
        IF p.vx <> 0 THEN IF Wall%(p, SGN(p.vx)) THEN p.vx = 0
        p.y = p.y + p.vy * STEPT / n
        IF p.vy < 0 THEN
            y = CINT(p.y) - HH
            IF Solid%(CINT(p.x), y) OR Solid%(CINT(p.x) - 4, y) OR Solid%(CINT(p.x) + 4, y) THEN
                p.y = (INT(y / TS) + 1) * TS + HH
                p.vy = 0
            END IF
        ELSEIF Land%(p, d) THEN
            EXIT FOR
        END IF
    NEXT
END SUB

' v toward 0 by f a second, not past it.
SUB Fric (v AS SINGLE, f AS SINGLE)
    IF v > 0 THEN
        v = v - f * STEPT: IF v < 0 THEN v = 0
    ELSEIF v < 0 THEN
        v = v + f * STEPT: IF v > 0 THEN v = 0
    END IF
END SUB

' Pip against beads, beetles, springs and posts.
SUB Touch (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
    DIM i AS INTEGER, c AS INTEGER, px AS INTEGER, py AS INTEGER
    px = CINT(p.x): py = CINT(p.y)
    c = (px - 16) \ TS
    IF c < 0 THEN c = 0
    i = First(c)
    DO WHILE i < NBeads
        IF BX(i) > px + 12 THEN EXIT DO
        IF BOn(i) THEN
            IF ABS(BX(i) - px) < 10 AND ABS(BY(i) - py + 11) < 14 THEN
                BOn(i) = 0: g.beads = g.beads + 1: g.score = g.score + 10
                Burst pt(), CSNG(BX(i)), CSNG(BY(i)), 4, 50, CGLINT
                Sfx SBEAD
            END IF
        END IF
        i = i + 1
    LOOP
    FOR i = 0 TO NLOOSE - 1
        IF lo(i).t > 0 AND lo(i).t < 3.3 THEN
            IF ABS(lo(i).x - px) < 10 AND ABS(lo(i).y - py + 11) < 14 THEN
                lo(i).t = 0: g.beads = g.beads + 1
                Sfx SBEAD
            END IF
        END IF
    NEXT
    FOR i = 0 TO NBugs - 1
        IF Bugs(i).alive THEN
            IF ABS(Bugs(i).x - px) < 13 AND ABS(Bugs(i).y - 5 - py + 11) < 14 THEN
                IF p.roll OR p.charging THEN
                    Bugs(i).alive = 0: g.score = g.score + 100
                    Burst pt(), Bugs(i).x, Bugs(i).y - 5, 10, 120, CSHELL3
                    Sfx SPOP
                    IF NOT p.ground AND p.vy > 0 THEN p.vy = -BOUNCEV
                ELSE
                    Hurt g, p, Bugs(i).x, lo()
                END IF
            END IF
        END IF
    NEXT
    FOR i = 0 TO NSprings - 1
        IF ABS(SpX(i) - px) < 10 AND py >= SpY(i) - 10 AND py <= SpY(i) + 2 THEN
            IF p.ground OR p.vy >= 0 THEN
                IF p.ground THEN p.vx = p.gsp
                p.ground = 0: p.vy = -SPRINGV: p.y = SpY(i) - 10
                p.roll = 0: p.jumped = 0: p.sprung = -1
                SpT(i) = .2
                Sfx SSPRING
            END IF
        END IF
    NEXT
    FOR i = 0 TO NMarks - 1
        IF NOT MkOn(i) AND px >= MkX(i) THEN
            MkOn(i) = -1: g.homeX = MkX(i): g.homeY = MkY(i)
            Burst pt(), CSNG(MkX(i)), CSNG(MkY(i) - 30), 8, 80, CFLAME
            Sfx SMARK
        END IF
    NEXT
END SUB

' Hit: the beads fly loose and Pip is knocked back; with none, it is out.
SUB Hurt (g AS Game, p AS Hero, fromX AS SINGLE, lo() AS Loose)
    DIM i AS INTEGER, n AS INTEGER, a AS SINGLE, v AS SINGLE
    IF p.inv > 0 THEN EXIT SUB
    IF g.beads = 0 THEN Lose p: EXIT SUB
    n = g.beads
    IF n > NLOOSE THEN n = NLOOSE
    g.beads = 0
    FOR i = 0 TO n - 1
        a = PI * (.15 + .7 * i / n)
        v = 220 - 60 * (i AND 1)
        lo(i).x = p.x: lo(i).y = p.y - 11
        lo(i).vx = COS(a) * v * (1 - 2 * (i AND 1)) * .8
        lo(i).vy = -SIN(a) * v
        lo(i).t = 4
    NEXT
    p.ground = 0: p.roll = 0: p.charging = 0: p.crouch = 0: p.jumped = 0
    p.stun = -1: p.inv = 2
    p.vy = -250
    IF p.x < fromX THEN p.vx = -130 ELSE p.vx = 130
    Sfx SHURT
END SUB

SUB Lose (p AS Hero)
    IF p.dead THEN EXIT SUB
    p.dead = -1: p.deadT = 0: p.vy = -320: p.ground = 0
    Sfx SDIE
END SUB

' Out: Pip rises and falls through everything; then the next life.
SUB Dying (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
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

' Beetles walk, turning at edges and walls; springs settle; loose beads
' bounce and fade; sparks fall.
SUB MoveThings (g AS Game, lo() AS Loose, pt() AS Part)
    DIM i AS INTEGER, x AS INTEGER, y AS INTEGER, s AS INTEGER, ahead AS INTEGER
    DIM nx AS SINGLE
    FOR i = 0 TO NBugs - 1
        IF Bugs(i).alive AND ABS(Bugs(i).x - g.camx - SW \ 2) < 400 THEN
            nx = Bugs(i).x + Bugs(i).vx * STEPT
            ahead = CINT(nx) + SGN(Bugs(i).vx) * 7
            y = CINT(Bugs(i).y)
            s = Surf%(ahead, y)
            IF ABS(s - y) > 6 OR Solid%(ahead, y - 6) THEN
                Bugs(i).vx = -Bugs(i).vx
            ELSE
                Bugs(i).x = nx
                s = Surf%(CINT(nx), y)
                IF ABS(s - y) <= 6 THEN Bugs(i).y = s
            END IF
        END IF
    NEXT
    FOR i = 0 TO NSprings - 1
        IF SpT(i) > 0 THEN SpT(i) = SpT(i) - STEPT
    NEXT
    FOR i = 0 TO NLOOSE - 1
        IF lo(i).t > 0 THEN
            lo(i).t = lo(i).t - STEPT
            lo(i).vy = lo(i).vy + GRAV * .6 * STEPT
            lo(i).x = lo(i).x + lo(i).vx * STEPT
            IF Solid%(CINT(lo(i).x), CINT(lo(i).y) - 3) THEN lo(i).vx = -lo(i).vx
            lo(i).y = lo(i).y + lo(i).vy * STEPT
            x = CINT(lo(i).x): y = CINT(lo(i).y) + 3
            s = Surf%(x, y)
            IF s <= y AND y - s < 12 AND lo(i).vy > 0 THEN
                lo(i).y = s - 3: lo(i).vy = -lo(i).vy * .7
            END IF
            IF lo(i).y > MH * TS + 16 THEN lo(i).t = 0
        END IF
    NEXT
    FOR i = 0 TO NPART - 1
        IF pt(i).t > 0 THEN
            pt(i).t = pt(i).t - STEPT
            pt(i).x = pt(i).x + pt(i).vx * STEPT
            pt(i).y = pt(i).y + pt(i).vy * STEPT
            pt(i).vy = pt(i).vy + 300 * STEPT
        END IF
    NEXT
END SUB

SUB Emit (pt() AS Part, x AS SINGLE, y AS SINGLE, vx AS SINGLE, vy AS SINGLE, t AS SINGLE, c AS INTEGER)
    DIM i AS INTEGER
    FOR i = 0 TO NPART - 1
        IF pt(i).t <= 0 THEN
            pt(i).x = x: pt(i).y = y: pt(i).vx = vx: pt(i).vy = vy
            pt(i).t = t: pt(i).c = c
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

' The view ahead of Pip's run, kept in the level; snap puts it there.
SUB Camera (g AS Game, p AS Hero, snap AS INTEGER)
    DIM tx AS SINGLE, ty AS SINGLE, v AS SINGLE
    IF p.ground THEN v = p.gsp ELSE v = p.vx
    v = v * .25
    IF v > 50 THEN v = 50
    IF v < -50 THEN v = -50
    tx = p.x - SW \ 2 + v
    ty = p.y - 120
    IF snap THEN
        g.camx = tx: g.camy = ty
    ELSE
        g.camx = g.camx + (tx - g.camx) * 6 * STEPT
        g.camy = g.camy + (ty - g.camy) * 5 * STEPT
        IF p.x - g.camx > 240 THEN g.camx = p.x - 240
        IF p.x - g.camx < 80 THEN g.camx = p.x - 80
        IF p.y - g.camy > 170 THEN g.camy = p.y - 170
        IF p.y - g.camy < 40 THEN g.camy = p.y - 40
    END IF
    IF g.camx < 0 THEN g.camx = 0
    IF g.camx > LW * TS - SW THEN g.camx = LW * TS - SW
    IF g.camy < 0 THEN g.camy = 0
    IF g.camy > MH * TS - SH THEN g.camy = MH * TS - SH
END SUB

' Column col's top, in pixels.
FUNCTION GroundTop% (col AS INTEGER)
    DIM ty AS INTEGER
    IF col >= LW THEN col = LW - 1
    FOR ty = 0 TO MH - 1
        IF Map(col, ty) THEN EXIT FOR
    NEXT
    GroundTop% = ty * TS
END FUNCTION

' Back to front, then the screen's words.
SUB Render (g AS Game, p AS Hero, lo() AS Loose, pt() AS Part)
    DIM cx AS INTEGER, cy AS INTEGER
    cx = INT(g.camx): cy = INT(g.camy)
    DrawBack cx
    DrawTiles cx, cy
    DrawProps cx, cy
    DrawBeads cx, cy, lo()
    IF g.mode <> TITLE THEN DrawBugs cx, cy: DrawHero p, cx, cy
    DrawParts pt(), cx, cy
    SELECT CASE g.mode
    CASE TITLE
        Blit (SW - TITW) \ 2, 30, ITITLE, TITW, TITH
        IF (Ticks \ 30) MOD 2 THEN Banner 13, "PRESS SPACE", CYELLOW
        Banner 17, "ARROWS RUN, SPACE JUMPS", CWHITE
        Banner 18, "DOWN ROLLS; STILL, DOWN+SPACE WINDS UP", CWHITE
        Banner 23, "M SOUND   ESC LEAVES", CWHITE
    CASE PLAYING
        Hud g
    CASE CLEARED
        Hud g
        Banner 8, "AMBER DOWNS CLEARED!", CYELLOW
        IF g.modeT > .8 THEN Banner 10, "BONUS " + Num$(g.bonus, 6), CWHITE
        IF g.modeT > 1.4 THEN Banner 12, "SCORE " + Num$(g.score, 6), CWHITE
    CASE OVER
        Hud g
        Banner 10, "GAME OVER", CYELLOW
    END SELECT
END SUB

' The backdrop, scrolled a quarter of the view, wrapping.
SUB DrawBack (cx AS INTEGER)
    DIM o AS INTEGER
    o = (cx \ 4) MOD SW
    PUT (-o, 0), Img(At(IBG)), PSET
    IF o THEN PUT (SW - o, 0), Img(At(IBG)), PSET
END SUB

SUB DrawTiles (cx AS INTEGER, cy AS INTEGER)
    DIM tx AS INTEGER, ty AS INTEGER, tx0 AS INTEGER, ty0 AS INTEGER, tx1 AS INTEGER, ty1 AS INTEGER
    DIM x AS INTEGER, y AS INTEGER, v AS INTEGER, k AS INTEGER
    tx0 = cx \ TS: ty0 = cy \ TS
    tx1 = tx0 + SW \ TS: ty1 = ty0 + SH \ TS + 1
    IF tx1 > LW - 1 THEN tx1 = LW - 1
    IF ty1 > MH - 1 THEN ty1 = MH - 1
    FOR ty = ty0 TO ty1
        y = ty * TS - cy
        FOR tx = tx0 TO tx1
            v = Vis(tx, ty)
            IF v THEN
                x = tx * TS - cx
                k = ITILE + (v - 1) * 2
                IF v <= 3 THEN
                    PUT (x, y), Img(At(k)), PSET
                ELSE
                    PUT (x, y), Img(At(k + 1)), AND
                    PUT (x, y), Img(At(k)), OR
                END IF
            END IF
        NEXT
    NEXT
END SUB

' Posts, the goal's spinning disc, springs.
SUB DrawProps (cx AS INTEGER, cy AS INTEGER)
    DIM i AS INTEGER, x AS INTEGER, y AS INTEGER, c AS INTEGER, rx AS INTEGER
    FOR i = 0 TO NMarks - 1
        x = MkX(i) - cx: y = MkY(i) - cy
        IF x > -8 AND x < SW + 8 THEN
            LINE (x - 1, y - 28)-(x, y - 1), CMETAL2, BF
            c = CMETAL
            IF MkOn(i) THEN c = CFLAME
            FillEll x, y - 31, 3, 3, c
        END IF
    NEXT
    x = GoalX - cx: y = GoalY - cy
    IF x > -16 AND x < SW + 16 THEN
        LINE (x - 1, y - 34)-(x + 1, y - 1), CWOOD2, BF
        rx = CINT(ABS(COS(Ticks * .06)) * 9)
        FillEll x, y - 44, rx + 1, 10, CGOLD3
        IF rx > 1 THEN FillEll x, y - 44, rx - 1, 8, CGOLD2
        PSET (x - rx \ 2, y - 48), CGLINT
    END IF
    FOR i = 0 TO NSprings - 1
        x = SpX(i) - 8 - cx
        IF SpT(i) > 0 THEN c = 2 ELSE c = 0
        Blit x, SpY(i) - 10 - cy, ISPRING + c, 16, 10
    NEXT
END SUB

SUB DrawBeads (cx AS INTEGER, cy AS INTEGER, lo() AS Loose)
    DIM i AS INTEGER, c AS INTEGER, k AS INTEGER
    c = cx \ TS
    IF c > MW THEN c = MW
    k = IBEAD + ((Ticks \ 6) MOD 4) * 2
    i = First(c)
    DO WHILE i < NBeads
        IF BX(i) > cx + SW + 4 THEN EXIT DO
        IF BOn(i) THEN Blit BX(i) - 3 - cx, BY(i) - 3 - cy, k, 7, 7
        i = i + 1
    LOOP
    FOR i = 0 TO NLOOSE - 1
        IF lo(i).t > 1 OR (lo(i).t > 0 AND (Ticks \ 3) MOD 2) THEN
            Blit CINT(lo(i).x) - 3 - cx, CINT(lo(i).y) - 3 - cy, k, 7, 7
        END IF
    NEXT
END SUB

SUB DrawBugs (cx AS INTEGER, cy AS INTEGER)
    DIM i AS INTEGER, k AS INTEGER
    FOR i = 0 TO NBugs - 1
        IF Bugs(i).alive THEN
            k = IBUG + ((Ticks \ 8) MOD 2) * 2
            IF Bugs(i).vx < 0 THEN k = k + 4
            Blit CINT(Bugs(i).x) - 8 - cx, CINT(Bugs(i).y) - 11 - cy, k, 16, 12
        END IF
    NEXT
END SUB

SUB DrawHero (p AS Hero, cx AS INTEGER, cy AS INTEGER)
    DIM f AS INTEGER
    IF p.inv > 0 AND NOT p.stun AND (Ticks \ 4) MOD 2 THEN EXIT SUB
    f = HeroFrame%(p)
    Blit CINT(p.x) - 12 - cx, CINT(p.y) - 23 - cy, IHERO + (p.face * NF + f) * 2, 24, 24
END SUB

FUNCTION HeroFrame% (p AS Hero)
    IF p.dead OR p.stun THEN
        HeroFrame% = FHURT
    ELSEIF p.roll OR p.charging THEN
        HeroFrame% = FBALL + INT(p.anim) MOD 4
    ELSEIF NOT p.ground THEN
        IF p.sprung THEN HeroFrame% = FUP ELSE HeroFrame% = FRUN + INT(p.anim) MOD 4
    ELSEIF p.skid THEN
        HeroFrame% = FSKID
    ELSEIF p.crouch THEN
        HeroFrame% = FCROUCH
    ELSEIF ABS(p.gsp) < 5 THEN
        HeroFrame% = FIDLE
    ELSE
        HeroFrame% = FRUN + INT(p.anim) MOD 4
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

SUB Hud (g AS Game)
    DIM c AS INTEGER, s AS INTEGER
    LINE (0, 0)-(SW - 1, 8), CHUD, BF
    c = CWHITE
    IF g.beads = 0 AND (Ticks \ 20) MOD 2 THEN c = CRED
    Txt 1, 2, "BEADS", CYELLOW
    Txt 1, 8, Num$(CLNG(g.beads), 3), c
    s = INT(g.clock)
    Txt 1, 13, "TIME", CYELLOW
    Txt 1, 18, LTRIM$(STR$(s \ 60)) + ":" + Num$(CLNG(s MOD 60), 2), CWHITE
    Txt 1, 24, "SCORE", CYELLOW
    Txt 1, 30, Num$(g.score, 6), CWHITE
    Txt 1, 37, "x" + LTRIM$(STR$(g.lives)), CYELLOW
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
