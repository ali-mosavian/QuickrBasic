' MOTHLIGHT: a little moth's big day out, through the standing stones.
' Space, Enter or Up flaps; M turns the music off and on; Esc quits. Driven by TIMER: the same speed on any machine.
' SCREEN 13's 256 colours on two pages: one is drawn while the other is shown.
DECLARE SUB Setup ()
DECLARE SUB BuildGlyphs ()
DECLARE SUB BuildMoth ()
DECLARE SUB NewGame ()
DECLARE SUB Flap ()
DECLARE SUB Die ()
DECLARE SUB Tick (flapped AS INTEGER)
DECLARE SUB Render ()
DECLARE SUB DrawSky ()
DECLARE SUB DrawSun ()
DECLARE SUB DrawHills ()
DECLARE SUB DrawStone (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER, capTop AS INTEGER, kind AS INTEGER)
DECLARE SUB DrawLake ()
DECLARE SUB DrawSparks ()
DECLARE SUB DrawMoth ()
DECLARE SUB DrawPanel ()
DECLARE SUB Emit (n AS INTEGER, spread AS SINGLE, kick AS SINGLE)
DECLARE SUB Txt (t AS STRING, x AS INTEGER, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, sh AS INTEGER)
DECLARE SUB TxtC (t AS STRING, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, sh AS INTEGER)
DECLARE SUB Music ()
DECLARE SUB StartSong (id AS INTEGER)
DECLARE SUB SetPalette ()
DECLARE SUB Disc (cx AS INTEGER, cy AS INTEGER, r AS INTEGER, c AS INTEGER)
DECLARE SUB Dither (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, c AS INTEGER, level AS INTEGER)
DECLARE FUNCTION Mix& (a AS LONG, b AS LONG, t AS SINGLE)
DECLARE FUNCTION NextGap% (prev AS INTEGER)
DECLARE FUNCTION Hit% ()
DECLARE FUNCTION RectHit% (x1 AS SINGLE, y1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE)
DECLARE FUNCTION Ell! (px AS SINGLE, py AS SINGLE, cx AS SINGLE, cy AS SINGLE, ang AS SINGLE, a AS SINGLE, b AS SINGLE)
DECLARE FUNCTION SegD! (px AS SINGLE, py AS SINGLE, ax AS SINGLE, ay AS SINGLE, bx AS SINGLE, by AS SINGLE)

CONST SW = 320, SH = 200, GROUNDY = 184, LAKEH = 16, HOMEX = 85, ASPECT = 1.2!
CONST NSTONE = 4, STONEW = 29, OVERHANG = 3, CAPH = 7, GAP = 66, SPACING = 124, NKIND = 5
CONST SPRW = 34, SPRH = 30, SPRSZ = 256, MSCALE = .62!, NSPR = 14, NSPARK = 64
CONST GRAV = 660!, FLAPV = -190!, SPEED = 72!, STEPT = 1! / 120!
CONST TITLE = 0, PLAYING = 1, DYING = 2, OVER = 3
CONST NOSONG = 0, TITLESONG = 1, GAMESONG = 2, TITLEBEATS = 32, GAMEBEATS = 64
CONST SUNX = 258, SUNY = 58, NFLY = 10, NCLOUD = 3, DAY = 0, GOLDEN = 1, SUNSET = 2, NKEY = 15
' The colours. The lake ORs 64 into what it mirrors, so 0-15 and 128-191 have water-tinted copies
' 64 higher; the rest are only drawn above the lake's reach.
CONST CINK = 1, CWHITE = 2, CWING = 3, CWEDGE = 4, CBODY = 5, CSTRIPE = 6, CCOLLAR = 7, CHIND = 8, CHINDE = 9
CONST CSPARK = 10, CSKY = 16, NSKY = 48, CTITLE = 80, CPANEL = 88, CPANEL2 = 89
CONST CSUN = 90, CRAY = 91, CCHEEK = 92, CCLOUD = 93, CCLOUDLO = 94, CMASK = 255
CONST CMTN = 128, CFAR = 144, CNEAR = 160, CTREE = 176, CSTONE = 180, CMOSS = 188, CRUNE = 189, CSHROOM = 190
CONST CVINE = 191, WATER = 64

'$DYNAMIC
DIM SHARED state AS INTEGER, score AS INTEGER, best AS INTEGER, newBest AS INTEGER, drawPage AS INTEGER
DIM SHARED mothX AS SINGLE, mothY AS SINGLE, vy AS SINGLE, scroll AS SINGLE, clock AS SINGLE, phase AS SINGLE
DIM SHARED boost AS SINGLE, flash AS SINGLE, stateT AS SINGLE, sparkT AS SINGLE, behind AS INTEGER
DIM SHARED squashT AS SINGLE, popT AS SINGLE, shakeT AS SINGLE, palT AS SINGLE
DIM SHARED stoneX(NSTONE - 1) AS SINGLE, stoneGap(NSTONE - 1) AS INTEGER, stoneDone(NSTONE - 1) AS INTEGER
DIM SHARED stoneKind(NSTONE - 1) AS INTEGER, StoneCol(STONEW + 2 * OVERHANG) AS INTEGER
DIM SHARED MtnTop(639) AS INTEGER, FarTop(639) AS INTEGER, NearTop(639) AS INTEGER
DIM SHARED TreeX(15) AS INTEGER, TreeH(15) AS INTEGER
DIM SHARED FlyX(NFLY - 1) AS INTEGER, FlyY(NFLY - 1) AS INTEGER, FlyPh(NFLY - 1) AS SINGLE
DIM SHARED CloudX(NCLOUD - 1) AS INTEGER, CloudY(NCLOUD - 1) AS INTEGER
DIM SHARED Rune(3) AS STRING, sky AS INTEGER, fade AS SINGLE, fadeTarget AS SINGLE
DIM SHARED toTitle AS INTEGER, quitting AS INTEGER
DIM SHARED Rgb(255) AS LONG, SkyKey(2, NKEY - 1) AS LONG, CurKey(NKEY - 1, 2) AS SINGLE
DIM SHARED Glyph(126, 7) AS INTEGER, Bit(7) AS INTEGER
DIM SHARED Spr(SPRSZ * NSPR * 2 - 1) AS LONG, Strip(80) AS LONG, Tinter(SW * LAKEH \ 4) AS LONG, Frame(SW * SH \ 4) AS LONG
DIM SHARED Tune(1 TO 2, GAMEBEATS - 1) AS STRING, songLen(1 TO 2) AS INTEGER
DIM SHARED song AS INTEGER, beat AS INTEGER, musicOn AS INTEGER, cue AS STRING
DIM SHARED sx(NSPARK - 1) AS SINGLE, sy(NSPARK - 1) AS SINGLE
DIM SHARED svx(NSPARK - 1) AS SINGLE, svy(NSPARK - 1) AS SINGLE, slife(NSPARK - 1) AS SINGLE

DIM t AS SINGLE, lastT AS SINGLE, dt AS SINGLE, acc AS SINGLE
DIM flapped AS INTEGER, k AS STRING, i AS INTEGER, j AS INTEGER, r AS LONG, g AS LONG, b AS LONG

