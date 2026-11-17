' MOTHLIGHT: a moth flies by moonlight through the standing stones.
' Space, Enter or Up flaps; M turns the music off and on; Esc quits. Driven by TIMER: the same speed on any machine.
' SCREEN 9's two pages: one is drawn while the other is shown.
DECLARE SUB Setup ()
DECLARE SUB BuildGlyphs ()
DECLARE SUB BuildMoth ()
DECLARE SUB NewGame ()
DECLARE SUB Flap ()
DECLARE SUB Die ()
DECLARE SUB Tick (flapped AS INTEGER)
DECLARE SUB Render ()
DECLARE SUB DrawSky ()
DECLARE SUB DrawMoon ()
DECLARE SUB DrawHills ()
DECLARE SUB DrawFireflies ()
DECLARE SUB DrawStone (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER, capTop AS INTEGER, kind AS INTEGER)
DECLARE SUB DrawLake ()
DECLARE SUB DrawSparks ()
DECLARE SUB DrawGlow ()
DECLARE SUB DrawMoth ()
DECLARE SUB DrawPanel ()
DECLARE SUB Emit (n AS INTEGER, spread AS SINGLE, kick AS SINGLE)
DECLARE SUB Txt (t AS STRING, x AS INTEGER, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, c3 AS INTEGER, sh AS INTEGER)
DECLARE SUB TxtC (t AS STRING, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, c3 AS INTEGER, sh AS INTEGER)
DECLARE SUB Music ()
DECLARE SUB StartSong (id AS INTEGER)
DECLARE SUB SetPalette ()
DECLARE SUB Dither (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, c AS INTEGER, level AS INTEGER)
DECLARE FUNCTION Ega% (r AS INTEGER, g AS INTEGER, b AS INTEGER)
DECLARE FUNCTION NextGap% (prev AS INTEGER)
DECLARE FUNCTION Hit% ()
DECLARE FUNCTION RectHit% (x1 AS SINGLE, y1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE)
DECLARE FUNCTION Ell! (px AS SINGLE, py AS SINGLE, cx AS SINGLE, cy AS SINGLE, ang AS SINGLE, a AS SINGLE, b AS SINGLE)
DECLARE FUNCTION SegD! (px AS SINGLE, py AS SINGLE, ax AS SINGLE, ay AS SINGLE, bx AS SINGLE, by AS SINGLE)

CONST GROUNDY = 322, HOMEX = 170, ASPECT = 1.37!
CONST NSTONE = 4, STONEW = 58, CAPH = 12, GAP = 116, SPACING = 240, NKIND = 5
CONST SPRW = 64, SPRH = 56, SPRSZ = 900, MSCALE = 1.35!, NSPR = 14, NSPARK = 64
CONST GRAV = 1150!, FLAPV = -330!, SPEED = 140!, STEPT = 1! / 120!
CONST TITLE = 0, PLAYING = 1, DYING = 2, OVER = 3
CONST NOSONG = 0, TITLESONG = 1, GAMESONG = 2, TITLEBEATS = 32, GAMEBEATS = 64
CONST MOONX = 560, MOONY = 64, NFLY = 12, NCLOUD = 3, DUSK = 0, NIGHT = 1, DAWN = 2

'$DYNAMIC
DIM SHARED state AS INTEGER, score AS INTEGER, best AS INTEGER, newBest AS INTEGER, drawPage AS INTEGER
DIM SHARED mothX AS SINGLE, mothY AS SINGLE, vy AS SINGLE, scroll AS SINGLE, clock AS SINGLE, phase AS SINGLE
DIM SHARED boost AS SINGLE, flash AS SINGLE, stateT AS SINGLE, sparkT AS SINGLE, behind AS INTEGER
DIM SHARED squashT AS SINGLE, popT AS SINGLE, shakeT AS SINGLE, shakeDX AS INTEGER, shakeDY AS INTEGER
DIM SHARED shootT AS SINGLE, shootX AS SINGLE, shootY AS SINGLE, shootLife AS SINGLE
DIM SHARED stoneX(NSTONE - 1) AS SINGLE, stoneGap(NSTONE - 1) AS INTEGER, stoneDone(NSTONE - 1) AS INTEGER
DIM SHARED stoneKind(NSTONE - 1) AS INTEGER
DIM SHARED MtnTop(639) AS INTEGER, FarTop(639) AS INTEGER, NearTop(639) AS INTEGER
DIM SHARED TreeX(15) AS INTEGER, TreeH(15) AS INTEGER, StarX(39) AS INTEGER, StarY(39) AS INTEGER
DIM SHARED FlyX(NFLY - 1) AS INTEGER, FlyY(NFLY - 1) AS INTEGER, FlyPh(NFLY - 1) AS SINGLE
DIM SHARED CloudX(NCLOUD - 1) AS INTEGER, CloudY(NCLOUD - 1) AS INTEGER, CloudW(NCLOUD - 1) AS INTEGER
DIM SHARED Rune(3) AS STRING
DIM SHARED PalSet(2, 15) AS INTEGER, PalCur(15) AS INTEGER, sky AS INTEGER, palT AS SINGLE
DIM SHARED dimLvl AS INTEGER, dimTarget AS INTEGER, fadeT AS SINGLE, toTitle AS INTEGER, quitting AS INTEGER
DIM SHARED Glyph(126, 13) AS INTEGER, Bit(7) AS INTEGER
DIM SHARED Spr(SPRSZ * NSPR * 2 - 1) AS INTEGER, Strip(170) AS INTEGER
DIM SHARED Tune(1 TO 2, GAMEBEATS - 1) AS STRING, songLen(1 TO 2) AS INTEGER
DIM SHARED song AS INTEGER, beat AS INTEGER, musicOn AS INTEGER, cue AS STRING
DIM SHARED sx(NSPARK - 1) AS SINGLE, sy(NSPARK - 1) AS SINGLE
DIM SHARED svx(NSPARK - 1) AS SINGLE, svy(NSPARK - 1) AS SINGLE, slife(NSPARK - 1) AS SINGLE

