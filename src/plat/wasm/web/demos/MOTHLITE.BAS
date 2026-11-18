' MOTHLIGHT: a little moth's big day out.
' Space, Enter or Up flaps; M mutes; Esc quits.
' SCREEN 13 on two pages: one drawn, one shown.
' Each subsystem owns a record; World holds them
' all. The loop reads input, updates, then draws.

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

TYPE Moth
    x AS SINGLE
    y AS SINGLE
    vy AS SINGLE
    beat AS SINGLE
    boost AS SINGLE
    squash AS SINGLE
    behind AS INTEGER
END TYPE

TYPE Stone
    x AS SINGLE
    gap AS INTEGER
    passed AS INTEGER
    kind AS INTEGER
END TYPE

TYPE Spark
    x AS SINGLE
    y AS SINGLE
    vx AS SINGLE
    vy AS SINGLE
    life AS SINGLE
END TYPE

TYPE Burst
    n AS INTEGER
    spread AS SINGLE
    kick AS SINGLE
END TYPE

TYPE Fx
    flash AS SINGLE
    shake AS SINGLE
    pop AS SINGLE
END TYPE

TYPE Sky
    phase AS INTEGER
    fade AS SINGLE
    fadeTo AS SINGLE
    ease AS SINGLE
END TYPE

' The music: the song and its beat; the sky's mood,
' latched each bar, and the score, for the layers.
TYPE Song
    id AS INTEGER
    beat AS INTEGER
    playing AS INTEGER
    mood AS INTEGER
    bar AS INTEGER
    heat AS INTEGER
END TYPE

TYPE Game
    mode AS INTEGER
    modeT AS SINGLE
    clock AS SINGLE
    scroll AS SINGLE
    score AS INTEGER
    best AS INTEGER
    newBest AS INTEGER
    trail AS SINGLE
    toTitle AS INTEGER
    quitting AS INTEGER
END TYPE

TYPE World
    g AS Game
    m AS Moth
    fx AS Fx
    sky AS Sky
    song AS Song
    page AS INTEGER
END TYPE

TYPE Pad
    flap AS INTEGER
    mute AS INTEGER
    back AS INTEGER
END TYPE

TYPE Look
    sc AS INTEGER
    c1 AS INTEGER
    c2 AS INTEGER
    sh AS INTEGER
END TYPE

TYPE Ridge
    speed AS SINGLE
    c AS INTEGER
    d1 AS INTEGER
    d2 AS INTEGER
    band AS INTEGER
    bottom AS INTEGER
    stp AS INTEGER
END TYPE

CONST SW = 320, SH = 200, GROUNDY = 184, LAKEH = 16
CONST HOMEX = 85, ASPECT = 1.2!, STEPT = 1! / 120!
CONST NSTONE = 4, STONEW = 29, OVERHANG = 3
CONST CAPH = 7, GAP = 66, SPACING = 124, NKIND = 5
CONST SPRW = 34, SPRH = 30, SPRSZ = 256
CONST MSCALE = .62!, NSPR = 14, NSPARK = 64
CONST GRAV = 660!, FLAPV = -190!, SPEED = 72!
CONST TITLE = 0, PLAYING = 1, DYING = 2, OVER = 3
CONST NOSONG = 0, TITLESONG = 1, GAMESONG = 2
CONST MAXBEATS = 64, SUNX = 258, SUNY = 58
' the score the faster hi-hats come in at; how
' near a cap is a near miss
CONST HEAT2 = 8, NEARMISS = 9
CONST NFLY = 10, NCLOUD = 3, NTREE = 16
CONST DAY = 0, GOLDEN = 1, SUNSET = 2, NSKIES = 3
CONST NKEY = 15, NRIDGE = 3
' the layers' sounds: the bass and its sub an octave
' under, the tune's echo; the arpeggio and the
' title's pad by day, at golden hour and at sunset
CONST BASSSND = "MSW0Q40Y1,1600,4H0,3,6,2V15"
CONST SUBSND = "MNW1H0,2,10,3V15"
CONST ECHOSND = "MNW1Y1,1800,4H0,6,6,6V7"
CONST ARPDAY = "MNW0Q25H0,3,6,2V6"
CONST ARPGOLD = "MNW2Y1,1400,6H0,4,7,3V6"
CONST ARPDUSK = "MNW1H0,5,8,4V7"
CONST PADDAY = "MLW0Q30Y1,2400,6H6,6,10,8V7"
CONST PADGOLD = "MLW2Y1,900,6H9,6,10,9V8"
CONST PADDUSK = "MLW1H11,8,11,10V9"
' sound effects, sparks and text looks, by table
CONST SFXCRASH = 0, SFXFLAP = 1, SFXCHIME = 2
CONST SFXSWISH = 3, SFXBEST = 4, SFXOVER = 5
CONST NSFX = 6
CONST BTRAIL = 0, BFLAP = 1, BSCORE = 2, BLAND = 3
CONST NBURST = 4
CONST LNAME = 0, LGLEAM = 1, LSMALL = 2
CONST LPROMPT = 3, LBEST = 4, LSCORE = 5, LPOP = 6
CONST LHEAD = 7, LBIGW = 8, LBIGG = 9, LNEW = 10
CONST LSOFT = 11, NLOOK = 12
' The colours. The lake ORs 64 into what it mirrors,
' so 0-15 and 128-191 have water-tinted copies 64
' up; the rest are only drawn above its reach.
CONST CINK = 1, CWHITE = 2, CWING = 3, CWEDGE = 4
CONST CBODY = 5, CSTRIPE = 6, CCOLLAR = 7
CONST CHIND = 8, CHINDE = 9, CSPARK = 10
CONST CSKY = 16, NSKY = 48, CTITLE = 80
CONST CPANEL = 88, CPANEL2 = 89, CSUN = 90
CONST CRAY = 91, CCHEEK = 92, CCLOUD = 93
CONST CCLOUDLO = 94, CMASK = 255, CMTN = 128
CONST CFAR = 144, CNEAR = 160, CTREE = 176
CONST CSTONE = 180, CMOSS = 188, CRUNE = 189
CONST CSHROOM = 190, CVINE = 191, WATER = 64
' The pictures drawn once and PUT after: the top of
' the screen (sky, clouds; the sun and a moth behind
' it go over), the parallax ridges as two halves of
' their 640 columns, each at both column parities.
' Rows: ridge 0's top over the sky; under it, ridge 0
' with ridge 1 over it, kept as one; ridge 2 last.
CONST TOPH = 109, R0Y = 92, R0H = 17
CONST C1Y = 109, C1H = 61, R1H = 35
CONST R1OY = 144, R1OH = 26
CONST R2Y = 130, R2H = 40, R2OY = 170, R2OH = 14
CONST CLOUDW = 39, CLOUDH = 22, SUNW = 33, SUNH = 27
' A GET's LONGs: of a row 320 wide, of the sprites.
CONST R0N = 80 * R0H + 2, R1N = 80 * R1H + 2
CONST R1ON = 80 * R1OH + 2, R2N = 80 * R2H + 2
CONST R2ON = 80 * R2OH + 2
CONST CLOUDN = (CLOUDW * CLOUDH + 7) \ 4
CONST SUNN = (SUNW * SUNH + 7) \ 4