' The songs, a beat to a string: the tune on the beat, then two notes of its chord.
songLen(GAMESONG) = GAMEBEATS
RESTORE GameTune
FOR i = 0 TO GAMEBEATS - 1: READ Tune(GAMESONG, i): NEXT
songLen(TITLESONG) = TITLEBEATS
RESTORE TitleTune
FOR i = 0 TO TITLEBEATS - 1: READ Tune(TITLESONG, i): NEXT
musicOn = -1
RESTORE Skies
FOR i = 0 TO 2
    FOR j = 0 TO NKEY - 1
        READ r, g, b
        SkyKey(i, j) = r + g * 256 + b * 65536
    NEXT
NEXT
RESTORE Fixed
DO
    READ i
    IF i < 0 THEN EXIT DO
    READ r, g, b
    Rgb(i) = r + g * 256 + b * 65536
LOOP

Setup
BuildGlyphs
BuildMoth
PLAY "MB"
StartSong TITLESONG

lastT = TIMER
DO
    DO: t = TIMER: LOOP WHILE t = lastT
    dt = t - lastT
    lastT = t
    IF dt < 0 OR dt > .1 THEN dt = .1

    flapped = 0
    DO
        k = INKEY$
        SELECT CASE k
        CASE " ", CHR$(13), CHR$(0) + "H"
            flapped = -1
        CASE "m", "M"
            musicOn = NOT musicOn
            IF musicOn = 0 THEN SOUND 37, 0
        CASE CHR$(27)
            IF state = TITLE THEN quitting = -1 ELSE toTitle = -1
            fadeTarget = 0
        END SELECT
    LOOP WHILE LEN(k)

    acc = acc + dt
    DO WHILE acc >= STEPT
        Tick flapped
        flapped = 0
        acc = acc - STEPT
    LOOP
    Music
    Render
LOOP UNTIL quitting AND fade <= 0

SOUND 37, 0
SCREEN 0
WIDTH 80
END

' The skies' key colours as red, green, blue from 0 to 255: sky top, middle, low; cloud, its shade;
' mountains; far hills top, low; near hills top, low; trees light, dark; water; stone light, dark.
Skies:
DATA 30,110,240, 90,170,255, 190,235,255, 255,255,255, 205,220,245, 120,160,225
DATA 140,215,120, 70,160,90, 90,205,80, 40,130,50, 50,160,70, 20,100,45, 40,140,230, 255,215,150, 170,100,60
DATA 70,90,220, 170,140,240, 255,215,160, 255,250,235, 240,200,190, 190,150,200
DATA 200,200,110, 130,140,70, 140,200,70, 70,120,40, 80,150,60, 40,90,40, 80,120,200, 255,200,120, 160,90,50
DATA 90,50,170, 240,110,170, 255,200,120, 255,220,230, 230,150,190, 170,100,170
DATA 200,120,160, 120,70,120, 110,170,120, 50,100,80, 60,120,80, 30,70,60, 120,80,180, 255,170,120, 140,70,60
' The colours that stay: index, red, green, blue.
Fixed:
DATA 1,25,25,70, 2,255,255,255, 3,255,226,80, 4,255,130,30, 5,150,80,40, 6,255,170,70
DATA 7,255,250,235, 8,230,120,50, 9,150,60,30, 10,255,255,255, 11,255,240,140, 12,255,170,200
DATA 88,60,110,220, 89,255,230,120, 90,255,236,90, 91,255,200,60, 92,255,150,150
DATA 188,100,210,70, 189,120,255,255, 190,240,60,60, 191,60,160,60, -1

GameTune:
DATA "T160MNO4E8O3C16O3G16", "T160MNO4G8O3C16O3G16", "T160MNO5C8O3C16O3G16", "T160MNO4G8O3C16O3G16"
DATA "T160MNO4D8O2G16O3D16", "T160MNO4G8O2G16O3D16", "T160MNO4B8O2G16O3D16", "T160MNO4G8O2G16O3D16"
DATA "T160MNO4C8O2A16O3E16", "T160MNO4E8O2A16O3E16", "T160MNO4A8O2A16O3E16", "T160MNO4E8O2A16O3E16"
DATA "T160MNO4F8O2F16O3C16", "T160MNO4A8O2F16O3C16", "T160MNO5C8O2F16O3C16", "T160MNO4A8O2F16O3C16"
DATA "T160MNO4G8O3C16O3G16", "T160MNO4E8O3C16O3G16", "T160MNO4C8O3C16O3G16", "T160MNO4E8O3C16O3G16"
DATA "T160MNO3B8O2G16O3D16", "T160MNO4D8O2G16O3D16", "T160MNO4G8O2G16O3D16", "T160MNO4D8O2G16O3D16"
DATA "T160MNO3A8O2F16O3C16", "T160MNO4C8O2F16O3C16", "T160MNO4F8O2F16O3C16", "T160MNO4A8O2F16O3C16"
DATA "T160MNO4G8O2G16O3D16", "T160MNO4F8O2G16O3D16", "T160MNO4E8O2G16O3D16", "T160MNO4D8O2G16O3D16"
DATA "T160MNO4C8O2F16O3C16", "T160MNO4F8O2F16O3C16", "T160MNO4A8O2F16O3C16", "T160MNO4F8O2F16O3C16"
DATA "T160MNO4D8O2G16O3D16", "T160MNO4G8O2G16O3D16", "T160MNO4B8O2G16O3D16", "T160MNO4G8O2G16O3D16"
DATA "T160MNO4E8O2E16O2B16", "T160MNO4G8O2E16O2B16", "T160MNO4B8O2E16O2B16", "T160MNO4G8O2E16O2B16"
DATA "T160MNO4A8O2A16O3E16", "T160MNO4G8O2A16O3E16", "T160MNO4E8O2A16O3E16", "T160MNO4C8O2A16O3E16"
DATA "T160MNO4F8O2F16O3C16", "T160MNO4E8O2F16O3C16", "T160MNO4F8O2F16O3C16", "T160MNO4A8O2F16O3C16"
DATA "T160MNO4G8O2G16O3D16", "T160MNO4A8O2G16O3D16", "T160MNO4B8O2G16O3D16", "T160MNO5D8O2G16O3D16"
DATA "T160MNO5C8O3C16O3G16", "T160MNO4G8O3C16O3G16", "T160MNO4E8O3C16O3G16", "T160MNO4G8O3C16O3G16"
DATA "T160MNO5C8O3C16O3G16", "T160MNO4E8O3C16O3G16", "T160MNO4G8O3C16O3G16", "T160MNO5C8O3C16O3G16"
TitleTune:
DATA "T120MSO3C16O3G16MLO4E8", "T120MSO3C16O3G16MLO4G8", "T120MSO3C16O3G16MLO5C8", "T120MSO3C16O3G16MLO4G8"
DATA "T120MSO2F16O3C16MLO4A8", "T120MSO2F16O3C16MLO4G8", "T120MSO2F16O3C16MLO4F8", "T120MSO2F16O3C16MLO4A8"
DATA "T120MSO2G16O3D16MLO4G8", "T120MSO2G16O3D16MLO4B8", "T120MSO2G16O3D16MLO5D8", "T120MSO2G16O3D16MLO4B8"
DATA "T120MSO3C16O3G16MLO5C8", "T120MSO3C16O3G16MLO4G8", "T120MSO3C16O3G16MLO4E8", "T120MSO3C16O3G16MLO4C8"
DATA "T120MSO2A16O3E16MLO4C8", "T120MSO2A16O3E16MLO4E8", "T120MSO2A16O3E16MLO4A8", "T120MSO2A16O3E16MLO4E8"
DATA "T120MSO2F16O3C16MLO4F8", "T120MSO2F16O3C16MLO4A8", "T120MSO2F16O3C16MLO5C8", "T120MSO2F16O3C16MLO4A8"
DATA "T120MSO2G16O3D16MLO4B8", "T120MSO2G16O3D16MLO4A8", "T120MSO2G16O3D16MLO4G8", "T120MSO2G16O3D16MLO4F8"
DATA "T120MSO3C16O3G16MLO4E8", "T120MSO3C16O3G16MLO4D8", "T120MSO3C16O3G16MLO4C8", "T120MSO3C16O3G16MLO4C8"
REM $STATIC
SUB BuildGlyphs
    DIM c AS INTEGER, r AS INTEGER, x AS INTEGER, b AS INTEGER
    COLOR CWHITE
    FOR c = 32 TO 126
        LOCATE 1, 1
        PRINT CHR$(c);
        FOR r = 0 TO 7
            b = 0
            FOR x = 0 TO 7
                IF POINT(x, r) THEN b = b OR Bit(x)
            NEXT
            Glyph(c, r) = b
        NEXT
    NEXT