DIM t AS SINGLE, lastT AS SINGLE, dt AS SINGLE, acc AS SINGLE
DIM flapped AS INTEGER, k AS STRING, i AS INTEGER

' The songs, a beat to a string: the tune on the beat, then two notes of its chord.
songLen(GAMESONG) = GAMEBEATS
RESTORE GameTune
FOR i = 0 TO GAMEBEATS - 1: READ Tune(GAMESONG, i): NEXT
songLen(TITLESONG) = TITLEBEATS
RESTORE TitleTune
FOR i = 0 TO TITLEBEATS - 1: READ Tune(TITLESONG, i): NEXT
musicOn = -1

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
            dimTarget = 3
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
LOOP UNTIL quitting AND dimLvl = 3

SOUND 37, 0
SCREEN 0
WIDTH 80
END

GameTune:
DATA "T150MNO3A8O2A16O3E16", "T150MNO4C8O2A16O3E16", "T150MNO4E8O2A16O3E16", "T150MNO4D8O2A16O3E16"
DATA "T150MNO4C8O2A16O3E16", "T150MNO3B8O2A16O3E16", "T150MNO3A8O2A16O3E16", "T150MNO3E8O2A16O3E16"
DATA "T150MNO3F8O2F16O3C16", "T150MNO3A8O2F16O3C16", "T150MNO4C8O2F16O3C16", "T150MNO4E8O2F16O3C16"
DATA "T150MNO4D8O2F16O3C16", "T150MNO4C8O2F16O3C16", "T150MNO3A8O2F16O3C16", "T150MNO4C8O2F16O3C16"
DATA "T150MNO4E8O3C16O2G16", "T150MNO4G8O3C16O2G16", "T150MNO4E8O3C16O2G16", "T150MNO4C8O3C16O2G16"
DATA "T150MNO4D8O3C16O2G16", "T150MNO4E8O3C16O2G16", "T150MNO4C8O3C16O2G16", "T150MNO3G8O3C16O2G16"
DATA "T150MNO3B8O2G16O3D16", "T150MNO4D8O2G16O3D16", "T150MNO4G8O2G16O3D16", "T150MNO4F8O2G16O3D16"
DATA "T150MNO4E8O2E16O2B16", "T150MNO4D8O2E16O2B16", "T150MNO3B8O2E16O2B16", "T150MNO3G#8O2E16O2B16"
DATA "T150MNO3A8O2F16O3C16", "T150MNO4C8O2F16O3C16", "T150MNO4F8O2F16O3C16", "T150MNO4E8O2F16O3C16"
DATA "T150MNO4D8O2G16O3D16", "T150MNO3B8O2G16O3D16", "T150MNO3G8O2G16O3D16", "T150MNO3B8O2G16O3D16"
DATA "T150MNO4E8O2E16O2B16", "T150MNO4G8O2E16O2B16", "T150MNO4B8O2E16O2B16", "T150MNO4G8O2E16O2B16"
DATA "T150MNO4A8O2A16O3E16", "T150MNO4E8O2A16O3E16", "T150MNO4C8O2A16O3E16", "T150MNO3A8O2A16O3E16"
DATA "T150MNO3F8O2F16O3C16", "T150MNO3A8O2F16O3C16", "T150MNO4C8O2F16O3C16", "T150MNO4F8O2F16O3C16"
DATA "T150MNO4G8O2G16O3D16", "T150MNO4F8O2G16O3D16", "T150MNO4D8O2G16O3D16", "T150MNO3B8O2G16O3D16"
DATA "T150MNO3G#8O2E16O2B16", "T150MNO3B8O2E16O2B16", "T150MNO4E8O2E16O2B16", "T150MNO3B8O2E16O2B16"
DATA "T150MNO4G#8O2E16O2B16", "T150MNO4E8O2E16O2B16", "T150MNO3B8O2E16O2B16", "T150MNO3G#8O2E16O2B16"
TitleTune:
DATA "T92MSO2A16O3E16MLO4E8", "T92MSO2A16O3E16MLO4C8", "T92MSO2A16O3E16MLO3B8", "T92MSO2A16O3E16MLO4C8"
DATA "T92MSO2A16O3E16MLO3A8", "T92MSO2A16O3E16MLO3B8", "T92MSO2A16O3E16MLO4C8", "T92MSO2A16O3E16MLO4E8"
DATA "T92MSO2F16O3C16MLO4F8", "T92MSO2F16O3C16MLO4E8", "T92MSO2F16O3C16MLO4C8", "T92MSO2F16O3C16MLO3A8"
DATA "T92MSO2F16O3C16MLO4C8", "T92MSO2F16O3C16MLO3A8", "T92MSO2F16O3C16MLO3G8", "T92MSO2F16O3C16MLO3A8"
DATA "T92MSO2C16O2G16MLO4G8", "T92MSO2C16O2G16MLO4E8", "T92MSO2C16O2G16MLO4D8", "T92MSO2C16O2G16MLO4C8"
DATA "T92MSO2G16O3D16MLO4D8", "T92MSO2G16O3D16MLO3B8", "T92MSO2G16O3D16MLO3G8", "T92MSO2G16O3D16MLO3B8"
DATA "T92MSO2A16O3E16MLO4C8", "T92MSO2A16O3E16MLO4E8", "T92MSO2A16O3E16MLO4A8", "T92MSO2A16O3E16MLO4G8"
DATA "T92MSO2E16O2B16MLO4G#8", "T92MSO2E16O2B16MLO4E8", "T92MSO2E16O2B16MLO3B8", "T92MSO2E16O2B16MLO3G#8"
REM $STATIC
SUB BuildGlyphs
    DIM c AS INTEGER, r AS INTEGER, x AS INTEGER, b AS INTEGER
    SCREEN , , 1, 0
    COLOR 13
    FOR c = 32 TO 126
        LOCATE 1, 1
        PRINT CHR$(c);
        FOR r = 0 TO 13
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

    SCREEN , , 1, 0
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
                IF SegD!(lx, ly, 10, -4, 17, -13) < .9 OR SegD!(lx, ly, 8, -4, 12, -14) < .8 THEN
                    c = 12
                ELSEIF (lx - 17) ^ 2 + (ly + 13) ^ 2 < 3 THEN
                    c = 12
                ELSEIF (lx - 11.5) ^ 2 + (ly + 1) ^ 2 < 3 THEN
                    c = 12
                ELSEIF (lx - 9) ^ 2 + ly ^ 2 < 21 THEN
                    c = 11
                ELSEIF Ell!(lx, ly, fx, fy, wa(w), wl(w), ww(w)) < 1 THEN
                    d = (lx - fx) ^ 2 + (ly - fy) ^ 2
                    e = Ell!(lx, ly, fx, fy, wa(w), wl(w), ww(w))
                    IF (lx - fx - .6) ^ 2 + (ly - fy + .6) ^ 2 < .7 THEN
                        c = 13
                    ELSEIF d < 3.2 THEN
                        c = 12
                    ELSEIF d < 8 OR e > .72 THEN
                        c = 11
                    ELSE
                        c = 10
                    END IF
                ELSEIF Ell!(lx, ly, 5, 0, 1.5708, 5, 2.5) < 1 THEN
                    c = 13
                ELSEIF Ell!(lx, ly, -3, 1, 0, 10.5, 5.2) < 1 THEN
                    IF lx < 0 AND ((INT(lx + 40) \ 3) MOD 2) = 0 THEN c = 11 ELSE c = 8
                ELSEIF Ell!(lx, ly, hx, hy, ha, .8 * wl(w), .9 * ww(w)) < 1 THEN
                    IF Ell!(lx, ly, hx, hy, ha, .8 * wl(w), .9 * ww(w)) > .65 THEN c = 9 ELSE c = 8
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
                IF c < 0 THEN c = 12
                PSET (px - 1, py - 1), c
            NEXT
        NEXT
        GET (0, 0)-(SPRW - 1, SPRH - 1), Spr(k * SPRSZ)
        FOR py = 1 TO SPRH
            FOR px = 1 TO SPRW
                IF g(px, py) THEN PSET (px - 1, py - 1), 0 ELSE PSET (px - 1, py - 1), 15
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
    vy = -120
    song = NOSONG
    cue = ""
    SOUND 37, 0
    PLAY "T150MLL32O2CO1GECL8O0G"
    IF score > best THEN
        best = score
        newBest = -1
    END IF
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