DECLARE SUB Main ()
DECLARE SUB OpenScreen ()
DECLARE SUB CloseScreen ()
DECLARE SUB Flip (page AS INTEGER)
DECLARE SUB LoadTables ()
DECLARE SUB LoadSongs ()
DECLARE SUB LoadSfx ()
DECLARE SUB LoadSkies ()
DECLARE SUB LoadLooks ()
DECLARE SUB LoadBursts ()
DECLARE SUB LoadRidges ()
DECLARE SUB LoadTerrain ()
DECLARE SUB BuildStones ()
DECLARE SUB BuildLake ()
DECLARE SUB BuildGlyphs ()
DECLARE SUB BuildMoth ()
DECLARE SUB Pose (k AS INTEGER, s AS Shape)
DECLARE SUB Sketch (g() AS INTEGER, s AS Shape)
DECLARE SUB Outline (g() AS INTEGER)
DECLARE SUB Capture (g() AS INTEGER, k AS INTEGER)
DECLARE SUB NewWorld (w AS World, cur() AS SINGLE)
DECLARE SUB ReadPad (k AS Pad)
DECLARE SUB Control (w AS World, k AS Pad)
DECLARE SUB Tick (w AS World, k AS Pad, st() AS Stone, sp() AS Spark)
DECLARE SUB AgeFx (f AS Fx)
DECLARE SUB AgeMoth (m AS Moth)
DECLARE SUB Orbit (w AS World, k AS Pad, st() AS Stone, sp() AS Spark)
DECLARE SUB Fly (w AS World, k AS Pad, st() AS Stone, sp() AS Spark)
DECLARE SUB Fall (w AS World, sp() AS Spark)
DECLARE SUB Launch (w AS World, st() AS Stone, sp() AS Spark)
DECLARE SUB Die (w AS World)
DECLARE SUB Scored (w AS World, sp() AS Spark)
DECLARE SUB BackToTitle (w AS World)
DECLARE SUB Trail (w AS World, sp() AS Spark)
DECLARE SUB Flap (m AS Moth, sp() AS Spark)
DECLARE SUB Gravity (m AS Moth)
DECLARE SUB PlaceStones (st() AS Stone)
DECLARE SUB MoveStone (st() AS Stone, i AS INTEGER)
DECLARE SUB Emit (sp() AS Spark, m AS Moth, id AS INTEGER)
DECLARE SUB MoveSparks (sp() AS Spark)
DECLARE SUB StartSong (s AS Song, id AS INTEGER)
DECLARE SUB StopSong (s AS Song)
DECLARE SUB FeedSong (s AS Song)
DECLARE SUB Layer (id AS INTEGER, b AS INTEGER, v AS INTEGER, pre AS STRING, k AS STRING)
DECLARE FUNCTION NoteOf$ (k AS STRING, steps AS INTEGER)
DECLARE FUNCTION Drums$ (s AS Song)
DECLARE FUNCTION Lift$ (s AS Song)
DECLARE FUNCTION PadSound$ (p AS INTEGER)
DECLARE FUNCTION ArpSound$ (p AS INTEGER)
DECLARE FUNCTION BassBeat$ (k AS STRING, b AS INTEGER, o AS STRING)
DECLARE FUNCTION Grazed% (st() AS Stone, i AS INTEGER, m AS Moth)
DECLARE SUB ToggleMusic (s AS Song)
DECLARE SUB PlaySfx (id AS INTEGER)
DECLARE SUB EaseSky (s AS Sky, cur() AS SINGLE)
DECLARE SUB ApplyPalette (s AS Sky, cur() AS SINGLE)
DECLARE SUB Shade (k() AS LONG, c() AS LONG)
DECLARE SUB Ramp (c() AS LONG, i0 AS INTEGER, n AS INTEGER, a AS LONG, b AS LONG, span AS INTEGER)
DECLARE SUB Render (w AS World, st() AS Stone, sp() AS Spark)
DECLARE SUB DrawSky (g AS Game)
DECLARE SUB SkyBands (y1 AS INTEGER, y2 AS INTEGER)
DECLARE SUB DrawTop (w AS World)
DECLARE SUB BuildLand ()
DECLARE SUB BuildSprites ()
DECLARE SUB SunFace ()
DECLARE SUB Shot (y AS INTEGER, h AS INTEGER, c AS INTEGER)
DECLARE SUB Keep (a() AS LONG, s AS LONG, y AS INTEGER, h AS INTEGER)
DECLARE SUB Masked (id AS INTEGER, o AS INTEGER, y AS INTEGER, h AS INTEGER, im() AS LONG, mk() AS LONG, s AS LONG)
DECLARE SUB Layer0 (id AS INTEGER, o AS INTEGER)
DECLARE SUB Pano (a() AS LONG, n AS LONG, y AS INTEGER, o AS INTEGER, id AS INTEGER, how AS INTEGER)
DECLARE FUNCTION Ik% (c AS INTEGER)
DECLARE FUNCTION Offset% (scroll AS SINGLE, id AS INTEGER)
DECLARE SUB DrawCloud (x AS INTEGER, y AS INTEGER)
DECLARE SUB DrawSun (t AS SINGLE)
DECLARE SUB DrawLand (g AS Game)
DECLARE SUB DrawRidge (id AS INTEGER, o AS INTEGER)
DECLARE SUB DrawCastle (o AS INTEGER)
DECLARE SUB DrawSeeds (g AS Game)
DECLARE SUB DrawTrees (o AS INTEGER)
DECLARE SUB DrawStones (st() AS Stone, t AS SINGLE)
DECLARE SUB DrawStone (st() AS Stone, i AS INTEGER, t AS SINGLE)
DECLARE SUB Shaft (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER)
DECLARE SUB Lintel (x AS INTEGER, cy AS INTEGER)
DECLARE SUB Broken (x AS INTEGER, cy AS INTEGER)
DECLARE SUB Moss (x AS INTEGER, cy AS INTEGER)
DECLARE SUB Drips (x AS INTEGER, y AS INTEGER)
DECLARE SUB Decorate (st() AS Stone, i AS INTEGER, t AS SINGLE)
DECLARE SUB Runes (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER, t AS SINGLE)
DECLARE SUB Vines (x AS INTEGER, y2 AS INTEGER, t AS SINGLE)
DECLARE SUB Shrooms (x AS INTEGER, cy AS INTEGER)
DECLARE SUB DrawMoth (m AS Moth, mode AS INTEGER)
DECLARE SUB DrawLake (t AS SINGLE)
DECLARE SUB DrawSparks (sp() AS Spark)
DECLARE SUB DrawHud (w AS World)
DECLARE SUB TitleText (g AS Game)
DECLARE SUB ScoreText (g AS Game, f AS Fx)
DECLARE SUB Panel (g AS Game)
DECLARE SUB DrawFx (f AS Fx)
DECLARE SUB Shake (amount AS SINGLE)
DECLARE SUB Disc (cx AS INTEGER, cy AS INTEGER, r AS INTEGER, c AS INTEGER)
DECLARE SUB Dither (y1 AS INTEGER, y2 AS INTEGER, c AS INTEGER, level AS INTEGER)
DECLARE SUB Txt (t AS STRING, x AS INTEGER, y AS INTEGER, id AS INTEGER)
DECLARE SUB TxtC (t AS STRING, y AS INTEGER, id AS INTEGER)
DECLARE SUB GlyphRow (b AS INTEGER, x AS INTEGER, y AS INTEGER, sc AS INTEGER, c AS INTEGER)
DECLARE FUNCTION NextTime! (last AS SINGLE)
DECLARE FUNCTION Elapsed! (a AS SINGLE, b AS SINGLE)
DECLARE FUNCTION NextGap% (prev AS INTEGER)
DECLARE FUNCTION Hit% (st() AS Stone, m AS Moth)
DECLARE FUNCTION Touches% (m AS Moth, x1 AS SINGLE, y1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE)
DECLARE FUNCTION MothFrame% (m AS Moth, mode AS INTEGER)
DECLARE FUNCTION MothPix% (lx AS SINGLE, ly AS SINGLE, s AS Shape)
DECLARE FUNCTION WingPix% (dx AS SINGLE, dy AS SINGLE, e AS SINGLE)
DECLARE FUNCTION Feeler% (lx AS SINGLE, ly AS SINGLE)
DECLARE FUNCTION Ell! (px AS SINGLE, py AS SINGLE, o AS Oval)
DECLARE FUNCTION SegD! (px AS SINGLE, py AS SINGLE, ax AS SINGLE, ay AS SINGLE, bx AS SINGLE, by AS SINGLE)
DECLARE FUNCTION Mix& (a AS LONG, b AS LONG, t AS SINGLE)
DECLARE FUNCTION Lit& (c AS LONG)
DECLARE FUNCTION Wet& (c AS LONG, lake AS LONG)
DECLARE FUNCTION Pack& (r AS SINGLE, g AS SINGLE, b AS SINGLE)
DECLARE FUNCTION Chan% (c AS LONG, j AS INTEGER)
DECLARE FUNCTION Dac& (c AS LONG, f AS SINGLE)

'$DYNAMIC
' Assets: built once at the start, only read after.
DIM SHARED Spr(SPRSZ * NSPR * 2 - 1) AS LONG
DIM SHARED Glyph(126, 7) AS INTEGER, Bit(7) AS INTEGER
DIM SHARED Crest(NRIDGE - 1, 639) AS INTEGER
DIM SHARED Ridges(NRIDGE - 1) AS Ridge
DIM SHARED TreeX(NTREE - 1) AS INTEGER
DIM SHARED TreeH(NTREE - 1) AS INTEGER
DIM SHARED FlyX(NFLY - 1) AS INTEGER
DIM SHARED FlyY(NFLY - 1) AS INTEGER
DIM SHARED FlyPh(NFLY - 1) AS SINGLE
DIM SHARED CloudX(NCLOUD - 1) AS INTEGER
DIM SHARED CloudY(NCLOUD - 1) AS INTEGER
DIM SHARED StoneCol(STONEW + 2 * OVERHANG) AS INTEGER
DIM SHARED Rune(3) AS STRING, Looks(NLOOK - 1) AS Look
DIM SHARED Tune(1 TO 2, MAXBEATS - 1, 2) AS STRING
DIM SHARED SongLen(1 TO 2) AS INTEGER
DIM SHARED SfxA(NSFX - 1) AS STRING
DIM SHARED SfxB(NSFX - 1) AS STRING
DIM SHARED SfxD(NSFX - 1) AS STRING
DIM SHARED Mood(1 TO 2, NSKIES - 1, MAXBEATS - 1) AS STRING
DIM SHARED Low(MAXBEATS - 1) AS STRING
DIM SHARED Fast(MAXBEATS - 1) AS STRING
DIM SHARED Echo(1 TO 2, MAXBEATS - 1) AS STRING
DIM SHARED Bursts(NBURST - 1) AS Burst
DIM SHARED SkyKey(NSKIES - 1, NKEY - 1) AS LONG
DIM SHARED Fixed(255) AS LONG
' The palette last shaded, its keys (and whether it
' was), what the DAC holds; 256 ^ 0 .. 2.
DIM SHARED Shaded(255) AS LONG, ShadeKey(NKEY) AS LONG
DIM SHARED DacNow(255) AS LONG, Pow256(2) AS LONG
DIM SHARED Tinter(SW * LAKEH \ 4) AS LONG
' Scratch for drawing.
DIM SHARED Strip(80) AS LONG, Frame(SW * SH \ 4) AS LONG
' The pictures kept (see TOPH), each a GET's: images
' over 0 for OR, masks 0 under them over 255 for AND.
' A panorama's slot is parity * 2 + half. Ink: when
' not -1, the colour every part of a picture takes.
DIM SHARED SkyImg(80 * TOPH + 1) AS LONG
DIM SHARED R0TI(R0N * 4) AS LONG, R0TM(R0N * 4) AS LONG
DIM SHARED R0B(R1N * 4) AS LONG
DIM SHARED R1I(R1N * 4) AS LONG, R1M(R1N * 4) AS LONG
DIM SHARED R1O(R1ON * 4) AS LONG, LandImg(80 * C1H + 1) AS LONG
DIM SHARED R2I(R2N * 2) AS LONG, R2M(R2N * 2) AS LONG
DIM SHARED R2O(R2ON * 2) AS LONG, LandKey(1) AS INTEGER
DIM SHARED CloudI(CLOUDN) AS LONG, CloudM(CLOUDN) AS LONG
DIM SHARED SunI(SUNN) AS LONG, SunM(SUNN) AS LONG
DIM SHARED Ink AS INTEGER

Main
END