END SUB

' Each sprite is drawn pixel by pixel from ellipses in the moth's own frame, turned by its tilt:
' 3 wing positions by 4 tilts, then squashed (just flapped) and stretched (diving);
' each with a mask for PUT AND, then OR.
SUB BuildMoth
    DIM g(SPRW + 1, SPRH + 1) AS INTEGER
    DIM tl(3) AS SINGLE, wa(2) AS SINGLE, wl(2) AS SINGLE, ww(2) AS SINGLE
    DIM t AS INTEGER, w AS INTEGER, px AS INTEGER, py AS INTEGER, c AS INTEGER, k AS INTEGER
    DIM ct AS SINGLE, st AS SINGLE, fx AS SINGLE, fy AS SINGLE, ha AS SINGLE, hx AS SINGLE, hy AS SINGLE
    DIM wx AS SINGLE, wy AS SINGLE, lx AS SINGLE, ly AS SINGLE, d AS SINGLE, e AS SINGLE
    DIM sw AS SINGLE, sh AS SINGLE
    tl(0) = -.4: tl(1) = 0: tl(2) = .45: tl(3) = 1
    wa(0) = -1.85: wl(0) = 14: ww(0) = 7.5
    wa(1) = -2.55: wl(1) = 8: ww(1) = 7.5
    wa(2) = 1.75: wl(2) = 12: ww(2) = 6.5

    FOR k = 0 TO NSPR - 1
        sw = 1: sh = 1
        IF k < 12 THEN
            t = k \ 3: w = k MOD 3
        ELSEIF k = 12 THEN
            t = 1: w = 2: sw = 1.18: sh = .82
        ELSE
            t = 3: w = 0: sw = .86: sh = 1.12
        END IF
        ct = COS(tl(t)): st = SIN(tl(t))
        fx = -1 + .85 * wl(w) * COS(wa(w)): fy = -2 + .85 * wl(w) * SIN(wa(w))
        ha = wa(w) + .3
        hx = -5 + .7 * wl(w) * COS(ha): hy = -3 + .7 * wl(w) * SIN(ha)
        FOR py = 1 TO SPRH
            FOR px = 1 TO SPRW
                wx = (px - 1 - SPRW / 2 + .5) / MSCALE: wy = (py - 1 - SPRH / 2 + .5) * ASPECT / MSCALE
                lx = (wx * ct + wy * st) / sw: ly = (-wx * st + wy * ct) / sh
                c = 0
                IF SegD!(lx, ly, 10, -4, 17, -13) < 1.2 OR SegD!(lx, ly, 8, -4, 12, -14) < 1.1 THEN
                    c = CINK
                ELSEIF (lx - 17) ^ 2 + (ly + 13) ^ 2 < 5 THEN
                    c = CINK
                ELSEIF (lx - 11.5) ^ 2 + (ly + 1) ^ 2 < 5 THEN
                    c = CINK
                ELSEIF (lx - 9) ^ 2 + ly ^ 2 < 21 THEN
                    c = CWEDGE
                ELSEIF Ell!(lx, ly, fx, fy, wa(w), wl(w), ww(w)) < 1 THEN
                    d = (lx - fx) ^ 2 + (ly - fy) ^ 2
                    e = Ell!(lx, ly, fx, fy, wa(w), wl(w), ww(w))
                    IF (lx - fx - .8) ^ 2 + (ly - fy + .8) ^ 2 < 1.6 THEN
                        c = CWHITE
                    ELSEIF d < 5 THEN
                        c = CINK
                    ELSEIF d < 11 OR e > .7 THEN
                        c = CWEDGE
                    ELSE
                        c = CWING
                    END IF
                ELSEIF Ell!(lx, ly, 5, 0, 1.5708, 5, 2.5) < 1 THEN
                    c = CCOLLAR
                ELSEIF Ell!(lx, ly, -3, 1, 0, 10.5, 5.2) < 1 THEN
                    IF lx < 0 AND ((INT(lx + 40) \ 4) MOD 2) = 0 THEN c = CSTRIPE ELSE c = CBODY
                ELSEIF Ell!(lx, ly, hx, hy, ha, .8 * wl(w), .9 * ww(w)) < 1 THEN
                    IF Ell!(lx, ly, hx, hy, ha, .8 * wl(w), .9 * ww(w)) > .6 THEN c = CHINDE ELSE c = CHIND
                END IF
                g(px, py) = c
            NEXT
        NEXT
        ' an outline of ink round the shape
        FOR py = 1 TO SPRH
            FOR px = 1 TO SPRW
                IF g(px, py) = 0 THEN
                    IF g(px - 1, py) > 0 OR g(px + 1, py) > 0 OR g(px, py - 1) > 0 OR g(px, py + 1) > 0 THEN g(px, py) = -1
                END IF
            NEXT
        NEXT
        FOR py = 1 TO SPRH
            FOR px = 1 TO SPRW
                c = g(px, py)
                IF c < 0 THEN c = CINK
                PSET (px - 1, py - 1), c
            NEXT
        NEXT
        GET (0, 0)-(SPRW - 1, SPRH - 1), Spr(k * SPRSZ)
        FOR py = 1 TO SPRH
            FOR px = 1 TO SPRW
                IF g(px, py) THEN PSET (px - 1, py - 1), 0 ELSE PSET (px - 1, py - 1), CMASK
            NEXT
        NEXT
        GET (0, 0)-(SPRW - 1, SPRH - 1), Spr((NSPR + k) * SPRSZ)
    NEXT
END SUB

SUB Die
    state = DYING
    stateT = 0
    flash = .18
    shakeT = .35
    vy = -70
    song = NOSONG
    cue = ""
    SOUND 37, 0
    PLAY "T150MLL32O2CO1GECL8O0G"
    IF score > best THEN
        best = score
        newBest = -1
    END IF
