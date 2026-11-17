' MOTHLIGHT: a moth flies by moonlight through the standing stones.
' Space, Enter or Up flaps; Esc quits. Driven by TIMER: the same speed on any machine.
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
DECLARE SUB DrawHills ()
DECLARE SUB DrawStone (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER, capTop AS INTEGER)
DECLARE SUB DrawGround ()
DECLARE SUB DrawSparks ()
DECLARE SUB DrawMoth ()
DECLARE SUB DrawPanel ()
DECLARE SUB Emit (n AS INTEGER, spread AS SINGLE)
DECLARE SUB Txt (t AS STRING, x AS INTEGER, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, c3 AS INTEGER, sh AS INTEGER)
DECLARE SUB TxtC (t AS STRING, y AS INTEGER, sc AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, c3 AS INTEGER, sh AS INTEGER)
DECLARE SUB Dither (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, c AS INTEGER, level AS INTEGER)
DECLARE FUNCTION Ega% (r AS INTEGER, g AS INTEGER, b AS INTEGER)
DECLARE FUNCTION NextGap% (prev AS INTEGER)
DECLARE FUNCTION Hit% ()
DECLARE FUNCTION RectHit% (x1 AS SINGLE, y1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE)
DECLARE FUNCTION Ell! (px AS SINGLE, py AS SINGLE, cx AS SINGLE, cy AS SINGLE, ang AS SINGLE, a AS SINGLE, b AS SINGLE)
DECLARE FUNCTION SegD! (px AS SINGLE, py AS SINGLE, ax AS SINGLE, ay AS SINGLE, bx AS SINGLE, by AS SINGLE)

CONST GROUNDY = 322, MOTHX = 170, ASPECT = 1.37!
CONST NSTONE = 4, STONEW = 58, CAPH = 12, GAP = 116, SPACING = 240
CONST SPRW = 64, SPRH = 56, SPRSZ = 900, MSCALE = 1.35!, NSPR = 12, NSPARK = 48
CONST GRAV = 1150!, FLAPV = -330!, SPEED = 140!, STEPT = 1! / 120!
CONST TITLE = 0, PLAYING = 1, DYING = 2, OVER = 3

'$DYNAMIC
DIM SHARED state AS INTEGER, score AS INTEGER, best AS INTEGER, newBest AS INTEGER, drawPage AS INTEGER
DIM SHARED mothY AS SINGLE, vy AS SINGLE, scroll AS SINGLE, clock AS SINGLE, phase AS SINGLE
DIM SHARED boost AS SINGLE, flash AS SINGLE, stateT AS SINGLE, sparkT AS SINGLE
DIM SHARED stoneX(NSTONE - 1) AS SINGLE, stoneGap(NSTONE - 1) AS INTEGER, stoneDone(NSTONE - 1) AS INTEGER
DIM SHARED FarTop(639) AS INTEGER, NearTop(639) AS INTEGER
DIM SHARED TreeX(15) AS INTEGER, TreeH(15) AS INTEGER, StarX(39) AS INTEGER, StarY(39) AS INTEGER
DIM SHARED Glyph(126, 13) AS INTEGER, Bit(7) AS INTEGER
DIM SHARED Spr(SPRSZ * NSPR * 2 - 1) AS INTEGER
DIM SHARED sx(NSPARK - 1) AS SINGLE, sy(NSPARK - 1) AS SINGLE
DIM SHARED svx(NSPARK - 1) AS SINGLE, svy(NSPARK - 1) AS SINGLE, slife(NSPARK - 1) AS SINGLE

DIM t AS SINGLE, lastT AS SINGLE, dt AS SINGLE, acc AS SINGLE
DIM flapped AS INTEGER, k AS STRING

Setup
BuildGlyphs
BuildMoth
PLAY "MBT140L16O4EGBO5L8E"

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
        CASE CHR$(27)
            IF state = TITLE THEN EXIT DO
            state = TITLE
        END SELECT
    LOOP WHILE LEN(k)
    IF k = CHR$(27) THEN EXIT DO

    acc = acc + dt
    DO WHILE acc >= STEPT
        Tick flapped
        flapped = 0
        acc = acc - STEPT
    LOOP
    Render
LOOP