SUB DrawFireflies
    DIM i AS INTEGER, x AS INTEGER, y AS INTEGER, o AS INTEGER
    o = INT(scroll * .25) MOD 640
    FOR i = 0 TO NFLY - 1
        IF SIN(clock * 2.3 + FlyPh(i) * 3) > .3 THEN
            x = FlyX(i) - o
            IF x < 0 THEN x = x + 640
            y = FlyY(i) + 5 * SIN(clock * .8 + FlyPh(i))
            PSET (x, y), 13
            PSET (x - 1, y), 10: PSET (x + 1, y), 10
            PSET (x, y - 1), 10: PSET (x, y + 1), 10
        END IF
    NEXT
END SUB

' The moth's glow, a halo of dots that breathes with its wings.
SUB DrawGlow
    DIM r AS SINGLE, ry AS INTEGER, dy AS INTEGER, hw AS INTEGER, x AS INTEGER, y AS INTEGER
    x = INT(mothX): y = INT(mothY)
    r = 34 + 4 * SIN(phase * 6.2832)
    ry = INT(r / ASPECT)
    FOR dy = -ry TO ry
        hw = r * SQR(1 - (dy / (ry + .5)) ^ 2)
        Dither x - hw, y + dy, x + hw, y + dy, 5, 1
    NEXT
    ry = INT(.55 * r / ASPECT)
    FOR dy = -ry TO ry
        hw = .55 * r * SQR(1 - (dy / (ry + .5)) ^ 2)
        Dither x - hw, y + dy, x + hw, y + dy, 5, 2
    NEXT
END SUB