END SUB

' A disc of radius r, round on the screen.
SUB Disc (cx AS INTEGER, cy AS INTEGER, r AS INTEGER, c AS INTEGER)
    DIM dy AS INTEGER, ry AS INTEGER, hw AS INTEGER
    ry = r / ASPECT
    FOR dy = -ry TO ry
        hw = r * SQR(1 - (dy / (ry + .5)) ^ 2)
        LINE (cx - hw, cy + dy)-(cx + hw, cy + dy), c
    NEXT
END SUB

' Dots of colour c over the box, a quarter (level 1), half (2) or three quarters (3) of them.
SUB Dither (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, c AS INTEGER, level AS INTEGER)
    DIM p0 AS INTEGER, p1 AS INTEGER, p AS INTEGER, y AS INTEGER
    SELECT CASE level
    CASE 1: p0 = &H8888: p1 = &H2222
    CASE 2: p0 = &HAAAA: p1 = &H5555
    CASE ELSE: p0 = &HEEEE: p1 = &HBBBB
    END SELECT
    FOR y = y1 TO y2
        IF y AND 1 THEN p = p1 ELSE p = p0
        LINE (x1, y)-(x2, y), c, , p
    NEXT
END SUB

' The mountains and their ruined castle, the far hills, the seeds drifting, the near hills and trees.
' A ridge is lit along its crest and shades away below it.
SUB DrawHills
    DIM o AS INTEGER, x AS INTEGER, y AS INTEGER, k AS INTEGER, tx AS INTEGER
    DIM h AS INTEGER, b AS INTEGER, r AS INTEGER, hw AS INTEGER, cx AS INTEGER
    o = INT(scroll * .05) MOD 640
    FOR x = 0 TO SW - 1 STEP 2
        y = MtnTop((x + o) MOD 640)
        LINE (x, y)-(x + 1, y + 1), CMTN, BF
        LINE (x, y + 2)-(x + 1, y + 9), CMTN + 3, BF
        LINE (x, y + 10)-(x + 1, 150), CMTN + 6, BF
    NEXT
    cx = 215 - o
    IF cx < -20 THEN cx = cx + 640
    b = MtnTop(215) + 3
    LINE (cx - 12, b - 15)-(cx - 6, b), CMTN + 2, BF
    LINE (cx - 6, b - 9)-(cx + 7, b), CMTN + 2, BF
    LINE (cx + 7, b - 12)-(cx + 12, b), CMTN + 2, BF
    FOR k = 0 TO 3
        PSET (cx - 12 + k * 2, b - 16), CMTN + 2
        PSET (cx + 6 + k * 2, b - 13), CMTN + 2
    NEXT
    PSET (cx - 9, b - 10), CSPARK + 1
    PSET (cx + 1, b - 4), CSPARK + 1

    o = INT(scroll * .15) MOD 640
    FOR x = 0 TO SW - 1 STEP 2
        y = FarTop((x + o) MOD 640)
        LINE (x, y)-(x + 1, y + 1), CFAR, BF
        LINE (x, y + 2)-(x + 1, y + 12), CFAR + 5, BF
        LINE (x, y + 13)-(x + 1, GROUNDY - 1), CFAR + 10, BF
    NEXT
    o = INT(scroll * .25) MOD 640
    FOR k = 0 TO NFLY - 1
        IF SIN(clock * 2.3 + FlyPh(k) * 3) > .3 THEN
            x = FlyX(k) - o
            IF x < 0 THEN x = x + 640
            y = FlyY(k) + 3 * SIN(clock * .8 + FlyPh(k))
            PSET (x, y), CWHITE
            PSET (x - 1, y), CSPARK + 1: PSET (x + 1, y), CSPARK + 1
            PSET (x, y - 1), CSPARK + 1: PSET (x, y + 1), CSPARK + 1
        END IF
    NEXT
    o = INT(scroll * .4) MOD 640
    FOR x = 0 TO SW - 1
        y = NearTop((x + o) MOD 640)
        LINE (x, y)-(x, y + 1), CNEAR
        LINE (x, y + 2)-(x, y + 8), CNEAR + 5
        LINE (x, y + 9)-(x, GROUNDY - 1), CNEAR + 11
    NEXT
    FOR k = 0 TO 15
        tx = TreeX(k) - o
        IF tx < -10 THEN tx = tx + 640
        IF tx < SW + 10 THEN
            h = TreeH(k)
            b = NearTop(TreeX(k)) + 2
            LINE (tx, b - 2)-(tx, b + 1), CTREE + 3
            FOR r = 0 TO h
                hw = 1 + (r MOD (h \ 2 + 1)) * 3 \ (h \ 2 + 1) + r \ 5
                LINE (tx - hw, b - h + r)-(tx, b - h + r), CTREE
                LINE (tx + 1, b - h + r)-(tx + hw, b - h + r), CTREE + 2
            NEXT
        END IF
    NEXT
END SUB

' The lake mirrors the rows above it, each shifted by its ripple, then ORs in 64: the water's tint.
SUB DrawLake
    DIM d AS INTEGER, ofs AS INTEGER, k AS INTEGER, x AS INTEGER, y AS INTEGER, w AS INTEGER
    LINE (0, GROUNDY)-(SW - 1, SH - 1), CFAR + 10, BF
    FOR d = 0 TO LAKEH - 1
        ofs = INT((1 + d / 4) * SIN(clock * 2.2 + d * .9))
        GET (6, GROUNDY - 1 - d)-(SW - 7, GROUNDY - 1 - d), Strip
        PUT (6 + ofs, GROUNDY + d), Strip, PSET
    NEXT
    PUT (0, GROUNDY), Tinter, OR
    LINE (0, GROUNDY)-(SW - 1, GROUNDY), CWHITE, , &HAAAA
    FOR d = 2 TO LAKEH - 1 STEP 2
        w = 7 - d \ 3 + INT(2 * SIN(clock * 5 + d))
        LINE (SUNX - w, GROUNDY + d)-(SUNX + w, GROUNDY + d), CSUN, , &HAAAA
    NEXT
    FOR k = 0 TO 9
        x = (k * 37 + INT(clock * 9)) MOD SW
        y = GROUNDY + 2 + (k * 5) MOD 13
        LINE (x, y)-(x + 2 + k MOD 3, y), CWHITE
    NEXT
END SUB

SUB DrawMoth
    DIM t AS INTEGER, w AS INTEGER, k AS INTEGER, x AS INTEGER, y AS INTEGER
    IF state = DYING OR state = OVER THEN
        t = 3: w = 0
    ELSEIF state = TITLE THEN
        t = 1
    ELSEIF vy < -60 THEN
        t = 0
    ELSEIF vy < 85 THEN
        t = 1
    ELSEIF vy < 220 THEN
        t = 2
    ELSE
        t = 3
    END IF
    IF state <= PLAYING THEN
        w = INT(phase * 4) MOD 4
        IF w = 3 THEN w = 1
    END IF
    k = t * 3 + w
    IF state = PLAYING AND squashT > 0 THEN k = 12
    IF state = PLAYING AND vy > 220 THEN k = 13
    x = INT(mothX) - SPRW \ 2: y = INT(mothY) - SPRH \ 2
    PUT (x, y), Spr((NSPR + k) * SPRSZ), AND
    PUT (x, y), Spr(k * SPRSZ), OR