SCREEN 0
WIDTH 80
END

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
' 3 wing positions by 4 tilts, each with a mask for PUT AND, then OR.
SUB BuildMoth
    DIM g(SPRW + 1, SPRH + 1) AS INTEGER
    DIM tl(3) AS SINGLE, wa(2) AS SINGLE, wl(2) AS SINGLE, ww(2) AS SINGLE
    DIM t AS INTEGER, w AS INTEGER, px AS INTEGER, py AS INTEGER, c AS INTEGER, k AS INTEGER
    DIM ct AS SINGLE, st AS SINGLE, fx AS SINGLE, fy AS SINGLE, ha AS SINGLE, hx AS SINGLE, hy AS SINGLE
    DIM wx AS SINGLE, wy AS SINGLE, lx AS SINGLE, ly AS SINGLE, d AS SINGLE, e AS SINGLE
    tl(0) = -.4: tl(1) = 0: tl(2) = .45: tl(3) = 1
    wa(0) = -1.85: wl(0) = 14: ww(0) = 7.5
    wa(1) = -2.55: wl(1) = 8: ww(1) = 7.5
    wa(2) = 1.75: wl(2) = 12: ww(2) = 6.5

    SCREEN , , 1, 0
    FOR t = 0 TO 3
        ct = COS(tl(t)): st = SIN(tl(t))
        FOR w = 0 TO 2
            fx = -1 + .85 * wl(w) * COS(wa(w)): fy = -2 + .85 * wl(w) * SIN(wa(w))
            ha = wa(w) + .3
            hx = -5 + .7 * wl(w) * COS(ha): hy = -3 + .7 * wl(w) * SIN(ha)
            FOR py = 1 TO SPRH
                FOR px = 1 TO SPRW
                    wx = (px - 1 - SPRW / 2 + .5) / MSCALE: wy = (py - 1 - SPRH / 2 + .5) * ASPECT / MSCALE
                    lx = wx * ct + wy * st: ly = -wx * st + wy * ct
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
            k = t * 3 + w
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
    NEXT
END SUB

SUB Die
    state = DYING
    stateT = 0
    flash = .18
    vy = -120
    PLAY "MBT150L32O3CO2GEL8C"
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

SUB DrawGround
    DIM o AS INTEGER, x AS INTEGER
    LINE (0, GROUNDY)-(639, 349), 15, BF
    LINE (0, GROUNDY)-(639, GROUNDY + 1), 14, BF
    o = INT(scroll) MOD 24
    FOR x = -o TO 639 STEP 24
        LINE (x, GROUNDY - 3)-(x + 1, GROUNDY), 14, BF
        LINE (x + 5, GROUNDY - 2)-(x + 6, GROUNDY), 14, BF
        LINE (x + 13, GROUNDY - 4)-(x + 13, GROUNDY), 14
    NEXT
    o = INT(scroll) MOD 96
    FOR x = -o TO 639 STEP 96
        PSET (x + 40, GROUNDY - 5), 10
        PSET (x + 72, GROUNDY - 6), 5
        LINE (x + 9, GROUNDY + 9)-(x + 12, GROUNDY + 9), 7
        LINE (x + 58, GROUNDY + 17)-(x + 60, GROUNDY + 17), 7
        PSET (x + 30, GROUNDY + 22), 7
    NEXT
END SUB

SUB DrawHills
    DIM o AS INTEGER, x AS INTEGER, y AS INTEGER, k AS INTEGER, tx AS INTEGER
    DIM h AS INTEGER, b AS INTEGER, r AS INTEGER, hw AS INTEGER
    o = INT(scroll * .15) MOD 640
    FOR x = 0 TO 638 STEP 2
        y = FarTop((x + o) MOD 640)
        LINE (x, y)-(x + 1, GROUNDY), 6, BF
        LINE (x, y)-(x + 1, y), 2
    NEXT
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
    x = MOTHX - SPRW \ 2: y = INT(mothY) - SPRH \ 2
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
    DIM top(5) AS INTEGER, b AS INTEGER, i AS INTEGER, c AS INTEGER, dy AS INTEGER, hw AS INTEGER
    top(0) = 0: top(1) = 60: top(2) = 120: top(3) = 170: top(4) = 215: top(5) = GROUNDY
    FOR b = 0 TO 4
        LINE (0, top(b))-(639, top(b + 1) - 1), b, BF
    NEXT
    FOR b = 1 TO 4
        Dither 0, top(b) - 9, 639, top(b) - 7, b, 1
        Dither 0, top(b) - 6, 639, top(b) - 4, b, 2
        Dither 0, top(b) - 3, 639, top(b) - 1, b, 3
    NEXT
    FOR i = 0 TO 39
        IF (INT(clock * 3) + i) MOD 9 THEN c = 13 ELSE c = 5
        PSET (StarX(i), StarY(i)), c
    NEXT
    FOR dy = -30 TO 30
        hw = 44 * SQR(1 - (dy / 30) ^ 2)
        Dither 560 - hw, 64 + dy, 560 + hw, 64 + dy, 2, 1
    NEXT
    FOR dy = -19 TO 19
        hw = 26 * SQR(1 - (dy / 19.5) ^ 2)
        LINE (560 - hw, 64 + dy)-(560 + hw, 64 + dy), 5
    NEXT
    CIRCLE (550, 58), 6, 4
    CIRCLE (569, 71), 4, 4
    CIRCLE (564, 52), 2, 4
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