SUB DrawHills
    DIM o AS INTEGER, x AS INTEGER, y AS INTEGER, k AS INTEGER, tx AS INTEGER
    DIM h AS INTEGER, b AS INTEGER, r AS INTEGER, hw AS INTEGER, cx AS INTEGER, m AS INTEGER, p AS INTEGER
    ' the mountains, hazy with distance, and their ruined castle, a lamp still lit in it
    o = INT(scroll * .05) MOD 640
    FOR x = 0 TO 639
        y = MtnTop((x + o) MOD 640)
        IF (x + y) AND 1 THEN p = &HAAAA ELSE p = &H5555
        LINE (x, y)-(x, 260), 1, , p
    NEXT
    cx = 430 - o
    IF cx < -40 THEN cx = cx + 640
    b = MtnTop(430) + 6
    Dither cx - 24, b - 30, cx - 12, b, 1, 3
    Dither cx - 12, b - 18, cx + 14, b, 1, 3
    Dither cx + 14, b - 24, cx + 24, b, 1, 3
    FOR m = cx - 24 TO cx - 14 STEP 4
        LINE (m, b - 33)-(m + 1, b - 30), 1, BF
    NEXT
    FOR m = cx - 12 TO cx + 12 STEP 5
        LINE (m, b - 21)-(m + 2, b - 18), 1, BF
    NEXT
    LINE (cx + 14, b - 28)-(cx + 17, b - 24), 1, BF
    LINE (cx + 21, b - 26)-(cx + 24, b - 24), 1, BF
    PSET (cx - 18, b - 22), 10
    PSET (cx + 2, b - 10), 10

    o = INT(scroll * .15) MOD 640
    FOR x = 0 TO 638 STEP 2
        y = FarTop((x + o) MOD 640)
        LINE (x, y)-(x + 1, GROUNDY), 6, BF
        LINE (x, y)-(x + 1, y), 2
    NEXT
    DrawFireflies
    o = INT(scroll * .4) MOD 640
    FOR x = 0 TO 638 STEP 2
        LINE (x, NearTop((x + o) MOD 640))-(x + 1, GROUNDY), 7, BF
    NEXT
    FOR k = 0 TO 15
        tx = TreeX(k) - o
        IF tx < -20 THEN tx = tx + 640
        h = TreeH(k)
        b = NearTop(TreeX(k)) + 4
        FOR r = 0 TO h STEP 2
            hw = 1 + (r MOD (h \ 2 + 1)) * 5 \ (h \ 2 + 1) + r \ 6
            LINE (tx - hw, b - h + r)-(tx + hw, b - h + r + 1), 15, BF
        NEXT
    NEXT
END SUB

' The lake mirrors the rows above it, each row shifted by its ripple, and the moon's path on it.
SUB DrawLake
    DIM d AS INTEGER, ofs AS INTEGER, last AS INTEGER, k AS INTEGER, x AS INTEGER, y AS INTEGER, w AS INTEGER
    LINE (0, GROUNDY)-(639, 349), 0, BF
    last = 27 - shakeDY
    FOR d = 0 TO last
        ofs = INT((1 + d / 6) * SIN(clock * 2.2 + d * .7))
        GET (12, GROUNDY - 1 - d)-(627, GROUNDY - 1 - d), Strip
        PUT (12 + ofs, GROUNDY + d), Strip, PSET
    NEXT
    Dither 0, GROUNDY, 639, GROUNDY + last, 0, 1
    LINE (0, GROUNDY)-(639, GROUNDY), 2
    FOR d = 2 TO last STEP 3
        w = 12 - d \ 3 + INT(3 * SIN(clock * 5 + d))
        LINE (MOONX - w, GROUNDY + d)-(MOONX + w, GROUNDY + d), 5, , &HCCCC
    NEXT
    FOR k = 0 TO 10
        x = (k * 61 + INT(clock * 18)) MOD 640
        y = GROUNDY + 3 + (k * 7) MOD 22
        IF y <= GROUNDY + last THEN LINE (x, y)-(x + 5 + (k MOD 3) * 3, y), 5
    NEXT
END SUB

SUB DrawMoon
    DIM dy AS INTEGER, hw AS INTEGER, i AS INTEGER, x AS INTEGER, y AS INTEGER, w AS INTEGER, r AS INTEGER, h AS INTEGER
    FOR dy = -30 TO 30
        hw = 44 * SQR(1 - (dy / 30) ^ 2)
        Dither MOONX - hw, MOONY + dy, MOONX + hw, MOONY + dy, 2, 1
    NEXT
    FOR dy = -19 TO 19
        hw = 26 * SQR(1 - (dy / 19.5) ^ 2)
        LINE (MOONX - hw, MOONY + dy)-(MOONX + hw, MOONY + dy), 5
    NEXT
    CIRCLE (MOONX - 10, MOONY - 6), 6, 4
    CIRCLE (MOONX + 9, MOONY + 7), 4, 4
    CIRCLE (MOONX + 4, MOONY - 12), 2, 4
    ' clouds drift across it
    FOR i = 0 TO NCLOUD - 1
        x = (CloudX(i) + INT(clock * 7) + INT(scroll * .03)) MOD 900 - 130
        y = CloudY(i): w = CloudW(i): h = 8
        FOR r = -h TO h
            hw = w * SQR(1 - (r / (h + .5)) ^ 2) * (.8 + .2 * SIN(r * 1.1 + i * 2))
            IF r < -h \ 2 THEN
                Dither x - hw, y + r, x + hw, y + r, 2, 2
            ELSE
                Dither x - hw, y + r, x + hw, y + r, 6, 2
            END IF
        NEXT
    NEXT
END SUB