' Each song: its length in beats, then each voice's
' sound and a string a beat: the tune, the chord's
' arpeggio, then the drums (the title's bass).
Songs:
' Title
DATA 32
DATA "T120MLW1H2,6,11,5V12"
DATA "O4E8", "O4G8", "O5C8", "O4G8", "O4A8", "O4G8", "O4F8"
DATA "O4A8", "O4G8", "O4B8", "O5D8", "O4B8", "O5C8", "O4G8"
DATA "O4E8", "O4C8", "O4C8", "O4E8", "O4A8", "O4E8", "O4F8"
DATA "O4A8", "O5C8", "O4A8", "O4B8", "O4A8", "O4G8", "O4F8"
DATA "O4E8", "O4D8", "O4C8", "O4C8"
DATA "T120MSW0Q30Y0H0,4,5,3V5"
DATA "O4C32O4E32O4G32O4E32", "O4C32O4E32O4G32O4E32"
DATA "O4C32O4E32O4G32O4E32", "O4C32O4E32O4G32O4E32"
DATA "O3F32O3A32O4C32O3A32", "O3F32O3A32O4C32O3A32"
DATA "O3F32O3A32O4C32O3A32", "O3F32O3A32O4C32O3A32"
DATA "O3G32O3B32O4D32O3B32", "O3G32O3B32O4D32O3B32"
DATA "O3G32O3B32O4D32O3B32", "O3G32O3B32O4D32O3B32"
DATA "O4C32O4E32O4G32O4E32", "O4C32O4E32O4G32O4E32"
DATA "O4C32O4E32O4G32O4E32", "O4C32O4E32O4G32O4E32"
DATA "O3A32O4C32O4E32O4C32", "O3A32O4C32O4E32O4C32"
DATA "O3A32O4C32O4E32O4C32", "O3A32O4C32O4E32O4C32"
DATA "O3F32O3A32O4C32O3A32", "O3F32O3A32O4C32O3A32"
DATA "O3F32O3A32O4C32O3A32", "O3F32O3A32O4C32O3A32"
DATA "O3G32O3B32O4D32O3B32", "O3G32O3B32O4D32O3B32"
DATA "O3G32O3B32O4D32O3B32", "O3G32O3B32O4D32O3B32"
DATA "O4C32O4E32O4G32O4E32", "O4C32O4E32O4G32O4E32"
DATA "O4C32O4E32O4G32O4E32", "O4C32O4E32O4G32O4E32"
DATA "T120MNW2Y1,500,6H0,7,8,4V8"
DATA "O3C8", "O3G8", "O3C8", "O3G8", "O2F8", "O3C8", "O2F8"
DATA "O3C8", "O2G8", "O3D8", "O2G8", "O3D8", "O3C8", "O3G8"
DATA "O3C8", "O3G8", "O2A8", "O3E8", "O2A8", "O3E8", "O2F8"
DATA "O3C8", "O2F8", "O3C8", "O2G8", "O3D8", "O2G8", "O3D8"
DATA "O3C8", "O3G8", "O3C8", "O3G8"
' Game
DATA 64
DATA "T160MNW0Q25H0,6,10,3V11"
DATA "O4E8", "O4G8", "O5C8", "O4G8", "O4D8", "O4G8", "O4B8"
DATA "O4G8", "O4C8", "O4E8", "O4A8", "O4E8", "O4F8", "O4A8"
DATA "O5C8", "O4A8", "O4G8", "O4E8", "O4C8", "O4E8", "O3B8"
DATA "O4D8", "O4G8", "O4D8", "O3A8", "O4C8", "O4F8", "O4A8"
DATA "O4G8", "O4F8", "O4E8", "O4D8", "O4C8", "O4F8", "O4A8"
DATA "O4F8", "O4D8", "O4G8", "O4B8", "O4G8", "O4E8", "O4G8"
DATA "O4B8", "O4G8", "O4A8", "O4G8", "O4E8", "O4C8", "O4F8"
DATA "O4E8", "O4F8", "O4A8", "O4G8", "O4A8", "O4B8", "O5D8"
DATA "O5C8", "O4G8", "O4E8", "O4G8", "O5C8", "O4E8", "O4G8"
DATA "O5C8"
DATA "T160MNW2Y1,900,9H0,3,9,2V7"
DATA "O3C32O3G32O4C32O3G32", "O3C32O3G32O4C32O3G32"
DATA "O3C32O3G32O4C32O3G32", "O3C32O3G32O4C32O3G32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O2A32O3E32O3A32O3E32", "O2A32O3E32O3A32O3E32"
DATA "O2A32O3E32O3A32O3E32", "O2A32O3E32O3A32O3E32"
DATA "O2F32O3C32O3F32O3C32", "O2F32O3C32O3F32O3C32"
DATA "O2F32O3C32O3F32O3C32", "O2F32O3C32O3F32O3C32"
DATA "O3C32O3G32O4C32O3G32", "O3C32O3G32O4C32O3G32"
DATA "O3C32O3G32O4C32O3G32", "O3C32O3G32O4C32O3G32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O2F32O3C32O3F32O3C32", "O2F32O3C32O3F32O3C32"
DATA "O2F32O3C32O3F32O3C32", "O2F32O3C32O3F32O3C32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O2F32O3C32O3F32O3C32", "O2F32O3C32O3F32O3C32"
DATA "O2F32O3C32O3F32O3C32", "O2F32O3C32O3F32O3C32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O2E32O2B32O3E32O2B32", "O2E32O2B32O3E32O2B32"
DATA "O2E32O2B32O3E32O2B32", "O2E32O2B32O3E32O2B32"
DATA "O2A32O3E32O3A32O3E32", "O2A32O3E32O3A32O3E32"
DATA "O2A32O3E32O3A32O3E32", "O2A32O3E32O3A32O3E32"
DATA "O2F32O3C32O3F32O3C32", "O2F32O3C32O3F32O3C32"
DATA "O2F32O3C32O3F32O3C32", "O2F32O3C32O3F32O3C32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O2G32O3D32O3G32O3D32", "O2G32O3D32O3G32O3D32"
DATA "O3C32O3G32O4C32O3G32", "O3C32O3G32O4C32O3G32"
DATA "O3C32O3G32O4C32O3G32", "O3C32O3G32O4C32O3G32"
DATA "O3C32O3G32O4C32O3G32", "O3C32O3G32O4C32O3G32"
DATA "O3C32O3G32O4C32O3G32", "O3C32O3G32O4C32O3G32"
DATA "T160MNW3Y0H0,2,0,1"
DATA "V10O1C16P16", "V4O6C32P32P16", "V8O4C16P16"
DATA "V4O6C32P32P16", "V10O1C16P16", "V4O6C32P32P16"
DATA "V8O4C16P16", "V4O6C32P32P16", "V10O1C16P16"
DATA "V4O6C32P32P16", "V8O4C16P16", "V4O6C32P32P16"
DATA "V10O1C16P16", "V4O6C32P32P16", "V8O4C16P16"
DATA "V8O4C16V6O4C16", "V10O1C16P16", "V4O6C32P32P16"
DATA "V8O4C16P16", "V4O6C32P32P16", "V10O1C16P16"
DATA "V4O6C32P32P16", "V8O4C16P16", "V4O6C32P32P16"
DATA "V10O1C16P16", "V4O6C32P32P16", "V8O4C16P16"
DATA "V4O6C32P32P16", "V10O1C16P16", "V4O6C32P32P16"
DATA "V8O4C16P16", "V8O4C16V6O4C16", "V10O1C16P16"
DATA "V4O6C32P32P16", "V8O4C16P16", "V4O6C32P32P16"
DATA "V10O1C16P16", "V4O6C32P32P16", "V8O4C16P16"
DATA "V4O6C32P32P16", "V10O1C16P16", "V4O6C32P32P16"
DATA "V8O4C16P16", "V4O6C32P32P16", "V10O1C16P16"
DATA "V4O6C32P32P16", "V8O4C16P16", "V8O4C16V6O4C16"
DATA "V10O1C16P16", "V4O6C32P32P16", "V8O4C16P16"
DATA "V4O6C32P32P16", "V10O1C16P16", "V4O6C32P32P16"
DATA "V8O4C16P16", "V4O6C32P32P16", "V10O1C16P16"
DATA "V4O6C32P32P16", "V8O4C16P16", "V4O6C32P32P16"
DATA "V10O1C16P16", "V4O6C32P32P16", "V8O4C16P16"
DATA "V8O4C16V6O4C16"

' Sound effects, a string each for voices 1, 2 and 4:
' the jingles play once the song has stopped, the
' rest on voice 4 over it: crash, flap, chime,
' near miss, new best, game over.
Sfx:
DATA "T150MLW2V12L32O2CO1GECL8O0G"
DATA "T150W3H0,9,0,8V12L4O1C", ""
DATA "", "", "T255W3Y3,3000,4H0,2,0,2V6L32O5C"
DATA "", "", "T160MNW1H0,3,8,4V11L64O5CEGO6CL16E"
DATA "", "", "T200W3Y2,2500,10H0,5,0,4V8L16O4C"
DATA "T160MNW0Q25V12L16O4CEGO5L8CO4L16GO5L4C"
DATA "T160MNW2V9L4O2CL8GL4O3C", ""
DATA "T140MNW1V12L16O4CO3GECL8EGL4O4C"
DATA "T140MNW2V9L4O2CO1GO2C", ""

' Sparks: how many, spread up and down, flung out.
Bursts:
DATA 1, 15, 0, 20, 130, 100, 8, 100, 0, 16, 80, 130

' Text: scale, top and bottom colour, shadow.
' 2 is white, 80-87 gold, 11 pale yellow.
Looks:
DATA 3, 80, 87, 2, 3, 2, 2, 0, 1, 2, 2, 1
DATA 2, 2, 82, 1, 1, 80, 83, 1, 2, 2, 83, 1
DATA 3, 2, 83, 1, 1, 80, 87, 1, 2, 2, 2, 1
DATA 2, 80, 84, 1, 1, 11, 11, 1, 1, 80, 80, 1

' Mountains, far and near hills: parallax, colour,
' shade under the crest and lower down, where the
' crest's band ends, the bottom, column width.
Ridges:
DATA .05, 128, 3, 6, 9, 150, 2
DATA .15, 144, 5, 10, 12, 183, 2
DATA .4, 160, 5, 11, 8, 183, 1

' The skies' key colours as red, green, blue: sky
' top, middle, low; cloud, its shade; mountains;
' far hills top, low; near hills top, low; trees
' light, dark; water; stone light, dark.
Skies:
DATA 30,110,240, 90,170,255, 190,235,255
DATA 255,255,255, 205,220,245, 120,160,225
DATA 140,215,120, 70,160,90, 90,205,80, 40,130,50
DATA 50,160,70, 20,100,45, 40,140,230
DATA 255,215,150, 170,100,60
' golden hour
DATA 70,90,220, 170,140,240, 255,215,160
DATA 255,250,235, 240,200,190, 190,150,200
DATA 200,200,110, 130,140,70, 140,200,70, 70,120,40
DATA 80,150,60, 40,90,40, 80,120,200
DATA 255,200,120, 160,90,50
' sunset
DATA 90,50,170, 240,110,170, 255,200,120
DATA 255,220,230, 230,150,190, 170,100,170
DATA 200,120,160, 120,70,120, 110,170,120, 50,100,80
DATA 60,120,80, 30,70,60, 120,80,180
DATA 255,170,120, 140,70,60

' The colours that stay: index, red, green, blue.
Fixed:
DATA 1,25,25,70, 2,255,255,255, 3,255,226,80
DATA 4,255,130,30, 5,150,80,40, 6,255,170,70
DATA 7,255,250,235, 8,230,120,50, 9,150,60,30
DATA 10,255,255,255, 11,255,240,140
DATA 12,255,170,200, 88,60,110,220
DATA 89,255,230,120, 90,255,236,90
DATA 91,255,200,60, 92,255,150,150
DATA 188,100,210,70, 189,120,255,255
DATA 190,240,60,60, 191,60,160,60, -1

REM $STATIC
SUB AgeFx (f AS Fx)
    f.flash = f.flash - STEPT
    f.shake = f.shake - STEPT
    f.pop = f.pop - STEPT
END SUB

' Wings beat 4 times a second, 9 just after a flap.
SUB AgeMoth (m AS Moth)
    m.boost = m.boost - STEPT
    m.squash = m.squash - STEPT
    m.beat = m.beat + STEPT * 4
    IF m.boost > 0 THEN m.beat = m.beat + STEPT * 5
END SUB

' The colours shaded again only when a key moved; a
' DAC entry set only when it changed.
SUB ApplyPalette (s AS Sky, cur() AS SINGLE)
    DIM k(NKEY - 1) AS LONG, i AS INTEGER
    DIM moved AS INTEGER, d AS LONG
    FOR i = 0 TO NKEY - 1
        k(i) = Pack&(cur(i, 0), cur(i, 1), cur(i, 2))
        IF k(i) <> ShadeKey(i) THEN moved = -1
        ShadeKey(i) = k(i)
    NEXT
    IF moved OR ShadeKey(NKEY) = 0 THEN
        ShadeKey(NKEY) = -1
        FOR i = 0 TO 255
            Shaded(i) = Fixed(i)
        NEXT
        Shade k(), Shaded()
    END IF
    FOR i = 0 TO 255
        d = Dac&(Shaded(i), s.fade)
        IF d <> DacNow(i) THEN
            PALETTE i, d
            DacNow(i) = d
        END IF
    NEXT
END SUB

SUB BackToTitle (w AS World)
    w.g.toTitle = 0
    w.g.mode = TITLE
    w.sky.phase = DAY
    w.sky.fadeTo = 1
    StartSong w.song, TITLESONG
END SUB

' A broken top: stumps of the cap, ragged.
SUB Broken (x AS INTEGER, cy AS INTEGER)
    DIM dx AS INTEGER, c1 AS INTEGER, h AS INTEGER
    DIM b AS INTEGER
    c1 = x - OVERHANG
    b = cy + CAPH - 1
    FOR dx = 0 TO STONEW - 1 + 2 * OVERHANG
        h = CAPH - ((dx \ 3) * 5 + x) MOD 4
        LINE (c1 + dx, b + 1 - h)-(c1 + dx, b), StoneCol(dx)
        PSET (c1 + dx, b + 1 - h), CINK
    NEXT
    LINE (c1, b)-(x + STONEW - 1 + OVERHANG, b), CINK
END SUB

' The ROM font's rows, read back off the screen.
SUB BuildGlyphs
    DIM c AS INTEGER, r AS INTEGER, x AS INTEGER
    DIM b AS INTEGER
    FOR x = 0 TO 7
        Bit(x) = 2 ^ (7 - x)
    NEXT
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