' A stone from y1 to y2, its cap at the top (capTop) or at the bottom.
SUB DrawStone (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER, capTop AS INTEGER)
    DIM x2 AS INTEGER, y AS INTEGER, cy AS INTEGER, c1 AS INTEGER, c2 AS INTEGER, m AS INTEGER, n AS INTEGER
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
    ELSE
        FOR m = c1 + 3 TO c2 - 3 STEP 9
            n = 4 + ((m - c1) * 7) MOD 11
            LINE (m, cy + CAPH)-(m, cy + CAPH + n), 14
            PSET (m + 1, cy + CAPH + n), 14
            PSET (m - 1, cy + CAPH + n \ 2), 14
        NEXT
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

SUB Emit (n AS INTEGER, spread AS SINGLE)
    DIM i AS INTEGER, todo AS INTEGER
    todo = n
    FOR i = 0 TO NSPARK - 1
        IF todo = 0 THEN EXIT FOR
        IF slife(i) <= 0 THEN
            sx(i) = MOTHX - 10 + RND * 6
            sy(i) = mothY + RND * 8 - 4
            svx(i) = -60 - RND * 80
            svy(i) = (RND - .5) * spread
            slife(i) = .6 + RND * .5
            todo = todo - 1
        END IF
    NEXT
END SUB

SUB Flap
    vy = FLAPV
    boost = .25
    Emit 5, 120
    IF PLAY(0) < 3 THEN PLAY "MBT255L64O4AO5E"
END SUB

FUNCTION Hit%
    DIM i AS INTEGER, x AS SINGLE, t AS SINGLE, b AS SINGLE
    Hit% = 0
    FOR i = 0 TO NSTONE - 1
        x = stoneX(i)
        IF x < MOTHX + 20 AND x + STONEW > MOTHX - 20 THEN
            t = stoneGap(i) - GAP \ 2: b = stoneGap(i) + GAP \ 2
            IF RectHit%(x, 0, x + STONEW - 1, t - CAPH) THEN Hit% = -1
            IF RectHit%(x - 6, t - CAPH, x + STONEW + 5, t) THEN Hit% = -1
            IF RectHit%(x - 6, b, x + STONEW + 5, b + CAPH) THEN Hit% = -1
            IF RectHit%(x, b + CAPH, x + STONEW - 1, GROUNDY) THEN Hit% = -1
        END IF
    NEXT
END FUNCTION

SUB NewGame
    DIM i AS INTEGER, g AS INTEGER
    state = PLAYING
    stateT = 0
    score = 0
    newBest = 0
    mothY = 175
    g = 170
    FOR i = 0 TO NSTONE - 1
        stoneX(i) = 720 + i * SPACING
        g = NextGap%(g)
        stoneGap(i) = g
        stoneDone(i) = 0
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
    nx = MOTHX: ny = mothY
    IF nx < x1 THEN nx = x1
    IF nx > x2 THEN nx = x2
    IF ny < y1 THEN ny = y1
    IF ny > y2 THEN ny = y2
    dx = MOTHX - nx: dy = (mothY - ny) * ASPECT
    RectHit% = dx * dx + dy * dy < 130
END FUNCTION