END SUB

' The game over panel drops in from above.
SUB DrawPanel
    DIM f AS SINGLE, y AS INTEGER
    f = stateT / .35
    IF f > 1 THEN f = 1
    y = 40 - INT((1 - f) ^ 3 * 200)
    LINE (54, y + 3)-(273, y + 103), CINK, BF
    LINE (50, y)-(270, y + 100), CPANEL, BF
    LINE (50, y)-(270, y + 100), CPANEL2, B
    LINE (52, y + 2)-(268, y + 98), CWHITE, B
    TxtC "WHAT A FLIGHT!", y + 10, 1, CTITLE, CTITLE + 7, 1
    TxtC "SCORE" + STR$(score), y + 26, 2, CWHITE, CWHITE, 1
    TxtC "BEST" + STR$(best), y + 48, 2, CTITLE, CTITLE + 4, 1
    IF newBest AND INT(clock * 4) MOD 2 = 0 THEN TxtC "NEW BEST!", y + 68, 1, CSPARK + 1, CSPARK + 1, 1
    IF stateT > .6 THEN
        TxtC "SPACE TO FLY AGAIN", y + 80, 1, CWHITE, CWHITE, 1
        TxtC "ESC FOR THE TITLE", y + 89, 1, CTITLE, CTITLE, 1
    END IF
END SUB

' The sky, a shade a band of rows, and the clouds drifting.
SUB DrawSky
    DIM k AS INTEGER, i AS INTEGER, x AS INTEGER, y AS INTEGER
    FOR k = 0 TO NSKY - 1
        LINE (0, k * GROUNDY \ NSKY)-(SW - 1, (k + 1) * GROUNDY \ NSKY - 1), CSKY + k, BF
    NEXT
    FOR i = 0 TO NCLOUD - 1
        x = (CloudX(i) + INT(clock * 4) + INT(scroll * .03)) MOD 450 - 65
        y = CloudY(i)
        Disc x - 9, y + 2, 9, CCLOUDLO
        Disc x + 10, y + 2, 8, CCLOUDLO
        Disc x, y, 11, CCLOUD
        Disc x - 10, y + 1, 7, CCLOUD
        Disc x + 10, y + 1, 7, CCLOUD
        LINE (x - 16, y + 6)-(x + 16, y + 6), CCLOUDLO
    NEXT
END SUB

SUB DrawSparks
    DIM i AS INTEGER, c AS INTEGER, x AS INTEGER, y AS INTEGER, l AS SINGLE
    FOR i = 0 TO NSPARK - 1
        l = slife(i)
        IF l > 0 THEN
            IF l > .7 THEN
                c = CSPARK
            ELSEIF l > .35 THEN
                c = CSPARK + 1
            ELSE
                c = CSPARK + 2
            END IF
            x = INT(sx(i)): y = INT(sy(i))
            PSET (x, y), c
            IF l > .6 THEN PSET (x + 1, y), c
        END IF
    NEXT
END SUB

' A stone from y1 to y2, rounded by its shading, its cap at the top (capTop) or at the bottom.
' Its kind: 1 runes, 2 vines, 3 a broken top, 4 mushrooms.
SUB DrawStone (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER, capTop AS INTEGER, kind AS INTEGER)
    DIM dx AS INTEGER, y AS INTEGER, cy AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, m AS INTEGER, n AS INTEGER
    DIM c AS INTEGER, j AS INTEGER, h AS INTEGER, vx AS INTEGER, px AS INTEGER, s AS INTEGER
    ' the shading in runs of a colour
    s = 0
    FOR dx = 1 TO STONEW
        IF dx = STONEW THEN
            LINE (x + s, y1)-(x + dx - 1, y2), StoneCol(s + OVERHANG), BF
        ELSEIF StoneCol(dx + OVERHANG) <> StoneCol(s + OVERHANG) THEN
            LINE (x + s, y1)-(x + dx - 1, y2), StoneCol(s + OVERHANG), BF
            s = dx
        END IF
    NEXT
    FOR y = y1 + 4 TO y2 STEP 9
        LINE (x + 1, y)-(x + STONEW - 2, y), CSTONE + 6
        LINE (x + 2, y + 1)-(x + 8, y + 1), CSTONE
    NEXT
    LINE (x, y1)-(x, y2), CINK: LINE (x + STONEW - 1, y1)-(x + STONEW - 1, y2), CINK
    IF capTop THEN cy = y1 ELSE cy = y2 - CAPH + 1
    c1 = x - OVERHANG: c2 = x + STONEW - 1 + OVERHANG

    IF capTop AND kind = 3 THEN
        FOR dx = 0 TO c2 - c1
            h = CAPH - ((dx \ 3) * 5 + x) MOD 4
            LINE (c1 + dx, cy + CAPH - h)-(c1 + dx, cy + CAPH - 1), StoneCol(dx)
            PSET (c1 + dx, cy + CAPH - h), CINK
        NEXT
        LINE (c1, cy + CAPH - 1)-(c2, cy + CAPH - 1), CINK
        EXIT SUB
    END IF

    LINE (c1, cy)-(c2, cy + CAPH - 1), CSTONE + 2, BF
    LINE (c1 + 2, cy)-(c1 + 8, cy + CAPH - 1), CSTONE, BF
    LINE (c2 - 8, cy)-(c2, cy + CAPH - 1), CSTONE + 5, BF
    LINE (c1 + 1, cy + 1)-(c2 - 1, cy + 1), CSTONE
    LINE (c1, cy)-(c2, cy + CAPH - 1), CINK, B
    IF capTop THEN
        LINE (c1 + 1, cy)-(c2 - 1, cy), CMOSS
        FOR m = c1 + 1 TO c2 - 1 STEP 3
            LINE (m, cy - 1 - (m - c1) MOD 2)-(m, cy), CMOSS
        NEXT
        IF kind = 4 THEN
            FOR j = 0 TO 2
                m = c1 + 5 + j * 12
                LINE (m, cy - 3)-(m, cy - 1), CWHITE
                LINE (m - 2, cy - 4)-(m + 2, cy - 4), CSHROOM
                LINE (m - 1, cy - 5)-(m + 1, cy - 5), CSHROOM
                PSET (m - 1, cy - 4), CWHITE
            NEXT
        END IF
        IF kind = 1 THEN
            IF SIN(clock * 3 + x * .05) > 0 THEN c = CRUNE ELSE c = CWHITE
            j = 0
            FOR y = cy + CAPH + 6 TO y2 - 8 STEP 10
                FOR n = 0 TO 14
                    IF MID$(Rune(j MOD 4), n + 1, 1) = "1" THEN PSET (x + 13 + n MOD 3, y + n \ 3), c
                NEXT
                j = j + 1
            NEXT
        END IF
    ELSE
        FOR m = c1 + 2 TO c2 - 2 STEP 4
            n = 1 + ((m - c1) * 7) MOD 5
            LINE (m, cy + CAPH)-(m, cy + CAPH + n), CVINE
        NEXT
        IF kind = 2 THEN
            FOR j = 0 TO 2
                vx = x + 5 + j * 9
                FOR y = 0 TO cy + CAPH + 12
                    px = vx + INT(1.5 * SIN(clock * 1.7 + y * .16 + j))
                    PSET (px, y), CVINE
                    IF (y + j * 2) MOD 6 = 0 THEN PSET (px + 1, y), CMOSS: PSET (px - 1, y + 1), CMOSS
                NEXT
            NEXT
        END IF
    END IF