' The lake's tint: an image its size, every pixel 64.
SUB BuildLake
    DIM i AS INTEGER
    Tinter(0) = SW * 8 + LAKEH * 65536
    FOR i = 1 TO SW * LAKEH \ 4
        Tinter(i) = &H40404040
    NEXT
END SUB

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

' A stone's columns: lit a quarter in, shaded right;
' and its runes, 3 by 5 dots.
SUB BuildStones
    DIM i AS INTEGER, k AS INTEGER, n AS INTEGER
    n = STONEW + 2 * OVERHANG
    FOR i = 0 TO n
        k = ABS(i - n \ 4) * 9 \ n
        IF k > 7 THEN k = 7
        StoneCol(i) = CSTONE + k
    NEXT
    Rune(0) = "111010010010111"
    Rune(1) = "101111101101101"
    Rune(2) = "110101110101110"
    Rune(3) = "010111010010010"
END SUB

FUNCTION ArpSound$ (p AS INTEGER)
    SELECT CASE p
    CASE DAY
        ArpSound$ = ARPDAY
    CASE GOLDEN
        ArpSound$ = ARPGOLD
    CASE ELSE
        ArpSound$ = ARPDUSK
    END SELECT
END FUNCTION

' A bar of bass, a note a beat, from the chord in
' the arpeggio's string k: root, root, fifth, octave,
' always in octave o, so it never jumps.
FUNCTION BassBeat$ (k AS STRING, b AS INTEGER, o AS STRING)
    DIM r AS STRING
    r = o + MID$(k, 3, 1)
    SELECT CASE b MOD 4
    CASE 2
        r = NoteOf$(r, 4)
    CASE 3
        r = NoteOf$(r, 7)
    END SELECT
    BassBeat$ = r + "8"
END FUNCTION

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

FUNCTION Chan% (c AS LONG, j AS INTEGER)
    Chan% = (c \ Pow256(j)) MOD 256
END FUNCTION

SUB CloseScreen
    SOUND 37, 0
    SCREEN 0
    WIDTH 80
END SUB

SUB Control (w AS World, k AS Pad)
    IF k.mute THEN ToggleMusic w.song
    IF k.back = 0 THEN EXIT SUB
    IF w.g.mode = TITLE THEN
        w.g.quitting = -1
    ELSE
        w.g.toTitle = -1
    END IF
    w.sky.fadeTo = 0
END SUB

' Each colour's channels from 0-255 to the DAC's
' 0-63, dimmed by the fade.
FUNCTION Dac& (c AS LONG, f AS SINGLE)
    DIM r AS LONG, g AS LONG, b AS LONG
    r = (c MOD 256) * f \ 4
    g = ((c \ 256) MOD 256) * f \ 4
    b = ((c \ 65536) MOD 256) * f \ 4
    Dac& = r + g * 256 + b * 65536
END FUNCTION

SUB Decorate (st() AS Stone, i AS INTEGER, t AS SINGLE)
    DIM x AS INTEGER, top AS INTEGER, bot AS INTEGER
    x = INT(st(i).x)
    top = st(i).gap - GAP \ 2
    bot = st(i).gap + GAP \ 2
    SELECT CASE st(i).kind
    CASE 1
        Runes x, bot + CAPH + 6, GROUNDY - 9, t
    CASE 2
        Vines x, top + 13, t
    CASE 4
        Shrooms x, bot
    END SELECT
END SUB

SUB Die (w AS World)
    w.g.mode = DYING
    w.g.modeT = 0
    w.fx.flash = .18
    w.fx.shake = .35
    w.m.vy = -70
    StopSong w.song
    PlaySfx SFXCRASH
    IF w.g.score > w.g.best THEN
        w.g.best = w.g.score
        w.g.newBest = -1
    END IF
END SUB

' Drawn as the eye sees it: wide as it is tall.
SUB Disc (cx AS INTEGER, cy AS INTEGER, r AS INTEGER, c AS INTEGER)
    DIM dy AS INTEGER, ry AS INTEGER, hw AS INTEGER
    ry = r / ASPECT
    FOR dy = -ry TO ry
        hw = r * SQR(1 - (dy / (ry + .5)) ^ 2)
        LINE (cx - hw, cy + dy)-(cx + hw, cy + dy), Ik%(c)
    NEXT
END SUB

' Dots of c over rows y1 to y2: a quarter (level 1),
' half (2) or three quarters (3).
SUB Dither (y1 AS INTEGER, y2 AS INTEGER, c AS INTEGER, level AS INTEGER)
    DIM p0 AS INTEGER, p1 AS INTEGER, y AS INTEGER
    SELECT CASE level
    CASE 1: p0 = &H8888: p1 = &H2222
    CASE 2: p0 = &HAAAA: p1 = &H5555
    CASE ELSE: p0 = &HEEEE: p1 = &HBBBB
    END SELECT
    FOR y = y1 TO y2
        IF y AND 1 THEN
            LINE (0, y)-(SW - 1, y), c, , p1
        ELSE
            LINE (0, y)-(SW - 1, y), c, , p0
        END IF
    NEXT
END SUB

' The ruined castle on the mountains, a lamp lit in it.
SUB DrawCastle (o AS INTEGER)
    DIM cx AS INTEGER, b AS INTEGER, k AS INTEGER
    DIM c AS INTEGER, l AS INTEGER
    cx = 215 - o
    IF cx < -20 THEN cx = cx + 640
    b = Crest(0, 215) + 3
    c = Ik%(CMTN + 2)
    l = Ik%(CSPARK + 1)
    LINE (cx - 12, b - 15)-(cx - 6, b), c, BF
    LINE (cx - 6, b - 9)-(cx + 7, b), c, BF
    LINE (cx + 7, b - 12)-(cx + 12, b), c, BF
    FOR k = 0 TO 3
        PSET (cx - 12 + k * 2, b - 16), c
        PSET (cx + 6 + k * 2, b - 13), c
    NEXT
    PSET (cx - 9, b - 10), l
    PSET (cx + 1, b - 4), l
END SUB

SUB DrawCloud (x AS INTEGER, y AS INTEGER)
    Disc x - 9, y + 2, 9, CCLOUDLO
    Disc x + 10, y + 2, 8, CCLOUDLO
    Disc x, y, 11, CCLOUD
    Disc x - 10, y + 1, 7, CCLOUD
    Disc x + 10, y + 1, 7, CCLOUD
    LINE (x - 16, y + 6)-(x + 16, y + 6), Ik%(CCLOUDLO)
END SUB

SUB DrawFx (f AS Fx)
    IF f.flash > 0 THEN
        Dither 0, SH - 1, CWHITE, 1 + INT(f.flash * 12)
    END IF
    IF f.shake > 0 THEN Shake f.shake
END SUB

SUB DrawHud (w AS World)
    SELECT CASE w.g.mode
    CASE TITLE
        TitleText w.g
    CASE PLAYING, DYING
        ScoreText w.g, w.fx
    CASE OVER
        Panel w.g
    END SELECT
END SUB

' The lake mirrors the rows above it, each shifted by
' its ripple, then ORs in 64: the water's tint.
SUB DrawLake (t AS SINGLE)
    DIM d AS INTEGER, o AS INTEGER, k AS INTEGER
    DIM x AS INTEGER, y AS INTEGER, hw AS INTEGER
    LINE (0, GROUNDY)-(SW - 1, SH - 1), CFAR + 10, BF
    FOR d = 0 TO LAKEH - 1
        o = INT((1 + d / 4) * SIN(t * 2.2 + d * .9))
        y = GROUNDY - 1 - d
        GET (6, y)-(SW - 7, y), Strip
        PUT (6 + o, GROUNDY + d), Strip, PSET
    NEXT
    PUT (0, GROUNDY), Tinter, OR
    LINE (0, GROUNDY)-(SW - 1, GROUNDY), CWHITE, , &HAAAA
    FOR d = 2 TO LAKEH - 1 STEP 2
        hw = 7 - d \ 3 + INT(2 * SIN(t * 5 + d))
        y = GROUNDY + d
        LINE (SUNX - hw, y)-(SUNX + hw, y), CSUN, , &HAAAA
    NEXT
    FOR k = 0 TO 9
        x = (k * 37 + INT(t * 9)) MOD SW
        y = GROUNDY + 2 + (k * 5) MOD 13
        LINE (x, y)-(x + 2 + k MOD 3, y), CWHITE
    NEXT
END SUB

' Ridges 0 and 1 kept in LandImg till either moves; the
' seeds; ridge 2 over them.
SUB DrawLand (g AS Game)
    DIM o0 AS INTEGER, o1 AS INTEGER, o2 AS INTEGER
    o0 = Offset%(g.scroll, 0)
    o1 = Offset%(g.scroll, 1)
    IF o0 <> LandKey(0) OR o1 <> LandKey(1) THEN
        Pano R0B(), R1N, C1Y, o0, 0, 0
        Pano R1M(), R1N, C1Y, o1, 1, 1
        Pano R1I(), R1N, C1Y, o1, 1, 2
        Pano R1O(), R1ON, R1OY, o1, 1, 0
        GET (0, C1Y)-(SW - 1, C1Y + C1H - 1), LandImg
        LandKey(0) = o0: LandKey(1) = o1
    ELSE
        PUT (0, C1Y), LandImg, PSET
    END IF
    DrawSeeds g
    o2 = Offset%(g.scroll, 2)
    Pano R2M(), R2N, R2Y, o2, 2, 1
    Pano R2I(), R2N, R2Y, o2, 2, 2
    Pano R2O(), R2ON, R2OY, o2, 2, 0
END SUB

' A panorama's two halves at offset o: PSET (how 0),
' AND (1) or OR (2).
SUB Pano (a() AS LONG, n AS LONG, y AS INTEGER, o AS INTEGER, id AS INTEGER, how AS INTEGER)
    DIM h AS INTEGER, x AS INTEGER, par AS INTEGER
    DIM s AS LONG
    par = o MOD Ridges(id).stp
    FOR h = 0 TO 1
        x = 320 * h + par - o
        IF x <= -320 THEN x = x + 640
        IF x < SW THEN
            s = (par * 2 + h) * n
            SELECT CASE how
            CASE 0: PUT (x, y), a(s), PSET
            CASE 1: PUT (x, y), a(s), AND
            CASE ELSE: PUT (x, y), a(s), OR
            END SELECT
        END IF
    NEXT
END SUB

FUNCTION Offset% (scroll AS SINGLE, id AS INTEGER)
    Offset% = INT(scroll * Ridges(id).speed) MOD 640
END FUNCTION

' The colour drawn: c, or Ink when it is set.
FUNCTION Ik% (c AS INTEGER)
    IF Ink < 0 THEN Ik% = c ELSE Ik% = Ink
END FUNCTION

SUB DrawMoth (m AS Moth, mode AS INTEGER)
    DIM k AS INTEGER, x AS INTEGER, y AS INTEGER
    k = MothFrame%(m, mode)
    x = INT(m.x) - SPRW \ 2
    y = INT(m.y) - SPRH \ 2
    PUT (x, y), Spr((NSPR + k) * SPRSZ), AND
    PUT (x, y), Spr(k * SPRSZ), OR
END SUB