SUB DrawMoth
    DIM t AS INTEGER, w AS INTEGER, k AS INTEGER, x AS INTEGER, y AS INTEGER
    IF state = DYING OR state = OVER THEN
        t = 3: w = 0
    ELSEIF state = TITLE THEN
        t = 1
    ELSEIF vy < -100 THEN
        t = 0
    ELSEIF vy < 150 THEN
        t = 1
    ELSEIF vy < 380 THEN
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
    IF state = PLAYING AND vy > 380 THEN k = 13
    x = INT(mothX) - SPRW \ 2: y = INT(mothY) - SPRH \ 2
    PUT (x, y), Spr((NSPR + k) * SPRSZ), AND
    PUT (x, y), Spr(k * SPRSZ), OR
END SUB

' The game over panel drops in from above.
SUB DrawPanel
    DIM f AS SINGLE, y AS INTEGER
    f = stateT / .35
    IF f > 1 THEN f = 1
    y = 84 - INT((1 - f) ^ 3 * 340)
    LINE (180, y + 5)-(466, y + 181), 12, BF
    LINE (174, y)-(460, y + 176), 0, BF
    LINE (174, y)-(460, y + 176), 4, B
    LINE (177, y + 3)-(457, y + 173), 2, B
    TxtC "THE LIGHT FADES", y + 14, 2, 5, 4, 11, 2
    TxtC "SCORE" + STR$(score), y + 54, 2, 13, 13, 5, 2
    TxtC "BEST" + STR$(best), y + 86, 2, 13, 13, 5, 2
    IF newBest AND INT(clock * 4) MOD 2 = 0 THEN TxtC "NEW BEST!", y + 120, 1, 10, 10, 11, 1
    IF stateT > .6 THEN
        TxtC "SPACE TO FLY AGAIN", y + 140, 1, 13, 13, 13, 1
        TxtC "ESC FOR THE TITLE", y + 156, 1, 5, 5, 5, 1
    END IF
END SUB

SUB DrawSky
    DIM top(5) AS INTEGER, b AS INTEGER, i AS INTEGER, c AS INTEGER
    top(0) = 0: top(1) = 60: top(2) = 120: top(3) = 170: top(4) = 215: top(5) = GROUNDY
    FOR b = 0 TO 4
        LINE (0, top(b))-(639, top(b + 1) - 1), b, BF
    NEXT
    FOR b = 1 TO 4
        Dither 0, top(b) - 9, 639, top(b) - 7, b, 1
        Dither 0, top(b) - 6, 639, top(b) - 4, b, 2
        Dither 0, top(b) - 3, 639, top(b) - 1, b, 3
    NEXT
    IF sky <> DAWN THEN
        FOR i = 0 TO 39
            IF (INT(clock * 3) + i) MOD 9 THEN c = 13 ELSE c = 5
            PSET (StarX(i), StarY(i)), c
        NEXT
        IF shootLife > 0 THEN
            LINE (shootX - 40, shootY - 15)-(shootX, shootY), 5, , &HAAAA
            LINE (shootX - 14, shootY - 5)-(shootX, shootY), 13
        END IF
    END IF
END SUB

SUB DrawSparks
    DIM i AS INTEGER, c AS INTEGER, x AS INTEGER, y AS INTEGER, l AS SINGLE
    FOR i = 0 TO NSPARK - 1
        l = slife(i)
        IF l > 0 THEN
            IF l > .7 THEN
                c = 13
            ELSEIF l > .4 THEN
                c = 5
            ELSEIF l > .2 THEN
                c = 4
            ELSE
                c = 3
            END IF
            x = INT(sx(i)): y = INT(sy(i))
            PSET (x, y), c
            IF l > .5 THEN PSET (x + 1, y), c
        END IF
    NEXT
END SUB

