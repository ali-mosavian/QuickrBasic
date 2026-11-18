' The moth's sprites, drawn: tools/mothassets.py runs this with MOTHLITE.BAS
' and writes the pictures into it as DATA.

TYPE Oval
    cx AS SINGLE
    cy AS SINGLE
    an AS SINGLE
    a AS SINGLE
    b AS SINGLE
END TYPE

' The moth's outline in its own frame, for a pose.
TYPE Shape
    tilt AS SINGLE
    sx AS SINGLE
    sy AS SINGLE
    fore AS Oval
    hind AS Oval
    neck AS Oval
    body AS Oval
END TYPE

DECLARE SUB BuildMoth ()
DECLARE SUB Pose (k AS INTEGER, s AS Shape)
DECLARE SUB Sketch (g() AS INTEGER, s AS Shape)
DECLARE SUB Outline (g() AS INTEGER)
DECLARE SUB Capture (g() AS INTEGER, k AS INTEGER)
DECLARE FUNCTION MothPix% (lx AS SINGLE, ly AS SINGLE, s AS Shape)
DECLARE FUNCTION WingPix% (dx AS SINGLE, dy AS SINGLE, e AS SINGLE)
DECLARE FUNCTION Feeler% (lx AS SINGLE, ly AS SINGLE)
DECLARE FUNCTION Ell! (px AS SINGLE, py AS SINGLE, o AS Oval)
DECLARE FUNCTION SegD! (px AS SINGLE, py AS SINGLE, ax AS SINGLE, ay AS SINGLE, bx AS SINGLE, by AS SINGLE)

' Each sprite with a mask for PUT AND, then OR.
SUB BuildMoth
    DIM g(SPRW + 1, SPRH + 1) AS INTEGER, s AS Shape
    DIM k AS INTEGER
    FOR k = 0 TO NSPR - 1
        Pose k, s
        Sketch g(), s
        Outline g()
        Capture g(), k
    NEXT
END SUB

SUB Capture (g() AS INTEGER, k AS INTEGER)
    DIM px AS INTEGER, py AS INTEGER, c AS INTEGER
    FOR py = 1 TO SPRH
        FOR px = 1 TO SPRW
            PSET (px - 1, py - 1), g(px, py)
        NEXT
    NEXT
    GET (0, 0)-(SPRW - 1, SPRH - 1), Spr(k * SPRSZ)
    FOR py = 1 TO SPRH
        FOR px = 1 TO SPRW
            c = CMASK
            IF g(px, py) THEN c = 0
            PSET (px - 1, py - 1), c
        NEXT
    NEXT
    GET (0, 0)-(SPRW - 1, SPRH - 1), Spr((NSPR + k) * SPRSZ)
END SUB

' Where (px, py) is in the oval: under 1 inside.
FUNCTION Ell! (px AS SINGLE, py AS SINGLE, o AS Oval)
    DIM qx AS SINGLE, qy AS SINGLE, u AS SINGLE
    DIM v AS SINGLE, ca AS SINGLE, sa AS SINGLE
    qx = px - o.cx: qy = py - o.cy
    ca = COS(o.an): sa = SIN(o.an)
    u = (qx * ca + qy * sa) / o.a
    v = (qy * ca - qx * sa) / o.b
    Ell! = u * u + v * v
END FUNCTION

FUNCTION Feeler% (lx AS SINGLE, ly AS SINGLE)
    DIM f AS INTEGER
    f = SegD!(lx, ly, 10, -4, 17, -13) < 1.2
    f = f OR SegD!(lx, ly, 8, -4, 12, -14) < 1.1
    f = f OR (lx - 17) ^ 2 + (ly + 13) ^ 2 < 5
    Feeler% = f OR (lx - 11.5) ^ 2 + (ly + 1) ^ 2 < 5
END FUNCTION

' The moth's colour at (lx, ly) in its own frame.
FUNCTION MothPix% (lx AS SINGLE, ly AS SINGLE, s AS Shape)
    DIM e AS SINGLE, dx AS SINGLE, dy AS SINGLE
    MothPix% = 0
    e = Ell!(lx, ly, s.fore)
    IF Feeler%(lx, ly) THEN
        MothPix% = CINK
    ELSEIF (lx - 9) ^ 2 + ly ^ 2 < 21 THEN
        MothPix% = CWEDGE
    ELSEIF e < 1 THEN
        dx = lx - s.fore.cx
        dy = ly - s.fore.cy
        MothPix% = WingPix%(dx, dy, e)
    ELSEIF Ell!(lx, ly, s.neck) < 1 THEN
        MothPix% = CCOLLAR
    ELSEIF Ell!(lx, ly, s.body) < 1 THEN
        MothPix% = CBODY
        IF lx < 0 THEN
            IF (INT(lx + 40) \ 4) MOD 2 = 0 THEN
                MothPix% = CSTRIPE
            END IF
        END IF
    ELSE
        e = Ell!(lx, ly, s.hind)
        IF e < 1 THEN MothPix% = CHIND
        IF e < 1 AND e > .6 THEN MothPix% = CHINDE
    END IF