END SUB

' A smiling sun, its rays turning.
SUB DrawSun
    DIM i AS INTEGER, a AS SINGLE, ca AS SINGLE, sa AS SINGLE
    FOR i = 0 TO 11
        a = i * .5236 + clock * .4
        ca = COS(a): sa = SIN(a) / ASPECT
        LINE (SUNX + 17 * ca, SUNY + 17 * sa)-(SUNX + 23 * ca, SUNY + 23 * sa), CRAY
    NEXT
    Disc SUNX, SUNY, 15, CRAY
    Disc SUNX, SUNY, 14, CSUN
    LINE (SUNX - 5, SUNY - 4)-(SUNX - 4, SUNY - 3), CINK, BF
    LINE (SUNX + 4, SUNY - 4)-(SUNX + 5, SUNY - 3), CINK, BF
    FOR i = 0 TO 8
        a = .5 + i * .27
        PSET (SUNX + 7 * COS(a), SUNY + 1 + 6 * SIN(a) / ASPECT), CINK
    NEXT
    Disc SUNX - 9, SUNY + 3, 2, CCHEEK
    Disc SUNX + 9, SUNY + 3, 2, CCHEEK
END SUB

' Where (px, py) is in the ellipse at (cx, cy), its long axis at ang: under 1 inside.
FUNCTION Ell! (px AS SINGLE, py AS SINGLE, cx AS SINGLE, cy AS SINGLE, ang AS SINGLE, a AS SINGLE, b AS SINGLE)
    DIM qx AS SINGLE, qy AS SINGLE, ca AS SINGLE, sa AS SINGLE, u AS SINGLE, v AS SINGLE
    qx = px - cx: qy = py - cy
    ca = COS(ang): sa = SIN(ang)
    u = (qx * ca + qy * sa) / a
    v = (qy * ca - qx * sa) / b
    Ell! = u * u + v * v
END FUNCTION

' n sparks from the moth: drifting back, spread up and down, kick flings them out.
SUB Emit (n AS INTEGER, spread AS SINGLE, kick AS SINGLE)
    DIM i AS INTEGER, todo AS INTEGER
    todo = n
    FOR i = 0 TO NSPARK - 1
        IF todo = 0 THEN EXIT FOR
        IF slife(i) <= 0 THEN
            sx(i) = mothX - 5 + RND * 3
            sy(i) = mothY + RND * 4 - 2
            svx(i) = -30 - RND * 40 + (RND - .5) * kick
            svy(i) = (RND - .5) * spread
            slife(i) = .6 + RND * .5
            todo = todo - 1
        END IF
    NEXT
END SUB

SUB Flap
    vy = FLAPV
    boost = .25
    squashT = .12
    Emit 20, 130, 100
    IF musicOn = 0 AND PLAY(0) < 3 THEN PLAY "T255MNL64O4AO5E"
END SUB

FUNCTION Hit%
    DIM i AS INTEGER, x AS SINGLE, t AS SINGLE, b AS SINGLE
    Hit% = 0
    FOR i = 0 TO NSTONE - 1
        x = stoneX(i)
        IF x < mothX + 12 AND x + STONEW > mothX - 12 THEN
            t = stoneGap(i) - GAP \ 2: b = stoneGap(i) + GAP \ 2
            IF RectHit%(x, 0, x + STONEW - 1, t - CAPH) THEN Hit% = -1
            IF RectHit%(x - OVERHANG, t - CAPH, x + STONEW - 1 + OVERHANG, t) THEN Hit% = -1
            IF RectHit%(x - OVERHANG, b, x + STONEW - 1 + OVERHANG, b + CAPH) THEN Hit% = -1
            IF RectHit%(x, b + CAPH, x + STONEW - 1, GROUNDY) THEN Hit% = -1
        END IF
    NEXT
END FUNCTION

' Colour b mixed into a by t, each as red + green * 256 + blue * 65536.
FUNCTION Mix& (a AS LONG, b AS LONG, t AS SINGLE)
    DIM i AS INTEGER, v AS LONG, ca AS LONG, cb AS LONG, s AS LONG
    s = 1
    FOR i = 0 TO 2
        ca = (a \ s) MOD 256: cb = (b \ s) MOD 256
        v = v + CLNG(ca + (cb - ca) * t) * s
        s = s * 256
    NEXT
    Mix& = v
END FUNCTION

' Keeps about a beat of the song queued, so a cue (the score's chime) takes the next beat's place.
SUB Music
    IF song = NOSONG OR musicOn = 0 THEN EXIT SUB
    IF PLAY(0) < 3 THEN
        IF LEN(cue) THEN
            PLAY cue
            cue = ""
        ELSE
            PLAY Tune(song, beat)
        END IF
        beat = (beat + 1) MOD songLen(song)
    END IF
END SUB

SUB NewGame
    DIM i AS INTEGER, g AS INTEGER
    StartSong GAMESONG
    IF state <> TITLE THEN mothY = 100
    state = PLAYING
    stateT = 0
    score = 0
    newBest = 0
    sky = DAY
    g = 100
    FOR i = 0 TO NSTONE - 1
        stoneX(i) = 360 + i * SPACING
        g = NextGap%(g)
        stoneGap(i) = g
        stoneDone(i) = 0
        stoneKind(i) = INT(RND * NKIND)
    NEXT
END SUB

FUNCTION NextGap% (prev AS INTEGER)
    DIM g AS INTEGER
    g = prev + INT(RND * 113) - 56
    IF g < GAP \ 2 + CAPH + 20 THEN g = GAP \ 2 + CAPH + 20
    IF g > GROUNDY - GAP \ 2 - CAPH - 14 THEN g = GROUNDY - GAP \ 2 - CAPH - 14
    NextGap% = g
END FUNCTION

' Whether the moth's body, round on the screen, meets the box.
FUNCTION RectHit% (x1 AS SINGLE, y1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE)
    DIM nx AS SINGLE, ny AS SINGLE, dx AS SINGLE, dy AS SINGLE
    nx = mothX: ny = mothY
    IF nx < x1 THEN nx = x1
    IF nx > x2 THEN nx = x2
    IF ny < y1 THEN ny = y1
    IF ny > y2 THEN ny = y2
    dx = mothX - nx: dy = (mothY - ny) * ASPECT
    RectHit% = dx * dx + dy * dy < 30
END FUNCTION