' A stone from y1 to y2, its cap at the top (capTop) or at the bottom. Its kind: 1 runes,
' 2 vines, 3 a broken top, 4 mushrooms.
SUB DrawStone (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER, capTop AS INTEGER, kind AS INTEGER)
    DIM x2 AS INTEGER, y AS INTEGER, cy AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, m AS INTEGER, n AS INTEGER
    DIM c AS INTEGER, j AS INTEGER, h AS INTEGER, vx AS INTEGER, ox AS INTEGER
    x2 = x + STONEW - 1
    LINE (x, y1)-(x2, y2), 8, BF
    LINE (x + 4, y1)-(x + 8, y2), 4, BF
    LINE (x + 9, y1)-(x + 11, y2), 3, BF
    LINE (x2 - 11, y1)-(x2, y2), 9, BF
    FOR y = y1 + 7 TO y2 STEP 18
        LINE (x, y)-(x2, y), 9
        LINE (x + 4, y + 1)-(x + 11, y + 1), 8
    NEXT
    LINE (x, y1)-(x2, y2), 12, B
    IF capTop THEN cy = y1 ELSE cy = y2 - CAPH
    c1 = x - 6: c2 = x2 + 6

    IF capTop AND kind = 3 THEN
        FOR m = c1 TO c2 STEP 4
            h = CAPH - ((m - c1) * 7 + x) MOD 8
            LINE (m, cy + CAPH - h)-(m + 3, cy + CAPH), 8, BF
            LINE (m, cy + CAPH - h)-(m + 3, cy + CAPH - h), 4
        NEXT
        LINE (c1, cy + CAPH - 1)-(c2, cy + CAPH), 9, BF
        EXIT SUB
    END IF

    LINE (c1, cy)-(c2, cy + CAPH), 8, BF
    LINE (c1, cy)-(c2, cy + 1), 4, BF
    LINE (c1 + 2, cy)-(c1 + 7, cy + CAPH), 4, BF
    LINE (c2 - 9, cy + 2)-(c2, cy + CAPH), 9, BF
    LINE (c1, cy + CAPH - 1)-(c2, cy + CAPH), 9, BF
    LINE (c1, cy)-(c2, cy + CAPH), 12, B
    IF capTop THEN
        LINE (c1, cy - 1)-(c2, cy + 1), 14, BF
        FOR m = c1 + 1 TO c2 - 1 STEP 5
            LINE (m, cy - 3 - (m - c1) MOD 3)-(m + 1, cy), 14, BF
        NEXT
        IF kind = 4 THEN
            FOR j = 0 TO 2
                m = c1 + 9 + j * 23 - (j = 2) * 4
                LINE (m, cy - 6)-(m + 1, cy - 1), 13, BF
                LINE (m - 3, cy - 9)-(m + 4, cy - 7), 11, BF
                LINE (m - 2, cy - 10)-(m + 3, cy - 10), 11
                PSET (m - 1, cy - 8), 13: PSET (m + 2, cy - 9), 13
            NEXT
        END IF
        IF kind = 1 THEN
            IF SIN(clock * 3 + x * .05) > 0 THEN c = 5 ELSE c = 10
            j = 0
            FOR y = cy + CAPH + 14 TO y2 - 14 STEP 24
                DRAW "C" + STR$(c) + " BM" + STR$(x + 27) + "," + STR$(y) + Rune(j MOD 4)
                j = j + 1
            NEXT
        END IF
    ELSE
        FOR m = c1 + 3 TO c2 - 3 STEP 9
            n = 4 + ((m - c1) * 7) MOD 11
            LINE (m, cy + CAPH)-(m, cy + CAPH + n), 14
            PSET (m + 1, cy + CAPH + n), 14
            PSET (m - 1, cy + CAPH + n \ 2), 14
        NEXT
        IF kind = 2 THEN
            FOR j = 0 TO 2
                vx = x + 10 + j * 19
                ox = vx
                FOR y = 0 TO cy + CAPH + 26 STEP 6
                    m = vx + INT(3 * SIN(clock * 1.7 + y * .08 + j))
                    LINE (ox, y - 6)-(m, y), 14
                    IF (y \ 6 + j) MOD 3 = 0 THEN PSET (m + 2, y), 14: PSET (m - 2, y + 1), 14
                    ox = m
                NEXT
            NEXT
        END IF
    END IF
END SUB

FUNCTION Ega% (r AS INTEGER, g AS INTEGER, b AS INTEGER)
    Ega% = (b \ 2) + (g \ 2) * 2 + (r \ 2) * 4 + (b AND 1) * 8 + (g AND 1) * 16 + (r AND 1) * 32
END FUNCTION

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
            sx(i) = mothX - 10 + RND * 6
            sy(i) = mothY + RND * 8 - 4
            svx(i) = -60 - RND * 80 + (RND - .5) * kick
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
    Emit 20, 260, 200
    IF musicOn = 0 AND PLAY(0) < 3 THEN PLAY "T255MNL64O4AO5E"
END SUB

FUNCTION Hit%
    DIM i AS INTEGER, x AS SINGLE, t AS SINGLE, b AS SINGLE
    Hit% = 0
    FOR i = 0 TO NSTONE - 1
        x = stoneX(i)
        IF x < mothX + 20 AND x + STONEW > mothX - 20 THEN
            t = stoneGap(i) - GAP \ 2: b = stoneGap(i) + GAP \ 2
            IF RectHit%(x, 0, x + STONEW - 1, t - CAPH) THEN Hit% = -1
            IF RectHit%(x - 6, t - CAPH, x + STONEW + 5, t) THEN Hit% = -1
            IF RectHit%(x - 6, b, x + STONEW + 5, b + CAPH) THEN Hit% = -1
            IF RectHit%(x, b + CAPH, x + STONEW - 1, GROUNDY) THEN Hit% = -1
        END IF
    NEXT
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
    IF state <> TITLE THEN mothY = 175
    state = PLAYING
    stateT = 0
    score = 0
    newBest = 0
    sky = DUSK
    dimTarget = 0
    g = 170
    FOR i = 0 TO NSTONE - 1
        stoneX(i) = 720 + i * SPACING
        g = NextGap%(g)
        stoneGap(i) = g
        stoneDone(i) = 0
        stoneKind(i) = INT(RND * NKIND)
    NEXT
END SUB

FUNCTION NextGap% (prev AS INTEGER)
    DIM g AS INTEGER
    g = prev + INT(RND * 201) - 100
    IF g < GAP \ 2 + CAPH + 40 THEN g = GAP \ 2 + CAPH + 40
    IF g > GROUNDY - GAP \ 2 - CAPH - 30 THEN g = GROUNDY - GAP \ 2 - CAPH - 30
    NextGap% = g
END FUNCTION

' Whether the moth's body, round in screen units, meets the box.
FUNCTION RectHit% (x1 AS SINGLE, y1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE)
    DIM nx AS SINGLE, ny AS SINGLE, dx AS SINGLE, dy AS SINGLE
    nx = mothX: ny = mothY
    IF nx < x1 THEN nx = x1
    IF nx > x2 THEN nx = x2
    IF ny < y1 THEN ny = y1
    IF ny > y2 THEN ny = y2
    dx = mothX - nx: dy = (mothY - ny) * ASPECT
    RectHit% = dx * dx + dy * dy < 130
END FUNCTION