SUB Render
    DIM i AS INTEGER, x AS INTEGER
    DrawSky
    DrawHills
    IF state <> TITLE THEN
        FOR i = 0 TO NSTONE - 1
            x = INT(stoneX(i))
            IF x < 646 AND x > -STONEW - 7 THEN
                DrawStone x, 0, stoneGap(i) - GAP \ 2, 0
                DrawStone x, stoneGap(i) + GAP \ 2, GROUNDY, -1
            END IF
        NEXT
    END IF
    DrawGround
    DrawSparks
    DrawMoth

    SELECT CASE state
    CASE TITLE
        TxtC "MOTHLIGHT", 30, 5, 5, 4, 11, 3
        TxtC "a moth, the moon and the standing stones", 108, 1, 5, 5, 5, 1
        IF INT(clock * 2) MOD 2 = 0 THEN TxtC "PRESS SPACE TO FLY", 222, 2, 13, 13, 5, 2
        IF best THEN TxtC "BEST" + STR$(best), 262, 1, 4, 4, 4, 1
        TxtC "SPACE, ENTER OR UP TO FLAP   ESC TO QUIT", 298, 1, 13, 13, 13, 1
    CASE PLAYING, DYING
        TxtC LTRIM$(STR$(score)), 12, 3, 13, 5, 4, 2
    CASE OVER
        DrawPanel
    END SELECT
    IF flash > 0 THEN Dither 0, 0, 639, 349, 13, 1 + INT(flash * 12)

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

' The palette, by levels of red, green and blue from 0 to 3, and the hills, trees and stars.
SUB Setup
    DIM i AS INTEGER, x AS INTEGER, k AS INTEGER, a AS SINGLE
    SCREEN 9
    PALETTE 0, Ega%(0, 0, 1)
    PALETTE 1, Ega%(1, 0, 2)
    PALETTE 2, Ega%(2, 1, 2)
    PALETTE 3, Ega%(3, 1, 1)
    PALETTE 4, Ega%(3, 2, 1)
    PALETTE 5, Ega%(3, 3, 2)
    PALETTE 6, Ega%(1, 0, 1)
    PALETTE 7, Ega%(0, 1, 1)
    PALETTE 8, Ega%(2, 1, 0)
    PALETTE 9, Ega%(1, 0, 0)
    PALETTE 10, Ega%(3, 3, 1)
    PALETTE 11, Ega%(3, 1, 0)
    PALETTE 12, Ega%(0, 0, 0)
    PALETTE 13, Ega%(3, 3, 3)
    PALETTE 14, Ega%(1, 2, 0)
    PALETTE 15, Ega%(0, 1, 0)
    RANDOMIZE TIMER
    FOR i = 0 TO 7: Bit(i) = 2 ^ (7 - i): NEXT
    FOR x = 0 TO 639
        a = x * 6.283185 / 640
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
    drawPage = 1
END SUB

SUB Tick (flapped AS INTEGER)
    DIM i AS INTEGER, j AS INTEGER, far AS INTEGER
    clock = clock + STEPT
    stateT = stateT + STEPT
    flash = flash - STEPT
    boost = boost - STEPT
    IF boost > 0 THEN phase = phase + STEPT * 9 ELSE phase = phase + STEPT * 4

    SELECT CASE state
    CASE TITLE
        mothY = 175 + 9 * SIN(clock * 2.6)
        scroll = scroll + 50 * STEPT
        IF flapped THEN
            NewGame
            Flap
        END IF
    CASE PLAYING
        IF flapped THEN Flap
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
            END IF
            IF stoneDone(i) = 0 AND stoneX(i) + STONEW < MOTHX THEN
                stoneDone(i) = -1
                score = score + 1
                PLAY "MBT220L32O5EGO6CL16E"
                Emit 8, 200
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
        END IF
    CASE OVER
        IF flapped AND stateT > .6 THEN
            NewGame
            Flap
        END IF
    END SELECT
    IF mothY < SPRH \ 2 + 2 THEN mothY = SPRH \ 2 + 2: vy = 0

    IF state <= PLAYING THEN
        sparkT = sparkT + STEPT
        IF sparkT > .05 THEN
            sparkT = 0
            Emit 1, 30
        END IF
    END IF
    FOR i = 0 TO NSPARK - 1
        IF slife(i) > 0 THEN
            sx(i) = sx(i) + svx(i) * STEPT
            sy(i) = sy(i) + svy(i) * STEPT
            svy(i) = svy(i) - 20 * STEPT
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