SUB Render
    DIM i AS INTEGER, x AS INTEGER, sc AS INTEGER, amp AS SINGLE, dx AS INTEGER, dy AS INTEGER
    DrawSky
    IF behind THEN DrawMoth
    DrawSun
    DrawHills
    IF state <> TITLE THEN
        FOR i = 0 TO NSTONE - 1
            x = INT(stoneX(i))
            IF x < SW + 4 AND x > -STONEW - 4 THEN
                DrawStone x, 0, stoneGap(i) - GAP \ 2, 0, stoneKind(i)
                DrawStone x, stoneGap(i) + GAP \ 2, GROUNDY - 1, -1, stoneKind(i)
            END IF
        NEXT
    END IF
    IF NOT behind THEN DrawMoth
    DrawLake
    DrawSparks

    SELECT CASE state
    CASE TITLE
        TxtC "MOTHLIGHT", 14, 3, CTITLE, CTITLE + 7, 2
        ' a gleam sweeps across the name
        x = INT(clock * 140) MOD 560 - 100
        IF x >= 52 AND x <= 260 THEN
            VIEW SCREEN (x, 14)-(x + 10, 38)
            TxtC "MOTHLIGHT", 14, 3, CWHITE, CWHITE, 0
            VIEW
        END IF
        TxtC "a little moth's big day out", 44, 1, CWHITE, CWHITE, 1
        IF INT(clock * 2) MOD 2 = 0 THEN TxtC "PRESS SPACE TO FLY", 116, 2, CWHITE, CTITLE + 2, 1
        IF best THEN TxtC "BEST" + STR$(best), 138, 1, CTITLE, CTITLE + 3, 1
        TxtC "SPACE/ENTER/UP FLAP  M MUSIC  ESC QUIT", 189, 1, CWHITE, CWHITE, 1
    CASE PLAYING, DYING
        sc = 2
        IF popT > 0 THEN sc = 3
        TxtC LTRIM$(STR$(score)), 6, sc, CWHITE, CTITLE + 3, 1
    CASE OVER
        DrawPanel
    END SELECT
    IF flash > 0 THEN Dither 0, 0, SW - 1, SH - 1, CWHITE, 1 + INT(flash * 12)

    ' a shake moves the whole frame down and right
    IF shakeT > 0 THEN
        amp = shakeT * 10
        dx = 1 + INT(RND * amp): dy = 1 + INT(RND * amp)
        GET (0, 0)-(SW - 1 - dx, SH - 1 - dy), Frame
        LINE (0, 0)-(SW - 1, dy - 1), CINK, BF
        LINE (0, 0)-(dx - 1, SH - 1), CINK, BF
        PUT (dx, dy), Frame, PSET
    END IF
    SCREEN , , 1 - drawPage, drawPage
    drawPage = 1 - drawPage
END SUB

' The distance from (px, py) to the segment from a to b.
FUNCTION SegD! (px AS SINGLE, py AS SINGLE, ax AS SINGLE, ay AS SINGLE, bx AS SINGLE, by AS SINGLE)
    DIM dx AS SINGLE, dy AS SINGLE, t AS SINGLE, ex AS SINGLE, ey AS SINGLE
    dx = bx - ax: dy = by - ay
    t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)
    IF t < 0 THEN t = 0
    IF t > 1 THEN t = 1
    ex = ax + t * dx - px: ey = ay + t * dy - py
    SegD! = SQR(ex * ex + ey * ey)
END FUNCTION

' The palette from the key colours in use (CurKey, easing toward the sky's), dimmed by the fade.
SUB SetPalette
    DIM i AS INTEGER, k(NKEY - 1) AS LONG, t AS SINGLE, c AS LONG, r AS LONG, g AS LONG, b AS LONG
    FOR i = 0 TO NKEY - 1
        k(i) = CLNG(CurKey(i, 0)) + CLNG(CurKey(i, 1)) * 256 + CLNG(CurKey(i, 2)) * 65536
    NEXT
    FOR i = 0 TO NSKY - 1
        t = i / (NSKY - 1)
        IF t < .5 THEN Rgb(CSKY + i) = Mix&(k(0), k(1), t * 2) ELSE Rgb(CSKY + i) = Mix&(k(1), k(2), (t - .5) * 2)
    NEXT
    FOR i = 0 TO 7
        Rgb(CTITLE + i) = Mix&(&H8CFFFF, &H1E78FF, i / 7)
    NEXT
    Rgb(CCLOUD) = k(3): Rgb(CCLOUDLO) = k(4)
    FOR i = 0 TO 15
        Rgb(CMTN + i) = Mix&(Mix&(k(5), &HFFFFFF, .3), k(2), i / 16)
        Rgb(CFAR + i) = Mix&(Mix&(k(6), &HFFFFFF, .3), k(7), i / 12)
        Rgb(CNEAR + i) = Mix&(Mix&(k(8), &HFFFFFF, .3), k(9), i / 12)
    NEXT
    FOR i = 0 TO 3
        Rgb(CTREE + i) = Mix&(k(10), k(11), i / 3)
    NEXT
    FOR i = 0 TO 7
        Rgb(CSTONE + i) = Mix&(k(13), k(14), i / 7)
    NEXT
    FOR i = 0 TO 15
        Rgb(WATER + i) = Mix&(Mix&(Rgb(i), k(12), .45), 0, .1)
    NEXT
    FOR i = CMTN TO CVINE
        Rgb(WATER + i) = Mix&(Mix&(Rgb(i), k(12), .45), 0, .1)
    NEXT
    FOR i = 0 TO 255
        c = Rgb(i)
        r = (c MOD 256) * fade: g = ((c \ 256) MOD 256) * fade: b = (c \ 65536) * fade
        PALETTE i, r \ 4 + (g \ 4) * 256 + (b \ 4) * 65536
    NEXT
END SUB

' The hills and mountains, trees, seeds, clouds and runes, the stone's shading, the water's tint.
SUB Setup
    DIM i AS INTEGER, j AS INTEGER, x AS INTEGER, k AS INTEGER, a AS SINGLE
    SCREEN 13, , 1, 0
    drawPage = 1
    FOR i = 0 TO NKEY - 1
        FOR j = 0 TO 2
            CurKey(i, j) = (SkyKey(DAY, i) \ 256 ^ j) MOD 256
        NEXT
    NEXT
    fade = 0
    fadeTarget = 1
    SetPalette

    RANDOMIZE TIMER
    FOR i = 0 TO 7: Bit(i) = 2 ^ (7 - i): NEXT
    FOR x = 0 TO 639
        a = x * 6.283185 / 640
        MtnTop(x) = 123 - (19 * ABS(SIN(1.5 * a + .3)) + 9 * ABS(SIN(4 * a + 1)) + 3 * ABS(SIN(11 * a)))
        FarTop(x) = 143 - (17 + 10 * SIN(2 * a) + 5 * SIN(5 * a + 1.3) + 2 * SIN(13 * a))
        NearTop(x) = 166 - (9 + 7 * SIN(3 * a + 2) + 4 * SIN(7 * a + .4) + 2 * SIN(17 * a))
    NEXT
    FOR k = 0 TO 15
        TreeX(k) = k * 40 + INT(RND * 24)
        TreeH(k) = 8 + INT(RND * 9)
    NEXT
    FOR i = 0 TO NFLY - 1
        FlyX(i) = INT(RND * 640)
        FlyY(i) = 118 + INT(RND * 40)
        FlyPh(i) = RND * 6.28
    NEXT
    FOR i = 0 TO NCLOUD - 1
        CloudX(i) = i * 150 + INT(RND * 50)
        CloudY(i) = 20 + i * 12
    NEXT
    ' the stone's columns: lit a quarter in, shaded to the right
    FOR i = 0 TO STONEW + 2 * OVERHANG
        k = ABS(i - (STONEW + 2 * OVERHANG) \ 4) * 9 \ (STONEW + 2 * OVERHANG)
        IF k > 7 THEN k = 7
        StoneCol(i) = CSTONE + k
    NEXT
    Rune(0) = "111010010010111"
    Rune(1) = "101111101101101"
    Rune(2) = "110101110101110"
    Rune(3) = "010111010010010"
    ' the lake's tint: an image of the lake's size, every pixel 64
    Tinter(0) = SW * 8 + LAKEH * 65536
    FOR i = 1 TO SW * LAKEH \ 4
        Tinter(i) = &H40404040
    NEXT