SUB Render
    DIM i AS INTEGER, x AS INTEGER, amp AS SINGLE, sc AS INTEGER
    shakeDX = 0: shakeDY = 0
    IF shakeT > 0 THEN
        amp = shakeT * 14
        shakeDX = INT(RND * amp): shakeDY = INT(RND * amp)
        LINE (0, 0)-(639, 349), 12, BF
        VIEW (shakeDX, shakeDY)-(639, 349)
    END IF

    DrawSky
    IF behind THEN DrawMoth
    DrawMoon
    DrawHills
    IF state <> TITLE THEN
        FOR i = 0 TO NSTONE - 1
            x = INT(stoneX(i))
            IF x < 646 AND x > -STONEW - 7 THEN
                DrawStone x, 0, stoneGap(i) - GAP \ 2, 0, stoneKind(i)
                DrawStone x, stoneGap(i) + GAP \ 2, GROUNDY, -1, stoneKind(i)
            END IF
        NEXT
    END IF
    IF NOT behind THEN
        DrawGlow
        DrawMoth
    END IF
    DrawLake
    DrawSparks

    SELECT CASE state
    CASE TITLE
        TxtC "MOTHLIGHT", 30, 5, 5, 4, 11, 3
        ' a gleam sweeps across the name
        x = INT(clock * 260) MOD 1100 - 200
        IF x >= 130 AND x <= 500 THEN
            VIEW SCREEN (x, 30)-(x + 18, 105)
            TxtC "MOTHLIGHT", 30, 5, 13, 13, 5, 0
            VIEW
        END IF
        TxtC "a moth, the moon and the standing stones", 108, 1, 5, 5, 5, 1
        IF INT(clock * 2) MOD 2 = 0 THEN TxtC "PRESS SPACE TO FLY", 222, 2, 13, 13, 5, 2
        IF best THEN TxtC "BEST" + STR$(best), 262, 1, 4, 4, 4, 1
        TxtC "SPACE, ENTER OR UP TO FLAP   M MUSIC   ESC TO QUIT", 298, 1, 13, 13, 13, 1
    CASE PLAYING, DYING
        sc = 3
        IF popT > 0 THEN sc = 3 + INT(popT * 7)
        TxtC LTRIM$(STR$(score)), 12 - (sc - 3) * 7, sc, 13, 5, 4, 2
    CASE OVER
        DrawPanel
    END SELECT
    IF flash > 0 THEN Dither 0, 0, 639, 349, 13, 1 + INT(flash * 12)

    IF shakeT > 0 THEN VIEW
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

' The palette in use, PalCur, darkened by dimLvl. A colour is levels of red, green and blue
' from 0 to 3, as r * 16 + g * 4 + b.
SUB SetPalette
    DIM a AS INTEGER, r AS INTEGER, g AS INTEGER, b AS INTEGER
    FOR a = 0 TO 15
        r = PalCur(a) \ 16 - dimLvl: g = (PalCur(a) \ 4) MOD 4 - dimLvl: b = PalCur(a) MOD 4 - dimLvl
        IF r < 0 THEN r = 0
        IF g < 0 THEN g = 0
        IF b < 0 THEN b = 0
        PALETTE a, Ega%(r, g, b)
    NEXT
END SUB

' The palettes for dusk, night and dawn, the hills, mountains, trees, stars, fireflies and clouds.
SUB Setup
    DIM i AS INTEGER, x AS INTEGER, k AS INTEGER, a AS SINGLE, s AS STRING, p AS INTEGER
    SCREEN 9
    FOR p = 0 TO 2
        SELECT CASE p
        CASE DUSK: s = "001102212311321332101011210100331310000333120010"
        CASE NIGHT: s = "000001101102112332001011111001331310000333010000"
        CASE DAWN: s = "102213323322332333212122321210331310000333120011"
        END SELECT
        FOR i = 0 TO 15
            PalSet(p, i) = VAL(MID$(s, i * 3 + 1, 1)) * 16 + VAL(MID$(s, i * 3 + 2, 1)) * 4 + VAL(MID$(s, i * 3 + 3, 1))
        NEXT
    NEXT
    FOR i = 0 TO 15: PalCur(i) = PalSet(DUSK, i): NEXT
    dimLvl = 3
    SetPalette

    RANDOMIZE TIMER
    FOR i = 0 TO 7: Bit(i) = 2 ^ (7 - i): NEXT
    FOR x = 0 TO 639
        a = x * 6.283185 / 640
        MtnTop(x) = 215 - (34 * ABS(SIN(1.5 * a + .3)) + 16 * ABS(SIN(4 * a + 1)) + 5 * ABS(SIN(11 * a)))
        FarTop(x) = 250 - (30 + 18 * SIN(2 * a) + 9 * SIN(5 * a + 1.3) + 4 * SIN(13 * a))
        NearTop(x) = 290 - (16 + 12 * SIN(3 * a + 2) + 7 * SIN(7 * a + .4) + 3 * SIN(17 * a))
    NEXT
    FOR k = 0 TO 15
        TreeX(k) = k * 40 + INT(RND * 24)
        TreeH(k) = 14 + INT(RND * 16)
    NEXT
    FOR i = 0 TO 39
        StarX(i) = INT(RND * 640)
        StarY(i) = INT(RND * 150)
    NEXT
    FOR i = 0 TO NFLY - 1
        FlyX(i) = INT(RND * 640)
        FlyY(i) = 205 + INT(RND * 70)
        FlyPh(i) = RND * 6.28
    NEXT
    FOR i = 0 TO NCLOUD - 1
        CloudX(i) = i * 300 + INT(RND * 100)
        CloudY(i) = 40 + i * 24
        CloudW(i) = 40 + INT(RND * 35)
    NEXT
    Rune(0) = "U10F5G5"
    Rune(1) = "U10R5D5L5"
    Rune(2) = "U10BR6D10BU5L6"
    Rune(3) = "E5F5BL5U10"
    shootT = 2
    drawPage = 1