' A ridge: lit along its crest, shading below it.
SUB DrawRidge (id AS INTEGER, o AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER
    DIM r AS INTEGER, c AS INTEGER, b AS INTEGER
    DIM d1 AS INTEGER, d2 AS INTEGER, bot AS INTEGER
    r = Ridges(id).stp - 1
    c = Ridges(id).c
    b = Ridges(id).band
    d1 = Ik%(c + Ridges(id).d1)
    d2 = Ik%(c + Ridges(id).d2)
    c = Ik%(c)
    bot = Ridges(id).bottom
    FOR x = 0 TO SW - 1 STEP Ridges(id).stp
        y = Crest(id, (x + o) MOD 640)
        LINE (x, y)-(x + r, y + 1), c, BF
        LINE (x, y + 2)-(x + r, y + b), d1, BF
        LINE (x, y + b + 1)-(x + r, bot), d2, BF
    NEXT
END SUB

' Seeds drifting over the far hills, twinkling.
SUB DrawSeeds (g AS Game)
    DIM k AS INTEGER, o AS INTEGER
    DIM x AS INTEGER, y AS INTEGER
    o = INT(g.scroll * .25) MOD 640
    FOR k = 0 TO NFLY - 1
        IF SIN(g.clock * 2.3 + FlyPh(k) * 3) > .3 THEN
            x = FlyX(k) - o
            IF x < 0 THEN x = x + 640
            y = FlyY(k) + 3 * SIN(g.clock * .8 + FlyPh(k))
            PSET (x, y), CWHITE
            PSET (x - 1, y), CSPARK + 1
            PSET (x + 1, y), CSPARK + 1
            PSET (x, y - 1), CSPARK + 1
            PSET (x, y + 1), CSPARK + 1
        END IF
    NEXT
END SUB

' The sky, a shade a band of rows, and its clouds.
SUB DrawSky (g AS Game)
    DIM i AS INTEGER, x AS INTEGER, y AS INTEGER
    PUT (0, 0), SkyImg, PSET
    FOR i = 0 TO NCLOUD - 1
        x = CloudX(i) + INT(g.clock * 4)
        x = (x + INT(g.scroll * .03)) MOD 450 - 65
        y = CloudY(i)
        PUT (x - 19, y - 10), CloudM, AND
        PUT (x - 19, y - 10), CloudI, OR
    NEXT
END SUB

' The sky's bands of rows y1 to y2.
SUB SkyBands (y1 AS INTEGER, y2 AS INTEGER)
    DIM k AS INTEGER, a AS INTEGER, b AS INTEGER
    FOR k = 0 TO NSKY - 1
        a = k * GROUNDY \ NSKY
        b = (k + 1) * GROUNDY \ NSKY - 1
        IF a < y1 THEN a = y1
        IF b > y2 THEN b = y2
        IF a <= b THEN LINE (0, a)-(SW - 1, b), CSKY + k, BF
    NEXT
END SUB

' Ridge 0's top, over the sky, the sun and a moth
' behind it.
SUB DrawTop (w AS World)
    DIM o AS INTEGER
    o = Offset%(w.g.scroll, 0)
    Pano R0TM(), R0N, R0Y, o, 0, 1
    Pano R0TI(), R0N, R0Y, o, 0, 2
END SUB

' Rows y to y + h - 1 filled with c.
SUB Shot (y AS INTEGER, h AS INTEGER, c AS INTEGER)
    LINE (0, y)-(SW - 1, y + h - 1), c, BF
END SUB

' Rows y to y + h - 1 kept in a() from s.
SUB Keep (a() AS LONG, s AS LONG, y AS INTEGER, h AS INTEGER)
    GET (0, y)-(SW - 1, y + h - 1), a(s)
END SUB

' Ridge id at offset o, and what stands on it.
SUB Layer0 (id AS INTEGER, o AS INTEGER)
    DrawRidge id, o
    IF id = 0 THEN DrawCastle o
    IF id = 2 THEN DrawTrees o
END SUB

' Rows y to y + h - 1 of a ridge kept twice from s:
' its colours over 0, and 0 over 255 as its mask.
SUB Masked (id AS INTEGER, o AS INTEGER, y AS INTEGER, h AS INTEGER, im() AS LONG, mk() AS LONG, s AS LONG)
    Shot y, h, 0
    Layer0 id, o
    Keep im(), s, y, h
    Shot y, h, CMASK
    Ink = 0
    Layer0 id, o
    Ink = -1
    Keep mk(), s, y, h
END SUB

' Each ridge's halves at each parity of its columns.
SUB BuildLand
    DIM id AS INTEGER, par AS INTEGER, h AS INTEGER
    DIM o AS INTEGER, s AS LONG
    Ink = -1
    SkyBands 0, GROUNDY - 1
    GET (0, 0)-(SW - 1, TOPH - 1), SkyImg
    FOR id = 0 TO NRIDGE - 1
        FOR par = 0 TO Ridges(id).stp - 1
            FOR h = 0 TO 1
                o = par + 320 * h
                s = par * 2 + h
                SELECT CASE id
                CASE 0
                    Masked 0, o, R0Y, R0H, R0TI(), R0TM(), s * R0N
                    SkyBands C1Y, C1Y + R1H - 1
                    Layer0 0, o
                    Keep R0B(), s * R1N, C1Y, R1H
                CASE 1
                    Masked 1, o, C1Y, R1H, R1I(), R1M(), s * R1N
                    Layer0 1, o
                    Keep R1O(), s * R1ON, R1OY, R1OH
                CASE ELSE
                    Masked 2, o, R2Y, R2H, R2I(), R2M(), s * R2N
                    Layer0 2, o
                    Keep R2O(), s * R2ON, R2OY, R2OH
                END SELECT
            NEXT
        NEXT
    NEXT
    LandKey(0) = -1
END SUB

' A cloud and the sun's face, each with its mask.
SUB BuildSprites
    DIM x AS INTEGER, y AS INTEGER
    Ink = -1
    x = SW \ 2: y = 50
    LINE (x - 19, y - 10)-(x + 19, y + 11), 0, BF
    DrawCloud x, y
    GET (x - 19, y - 10)-(x + 19, y + 11), CloudI
    LINE (x - 19, y - 10)-(x + 19, y + 11), CMASK, BF
    Ink = 0: DrawCloud x, y: Ink = -1
    GET (x - 19, y - 10)-(x + 19, y + 11), CloudM
    LINE (SUNX - 16, SUNY - 13)-(SUNX + 16, SUNY + 13), 0, BF
    SunFace
    GET (SUNX - 16, SUNY - 13)-(SUNX + 16, SUNY + 13), SunI
    LINE (SUNX - 16, SUNY - 13)-(SUNX + 16, SUNY + 13), CMASK, BF
    Ink = 0: SunFace: Ink = -1
    GET (SUNX - 16, SUNY - 13)-(SUNX + 16, SUNY + 13), SunM
END SUB

SUB DrawSparks (sp() AS Spark)
    DIM i AS INTEGER, c AS INTEGER, l AS SINGLE
    DIM x AS INTEGER, y AS INTEGER
    FOR i = 0 TO NSPARK - 1
        l = sp(i).life
        IF l > 0 THEN
            c = CSPARK + 2
            IF l > .35 THEN c = CSPARK + 1
            IF l > .7 THEN c = CSPARK
            x = INT(sp(i).x)
            y = INT(sp(i).y)
            PSET (x, y), c
            IF l > .6 THEN PSET (x + 1, y), c
        END IF
    NEXT
END SUB

' A stone: two shafts, each with its cap, the gap
' between them, and the kind's decoration: 1 runes,
' 2 vines, 3 a broken top, 4 mushrooms.
SUB DrawStone (st() AS Stone, i AS INTEGER, t AS SINGLE)
    DIM x AS INTEGER, top AS INTEGER, bot AS INTEGER
    x = INT(st(i).x)
    top = st(i).gap - GAP \ 2
    bot = st(i).gap + GAP \ 2
    Shaft x, 0, top
    Shaft x, bot, GROUNDY - 1
    Lintel x, top - CAPH + 1
    Drips x, top + 1
    IF st(i).kind = 3 THEN
        Broken x, bot
    ELSE
        Lintel x, bot
        Moss x, bot
    END IF
    Decorate st(), i, t
END SUB

SUB DrawStones (st() AS Stone, t AS SINGLE)
    DIM i AS INTEGER, x AS INTEGER
    FOR i = 0 TO NSTONE - 1
        x = INT(st(i).x)
        IF x < SW + 4 AND x > -STONEW - 4 THEN
            DrawStone st(), i, t
        END IF
    NEXT
END SUB

' A smiling sun, its rays turning.
SUB DrawSun (t AS SINGLE)
    DIM i AS INTEGER, a AS SINGLE
    DIM ca AS SINGLE, sa AS SINGLE
    DIM x1 AS INTEGER, y1 AS INTEGER
    DIM x2 AS INTEGER, y2 AS INTEGER
    FOR i = 0 TO 11
        a = i * .5236 + t * .4
        ca = COS(a): sa = SIN(a) / ASPECT
        x1 = SUNX + 17 * ca: y1 = SUNY + 17 * sa
        x2 = SUNX + 23 * ca: y2 = SUNY + 23 * sa
        LINE (x1, y1)-(x2, y2), CRAY
    NEXT
    PUT (SUNX - 16, SUNY - 13), SunM, AND
    PUT (SUNX - 16, SUNY - 13), SunI, OR
END SUB

SUB SunFace
    DIM i AS INTEGER, a AS SINGLE, c AS INTEGER
    DIM x AS INTEGER, y AS INTEGER
    Disc SUNX, SUNY, 15, CRAY
    Disc SUNX, SUNY, 14, CSUN
    c = Ik%(CINK)
    LINE (SUNX - 5, SUNY - 4)-(SUNX - 4, SUNY - 3), c, BF
    LINE (SUNX + 4, SUNY - 4)-(SUNX + 5, SUNY - 3), c, BF
    FOR i = 0 TO 8
        a = .5 + i * .27
        x = SUNX + 7 * COS(a)
        y = SUNY + 1 + 6 * SIN(a) / ASPECT
        PSET (x, y), c
    NEXT
    Disc SUNX - 9, SUNY + 3, 2, CCHEEK
    Disc SUNX + 9, SUNY + 3, 2, CCHEEK
END SUB

SUB DrawTrees (o AS INTEGER)
    DIM k AS INTEGER, x AS INTEGER, c0 AS INTEGER
    DIM h AS INTEGER, b AS INTEGER, r AS INTEGER
    DIM hw AS INTEGER, y AS INTEGER, q AS INTEGER
    DIM c2 AS INTEGER, c3 AS INTEGER
    c0 = Ik%(CTREE): c2 = Ik%(CTREE + 2): c3 = Ik%(CTREE + 3)
    FOR k = 0 TO NTREE - 1
        x = TreeX(k) - o
        IF x < -10 THEN x = x + 640
        IF x < SW + 10 THEN
            h = TreeH(k)
            q = h \ 2 + 1
            b = Crest(2, TreeX(k)) + 2
            LINE (x, b - 2)-(x, b + 1), c3
            FOR r = 0 TO h
                hw = 1 + (r MOD q) * 3 \ q + r \ 5
                y = b - h + r
                LINE (x - hw, y)-(x, y), c0
                LINE (x + 1, y)-(x + hw, y), c2
            NEXT
        END IF
    NEXT
END SUB

' Whether the moth passed stone i near a cap.
FUNCTION Grazed% (st() AS Stone, i AS INTEGER, m AS Moth)
    DIM t AS INTEGER, b AS INTEGER
    t = st(i).gap - GAP \ 2
    b = st(i).gap + GAP \ 2
    Grazed% = m.y - t < NEARMISS OR b - m.y < NEARMISS
END FUNCTION

' The drums, with faster hi-hats once the score is
' high.
FUNCTION Drums$ (s AS Song)
    Drums$ = Tune(s.id, s.beat, 2)
    IF s.id <> GAMESONG OR s.heat < HEAT2 THEN EXIT FUNCTION
    Drums$ = Fast(s.beat)
END FUNCTION

' Short vines hang under a cap.
SUB Drips (x AS INTEGER, y AS INTEGER)
    DIM m AS INTEGER, n AS INTEGER, c1 AS INTEGER
    c1 = x - OVERHANG
    FOR m = c1 + 2 TO x + STONEW - 3 + OVERHANG STEP 4
        n = 1 + ((m - c1) * 7) MOD 5
        LINE (m, y)-(m, y + n), CVINE
    NEXT
END SUB

FUNCTION Elapsed! (a AS SINGLE, b AS SINGLE)
    DIM d AS SINGLE
    d = b - a
    IF d < 0 OR d > .1 THEN d = .1
    Elapsed! = d
END FUNCTION

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

' Sparks from the moth, as the burst id says.
SUB Emit (sp() AS Spark, m AS Moth, id AS INTEGER)
    DIM i AS INTEGER, n AS INTEGER, k AS SINGLE
    n = Bursts(id).n
    k = Bursts(id).kick
    FOR i = 0 TO NSPARK - 1
        IF n = 0 THEN EXIT FOR
        IF sp(i).life <= 0 THEN
            sp(i).x = m.x - 5 + RND * 3
            sp(i).y = m.y + RND * 4 - 2
            sp(i).vx = -30 - RND * 40 + (RND - .5) * k
            sp(i).vy = (RND - .5) * Bursts(id).spread
            sp(i).life = .6 + RND * .5
            n = n - 1
        END IF
    NEXT
END SUB

' The fade and the sky's key colours ease toward
' theirs, 30 times a second.
SUB EaseSky (s AS Sky, cur() AS SINGLE)
    DIM i AS INTEGER, j AS INTEGER, moved AS INTEGER
    DIM d AS SINGLE
    s.ease = s.ease + STEPT
    IF s.ease < 1 / 30 THEN EXIT SUB
    s.ease = 0
    FOR i = 0 TO NKEY - 1
        FOR j = 0 TO 2
            d = Chan%(SkyKey(s.phase, i), j) - cur(i, j)
            IF ABS(d) > .5 THEN
                cur(i, j) = cur(i, j) + d * .06
                moved = -1
            END IF
        NEXT
    NEXT
    d = s.fadeTo - s.fade
    IF d <> 0 THEN
        s.fade = s.fade + SGN(d) * .1
        IF ABS(d) <= .1 THEN s.fade = s.fadeTo
        moved = -1
    END IF
    IF moved THEN ApplyPalette s, cur()
END SUB

SUB Fall (w AS World, sp() AS Spark)
    Gravity w.m
    IF w.m.y < GROUNDY - 5 THEN EXIT SUB
    w.m.y = GROUNDY - 5
    w.g.mode = OVER
    w.g.modeT = 0
    Emit sp(), w.m, BLAND
    IF w.song.playing = 0 THEN EXIT SUB
    IF w.g.newBest THEN
        PlaySfx SFXBEST
    ELSE
        PlaySfx SFXOVER
    END IF
END SUB

' Keeps a few beats queued: tune, bass, drums, the
' sky's arpeggio and the echo or sub; voice 4 is
' left to the sound effects. Every voice gets each
' beat, or a pad a bar, so they stay together.
SUB FeedSong (s AS Song)
    DIM b AS INTEGER, id AS INTEGER
    DIM m AS STRING, d AS STRING
    IF s.id = NOSONG OR s.playing = 0 THEN EXIT SUB
    IF PLAY(0) >= 3 THEN EXIT SUB
    id = s.id
    b = s.beat
    IF b MOD 4 = 0 THEN s.bar = s.mood
    m = Mood(id, s.bar, b)
    d = Drums$(s)
    PLAY Tune(id, b, 0), Tune(id, b, 1), d, "", m, Lift$(s)
    s.beat = (b + 1) MOD SongLen(id)
END SUB

FUNCTION Feeler% (lx AS SINGLE, ly AS SINGLE)
    DIM f AS INTEGER
    f = SegD!(lx, ly, 10, -4, 17, -13) < 1.2
    f = f OR SegD!(lx, ly, 8, -4, 12, -14) < 1.1
    f = f OR (lx - 17) ^ 2 + (ly + 13) ^ 2 < 5
    Feeler% = f OR (lx - 11.5) ^ 2 + (ly + 1) ^ 2 < 5
END FUNCTION

SUB Flap (m AS Moth, sp() AS Spark)
    m.vy = FLAPV
    m.boost = .25
    m.squash = .12
    Emit sp(), m, BFLAP
    PlaySfx SFXFLAP
END SUB

' Shows the page just drawn, draws on the other.
SUB Flip (page AS INTEGER)
    SCREEN , , 1 - page, page
    page = 1 - page
END SUB

SUB Fly (w AS World, k AS Pad, st() AS Stone, sp() AS Spark)
    DIM i AS INTEGER
    IF k.flap THEN Flap w.m, sp()
    w.m.x = w.m.x + (HOMEX - w.m.x) * 5 * STEPT
    Gravity w.m
    w.g.scroll = w.g.scroll + SPEED * STEPT
    FOR i = 0 TO NSTONE - 1
        MoveStone st(), i
        IF st(i).passed = 0 THEN
            IF st(i).x + STONEW < w.m.x THEN
                st(i).passed = -1
                Scored w, sp()
                IF Grazed%(st(), i, w.m) THEN
                    PlaySfx SFXSWISH
                END IF
            END IF
        END IF
    NEXT
    IF Hit%(st(), w.m) OR w.m.y > GROUNDY - 5 THEN Die w
END SUB

' One row of a glyph: its runs of set bits as boxes.
SUB GlyphRow (b AS INTEGER, x AS INTEGER, y AS INTEGER, sc AS INTEGER, c AS INTEGER)
    DIM cx AS INTEGER, s AS INTEGER, e AS INTEGER
    e = y + sc - 1
    DO WHILE cx < 8
        IF b AND Bit(cx) THEN
            s = cx
            DO WHILE cx < 8
                IF (b AND Bit(cx)) = 0 THEN EXIT DO
                cx = cx + 1
            LOOP
            LINE (x + s * sc, y)-(x + cx * sc - 1, e), c, BF
        ELSE
            cx = cx + 1
        END IF
    LOOP
END SUB

SUB Gravity (m AS Moth)
    m.vy = m.vy + GRAV * STEPT
    m.y = m.y + m.vy * STEPT
END SUB

FUNCTION Hit% (st() AS Stone, m AS Moth)
    DIM i AS INTEGER, x AS SINGLE, r AS SINGLE
    DIM t AS SINGLE, b AS SINGLE, l AS SINGLE
    DIM rr AS SINGLE, h AS INTEGER
    FOR i = 0 TO NSTONE - 1
        x = st(i).x
        r = x + STONEW - 1
        IF x < m.x + 12 AND r + 1 > m.x - 12 THEN
            t = st(i).gap - GAP \ 2
            b = st(i).gap + GAP \ 2
            l = x - OVERHANG
            rr = r + OVERHANG
            h = h OR Touches%(m, x, 0, r, t - CAPH)
            h = h OR Touches%(m, l, t - CAPH, rr, t)
            h = h OR Touches%(m, l, b, rr, b + CAPH)
            h = h OR Touches%(m, x, b + CAPH, r, GROUNDY)
        END IF
    NEXT
    Hit% = h
END FUNCTION

SUB Launch (w AS World, st() AS Stone, sp() AS Spark)
    StartSong w.song, GAMESONG
    IF w.g.mode <> TITLE THEN w.m.y = 100
    w.m.behind = 0
    w.g.mode = PLAYING
    w.g.modeT = 0
    w.g.score = 0
    w.g.newBest = 0
    w.sky.phase = DAY
    PlaceStones st()
    Flap w.m, sp()
END SUB

' The layers over a song's beat b, made from its
' voice v's string k: the tune's echo a 16th late;
' in the game a bass and its sub under the
' arpeggio, now an octave up, on the title a pad a
' bar; the drums with the hi-hats doubled. The
' arpeggio and pad sound as the sky.
SUB Layer (id AS INTEGER, b AS INTEGER, v AS INTEGER, pre AS STRING, k AS STRING)
    DIM t AS STRING, r AS STRING, i AS INTEGER
    DIM p AS INTEGER
    t = LEFT$(pre, 4)
    SELECT CASE v
    CASE 0
        r = "P16" + NoteOf$(k, 0) + "16"
        Echo(id, b) = t + ECHOSND + r
    CASE 1
        IF id = GAMESONG THEN
            ' the arpeggio moves up, in the sky's sound;
            ' the bass takes its place, its sub under it
            r = NoteOf$(k, 7) + "32" + NoteOf$(k, 11) + "32"
            r = r + NoteOf$(k, 14) + "32" + NoteOf$(k, 11)
            FOR p = 0 TO NSKIES - 1
                Mood(id, p, b) = t + ArpSound$(p) + r + "32"
            NEXT
            r = BassBeat$(k, b, "O2")
            Tune(id, b, 1) = t + BASSSND + r
            Low(b) = t + SUBSND + BassBeat$(k, b, "O1")
        ELSE
            r = NoteOf$(k, 9) + "2"
            FOR p = 0 TO NSKIES - 1
                Mood(id, p, b) = ""
                IF b MOD 4 = 0 THEN
                    Mood(id, p, b) = t + PadSound$(p) + r
                END IF
            NEXT
        END IF
    CASE 2
        i = INSTR(k, "P32P16")
        IF id = GAMESONG AND i > 0 THEN
            r = LEFT$(k, i - 1) + "P32V3O6C32P32"
            Fast(b) = pre + r
        ELSEIF id = GAMESONG THEN
            Fast(b) = pre + k
        END IF
    END SELECT
END SUB

' The tune's echo on the title; the sub bass in game.
FUNCTION Lift$ (s AS Song)
    Lift$ = Echo(s.id, s.beat)
    IF s.id = GAMESONG THEN Lift$ = Low(s.beat)
END FUNCTION

' A cap, its light edge and shadow, outlined in ink.
SUB Lintel (x AS INTEGER, cy AS INTEGER)
    DIM c1 AS INTEGER, c2 AS INTEGER, b AS INTEGER
    c1 = x - OVERHANG
    c2 = x + STONEW - 1 + OVERHANG
    b = cy + CAPH - 1
    LINE (c1, cy)-(c2, b), CSTONE + 2, BF
    LINE (c1 + 2, cy)-(c1 + 8, b), CSTONE, BF
    LINE (c2 - 8, cy)-(c2, b), CSTONE + 5, BF
    LINE (c1 + 1, cy + 1)-(c2 - 1, cy + 1), CSTONE
    LINE (c1, cy)-(c2, b), CINK, B
END SUB

FUNCTION Lit& (c AS LONG)
    Lit& = Mix&(c, &HFFFFFF, .3)
END FUNCTION

SUB LoadBursts
    DIM i AS INTEGER
    RESTORE Bursts
    FOR i = 0 TO NBURST - 1
        READ Bursts(i).n, Bursts(i).spread
        READ Bursts(i).kick
    NEXT
END SUB

SUB LoadLooks
    DIM i AS INTEGER
    RESTORE Looks
    FOR i = 0 TO NLOOK - 1
        READ Looks(i).sc, Looks(i).c1
        READ Looks(i).c2, Looks(i).sh
    NEXT
END SUB

SUB LoadRidges
    DIM i AS INTEGER
    RESTORE Ridges
    FOR i = 0 TO NRIDGE - 1
        READ Ridges(i).speed, Ridges(i).c
        READ Ridges(i).d1, Ridges(i).d2
        READ Ridges(i).band, Ridges(i).bottom
        READ Ridges(i).stp
    NEXT
END SUB

SUB LoadSfx
    DIM i AS INTEGER
    RESTORE Sfx
    FOR i = 0 TO NSFX - 1
        READ SfxA(i), SfxB(i), SfxD(i)
    NEXT
END SUB

' The skies' key colours, and the colours that stay.
SUB LoadSkies
    DIM i AS INTEGER, j AS INTEGER
    DIM r AS LONG, g AS LONG, b AS LONG
    RESTORE Skies
    FOR i = 0 TO NSKIES - 1
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
        Fixed(i) = r + g * 256 + b * 65536
    LOOP
END SUB

' Each voice's beats, its sound before each one, and
' the layers made from them.
SUB LoadSongs
    DIM id AS INTEGER, v AS INTEGER, b AS INTEGER
    DIM n AS INTEGER, pre AS STRING, k AS STRING
    RESTORE Songs
    FOR id = TITLESONG TO GAMESONG
        READ n
        SongLen(id) = n
        FOR v = 0 TO 2
            READ pre
            FOR b = 0 TO n - 1
                READ k
                Tune(id, b, v) = pre + k
                Layer id, b, v, pre, k
            NEXT
        NEXT
    NEXT
END SUB

SUB LoadTables
    DIM i AS INTEGER
    Pow256(0) = 1: Pow256(1) = 256: Pow256(2) = 65536
    FOR i = 0 TO 255
        DacNow(i) = -1
    NEXT
    LoadSongs
    LoadSfx
    LoadSkies
    LoadLooks
    LoadBursts
    LoadRidges
END SUB

' The ridges' heights, trees, seeds and clouds.
SUB LoadTerrain
    DIM x AS INTEGER, i AS INTEGER, a AS SINGLE
    DIM h AS SINGLE
    RANDOMIZE TIMER
    FOR x = 0 TO 639
        a = x * 6.283185 / 640
        h = 19 * ABS(SIN(1.5 * a + .3))
        h = h + 9 * ABS(SIN(4 * a + 1))
        h = h + 3 * ABS(SIN(11 * a))
        Crest(0, x) = 123 - h
        h = 17 + 10 * SIN(2 * a) + 5 * SIN(5 * a + 1.3)
        Crest(1, x) = 143 - (h + 2 * SIN(13 * a))
        h = 9 + 7 * SIN(3 * a + 2) + 4 * SIN(7 * a + .4)
        Crest(2, x) = 166 - (h + 2 * SIN(17 * a))
    NEXT
    FOR i = 0 TO NTREE - 1
        TreeX(i) = i * 40 + INT(RND * 24)
        TreeH(i) = 8 + INT(RND * 9)
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
    BuildStones
    BuildLake
END SUB

' The game: set up, then read input, update in fixed
' steps, and draw, until Esc has faded it out.
SUB Main
    DIM w AS World, joy AS Pad
    DIM stones(NSTONE - 1) AS Stone
    DIM sparks(NSPARK - 1) AS Spark
    DIM cur(NKEY - 1, 2) AS SINGLE
    DIM now AS SINGLE, last AS SINGLE, acc AS SINGLE

    OpenScreen
    LoadTables
    LoadTerrain
    BuildGlyphs
    BuildMoth
    BuildSprites
    BuildLand
    NewWorld w, cur()

    last = TIMER
    DO
        now = NextTime!(last)
        acc = acc + Elapsed!(last, now)
        last = now

        ReadPad joy
        Control w, joy
        DO WHILE acc >= STEPT
            Tick w, joy, stones(), sparks()
            EaseSky w.sky, cur()
            joy.flap = 0
            acc = acc - STEPT
        LOOP
        FeedSong w.song
        Render w, stones(), sparks()
    LOOP UNTIL w.g.quitting AND w.sky.fade <= 0

    CloseScreen
END SUB

' Colour b mixed into a by t; red + green * 256 +
' blue * 65536.
FUNCTION Mix& (a AS LONG, b AS LONG, t AS SINGLE)
    DIM v AS LONG, ca AS LONG, cb AS LONG
    ca = a MOD 256: cb = b MOD 256
    v = CLNG(ca + (cb - ca) * t)
    ca = (a \ 256) MOD 256: cb = (b \ 256) MOD 256
    v = v + CLNG(ca + (cb - ca) * t) * 256
    ca = (a \ 65536) MOD 256: cb = (b \ 65536) MOD 256
    Mix& = v + CLNG(ca + (cb - ca) * t) * 65536
END FUNCTION

' Moss on a cap's top, in tufts.
SUB Moss (x AS INTEGER, cy AS INTEGER)
    DIM m AS INTEGER, c1 AS INTEGER, c2 AS INTEGER
    c1 = x - OVERHANG
    c2 = x + STONEW - 1 + OVERHANG
    LINE (c1 + 1, cy)-(c2 - 1, cy), CMOSS
    FOR m = c1 + 1 TO c2 - 1 STEP 3
        LINE (m, cy - 1 - (m - c1) MOD 2)-(m, cy), CMOSS
    NEXT
END SUB

' The sprite for the moth's tilt and wing beat.
FUNCTION MothFrame% (m AS Moth, mode AS INTEGER)
    DIM t AS INTEGER, w AS INTEGER, k AS INTEGER
    IF mode = DYING OR mode = OVER THEN
        MothFrame% = 9
        EXIT FUNCTION
    END IF
    t = 3
    IF m.vy < 220 THEN t = 2
    IF m.vy < 85 THEN t = 1
    IF m.vy < -60 THEN t = 0
    IF mode = TITLE THEN t = 1
    w = INT(m.beat * 4) MOD 4
    IF w = 3 THEN w = 1
    k = t * 3 + w
    IF mode = PLAYING THEN
        IF m.squash > 0 THEN k = 12
        IF m.vy > 220 THEN k = 13
    END IF
    MothFrame% = k
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

SUB MoveSparks (sp() AS Spark)
    DIM i AS INTEGER
    FOR i = 0 TO NSPARK - 1
        IF sp(i).life > 0 THEN
            sp(i).x = sp(i).x + sp(i).vx * STEPT
            sp(i).y = sp(i).y + sp(i).vy * STEPT
            sp(i).vx = sp(i).vx * (1 - 1.5 * STEPT)
            sp(i).vy = sp(i).vy * (1 - 2 * STEPT)
            sp(i).vy = sp(i).vy - 10 * STEPT
            sp(i).life = sp(i).life - STEPT
        END IF
    NEXT
END SUB

' The note of string k (as "O4E"), steps up the
' scale.
FUNCTION NoteOf$ (k AS STRING, steps AS INTEGER)
    DIM i AS INTEGER, o AS INTEGER, n AS STRING
    o = VAL(MID$(k, 2, 1))
    i = INSTR("CDEFGAB", MID$(k, 3, 1)) - 1 + steps
    o = o + i \ 7
    n = MID$("CDEFGAB", i MOD 7 + 1, 1)
    NoteOf$ = "O" + MID$(STR$(o), 2) + n
END FUNCTION

' Scrolls stone i; one gone off the left comes back
' a spacing after the farthest, with a new gap.
SUB MoveStone (st() AS Stone, i AS INTEGER)
    DIM j AS INTEGER, far AS INTEGER
    st(i).x = st(i).x - SPEED * STEPT
    IF st(i).x + STONEW + OVERHANG >= 0 THEN EXIT SUB
    FOR j = 0 TO NSTONE - 1
        IF st(j).x > st(far).x THEN far = j
    NEXT
    st(i).x = st(far).x + SPACING
    st(i).gap = NextGap%(st(far).gap)
    st(i).passed = 0
    st(i).kind = INT(RND * NKIND)
END SUB

SUB NewWorld (w AS World, cur() AS SINGLE)
    DIM i AS INTEGER, j AS INTEGER
    w.page = 1
    w.g.mode = TITLE
    w.sky.phase = DAY
    w.sky.fade = 0
    w.sky.fadeTo = 1
    FOR i = 0 TO NKEY - 1
        FOR j = 0 TO 2
            cur(i, j) = Chan%(SkyKey(DAY, i), j)
        NEXT
    NEXT
    ApplyPalette w.sky, cur()
    w.song.playing = -1
    PLAY "MB"
    StartSong w.song, TITLESONG
END SUB

FUNCTION NextGap% (prev AS INTEGER)
    DIM g AS INTEGER, lo AS INTEGER, hi AS INTEGER
    lo = GAP \ 2 + CAPH + 20
    hi = GROUNDY - GAP \ 2 - CAPH - 14
    g = prev + INT(RND * 113) - 56
    IF g < lo THEN g = lo
    IF g > hi THEN g = hi
    NextGap% = g
END FUNCTION

' The next TIMER value, waited for.
FUNCTION NextTime! (last AS SINGLE)
    DIM t AS SINGLE
    DO
        t = TIMER
    LOOP WHILE t = last
    NextTime! = t
END FUNCTION

SUB OpenScreen
    SCREEN 13, , 1, 0
END SUB

' Circling the sun, behind it on the lower half.
SUB Orbit (w AS World, k AS Pad, st() AS Stone, sp() AS Spark)
    DIM a AS SINGLE
    a = w.g.clock * .8
    w.m.x = SUNX + 26 * COS(a)
    w.m.y = SUNY + 8 + 28 * SIN(a)
    w.m.behind = SIN(a) > 0
    w.g.scroll = w.g.scroll + 25 * STEPT
    IF k.flap THEN Launch w, st(), sp()
END SUB

FUNCTION PadSound$ (p AS INTEGER)
    SELECT CASE p
    CASE DAY
        PadSound$ = PADDAY
    CASE GOLDEN
        PadSound$ = PADGOLD
    CASE ELSE
        PadSound$ = PADDUSK
    END SELECT
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

FUNCTION Pack& (r AS SINGLE, g AS SINGLE, b AS SINGLE)
    Pack& = CLNG(r) + CLNG(g) * 256 + CLNG(b) * 65536
END FUNCTION

' The game over panel drops in from above.
SUB Panel (g AS Game)
    DIM f AS SINGLE, y AS INTEGER
    f = g.modeT / .35
    IF f > 1 THEN f = 1
    y = 40 - INT((1 - f) ^ 3 * 200)
    LINE (54, y + 3)-(273, y + 103), CINK, BF
    LINE (50, y)-(270, y + 100), CPANEL, BF
    LINE (50, y)-(270, y + 100), CPANEL2, B
    LINE (52, y + 2)-(268, y + 98), CWHITE, B
    TxtC "WHAT A FLIGHT!", y + 10, LHEAD
    TxtC "SCORE" + STR$(g.score), y + 26, LBIGW
    TxtC "BEST" + STR$(g.best), y + 48, LBIGG
    IF g.newBest AND INT(g.clock * 4) MOD 2 = 0 THEN
        TxtC "NEW BEST!", y + 68, LNEW
    END IF
    IF g.modeT > .6 THEN
        TxtC "SPACE TO FLY AGAIN", y + 80, LSMALL
        TxtC "ESC FOR THE TITLE", y + 89, LSOFT
    END IF
END SUB

SUB PlaceStones (st() AS Stone)
    DIM i AS INTEGER, g AS INTEGER
    g = 100
    FOR i = 0 TO NSTONE - 1
        g = NextGap%(g)
        st(i).x = 360 + i * SPACING
        st(i).gap = g
        st(i).passed = 0
        st(i).kind = INT(RND * NKIND)
    NEXT
END SUB

SUB PlaySfx (id AS INTEGER)
    PLAY SfxA(id), SfxB(id), "", SfxD(id)
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

' Colours i0 to i0 + n - 1 from a toward b, reaching
' b after span steps.
SUB Ramp (c() AS LONG, i0 AS INTEGER, n AS INTEGER, a AS LONG, b AS LONG, span AS INTEGER)
    DIM i AS INTEGER
    FOR i = 0 TO n - 1
        c(i0 + i) = Mix&(a, b, i / span)
    NEXT
END SUB

SUB ReadPad (k AS Pad)
    DIM c AS STRING
    k.mute = 0
    k.back = 0
    DO
        c = INKEY$
        SELECT CASE c
        CASE " ", CHR$(13), CHR$(0) + "H"
            k.flap = -1
        CASE "m", "M"
            k.mute = -1
        CASE CHR$(27)
            k.back = -1
        END SELECT
    LOOP WHILE LEN(c)
END SUB

' Back to front, then the screen's effects, then the
' drawn page is shown.
SUB Render (w AS World, st() AS Stone, sp() AS Spark)
    DrawSky w.g
    IF w.m.behind THEN DrawMoth w.m, w.g.mode
    DrawSun w.g.clock
    DrawTop w
    DrawLand w.g
    IF w.g.mode <> TITLE THEN DrawStones st(), w.g.clock
    IF NOT w.m.behind THEN DrawMoth w.m, w.g.mode
    DrawLake w.g.clock
    DrawSparks sp()
    DrawHud w
    DrawFx w.fx
    Flip w.page
END SUB

' Runes down a shaft, glowing on and off.
SUB Runes (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER, t AS SINGLE)
    DIM c AS INTEGER, j AS INTEGER, n AS INTEGER
    DIM y AS INTEGER
    c = CWHITE
    IF SIN(t * 3 + x * .05) > 0 THEN c = CRUNE
    FOR y = y1 TO y2 STEP 10
        FOR n = 0 TO 14
            IF MID$(Rune(j MOD 4), n + 1, 1) = "1" THEN
                PSET (x + 13 + n MOD 3, y + n \ 3), c
            END IF
        NEXT
        j = j + 1
    NEXT
END SUB

SUB Scored (w AS World, sp() AS Spark)
    w.g.score = w.g.score + 1
    w.fx.pop = .25
    w.sky.phase = (w.g.score \ 10) MOD NSKIES
    w.song.mood = w.sky.phase
    w.song.heat = w.g.score
    PlaySfx SFXCHIME
    Emit sp(), w.m, BSCORE
END SUB

SUB ScoreText (g AS Game, f AS Fx)
    DIM id AS INTEGER
    id = LSCORE
    IF f.pop > 0 THEN id = LPOP
    TxtC LTRIM$(STR$(g.score)), 6, id
END SUB

' The colours from the sky's key colours k: sky,
' title, clouds, ridges, trees, stone, and the
' water's copies.
SUB Shade (k() AS LONG, c() AS LONG)
    DIM i AS INTEGER, h AS INTEGER
    h = NSKY \ 2
    Ramp c(), CSKY, h, k(0), k(1), h
    Ramp c(), CSKY + h, h, k(1), k(2), h - 1
    Ramp c(), CTITLE, 8, &H8CFFFF, &H1E78FF, 7
    c(CCLOUD) = k(3)
    c(CCLOUDLO) = k(4)
    Ramp c(), CMTN, 16, Lit&(k(5)), k(2), 16
    Ramp c(), CFAR, 16, Lit&(k(6)), k(7), 12
    Ramp c(), CNEAR, 16, Lit&(k(8)), k(9), 12
    Ramp c(), CTREE, 4, k(10), k(11), 3
    Ramp c(), CSTONE, 8, k(13), k(14), 7
    FOR i = 0 TO 15
        c(WATER + i) = Wet&(c(i), k(12))
    NEXT
    FOR i = CMTN TO CVINE
        c(WATER + i) = Wet&(c(i), k(12))
    NEXT
END SUB

' A stone's shaft: shaded in runs of a colour,
' mortar lines, inked sides.
SUB Shaft (x AS INTEGER, y1 AS INTEGER, y2 AS INTEGER)
    DIM dx AS INTEGER, s AS INTEGER, c AS INTEGER
    DIM y AS INTEGER, r AS INTEGER
    r = x + STONEW - 1
    FOR dx = 1 TO STONEW
        c = StoneCol(s + OVERHANG)
        IF dx = STONEW THEN
            LINE (x + s, y1)-(x + dx - 1, y2), c, BF
        ELSEIF StoneCol(dx + OVERHANG) <> c THEN
            LINE (x + s, y1)-(x + dx - 1, y2), c, BF
            s = dx
        END IF
    NEXT
    FOR y = y1 + 4 TO y2 STEP 9
        LINE (x + 1, y)-(r - 1, y), CSTONE + 6
        LINE (x + 2, y + 1)-(x + 8, y + 1), CSTONE
    NEXT
    LINE (x, y1)-(x, y2), CINK
    LINE (r, y1)-(r, y2), CINK
END SUB

' The whole frame moved down and right.
SUB Shake (amount AS SINGLE)
    DIM dx AS INTEGER, dy AS INTEGER
    dx = 1 + INT(RND * amount * 10)
    dy = 1 + INT(RND * amount * 10)
    GET (0, 0)-(SW - 1 - dx, SH - 1 - dy), Frame
    LINE (0, 0)-(SW - 1, dy - 1), CINK, BF
    LINE (0, 0)-(dx - 1, SH - 1), CINK, BF
    PUT (dx, dy), Frame, PSET
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

' Mushrooms on a cap.
SUB Shrooms (x AS INTEGER, cy AS INTEGER)
    DIM j AS INTEGER, m AS INTEGER
    FOR j = 0 TO 2
        m = x - OVERHANG + 5 + j * 12
        LINE (m, cy - 3)-(m, cy - 1), CWHITE
        LINE (m - 2, cy - 4)-(m + 2, cy - 4), CSHROOM
        LINE (m - 1, cy - 5)-(m + 1, cy - 5), CSHROOM
        PSET (m - 1, cy - 4), CWHITE
    NEXT
END SUB

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

SUB StartSong (s AS Song, id AS INTEGER)
    SOUND 37, 0
    s.id = id
    s.beat = 0
    s.mood = DAY
    s.bar = DAY
    s.heat = 0
END SUB

SUB StopSong (s AS Song)
    StartSong s, NOSONG
END SUB

' One fixed step: the screen's timers, the moth's,
' then what the mode does.
SUB Tick (w AS World, k AS Pad, st() AS Stone, sp() AS Spark)
    w.g.clock = w.g.clock + STEPT
    w.g.modeT = w.g.modeT + STEPT
    AgeFx w.fx
    AgeMoth w.m
    SELECT CASE w.g.mode
    CASE TITLE
        Orbit w, k, st(), sp()
    CASE PLAYING
        Fly w, k, st(), sp()
    CASE DYING
        Fall w, sp()
    CASE OVER
        IF k.flap AND w.g.modeT > .6 THEN
            Launch w, st(), sp()
        END IF
    END SELECT
    IF w.m.y < SPRH \ 2 + 2 THEN
        w.m.y = SPRH \ 2 + 2
        w.m.vy = 0
    END IF
    IF w.g.toTitle AND w.sky.fade <= 0 THEN BackToTitle w
    Trail w, sp()
    MoveSparks sp()
END SUB

SUB TitleText (g AS Game)
    DIM x AS INTEGER, h AS STRING
    TxtC "MOTHLIGHT", 14, LNAME
    ' a gleam sweeps across the name
    x = INT(g.clock * 140) MOD 560 - 100
    IF x >= 52 AND x <= 260 THEN
        VIEW SCREEN (x, 14)-(x + 10, 38)
        TxtC "MOTHLIGHT", 14, LGLEAM
        VIEW
    END IF
    TxtC "a little moth's big day out", 44, LSMALL
    IF INT(g.clock * 2) MOD 2 = 0 THEN
        TxtC "PRESS SPACE TO FLY", 116, LPROMPT
    END IF
    IF g.best THEN TxtC "BEST" + STR$(g.best), 138, LBEST
    h = "SPACE/ENTER/UP FLAP  M MUSIC  ESC QUIT"
    TxtC h, 189, LSMALL
END SUB

SUB ToggleMusic (s AS Song)
    s.playing = NOT s.playing
    IF s.playing = 0 THEN SOUND 37, 0
END SUB

' Whether the moth's body, round on the screen,
' meets the box.
FUNCTION Touches% (m AS Moth, x1 AS SINGLE, y1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE)
    DIM nx AS SINGLE, ny AS SINGLE
    DIM dx AS SINGLE, dy AS SINGLE
    nx = m.x: ny = m.y
    IF nx < x1 THEN nx = x1
    IF nx > x2 THEN nx = x2
    IF ny < y1 THEN ny = y1
    IF ny > y2 THEN ny = y2
    dx = m.x - nx
    dy = (m.y - ny) * ASPECT
    Touches% = dx * dx + dy * dy < 30
END FUNCTION

' A trail of sparks behind the moth while it flies.
SUB Trail (w AS World, sp() AS Spark)
    IF w.g.mode > PLAYING THEN EXIT SUB
    w.g.trail = w.g.trail + STEPT
    IF w.g.trail <= .05 THEN EXIT SUB
    w.g.trail = 0
    Emit sp(), w.m, BTRAIL
END SUB

' Text from the ROM font in a look: its scale, rows
' shaded from c1 to c2, over an inked shadow.
SUB Txt (t AS STRING, x AS INTEGER, y AS INTEGER, id AS INTEGER)
    DIM pass AS INTEGER, o AS INTEGER, i AS INTEGER
    DIM r AS INTEGER, c AS INTEGER, sc AS INTEGER
    DIM gx AS INTEGER, ch AS INTEGER, span AS INTEGER
    DIM c1 AS INTEGER, gy AS INTEGER
    sc = Looks(id).sc
    c1 = Looks(id).c1
    span = Looks(id).c2 - Looks(id).c1 + 1
    FOR pass = 0 TO 1
        o = 0
        IF pass = 0 THEN o = Looks(id).sh
        IF pass = 1 OR o > 0 THEN
            FOR i = 1 TO LEN(t)
                ch = ASC(MID$(t, i, 1))
                gx = x + (i - 1) * 8 * sc + o
                FOR r = 0 TO 7
                    c = CINK
                    IF pass THEN c = c1 + r * span \ 8
                    gy = y + r * sc + o
                    GlyphRow Glyph(ch, r), gx, gy, sc, c
                NEXT
            NEXT
        END IF
    NEXT
END SUB

SUB TxtC (t AS STRING, y AS INTEGER, id AS INTEGER)
    Txt t, (SW - LEN(t) * 8 * Looks(id).sc) \ 2, y, id
END SUB

' Vines hang from the top and sway, leaves on them.
SUB Vines (x AS INTEGER, y2 AS INTEGER, t AS SINGLE)
    DIM j AS INTEGER, y AS INTEGER, px AS INTEGER
    FOR j = 0 TO 2
        FOR y = 0 TO y2
            px = x + 5 + j * 9
            px = px + INT(1.5 * SIN(t * 1.7 + y * .16 + j))
            PSET (px, y), CVINE
            IF (y + j * 2) MOD 6 = 0 THEN
                PSET (px + 1, y), CMOSS
                PSET (px - 1, y + 1), CMOSS
            END IF
        NEXT
    NEXT
END SUB

FUNCTION Wet& (c AS LONG, lake AS LONG)
    Wet& = Mix&(Mix&(c, lake, .45), 0, .1)
END FUNCTION

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