END SUB

SUB StartSong (id AS INTEGER)
    SOUND 37, 0
    song = id
    beat = 0
    cue = ""
END SUB

SUB Tick (flapped AS INTEGER)
    DIM i AS INTEGER, j AS INTEGER, far AS INTEGER, changed AS INTEGER, want AS SINGLE, d AS SINGLE
    clock = clock + STEPT
    stateT = stateT + STEPT
    flash = flash - STEPT
    boost = boost - STEPT
    squashT = squashT - STEPT
    popT = popT - STEPT
    shakeT = shakeT - STEPT
    IF boost > 0 THEN phase = phase + STEPT * 9 ELSE phase = phase + STEPT * 4

    SELECT CASE state
    CASE TITLE
        ' circling the sun, behind it on the lower half
        mothX = SUNX + 26 * COS(clock * .8)
        mothY = SUNY + 8 + 28 * SIN(clock * .8)
        behind = SIN(clock * .8) > 0
        scroll = scroll + 25 * STEPT
        IF flapped THEN
            behind = 0
            NewGame
            Flap
        END IF
    CASE PLAYING
        IF flapped THEN Flap
        mothX = mothX + (HOMEX - mothX) * 5 * STEPT
        vy = vy + GRAV * STEPT
        mothY = mothY + vy * STEPT
        scroll = scroll + SPEED * STEPT
        FOR i = 0 TO NSTONE - 1
            stoneX(i) = stoneX(i) - SPEED * STEPT
            IF stoneX(i) + STONEW + OVERHANG < 0 THEN
                far = 0
                FOR j = 1 TO NSTONE - 1
                    IF stoneX(j) > stoneX(far) THEN far = j
                NEXT
                stoneX(i) = stoneX(far) + SPACING
                stoneGap(i) = NextGap%(stoneGap(far))
                stoneDone(i) = 0
                stoneKind(i) = INT(RND * NKIND)
            END IF
            IF stoneDone(i) = 0 AND stoneX(i) + STONEW < mothX THEN
                stoneDone(i) = -1
                score = score + 1
                popT = .25
                sky = (score \ 10) MOD 3
                IF musicOn THEN cue = "T160MNL32O4EGO5CEL8G" ELSE PLAY "T220MNL32O4EGO5CL16E"
                Emit 8, 100, 0
            END IF
        NEXT
        IF Hit% OR mothY > GROUNDY - 5 THEN Die
    CASE DYING
        vy = vy + GRAV * STEPT
        mothY = mothY + vy * STEPT
        IF mothY >= GROUNDY - 5 THEN
            mothY = GROUNDY - 5
            state = OVER
            stateT = 0
            Emit 16, 80, 130
            IF musicOn THEN
                IF newBest THEN PLAY "T160MNL16O4CEGO5L8CO4L16GO5L4C" ELSE PLAY "T140MNL16O4CO3GECL8EGL4O4C"
            END IF
        END IF
    CASE OVER
        IF flapped AND stateT > .6 THEN
            NewGame
            Flap
        END IF
    END SELECT
    IF mothY < SPRH \ 2 + 2 THEN mothY = SPRH \ 2 + 2: vy = 0

    ' Esc faded the screen out: the title comes back by day
    IF toTitle AND fade <= 0 THEN
        toTitle = 0
        state = TITLE
        sky = DAY
        fadeTarget = 1
        StartSong TITLESONG
    END IF

    ' the key colours ease toward the sky's, the fade toward its target
    palT = palT + STEPT
    IF palT >= 1 / 30 THEN
        palT = 0
        changed = 0
        FOR i = 0 TO NKEY - 1
            FOR j = 0 TO 2
                want = (SkyKey(sky, i) \ 256& ^ j) MOD 256
                d = want - CurKey(i, j)
                IF ABS(d) > .5 THEN CurKey(i, j) = CurKey(i, j) + d * .06: changed = -1
            NEXT
        NEXT
        IF fade < fadeTarget THEN fade = fade + .1: changed = -1
        IF fade > fadeTarget THEN fade = fade - .1: changed = -1
        IF fade < 0 THEN fade = 0
        IF fade > 1 THEN fade = 1
        IF changed THEN SetPalette
    END IF

    IF state <= PLAYING THEN
        sparkT = sparkT + STEPT
        IF sparkT > .05 THEN
            sparkT = 0
            Emit 1, 15, 0
        END IF
    END IF
    FOR i = 0 TO NSPARK - 1
        IF slife(i) > 0 THEN
            sx(i) = sx(i) + svx(i) * STEPT
            sy(i) = sy(i) + svy(i) * STEPT
            svx(i) = svx(i) * (1 - 1.5 * STEPT)
            svy(i) = svy(i) * (1 - 2 * STEPT) - 10 * STEPT
            slife(i) = slife(i) - STEPT
        END IF
    NEXT
END SUB

' Text from the ROM font, sc times as big, its rows shaded from c1 to c2, over a shadow sh pixels
' down and right.
SUB Txt (t AS STRING, x AS INTEGER, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, sh AS INTEGER)
    DIM pass AS INTEGER, o AS INTEGER, i AS INTEGER, ch AS INTEGER, gx AS INTEGER, gy AS INTEGER
    DIM r AS INTEGER, b AS INTEGER, c AS INTEGER, cx AS INTEGER, s AS INTEGER
    FOR pass = 0 TO 1
        IF pass = 0 THEN
            IF sh = 0 THEN pass = 1
        END IF
        IF pass = 0 THEN o = sh ELSE o = 0
        FOR i = 1 TO LEN(t)
            ch = ASC(MID$(t, i, 1))
            gx = x + (i - 1) * 8 * sc + o
            FOR r = 0 TO 7
                b = Glyph(ch, r)
                IF b THEN
                    IF pass = 0 THEN c = CINK ELSE c = c1 + (r * (c2 - c1 + 1)) \ 8
                    gy = y + r * sc + o
                    cx = 0
                    DO WHILE cx < 8
                        IF b AND Bit(cx) THEN
                            s = cx
                            DO WHILE cx < 8
                                IF (b AND Bit(cx)) = 0 THEN EXIT DO
                                cx = cx + 1
                            LOOP
                            LINE (gx + s * sc, gy)-(gx + cx * sc - 1, gy + sc - 1), c, BF
                        ELSE
                            cx = cx + 1
                        END IF
                    LOOP
                END IF
            NEXT
        NEXT
    NEXT
END SUB

SUB TxtC (t AS STRING, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, sh AS INTEGER)
    Txt t, (SW - LEN(t) * 8 * sc) \ 2, y, sc, c1, c2, sh
END SUB