END SUB

SUB StartSong (id AS INTEGER)
    SOUND 37, 0
    song = id
    beat = 0
    cue = ""
END SUB

SUB Tick (flapped AS INTEGER)
    DIM i AS INTEGER, j AS INTEGER, far AS INTEGER, a AS INTEGER, ch AS INTEGER
    DIM cur AS INTEGER, want AS INTEGER, v AS INTEGER, changed AS INTEGER
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
        ' circling the moon, behind it on the lower half
        mothX = MOONX + 46 * COS(clock * .8)
        mothY = MOONY + 16 + 50 * SIN(clock * .8)
        behind = SIN(clock * .8) > 0
        scroll = scroll + 50 * STEPT
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
            IF stoneX(i) + STONEW + 6 < 0 THEN
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
                popT = .3
                sky = (score \ 10) MOD 3
                IF musicOn THEN cue = "T150MNL32O4EGO5CEL8G" ELSE PLAY "T220MNL32O4EGO5CL16E"
                Emit 8, 200, 0
            END IF
        NEXT
        IF Hit% OR mothY > GROUNDY - 9 THEN Die
    CASE DYING
        vy = vy + GRAV * STEPT
        mothY = mothY + vy * STEPT
        IF mothY >= GROUNDY - 9 THEN
            mothY = GROUNDY - 9
            state = OVER
            stateT = 0
            dimTarget = 1
            Emit 16, 160, 260
            IF musicOn THEN
                IF newBest THEN PLAY "T160MNL16O4CEGO5L8CO4L16GO5L4C" ELSE PLAY "T110MLL8O3EDCO2BL4A"
            END IF
        END IF
    CASE OVER
        IF flapped AND stateT > .6 THEN
            NewGame
            Flap
        END IF
    END SELECT
    IF mothY < SPRH \ 2 + 2 THEN mothY = SPRH \ 2 + 2: vy = 0

    ' Esc faded the screen out: the title comes back in at dusk
    IF toTitle AND dimLvl = 3 THEN
        toTitle = 0
        state = TITLE
        sky = DUSK
        FOR a = 0 TO 15: PalCur(a) = PalSet(DUSK, a): NEXT
        dimTarget = 0
        StartSong TITLESONG
    END IF

    ' the sky steps a level a channel toward its palette; a fade steps the darkening
    changed = 0
    palT = palT + STEPT
    IF palT >= .3 THEN
        palT = 0
        FOR a = 0 TO 15
            cur = PalCur(a): want = PalSet(sky, a): v = 0
            FOR ch = 0 TO 2
                j = 4 ^ ch
                i = (cur \ j) MOD 4 - (want \ j) MOD 4
                IF i > 0 THEN v = v - j
                IF i < 0 THEN v = v + j
            NEXT
            IF v THEN PalCur(a) = cur + v: changed = -1
        NEXT
    END IF
    fadeT = fadeT + STEPT
    IF fadeT >= .08 AND dimLvl <> dimTarget THEN
        fadeT = 0
        IF dimLvl < dimTarget THEN dimLvl = dimLvl + 1 ELSE dimLvl = dimLvl - 1
        changed = -1
    END IF
    IF changed THEN SetPalette

    shootT = shootT - STEPT
    IF shootT <= 0 THEN
        shootT = 3 + RND * 5
        shootX = 40 + RND * 400: shootY = 10 + RND * 60
        shootLife = .55
    END IF
    IF shootLife > 0 THEN
        shootX = shootX + 520 * STEPT: shootY = shootY + 190 * STEPT
        shootLife = shootLife - STEPT
    END IF

    IF state <= PLAYING THEN
        sparkT = sparkT + STEPT
        IF sparkT > .05 THEN
            sparkT = 0
            Emit 1, 30, 0
        END IF
    END IF
    FOR i = 0 TO NSPARK - 1
        IF slife(i) > 0 THEN
            sx(i) = sx(i) + svx(i) * STEPT
            sy(i) = sy(i) + svy(i) * STEPT
            svx(i) = svx(i) * (1 - 1.5 * STEPT)
            svy(i) = svy(i) * (1 - 2 * STEPT) - 20 * STEPT
            slife(i) = slife(i) - STEPT
        END IF
    NEXT
END SUB

' Text from the ROM font, sc times as big, without a background: rows 0-4 in c1, 5-8 in c2,
' the rest in c3, over a shadow sh pixels down and right.
SUB Txt (t AS STRING, x AS INTEGER, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, c3 AS INTEGER, sh AS INTEGER)
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
            FOR r = 0 TO 13
                b = Glyph(ch, r)
                IF b THEN
                    IF pass = 0 THEN
                        c = 12
                    ELSEIF r < 5 THEN
                        c = c1
                    ELSEIF r < 9 THEN
                        c = c2
                    ELSE
                        c = c3
                    END IF
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

SUB TxtC (t AS STRING, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, c3 AS INTEGER, sh AS INTEGER)
    Txt t, (640 - LEN(t) * 8 * sc) \ 2, y, sc, c1, c2, c3, sh
END SUB