END FUNCTION

' An outline of ink round the shape.
SUB Outline (g() AS INTEGER)
    DIM px AS INTEGER, py AS INTEGER, n AS INTEGER
    FOR py = 1 TO SPRH
        FOR px = 1 TO SPRW
            IF g(px, py) = 0 THEN
                n = g(px - 1, py) > 0 OR g(px + 1, py) > 0
                n = n OR g(px, py - 1) > 0
                n = n OR g(px, py + 1) > 0
                IF n THEN g(px, py) = -1
            END IF
        NEXT
    NEXT
    FOR py = 1 TO SPRH
        FOR px = 1 TO SPRW
            IF g(px, py) < 0 THEN g(px, py) = CINK
        NEXT
    NEXT
END SUB

' Sprite k's tilt, stretch and wings: 3 wing
' positions by 4 tilts, then squashed (just
' flapped) and stretched (diving).
SUB Pose (k AS INTEGER, s AS Shape)
    DIM t AS INTEGER, w AS INTEGER, a AS SINGLE
    DIM l AS SINGLE, d AS SINGLE, h AS SINGLE
    s.sx = 1: s.sy = 1
    IF k < 12 THEN
        t = k \ 3: w = k MOD 3
    ELSEIF k = 12 THEN
        t = 1: w = 2: s.sx = 1.18: s.sy = .82
    ELSE
        t = 3: w = 0: s.sx = .86: s.sy = 1.12
    END IF
    s.tilt = -.4
    IF t = 1 THEN s.tilt = 0
    IF t = 2 THEN s.tilt = .45
    IF t = 3 THEN s.tilt = 1
    a = -1.85: l = 14: d = 7.5
    IF w = 1 THEN a = -2.55: l = 8
    IF w = 2 THEN a = 1.75: l = 12: d = 6.5
    s.fore.cx = -1 + .85 * l * COS(a)
    s.fore.cy = -2 + .85 * l * SIN(a)
    s.fore.an = a: s.fore.a = l: s.fore.b = d
    h = a + .3
    s.hind.cx = -5 + .7 * l * COS(h)
    s.hind.cy = -3 + .7 * l * SIN(h)
    s.hind.an = h: s.hind.a = .8 * l: s.hind.b = .9 * d
    s.neck.cx = 5: s.neck.cy = 0: s.neck.an = 1.5708
    s.neck.a = 5: s.neck.b = 2.5
    s.body.cx = -3: s.body.cy = 1: s.body.an = 0
    s.body.a = 10.5: s.body.b = 5.2
END SUB

' The distance from (px, py) to the segment a-b.
FUNCTION SegD! (px AS SINGLE, py AS SINGLE, ax AS SINGLE, ay AS SINGLE, bx AS SINGLE, by AS SINGLE)
    DIM dx AS SINGLE, dy AS SINGLE, t AS SINGLE
    DIM ex AS SINGLE, ey AS SINGLE
    dx = bx - ax: dy = by - ay
    t = ((px - ax) * dx + (py - ay) * dy)
    t = t / (dx * dx + dy * dy)
    IF t < 0 THEN t = 0
    IF t > 1 THEN t = 1
    ex = ax + t * dx - px: ey = ay + t * dy - py
    SegD! = SQR(ex * ex + ey * ey)
END FUNCTION

' Sprite pixels from the shape, turned by the tilt.
SUB Sketch (g() AS INTEGER, s AS Shape)
    DIM px AS INTEGER, py AS INTEGER
    DIM ct AS SINGLE, st AS SINGLE, wx AS SINGLE
    DIM wy AS SINGLE, lx AS SINGLE, ly AS SINGLE
    ct = COS(s.tilt): st = SIN(s.tilt)
    FOR py = 1 TO SPRH
        wy = (py - .5 - SPRH / 2) * ASPECT / MSCALE
        FOR px = 1 TO SPRW
            wx = (px - .5 - SPRW / 2) / MSCALE
            lx = (wx * ct + wy * st) / s.sx
            ly = (wy * ct - wx * st) / s.sy
            g(px, py) = MothPix%(lx, ly, s)
        NEXT
    NEXT
END SUB

' Inside the forewing: a white eyespot, an ink ring
' round it, an orange edge.
FUNCTION WingPix% (dx AS SINGLE, dy AS SINGLE, e AS SINGLE)
    DIM d AS SINGLE
    d = dx * dx + dy * dy
    IF (dx - .8) ^ 2 + (dy + .8) ^ 2 < 1.6 THEN
        WingPix% = CWHITE
    ELSEIF d < 5 THEN
        WingPix% = CINK
    ELSEIF d < 11 OR e > .7 THEN
        WingPix% = CWEDGE
    ELSE
        WingPix% = CWING
    END IF
END FUNCTION
