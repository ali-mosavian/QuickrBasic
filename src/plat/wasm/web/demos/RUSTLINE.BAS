' RUSTLINE: the war for the red ore of Kesh.
' Lead the Coalition: mine ore, build a base, destroy the Scorch Legion.
' Mouse: left drags a box to select, right orders (move, attack, harvest); the sidebar builds.
' Arrows or the screen's edges scroll; H home, A all army, S stop, M sound, Esc leaves.
' PEN reads the mouse. SCREEN 13, 256 colours, on two pages: one drawn while the other is shown
' (QB 4.5's SCREEN 13 has one; QuickrBasic's has two).
' Driven by TIMER: the same speed on any machine.
'
' No shared state: the game is a GameT and the collections the main module owns, passed down.
' The rules (hit points, costs) are functions of a kind.
DEFINT A-Z

' TS: a tile's side in the world's pixels. The screen shows the world isometric: a tile is a
' diamond 32 by 16; the plane's x is the world's x - y, its y half x + y. The map's view is
' VX0, VY0 to SBX, VIEWB on the screen, the camera in the plane's pixels, by half tiles.
CONST MAPW = 40, MAPH = 30, TS = 16, SBX = 240, VIEWH = 192
CONST VX0 = 16, VY0 = 22, VIEWB = 184, VWP = SBX - VX0, VHP = VIEWB - VY0
' STEPH: a height's step in pixels, 2 a level; MAXH the highest
CONST PLANEW = (MAPW + MAPH) * 16, PLANEH = (MAPW + MAPH) * 8, ROCKH = 6, STEPH = 6, MAXH = 4
CONST MAXU = 64, MAXB = 32, MAXFX = 64, NF = 24, NPART = 48
CONST STEPT = .0333333
CONST GTITLE = 0, GPLAY = 1, GWIN = 2, GLOSE = 3
CONST TGROUND = 0, TGROUND2 = 1, TDIRT = 2, TROCK = 3, TWATER = 4, TORE = 5, TTREE = 6
CONST UHARV = 1, URIFLE = 2, UTANK = 3
CONST BHQ = 1, BREF = 2, BBAR = 3, BTUR = 4
CONST SIDLE = 0, SMOVE = 1, SATTACK = 2, SSEEK = 3, SGATHER = 4, SRETURN = 5, SUNLOAD = 6
CONST FXBOOM = 1, FXBIG = 2, FXTRACE = 3, FXSHELL = 4, FXBEAM = 5, FXSMOKE = 6, FXMARK = 7
CONST OREKEY = -2, NOKEY = -99, FAR = 32000, LOADMAX = 10, LOADVAL = 25
CONST MINIX = 240, MINIY = 11, BTNY = 72, BTNH = 18
' the palette: 0 to 103 the land's, units' and buildings' (from 32 the art tool's), 104 on the
' water's, turned round to move it (WATERC), 112 on fire's;
' in play 128 to 255 the same in the fog (FOG OR c), on the title and the end their ramps
CONST WATERC = 104, NWATER = 8, FOG = 128, FIRE = 112, SKY = 128, PLANET = 160, LOGOC = 192
' the art, one array of IMG-element slots: images of 16 by 16 (a unit's sprite, mask and shadow),
' the minimap, the full screen.
' Past 64 KB: QB 4.5 needs /AH.
' the land, diamonds of 2 slots: 16 of grass at each of 3 levels, a 4 by 4 piece of it each, the diamond's mask, the
' fog; what lies on the grass, an image and a mask each: dirt, flowers, ore thin to thick, water by
' which of its 4 sides go on (OWATER + sides); rock, raised, by its
' sides: image, mask, fog, 3 slots each; a tree's image, mask, fog and shade, 2 slots each; a cliff's
' step, its left and right faces' image, mask and fog, a slot each (left then right); the shade at
' a cliff's foot by its left and right back edges; the
' buildings' image, mask and fog, BLDW slots each
CONST OWATER = 5, NOVL = 21, BLDW = 15
CONST IMG = 130, ASPR = 0, AMSK = 96, ASHD = 192, AGRS = 288, ADIA = AGRS + 96, AFOG = ADIA + 2
CONST AOVL = AFOG + 2, AROK = AOVL + NOVL * 4, ATREE = AROK + 16 * 9, AFACE = ATREE + 8, ASHA = AFACE + 6, ABLD = ASHA + 2
CONST AICN = ABLD + 24 * BLDW, AMINI = AICN + 3, ABACK = AMINI + 19, NSLOT = ABACK + 247

TYPE CellT
    terr AS INTEGER
    amt AS INTEGER   ' ore left
    seen AS INTEGER  ' the Coalition has seen it
    vis AS INTEGER   ' the Coalition sees it now
    occ AS INTEGER   ' the unit on it, or moving onto it
    bld AS INTEGER   ' the building on it
    hgt AS INTEGER   ' its height, 2 a level: odd, a ramp
END TYPE

TYPE UnitT
    alive AS INTEGER
    kind AS INTEGER
    team AS INTEGER
    hp AS INTEGER
    x AS INTEGER     ' its tile
    y AS INTEGER
    nx AS INTEGER    ' the tile it moves to; x, y when still
    ny AS INTEGER
    px AS SINGLE     ' its middle, in the map's pixels
    py AS SINGLE
    dir AS INTEGER   ' 0 east, 2 south, 4 west, 6 north
    st AS INTEGER    ' S*
    gx AS INTEGER    ' where it goes
    gy AS INTEGER
    tk AS INTEGER    ' its target: 1 a unit, 2 a building
    ti AS INTEGER
    cool AS SINGLE
    anim AS SINGLE
    load AS INTEGER
    work AS SINGLE
    sel AS INTEGER
    stuck AS SINGLE
    agg AS INTEGER   ' on the march: it fights what it meets
    scan AS SINGLE
END TYPE

TYPE BldgT
    alive AS INTEGER
    kind AS INTEGER
    team AS INTEGER
    hp AS INTEGER
    x AS INTEGER     ' its top left tile
    y AS INTEGER
    cool AS SINGLE
    ti AS INTEGER    ' a turret's target unit
    ang AS SINGLE    ' a turret's gun
    scan AS SINGLE
    smoke AS SINGLE
END TYPE

TYPE FxT
    alive AS INTEGER
    kind AS INTEGER
    x AS SINGLE
    y AS SINGLE
    x2 AS SINGLE
    y2 AS SINGLE
    t AS SINGLE
    life AS SINGLE
END TYPE

TYPE PartT
    x AS SINGLE
    y AS SINGLE
    v AS SINGLE
    ph AS SINGLE
END TYPE

' A flow field's goal (a tile, or OREKEY) and when it was last used.
TYPE FieldT
    goal AS INTEGER
    used AS SINGLE
END TYPE

' A side: its credits, the building it builds, its two factories' work and queues.
TYPE TeamT
    cash AS INTEGER
    sKind AS INTEGER ' the building being built, -1 none
    sProg AS SINGLE
    sReady AS INTEGER
    hallKind AS INTEGER
    hallProg AS SINGLE
    denKind AS INTEGER
    denProg AS SINGLE
    q3 AS INTEGER    ' harvesters, riflemen, tanks waiting
    q4 AS INTEGER
    q5 AS INTEGER
    built AS INTEGER
    harvested AS LONG
END TYPE

TYPE MouseT
    x AS INTEGER
    y AS INTEGER
    down AS INTEGER
    lHit AS INTEGER  ' pressed since the last frame, and where
    lX AS INTEGER
    lY AS INTEGER
    rHit AS INTEGER
    rX AS INTEGER
    rY AS INTEGER
    dragOn AS INTEGER
    dX0 AS INTEGER
    dY0 AS INTEGER
END TYPE

TYPE ViewT
    camX AS INTEGER  ' the plane's pixel at the view's top left
    camY AS INTEGER
    page AS INTEGER  ' the page being drawn
    orePh AS INTEGER
    waterF AS INTEGER  ' the water's colours' turn
    miniDirty AS INTEGER
    miniT AS SINGLE
    scrollAcc AS SINGLE
END TYPE

TYPE AiT
    think AS SINGLE
    wave AS INTEGER
    size AS INTEGER
    nextWave AS SINGLE
    alarmK AS INTEGER ' who last hit the Legion
    alarmI AS INTEGER
    alarmT AS SINGLE
    coin AS SINGLE
END TYPE

TYPE SoundT
    enabled AS INTEGER
    song AS INTEGER
    beat AS INTEGER
    last AS SINGLE    ' the last rifle's or credit's beep
    due AS SINGLE     ' when the battle's bass runs out
    noise AS SINGLE   ' when voice 3's last sound ends
END TYPE

TYPE GameT
    state AS INTEGER
    won AS INTEGER
    quitting AS INTEGER
    placeKind AS INTEGER
    now AS SINGLE     ' the game's seconds
    clock AS SINGLE   ' the real ones
    endT AS SINGLE
    escT AS SINGLE
    msgT AS SINGLE
    alarmT AS SINGLE
    visT AS SINGLE
    checkT AS SINGLE
    clickT AS SINGLE  ' the last click on a unit of ours, and the unit
    clickU AS INTEGER
    alertX AS INTEGER ' the tile of the last alarm, -1 none
    alertY AS INTEGER
    losses AS INTEGER ' the Coalition's units and buildings lost
    kills AS INTEGER  ' the Legion's
    msg AS STRING * 64
    ms AS MouseT
    vw AS ViewT
    ai AS AiT
    snd AS SoundT
END TYPE

DECLARE SUB Setup (g AS GameT, tune() AS STRING)
DECLARE SUB BuildGlyphs (gl() AS INTEGER)
DECLARE SUB BuildArt (art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB Progress (n AS INTEGER, total AS INTEGER, gl() AS INTEGER)
DECLARE FUNCTION Overlay% (g AS GameT, m() AS CellT, x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION Sides% (m() AS CellT, x AS INTEGER, y AS INTEGER)
DECLARE SUB NewGame (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT)
DECLARE SUB GoTitle (g AS GameT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB GoEnd (g AS GameT, tm() AS TeamT, won AS INTEGER, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB ReadMouse (ms AS MouseT)
DECLARE SUB Keys (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB Controls (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, dt AS SINGLE)
DECLARE SUB Tick (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fd() AS INTEGER, fk() AS FieldT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB UnitTick (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fd() AS INTEGER, fk() AS FieldT, i AS INTEGER, dt AS SINGLE)
DECLARE SUB TurretTick (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, b AS INTEGER, dt AS SINGLE)
DECLARE SUB Produce (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, ti AS INTEGER, dt AS SINGLE)
DECLARE SUB Train (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, ti AS INTEGER, fb AS INTEGER, kind AS INTEGER, prog AS SINGLE, dt AS SINGLE)
DECLARE SUB AIThink (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fk() AS FieldT)
DECLARE SUB AIPlace (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fk() AS FieldT)
DECLARE SUB UpdateVis (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT)
DECLARE SUB Fire (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, i AS INTEGER)
DECLARE SUB Hit (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, k AS INTEGER, idx AS INTEGER, dmg AS INTEGER, sk AS INTEGER, si AS INTEGER)
DECLARE SUB KillUnit (g AS GameT, m() AS CellT, u() AS UnitT, fx() AS FxT, i AS INTEGER)
DECLARE SUB KillBuilding (g AS GameT, m() AS CellT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, b AS INTEGER)
DECLARE SUB PlaceBuilding (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fk() AS FieldT, ti AS INTEGER, kind AS INTEGER, x AS INTEGER, y AS INTEGER)
DECLARE SUB ClearFields (fk() AS FieldT)
DECLARE SUB BuildField (m() AS CellT, fd() AS INTEGER, fi AS INTEGER, goal AS INTEGER)
DECLARE SUB DepleteOre (g AS GameT, m() AS CellT, fk() AS FieldT, x AS INTEGER, y AS INTEGER)
DECLARE SUB AddFx (fx() AS FxT, k AS INTEGER, x AS SINGLE, y AS SINGLE, x2 AS SINGLE, y2 AS SINGLE, life AS SINGLE)
DECLARE SUB FxTick (fx() AS FxT, dt AS SINGLE)
DECLARE SUB Order (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, wx AS INTEGER, wy AS INTEGER, pu AS INTEGER, pb AS INTEGER)
DECLARE SUB SelectBox (g AS GameT, m() AS CellT, u() AS UnitT, x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER)
DECLARE SUB ButtonClick (g AS GameT, tm() AS TeamT, bl() AS BldgT, k AS INTEGER, rightButton AS INTEGER)
DECLARE SUB CenterOn (cam AS ViewT, x AS INTEGER, y AS INTEGER)
DECLARE SUB RenderGame (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB DrawObjects (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, art() AS INTEGER, tx() AS INTEGER, ty() AS INTEGER, nt AS INTEGER)
DECLARE SUB DrawFx (g AS GameT, m() AS CellT, fx() AS FxT)
DECLARE SUB DrawSidebar (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB DrawMinimap (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, art() AS INTEGER)
DECLARE SUB DrawCursor (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT)
DECLARE SUB Arrow (x AS INTEGER, y AS INTEGER)
DECLARE SUB DrawTitleBack (art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB DrawEndBack (g AS GameT, tm() AS TeamT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB RenderTitle (g AS GameT, pt() AS PartT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB RenderEnd (g AS GameT, pt() AS PartT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB Particles (pt() AS PartT, falling AS INTEGER, dt AS SINGLE)
DECLARE SUB Bar (x AS INTEGER, y AS INTEGER, w AS INTEGER, f AS SINGLE, c AS INTEGER)
DECLARE SUB Txt (gl() AS INTEGER, t AS STRING, x AS INTEGER, y AS INTEGER, sc AS INTEGER, c AS INTEGER)
DECLARE SUB TxtC (gl() AS INTEGER, t AS STRING, y AS INTEGER, sc AS INTEGER, c AS INTEGER)
DECLARE SUB Logo (gl() AS INTEGER, t AS STRING, y AS INTEGER, bw AS INTEGER, bh AS INTEGER)
DECLARE SUB Dither (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, c AS INTEGER)
DECLARE SUB Sprite (art() AS INTEGER, idx AS INTEGER, x AS INTEGER, y AS INTEGER)
DECLARE SUB Say (g AS GameT, t AS STRING)
DECLARE SUB Sfx (g AS GameT, k AS INTEGER)
DECLARE SUB Music (g AS GameT, tune() AS STRING)
DECLARE SUB Flip (cam AS ViewT)
DECLARE SUB Enqueue (t AS TeamT, k AS INTEGER, n AS INTEGER)
DECLARE FUNCTION Queued% (t AS TeamT, k AS INTEGER)
DECLARE FUNCTION Rgb& (r AS INTEGER, g AS INTEGER, b AS INTEGER)
DECLARE SUB GamePalette ()
DECLARE SUB WaterColours (t AS INTEGER)
DECLARE SUB Ramp (c0 AS INTEGER, n AS INTEGER, r0 AS INTEGER, g0 AS INTEGER, b0 AS INTEGER, r1 AS INTEGER, g1 AS INTEGER, b1 AS INTEGER)
DECLARE SUB Shade (c AS INTEGER, r AS INTEGER, g AS INTEGER, b AS INTEGER)
DECLARE SUB IsoUV (px AS INTEGER, py AS INTEGER, u AS SINGLE, v AS SINGLE)
DECLARE SUB ScreenTile (cam AS ViewT, sx AS INTEGER, sy AS INTEGER, wx AS INTEGER, wy AS INTEGER)
DECLARE SUB ClampCam (cam AS ViewT)
DECLARE FUNCTION PickUnit% (g AS GameT, m() AS CellT, u() AS UnitT, sx AS INTEGER, sy AS INTEGER, team AS INTEGER)
DECLARE FUNCTION PickBld% (g AS GameT, m() AS CellT, bl() AS BldgT, sx AS INTEGER, sy AS INTEGER)
DECLARE FUNCTION IsoDir% (d AS INTEGER)
DECLARE FUNCTION BldH% (bt AS INTEGER)
DECLARE SUB MaskFog (mx AS INTEGER, fx AS INTEGER, w AS INTEGER, h AS INTEGER)
DECLARE SUB Unpack (ox AS INTEGER, mx AS INTEGER, hx AS INTEGER, oy AS INTEGER)
DECLARE FUNCTION Passable% (m() AS CellT, x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION Walk% (m() AS CellT, x AS INTEGER, y AS INTEGER, d AS INTEGER)
DECLARE FUNCTION Lift% (m() AS CellT, x AS INTEGER, y AS INTEGER)
DECLARE SUB UnitScreen (g AS GameT, m() AS CellT, u() AS UnitT, i AS INTEGER, sx AS INTEGER, sy AS INTEGER)
DECLARE SUB PickTile (g AS GameT, m() AS CellT, sx AS INTEGER, sy AS INTEGER, wx AS INTEGER, wy AS INTEGER)
DECLARE FUNCTION FlowField% (m() AS CellT, fd() AS INTEGER, fk() AS FieldT, goal AS INTEGER, now AS SINGLE)
DECLARE FUNCTION MoveStep% (g AS GameT, m() AS CellT, u() AS UnitT, fd() AS INTEGER, fk() AS FieldT, i AS INTEGER, goal AS INTEGER, exact AS INTEGER, dt AS SINGLE)
DECLARE FUNCTION DirOf% (dx AS SINGLE, dy AS SINGLE)
DECLARE FUNCTION Ang! (dx AS SINGLE, dy AS SINGLE)
DECLARE FUNCTION SpawnUnit% (tm() AS TeamT, m() AS CellT, u() AS UnitT, ti AS INTEGER, ut AS INTEGER, x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION FindSpawn% (m() AS CellT, bl() AS BldgT, b AS INTEGER)
DECLARE FUNCTION FindB% (bl() AS BldgT, ti AS INTEGER, bt AS INTEGER)
DECLARE FUNCTION CountB% (bl() AS BldgT, ti AS INTEGER, bt AS INTEGER)
DECLARE FUNCTION CountU% (u() AS UnitT, ti AS INTEGER, ut AS INTEGER)
DECLARE FUNCTION CanPlace% (m() AS CellT, bl() AS BldgT, ti AS INTEGER, kind AS INTEGER, x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION NearestRef% (u() AS UnitT, bl() AS BldgT, i AS INTEGER)
DECLARE FUNCTION TargetDist! (u() AS UnitT, bl() AS BldgT, i AS INTEGER)
DECLARE FUNCTION Scan% (u() AS UnitT, bl() AS BldgT, i AS INTEGER, radius AS SINGLE)
DECLARE FUNCTION Allowed% (bl() AS BldgT, k AS INTEGER)
DECLARE FUNCTION Clock2$ (s AS SINGLE)
DECLARE FUNCTION StepX% (d AS INTEGER)
DECLARE FUNCTION StepY% (d AS INTEGER)
DECLARE FUNCTION UHp% (k AS INTEGER)
DECLARE FUNCTION USpeed! (k AS INTEGER)
DECLARE FUNCTION URange! (k AS INTEGER)
DECLARE FUNCTION UDmg% (k AS INTEGER)
DECLARE FUNCTION URate! (k AS INTEGER)
DECLARE FUNCTION USight% (k AS INTEGER)
DECLARE FUNCTION BHp% (k AS INTEGER)
DECLARE FUNCTION BSize% (k AS INTEGER)
DECLARE FUNCTION KName$ (k AS INTEGER)
DECLARE FUNCTION KCost% (k AS INTEGER)
DECLARE FUNCTION KSecs! (k AS INTEGER)
DECLARE FUNCTION KMakes% (k AS INTEGER)

'$DYNAMIC
DIM g AS GameT, tm(1) AS TeamT, m(MAPW - 1, MAPH - 1) AS CellT
DIM u(1 TO MAXU) AS UnitT, bl(1 TO MAXB) AS BldgT, fx(1 TO MAXFX) AS FxT, pt(1 TO NPART) AS PartT
DIM fd(NF - 1, MAPW - 1, MAPH - 1), fk(NF - 1) AS FieldT
DIM art(IMG - 1, NSLOT - 1), gl(126, 7), tune(1 TO 4, 127) AS STRING
DIM t AS SINGLE, lastT AS SINGLE, dt AS SINGLE, acc AS SINGLE

Setup g, tune()
BuildGlyphs gl()
BuildArt art(), gl()
PEN ON
PLAY "MB"
GoTitle g, art(), gl()

lastT = TIMER
DO
    DO: t = TIMER: LOOP WHILE t - lastT < .015 AND t >= lastT
    dt = t - lastT
    lastT = t
    IF dt < 0 THEN dt = .02 ELSE IF dt > .25 THEN dt = .25
    g.clock = g.clock + dt

    ReadMouse g.ms
    Keys g, tm(), m(), u(), bl(), fx(), fk(), art(), gl()
    SELECT CASE g.state
    CASE GPLAY
        Controls g, tm(), m(), u(), bl(), fx(), fk(), dt
        acc = acc + dt
        DO WHILE acc >= STEPT AND g.state = GPLAY
            Tick g, tm(), m(), u(), bl(), fx(), fd(), fk(), art(), gl()
            acc = acc - STEPT
        LOOP
        IF g.state = GPLAY THEN RenderGame g, tm(), m(), u(), bl(), fx(), art(), gl()
    CASE GTITLE
        Particles pt(), 0, dt
        RenderTitle g, pt(), art(), gl()
    CASE ELSE
        g.endT = g.endT + dt
        Particles pt(), g.state = GLOSE, dt
        RenderEnd g, pt(), art(), gl()
    END SELECT
    Music g, tune()
    Flip g.vw
LOOP UNTIL g.quitting

SOUND 37, 0
SCREEN 0
WIDTH 80
END

' The map, a row to a string: . , ground, : dirt, # rock, ~ water, m ore, T palm.
HeightData:
' each tile's height, 2 a level: odd ones are ramps
DATA "2222222222222244444422222244444444444444"
DATA "2222222222222224444222222244444444444444"
DATA "2222222222222224442222222244444444444444"
DATA "2222222222222222432222222244444444444444"
DATA "2222222222222222222222222244444444444444"
DATA "2222222202222222222222222244444444444444"
DATA "2222222002222222222222222244444444444444"
DATA "0222220000222222222222222244444444444444"
DATA "0002210000022222222222222244444444444444"
DATA "2200000000022222222222222244444444444444"
DATA "2220000000022222222222222222222222322444"
DATA "0100000000022222222222222222222222222444"
DATA "0000000000022222222222222222222222222244"
DATA "0000000000022222222222222232222222222244"
DATA "0000000000002222222223344444423222222244"
DATA "0000000000000022222224444444444322222222"
DATA "0000000000022000222244444444444444422222"
DATA "0000000000010000022232444444442222222222"
DATA "0000000000000002000222222222322222222222"
DATA "0000000000000002222222222222222222222222"
DATA "0000000000000002222200222222222222222222"
DATA "0000000000000002222220022222222222222222"
DATA "0000000000000002222222222222222222222222"
DATA "0000000000000000000000000222222222222222"
DATA "0000000000000000000000000000001002222222"
DATA "0000000000000000000000000000000222222222"
DATA "0000000000000000000000000000000022222222"
DATA "0000000000000000000000000000122002222222"
DATA "0000000000000000000000000000222200022222"
DATA "0000000000000000000000000000222222222222"

PalData:
' colours 32 to 103: red, green, blue
DATA 31, 29, 34
DATA 63, 31, 7
DATA 18, 28, 10
DATA 0, 33, 34
DATA 16, 12, 8
DATA 12, 12, 15
DATA 29, 23, 15
DATA 40, 50, 60
DATA 15, 25, 10
DATA 15, 13, 15
DATA 18, 29, 11
DATA 19, 29, 11
DATA 17, 27, 10
DATA 6, 6, 8
DATA 19, 30, 11
DATA 13, 10, 7
DATA 12, 22, 9
DATA 17, 10, 19
DATA 17, 14, 17
DATA 16, 26, 10
DATA 11, 8, 5
DATA 48, 21, 3
DATA 24, 35, 12
DATA 23, 51, 49
DATA 13, 11, 13
DATA 22, 33, 12
DATA 24, 20, 24
DATA 55, 44, 11
DATA 20, 14, 8
DATA 24, 10, 2
DATA 7, 6, 7
DATA 10, 23, 23
DATA 27, 21, 6
DATA 14, 15, 17
DATA 5, 5, 6
DATA 7, 6, 3
DATA 13, 23, 9
DATA 33, 33, 38
DATA 13, 13, 16
DATA 14, 12, 14
DATA 19, 19, 22
DATA 14, 12, 2
DATA 14, 24, 9
DATA 10, 10, 12
DATA 0, 18, 18
DATA 22, 19, 22
DATA 7, 15, 15
DATA 37, 16, 2
DATA 20, 17, 20
DATA 31, 13, 2
DATA 21, 21, 25
DATA 18, 15, 18
DATA 4, 4, 5
DATA 13, 29, 30
DATA 12, 21, 9
DATA 13, 23, 9
DATA 15, 25, 10
DATA 14, 24, 9
DATA 28, 26, 31
DATA 16, 26, 10
DATA 17, 27, 10
DATA 42, 43, 49
DATA 11, 20, 8
DATA 16, 17, 19
DATA 10, 19, 8
DATA 16, 36, 36
DATA 11, 11, 13
DATA 9, 8, 10
DATA 5, 52, 54
DATA 9, 18, 8
DATA 38, 30, 8
DATA 20, 31, 11

ArtData:
' grass by level, what lies on it, rock, the tree, a cliff's faces, the buildings, the units:
' width, height, then rows of symbols (Unpack); tools/rustline_art.py writes these
DATA 32, 16
DATA "pcS!'}cSd!'S!'yc!A!9S!9!'!'!-!-VVuc!9!9!A!9SS!-!-VVOOVVqcS!'S!'!'S!-!'VKeOeVmcS!9SS!'S!'!-VKOOEdOeVK"
DATA "Kic!9SS!9dS!'!-!-VVOEEdOVVKV!-!-!'!'ec!'Sf!'dS!'!'KKVKeVKK!-e!'S!'!'ccd!'!-!'!'S!'!9SS!9i!'S!'!9S!A"
DATA "!9!A!9!9Sece!'SS!A!9fS!9eS!9S!A!9!A!A!C!AicS!'SS!9SS!9S!9!9dSf!'S!9Smc!9dS!'fS!'!'e!-!'!'qcdS!'!'S"
DATA "f!'d!-uc!'SSf!'!-!'ycS!'!'S!'!-}c!'Spc"
DATA ""
DATA 32, 16
DATA "pc!9S}c!A!A!9!A!9Syc!A!9!A!9!9!AS!9SSucSSf!9!A!9!9Sd!9qc!'!'S!9!9!A!9!A!9!A!A!9!9S!9!A!9!Amc!-!-!'!-"
DATA "!'dS!9S!9!9!A!9dS!9e!Aic!-!-K!-!-K!'!-!'S!'SS!'!9SS!9!9S!A!9!A!9!9!Aec!'!'e!-d!'iS!9!A!9!A!A!9!A!9!9"
DATA "Sd!9ccS!'S!9!9S!9!A!A!C!A!C!A!9!A!9!A!9!A!C!A!C!A!9!9SS!9SSec!9!9!A!Ae!C!A!C!Ad!C!A!C!Ad!Cd!A!9S!9ic"
DATA "!9e!A!C!A!C!C!Ad!C!Ad!C!H!C!C!A!CmcS!9e!Ai!C!Hd!CqcS!'!9!A!A!C!A!Cd!A!C!C!AucSS!A!9!A!9!A!9!A!AycS!9"
DATA "S!9!9S}c!'Spc"
DATA ""
DATA 32, 16
DATA "pcS!'}c!'S!'!'S!'yc!A!9!9!A!9!A!9!A!A!Cucg!A!Cd!A!C!C!H!Hqc!A!C!A!9!C!Ad!C%d!C!H!C!H!H!Cmc!9f!Ad!C!H"
DATA "!H!C!Cd!H!C!He!Cic!9!A!A!9d!A!C!C!A!C!H!H!C!H!Cd!H!C!C!H!C!A!9!AecSSf!9f!Ad!Cd!H!C!H!C!Ce!A!9!9SSccS"
DATA "!9S!9S!9S!9!9!A!A!9!A!C!C!H!C!H!A!C!9!A!9S!9!A!A!9!9!AecSS!9SS!9!9d!A!9f!A!9!AS!9S!9!Ad!9icS!'SS!A!9"
DATA "!A!A!9!A!9S!9!A!A!9!9!A!A!9!A!9mcS!'!9!9e!Ae!9!Ae!9!AqcS!9!A!9!A!C!A!C!A!9!A!9!9!Auc!A!9!C!A!A!C!9!A"
DATA "!A!9yc!A!C!A!A!9S}c!9!9pc"
DATA ""
DATA 32, 16
DATA "pcS!9}cS!9SS!'!'yc!9SS!9S!9!'!'!-!'uc!A!Ad!9dS!'!'!-!'!-!-qc!A!9!A!9!9SS!'!'Sf!'SS!9mc!A!9!A!9!9!A!9"
DATA "!9SSe!'SS!9!9!A!A!9!AicS!'S!'S!9!A!9!9!AS!9S!'S!'S!9!9e!A!C!A!9ecSS!'!-!-!'SSe!9eSg!9d!A!C!A!ASScc!'"
DATA "!-!-K!-!'S!9!A!9!9S!'SS!9!A!9!A!9!9!A!A!9!A!C!9S!'Sec!-!-!'!'!9!9!A!Ad!9S!9S!A!9!A!9!A!9e!A!9!9ic!'S"
DATA "!9!A!A!9!9!A!9SS!9S!'SS!9S!A!9!A!9mcSSg!A!9!ASS!'eS!9qc!A!9!Ae!C!A!9!AS!9S!9ucd!A!C!A!A!9!A!9!9yc!A"
DATA "!9!9!AS!9}c!9Spc"
DATA ""
DATA 32, 16
DATA "pc!'S}cg!'ycdK!-!-K!'!-S!'ucVVOVKK!-K!'!'SS!9!9qcKVeOKV!-!-!'!'!9!A!A!C!9!Amc!-!-VVeOVV!-!-S!'!9!A!A"
DATA "!C!A!ASSicS!'!-!-VVeOVO!-KS!'!A!9!A!C!9Se!'ec!9!9SS!-!-OVeEOO!-K!'!'Sd!9S!9SS!'dScce!'!-!-VVEMNMENVV"
DATA "!-K!'!'SS!9!A!9!AS!9S!9ece!-VKEOEdNOEKV!-!-S!'!A!9!9!A!9Sic!-KVKOVEOMEEMVO!-K!'!'!9S!A!9mceVOOEEMEOO"
DATA "VK!-!-!'SqcdVeOEOEVVK!-ucdOfVKKycVVKK!-!-}c!'!'pc"
DATA ""
DATA 32, 16
DATA "pcS!9}c!9fSyc!A!9S!9S!'S!'SSuc!A!A!9!AgS!'dSqc!'!'S!9S!'!'SS!'S!'S!'!'S!'Smc!-!-!'!'Sd!'!-h!'Sf!'ic"
DATA "!'!'S!'!'Se!'!-!'!'!-d!'SS!9Sd!'SSecSS!Ad!9!'!'!-!-!'d!-!'Sd!9d!A!9!9gScc!A!C!A!C!9S!'!'!-!-!'!-!'!'"
DATA "!9S!C!A!C!H!C!A!A!9!9S!9!A!9Sec!A!ASS!'!'!-e!'S!A!9g!C!A!Ae!9icS!9Sd!'!-!'S!'!A!9!A!C!C!A!A!C!9!AS!'"
DATA "mcSSe!'!9!9e!A!Cd!A!9!9qcS!9!9S!A!9!A!C!A!C!C!A!C!Aucd!9f!A!C!Cyc!9S!A!9!A!9}c!9!9pc"
DATA ""
DATA 32, 16
DATA "pcS!9}c!9dS!'Sycd!'!-d!'!-!'!'uc!'!'eKi!-qc!9!A!'!-KeVKVKVK!-!-!'Smc!C!C!A!AS!'!-!-VVOVVOVV!-K!'!'SS"
DATA "ic!Ae!C!A!9!A!'S!-!-KdVKV!-K!'S!9S!A!Aec!9!9!A!Ad!Cd!A!9!9SS!-!-KK!-K!-!-!'!'!9!9e!Acc!'S!9Sd!A!C!A"
DATA "!9!9SS!9!'S!-dK!-K!'!-S!9!A!9S!9ec!9!9e!A!9!Ad!9SS!'d!-KK!-!'!-SS!9!9ic!A!C!A!A!9!A!9SS!'!'j!-!'!-mc"
DATA "SSe!'!-!-Kj!-qc!-K!-!-K!-K!-eK!-!-uc!-!-K!-!-KVdKyc!-!'!-!'!-K}cSSpc"
DATA ""
DATA 32, 16
DATA "pc!'!'}c!'S!-!-K!-yc!'S!'!'d!-K!-!'ucd!-d!'!-!-dK!-!-!'qc!-!'!-!-!'SS!9!'!'K!-KVKK!'!'mc!'!-e!'SS!A"
DATA "!9SS!-!-KKVK!-!-S!9ic!-!-d!'SS!'!A!9!9!ASd!'!-dK!-!-S!9!A!9ece!'eS!9!9!A!A!9!9Se!'f!-SS!Ad!9cce!'S!9"
DATA "!9!A!A!9!A!C!A!9S!9S!'!'Sf!'SS!9S!9ecSS!9e!Ad!C!A!A!9!9SS!'!'Sd!'!-!'!'!-ic!C!H!H!C!C%!A!C!9!AS!9!'S"
DATA "e!'!-!'KKmce!H!C!C!A!A!9!ASSe!'!-!-qcd!H!C!H!C!C%!9!A!'S!-!'uce!H!C!C!A!9!'!'yc!C!H!A!C!9!A}cS!9pc"
DATA ""
DATA 32, 16
DATA "pc!'S}c!'S!-!'!-!-ycSd!'d!-K!-!-uc!'!'Se!'d!-K!-KKqc!-!-!'!-S!'S!9!'S!-!-K!-VKVVmcVeK!-d!'SS!'!'e!-"
DATA "dKOVicgVKK!-K!-e!'SSd!'!-KVVOVecKKVdK!-KK!-Kd!-S!'!9!9!A!9SS!'!'!-!-KK!-KccS!'!'SS!'S!'SS!'S!'!'S!9"
DATA "d!A!C!A!9!A!9!9!AS!9S!9ec!A!9e!A!9!Ad!9S!A!9g!C!A!C!Cd!Aic!A!C!A!9!9!A!9!A!9S!A!A!Ce!He!C!Amc!Af!9"
DATA "d!Ad!Ce!H!C!HqcS!9!9!A!C!A!C!H!C!H!C!H!C!Huc!9!9!C!C!H!C!Hd!Cyc!A!9!A!C!C!A}cS!9pc"
DATA ""
DATA 32, 16
DATA "pc!9!A}c!Af!Cycd!C!H!C!H!C%!A!Aucd!Cd!H!C!H!C!C!A!A!9!9qc!H!Cf!H$!H!H!C!H!A!CS!9S!'mc!A!C!Cl!H!C!CS"
DATA "!9SS!9!9ic!A!C!A!C!C!A!C%!H!C!H!Ce!H!C!C!A!9!9S!9!AS!9eck!A!C!C!H!H!C!H!C!C!A!Ag!9eScc!9S!9!A!9S!9S"
DATA "!9S!Ad!C!A!C!9!A!9S!9S!9SS!9!'S!'!'ec!'!'S!'SS!9S!A!A!Cd!AS!9!A!9!A!9!9Se!'ic!'S!9!A!A!9!Ae!C!A!A!9"
DATA "d!A!CS!9!'!-mc!A!Aj!C!A!A!C!A!ASSqc!C!A!C%!C!C!A!C!A!C!A!C!9!Auce!C!A!C!A!A!9!9yc!C!H!C!A!A!9}c!A!9"
DATA "pc"
DATA ""
DATA 32, 16
DATA "pc!'!'}c!'!'d!-!'yc!'!-!'!'!-!-!'!'!9!Auc!-!'!-d!'!-!-!'!'S!9!A!AqcSi!'!-!'!'S!'!'S!-!'mc!'SS!'Se!'"
DATA "!-!'!'!-!'d!-eK!-icSf!'Sd!'g!-VKVVKKe!-ecdSd!'Sf!'g!-eVKK!-!-!'!'SSccd!'SS!'S!9S!9S!'!'e!-KVVOOVK!-"
DATA "!'!9S!9!Aec!'!'S!'SS!A!9SS!'d!-KKOOEOVV!'!'!9!9ic!-d!'S!9!'!'!-KKdVOVOVKV!-!'mce!'!-!-dKfVdK!-qcd!'"
DATA "d!-K!-dVOKKuce!'!-!'KKVVycS!9S!'!-!-}cSSpc"
DATA ""
DATA 32, 16
DATA "pcS!9}c!9!AS!9!'SycS!9!A!9d!A!C!A!Cuc!-!-!'S!A!A!C!Cg!Hqc!-K!-!'S!9!C!A!H!Ci!HmcVK!-K!-!'!A!9!C!C!H"
DATA "!Ce!H$f!HiceK!-K!'!'!9S!A!C!A!C!C%!H$!H$!H$d!H!Cec!'!'!-dK!-!-!'!'eS!9S!A!A!H!Cf!H!C!A!C!9!9cc!-!'eK"
DATA "!-!-j!'S!9!A!A!C!A!Cd!A!9SSecm!-K!-!-K!'!'!9S!9!9Sd!9ic!'!'!-!'!-dKVKeVK!-S!'S!9SSmce!-KKVVOdVKK!'!'"
DATA "S!'qc!'d!-VVOdVKK!-Kuc!-!-KKgVyc!-!-KdV}c!'!'pc"
DATA ""
DATA 32, 16
DATA "pc!9!A}c!H!C!C!A!A!9yc!H!C!C%!C!Cd!A!9uc!H!Hi!C!A!A!9!Aqc!C!H!H!C!C%!C!A!C%!A!C!9S!9S!9Smc!A!Ag!C!A"
DATA "!C!C!A!A!9!'SS!'S!'!'SicS!'SS!A!9!C!A!C!A!A!C!A!9!9S!'Se!-!'!'!9SecSSe!'SSd!9!AgS!'!-KK!-K!'!'eSccSS"
DATA "!'!'!-K!-K!-!-d!'e!-eKV!-K!'Sd!'!-ec!-!-dVOVV!-KK!-KKVOOVKK!-!-Sd!'icOVOOVOVKKVVOOEOOVK!-K!-!'mciVOV"
DATA "dOVKVKKqcVVeKdVOVOKKuc!-d!'!-KeVyc!'S!'d!-}cS!'pc"
DATA ""
DATA 32, 16
DATA "pc!'S}cd!-!'S!'yc!-!'h!-!'uc!-!-e!'!-!-K!-!-!'SSqc!-!-!'!'Sd!'e!-!'!'!9S!A!9mc!-Ki!'d!-!'S!'SS!9!9!C"
DATA "!Aicd!-!'!'!-!-!'!'!-!'!-!'!-!-!'!'!-S!'!9!A!C!A!C!HeceSl!'d!-K!-!'!'!9S!9f!Acc!9SS!9S!'!'S!'!'S!'S"
DATA "!'!-!-KVVK!-!-d!'SS!'S!'ecSS!9SS!'S!'eS!'!-KdVKVdK!-!'!'icS!9!'S!'!'dS!9!'!'K!-dVOVOKKmcSS!'!'eS!'!'"
DATA "K!-dVOVVqc!9S!9SS!'!-!'KVVOVVuc!A!ASS!'!-!-dKycS!9Sd!'}cSSpc"
DATA ""
DATA 32, 16
DATA "pc!'!'}c!'!'!-d!'yc!-!'!'!-!-f!'ucKKg!-d!'d!-qcK!-!'!-!'d!-!'!-!'!-!-f!'mcK!-!'!'SS!'!'!-!'g!-e!'!-"
DATA "!'ic!'!'S!'!9!A!A!9!9Sd!'!-!-g!'S!'SS!'ec!'dS!9!9g!A!9SS!9!9fS!A!9f!A!9cc!'!'S!9!A!9!C!Ad!C%!C!A!A!C"
DATA "!A!C!9!A!9!A!C%d!H!C!A!Aec!9!9!C!C!H!C!Ci!Hd!C!A!C!H!H$d!Hic!H!Cd!H$!H$!H$d!H!C!C!H!Cf!Hmcj!Hd!C!H!C"
DATA "!H!C!H!Cqc!H!C!C!H!A!9!A!9!A!9!A!9!A!Cuc!A!ASS!'!'S!'SSycS!'!'!-!'!-}c!'Spc"
DATA ""
DATA 32, 16
DATA "pcS!'}c!A!CSS!'!'yc!H!C!C%!A!9S!9S!'uc!C!C!Hd!C!A!AdS!'!-!-qc!A!C!A!C!C%!C!H!A!CS!9!'S!-!'!-!-mcSd!9"
DATA "!A!9h!A!9dSe!'!-ic!'SS!9dS!'dS!9!9S!9!A!9!A!9!ASS!'!-!-!'ecg!'!-!'!-!-!'!'SSf!Ad!C!A!AS!9eScc!'!'!-"
DATA "!'!-!-K!-K!-!-!'!9S!C!A!H!Ce!H!C!He!C!A!Cece!-KK!-K!'!'!A!9d!Cd!H$h!Hicd!-K!'!'!9S!A!9!Ad!Ci!Hmc!-!'"
DATA "SS!9d!A!C!A!C!C!H!C!H!C!C!Hqc!9!A!A!9!A!A!C!Ad!C%!C!Cuc!9!A!A!9g!Ayc!9!A!9!AS!9}cSSpc"
DATA ""
DATA 32, 16
DATA "pcKV}cKdVKVyc!-!-K!-VVOENQuce!-KKOOQQ!J!JQQqcKVKVVKOVNMd!JQQNQNmcK!-KKVKVONMg!JNQNQEMicK!-K!-eKOOQN"
DATA "d!J\!J!JQNMNOEVVecVKfVdKVVEENMQQNQMEVOVVKKVVccdVOVVKK!-dKfVOVVKVK!-!-!'!-!'!-KeceVKK!-!-eK!-fK!-K!-"
DATA "!-e!'icKVdK!-eK!-dKfVK!-Kmc!-dKVfKVVeOVVqcdKVVKVOVVOVOOucVKKgVOycKVVKVO}cVKpc"
DATA ""
DATA 32, 16
DATA "pc!-K}c!'!'d!-Kyc!-!'!'d!-eKucdKi!-KK!-qcVVKe!-Kf!-Ke!-mceOVdK!-Ke!-KK!-K!'!-!'!'icOOEME4OVVKVeK!-K"
DATA "!-!-K!-!'!'d!-ecVVeOdVhK!-Kj!-KK!-ccKVK!-!-K!-!-f!'!-!'!-!'!-e!'d!-fKecd!-!'S!'!'SS!'S!'!'SSe!'S!'!'"
DATA "d!-Kic!-!-h!'SS!'!'SS!'S!9S!9!'!'mcKd!-!'!'SS!9S!'dS!9!9SSqcKK!-!-i!'S!'ucKKg!-!'!-ycK!-KK!-K}cVKpc"
DATA ""
DATA 32, 16
DATA "pcKV}cVKVVKVycf!-K!-!-!'!'uck!'SS!9!9qcd!'!-S!'S!9S!9S!9!9S!9S!9Smc!'d!-!'!'SSk!9S!9SSicd!-!'!'!-d!'"
DATA "SSe!9!A!9!A!9!A!9S!'S!-KecKKi!-!'!-!'Sg!9SS!'!'e!-KKccK!-K!-K!-Kf!-!'!'!9S!9S!'!'d!-Kg!-ecKK!-K!-K!'"
DATA "!-!'!-!'!-e!'d!-K!-Ke!-icKVKK!-!'!'e!-Kj!-!'mcKVd!-!'!'l!-qcKd!-e!'!-!'!-!'!-!-uc!-!-f!'d!-yce!'!-K"
DATA "}cK!-pc"
DATA ""
DATA 32, 16
DATA "pcK!-}cK!-KKVVyc!-KK!-KKVVOOuc!'!'K!-eKdVdOqcd!-!'K!-KKVKVOdVdKmcg!-K!-KKeVdKd!-!'!-icKVKVKf!-K!-gK"
DATA "!-K!'!-!'!'!-!'ecKKeOKK!-!-K!-eK!-!-Kd!-!'!-!'!'!-!'KKccOVEOOVKKd!-KVKK!-!'!-!'e!-d!'!-KVKecOOVVd!-"
DATA "!'d!-dKe!-Ke!-!'!-!-icVKd!-!'d!-KK!-fK!-!-!'!'!-mcKK!-!'!-d!'!-!-KKVeK!-qc!-!-!'!'S!'!'S!-KK!-K!-uc"
DATA "!'!-Sd!'e!-yc!'d!-K!-}c!-Kpc"
DATA ""
DATA 32, 16
DATA "pcVK}cgVycMEEMEOVOKVucQQ!JQMMEEOVKK!-!-qcMNe!JNMOOVV!-!-!'!'!-!-mcEONQe!JNQOOKV!-!-S!'!-!-KKicKVOOQN"
DATA "e!JQNOEKV!-!-!'!'!-KeVecK!-KKOOQQ!J!J\d!JMEVV!-K!-!-K!-KKVdKccdVdONQ\!J\Y\!JQNOEVVKKe!-dK!-ecEOEOMN"
DATA "!J!Je\!J!JMNEOKVf!-KicEONQ!JQ!J\\!J\!JQ!JEOVV!-K!'!-mceQ!J!J\!J!J\!J!JNMOOKKqcQNQe!J\!J!JQNE4uc!J!J"
DATA "eQNQEEycQNMEOO}cVVpc"
DATA ""
DATA 32, 16
DATA "pcKK}c!-fKyc!-!'K!-KVKVKKuc!'!-!'!-gKVdKqcVVK!-KKVVKVdKVVKVKmcOOVVKeVOgVKfVicVVKVVKeVOOVOdVKK!-KdVKK"
DATA "ecKKe!-VVgOVKg!-K!-gKccd!'!-!-KVVOOVOVVK!-!'SS!9S!'d!-K!-KK!-ec!-!'KKdVOdVK!-!-!'S!9dS!'!-Kd!-icK!-K"
DATA "dVOVKV!-!-d!'S!'!'!-!-KVmcKKeVK!-i!'K!-qcKK!-K!'!-f!'SS!'ucf!-!'!-!'S!'yc!-Kd!-!'}cK!-pc"
DATA ""
DATA 32, 16
DATA "pcK!-}c!-dKVKycdVOdVOVVucOVMeEdOEdOqc!-KOVNMQNNQNMNMOEVKmcSS!'!-KVOOQNdQ!JQQ4EVVKKic!'!'S!9!'S!-!-VK"
DATA "OOMNQNNMOEVK!-K!'!-ec!-!-!'!-!'Se!'!-!-KKOOEMEEOOVVK!-!'!'!-!-ccVK!-K!'!-!'!'d!-KK!-VVEOEMOEVOKK!-!-"
DATA "KKecK!-!-d!'f!-KKVEOEE4dOKK!-!-ice!'d!-KKVVeOEfOVmcKKeVdOEOEeOEOqcO4OEE4E4MEEMOOucdOdEMNMMycOVOVEO}c"
DATA "KKpc"
DATA ""
DATA 32, 16
DATA "pcVV}cVKdOEycVKVVOOEOOVucdOdVOOEEOEVOqceOVKK!-VVE4MNEMVVmcOOeVKK!-!-KKOOMENMOOK!-icOEdVdKe!-KVVOEOME"
DATA "OEKK!-!'eceVfK!-!'d!-KdVOOEEOOKKe!-cceVK!-!-K!-e!'!-K!-KKVKfVeK!-ecKK!-!-!'!'S!'!'S!'!'!-!-KKVVKdVOV"
DATA "VOicSd!9S!9!'!'!-!-KKVKdVdOEMmc!9!A!9!9SS!'!'!-!-KKeVEOqc!9!A!9!A!9!AS!9!-!-VKOVuc!9!A!9!AS!9!-!-VV"
DATA "ycS!9!'!'!-!-}c!-Kpc"
DATA ""
DATA 32, 16
DATA "pcVK}cVKVOEOycKdVOEEdOucVVKdVfOEMMqcOOVOeKVKOOEMNMQNmcEMEMEOVVdKVVdO4EMMQQicQNQNNQMEOEOeVKKdVEOQN!JQ"
DATA "ecEENdMgEOOKVe!-KKVVOOMdEccKVVKKVKVKKVKVVKK!'!-!'!'f!-KK!-K!-ec!-!-f!'e!-K!-!-!'S!9dSg!'ic!'!'!-!'"
DATA "d!-K!-K!'!-!9S!9!A!9!AS!9S!'mce!-K!-!'!'S!'!9!9!Af!9qcKd!-S!'!9S!9S!9S!9Suc!-!-!'S!9!9S!9SSyc!-d!'S"
DATA "!'}c!-Kpc"
DATA ""
DATA 32, 16
DATA "pc!-!-}cS!'eSycS!'!9S!9SS!9!'!'uci!9SS!-!'!-!-qc!9!A!A!9!A!9!A!9!A!9!9S!'!-K!-KKmc!9!'!9!9!A!9!A!9!9"
DATA "e!A!9SSK!-dK!-icf!'!9Se!9!A!A!9!A!9S!9d!-K!-KK!-ecg!'!-d!'SSe!9SS!'!'!-!-K!-K!-eKcc!-K!-K!-K!-K!-K!'"
DATA "!'S!9!'!'d!-K!-K!-dKVKVVecVVKVKK!-Ke!'!-!-Kf!-KKeVicVK!-!-!'!-!'!'S!'!'S!'!-!'!-!'!'K!-OVmc!'!'gS!'S"
DATA "!'!'Sd!'KKqc!'SS!'Sh!'!-!-uc!9S!9S!'!'!-!'K!-yc!9SS!'!-!'}c!-!-pc"
DATA ""
DATA 32, 16
DATA "pcVV}cVVdOVycVOVVOOVV!-KucVOOdVOOVVKd!-qcKKdVOdVOVVKVVKOOmcVKKVKeVOVVOVOOMEMEOEicKVVOVVKdVfOENMQNEMO"
DATA "EOEecdKdVKfVeOEENdQEMOOVVKKccdVKKVgKOVOOE4QN!JQNQOV!-K!-!-eceVKK!-!-KKVOOEMEQd!JQQOV!-!-icOdVKKVVE4M"
DATA "NNQ!JQ!JQMEOVmceVOOEENNeQMMOEqcVVOVOOEMNQQNMEuceVOVEMQQycK!-KKOO}cKKpc"
DATA ""
DATA 32, 16
DATA "pcKK}c!'!-K!-VKycK!-!-!'!'!-e!'ucOOVK!-!-SSe!9!A!9qcOEOVK!-!'S!9S!9!A!A!9!A!9!A!9mcMNEEOV!-!-SS!9!9"
DATA "!A!9h!A!9icEMMEOEVVK!-e!'S!9!A!9!C!A!A!C!A!9!9!AecVVEEMMOOVVeK!-K!-!-S!9!A!A!9!A!9!9S!'!-!-ccOVEMMEO"
DATA "OjVK!-Kf!'d!-KKeckOEOEE4EVV!-KK!-K!-K!-icVVOOEOMENMNQQNE4KVK!-KKmceOEMgQMEVVKVqcVOOENQ!JQQNMEEOucOOM"
DATA "EfQNycOEMEQN}cVVpc"
DATA ""
DATA 32, 16
DATA "pc!-!-}cS!9!'!'!-!-yc!9!AS!9S!'!'!-!-!'uc!9!9S!9dS!9!'S!'d!-qc!9S!9!9S!9S!'S!9!'!'!-K!-K!-Kmc!-!'!'S"
DATA "!9eSd!'!-!-VKKVKVVKicKVKK!-!'!'!9Sd!'!-!'!-KVVeOVV!-KecKKeVKKe!-gKOOMdEVVeKccKKVVEOEdOdVeO4MEMNO4eVO"
DATA "VecOOQN!JQQNEEOEMM!JdQEMOOKVOVicQ!J!JQQ!JNMMEQ!J!J\!JQNME4OVmcfQNQNQQd!JQNNMEqcQNEMEMNQQ!JQ!JMEucOVO"
DATA "VEEQNQNycVKOVOO}cKVpc"
DATA ""
DATA 32, 16
DATA "pcVK}cEOVOKVycOVOEOEdOVucOOeVEOEEVOKKqcOOVVKdVOEOOVVK!-!'!-mc4EOhVdOVKVKK!-!-!'!'icOEOVVOOVVOVOVOOVV"
DATA "OKV!-K!'S!9SeceKeVOgVOOEOEVV!-K!-!-e!'cc!-KK!-KKVKVVKVKKOONMNMOEeVdKVecKK!-KKdVeKOOMNQQMNME4OVVicKKV"
DATA "KVVdK!-VVE4NQQ!JQQMEmcKKVVeKOVEEQN!JQNQqc!-K!-KKVOOMEQNNQuc!-!-KKOOMEEMycK!-KdV}cKKpc"
DATA ""
DATA 32, 16
DATA "pcVV}cdVOVVycOVVdOeVucEEgOdVdOqcE4OVOVOOVOVOOVVOVVmcOEVVKKdVhOeVOVicVOKV!-K!-!-K!-VVOVOVVOdVKVdKecV"
DATA "dKd!-e!'!-KK!-hK!-!-e!'!-!-ccVVKK!-!'!'SS!9S!9!'Se!'e!-S!9!A!9!9!A!'!-ec!-!-!'Se!9!A!A!9!A!9!9S!9S!'"
DATA "S!'!A!9!A!A!9!Aic!9!A!A!9!A!9!A!C!A!C!A!9!9!A!9S!9S!A!9!A!9mc!9f!A!9!9Sd!9Sf!9qc!9!AS!9!'!-!-!'!-!'"
DATA "!'!-!'!'uc!'!'KKVVKVKKycKVVOOV}cVKpc"
DATA ""
DATA 32, 16
DATA "pcKK}c!9!'KKVVyc!9!AS!9!-!'dKVuc!9!9S!9S!9!'!'dKVOOqc!'!-!'!'S!9!9S!'!'K!-VKOVOOmcKf!-e!'!-!'K!-dKeV"
DATA "OicVVjK!-!-Kf!-dKVdOecgVOVdOVKK!'!-!'!'S!'SS!'!'!-fKccVVOVOOE4EMOV!-K!'S!9S!A!9!9!A!9SS!9Sd!'ecOOEOE"
DATA "M4EVV!-!-!'S!9!9e!A!9!A!9!A!9!9icOOEOVO!-K!-d!'S!9!9!A!A!9!A!9!A!9mcVOKK!-!-e!'dSf!9qc!-K!-!-!'!-!'S"
DATA "S!9S!9S!9uce!-!'!'!-!'!'!-yc!-K!-KK!-}cKKpc"
DATA ""
DATA 32, 16
DATA "pcME}ceMNMycOVEOMQ!J!J\\uceOEE!J!Jd\Y\\qcMEMEMEQ!J\!JY\Ye\Ymc4O4EEMQQ!J\eY\Ye\!J\icEOE4E4E4!JQ\gY\\Y"
DATA "\d!JMQecMMQdMEMEEMQ!J!Jh\!JQQMMEdMccMQQNQNE4O4E4MQQMdQMMEO4VOVOO4ecMQMQEEOOEE4EOE4EEM4EOOeVicMEE4O4E"
DATA "4E4O4E4MQQMMEO4mcOEMMEMEE4EMM!JQ!J!JMQqcE4MEMNQNQMQ!J!JQuceMQQMdQycMEMNQN}cEMpc"
DATA ""
DATA 32, 16
DATA "pcO4}cVVOO4EycVOVdOE4E4ucEEiO4dOqcQNEiOVgOmc!J!JQQEMEEOEeOEE4OeVic!JQ!J\!J\Q!JMNMNMEEOEOO4VOVVOOecQQ"
DATA "!JQQ!JMQMMEMEE4EgOVdO4dOccMEE4eOjVOeVOVO4E4E4ecOOVVgKVdKVKVdKdVdOicOOdVKVKKVdKVdK!-K!-VKmceOVVKK!-fK"
DATA "!-!-KKqcMEOOVKfVdKuc4EhOVycE4E4OO}cEMpc"
DATA ""
DATA 32, 16
DATA "pcME}cEfMycOVgOVKucgVKdVKK!-!-qceVdK!-fK!-!-K!-KmcVOOdVdKd!-Kf!-K!-!-KicdOfVKVK!-!-Kh!-KKVOOecEEeOVO"
DATA "VOOVKKg!-KKeVOOEEccE4EOEOEeOVVK!-K!-KVKdO4dOVOOecEMOEOOVOOVOfVdOEOOVdOicMEE4VOVVdO4dOVdOVVOmcMMOOeV"
DATA "iOVOqcEOOVVKdVOVdOucOOKVVKOVOOycVKVVO4}cOOpc"
DATA ""
DATA 32, 16
DATA "pcE4}c4OEEQQycO4E4E4NMQ!JucVVdOE4EQMQQ!J!JqcOVVOO4E4MNdQMMNE4mceOVdOEMQMMQMMdOVOVicMEMEEOOVOOE4MEMEE"
DATA "4OOfVOecEM!JdQEMeOMM4EhOVKKOVEEccQQ!J!JQ!JE4OVOOMEE4dVeOdVOEMNec!J!JMQOOVVOO4EOEgOVOVVOOicMNOOVfOE4M"
DATA "EE4O4OdVmc4EhVOeMEEOOqcOdVdKVOOEOEOucdVKdVdOycVVOOEO}cOEpc"
DATA ""
DATA 32, 16
DATA "pcME}cMdQMQyc\!J!J\!J!JQQMEuc\\Ye\!JQQ4EOOqc\!JY\Y\\!J!JQMQOOVVOOmc!J!J\\eY\\!J!JMMOOVKVOEMicME!JQ\\"
DATA "Y\Y\\Y!J!JMEOdVO4MQMQecOO4EQQ\\gY\!JMM4fOEdMEEccMQQN!JQ\hY\Y!J!JQNE4eOE4EOece!J\\iY\\!J!JMMOOVOOEic"
DATA "!Jd\Y\gY\Y!J!JQMO4VVmcf\hY\\!J!JEMqcd\YY\Y\Y\\Y!J\uc\YYg\!Jyc\Y\!J!JQ}cMQpc"
DATA ""
DATA 32, 16
DATA "pcE4}c4E4E4EycOVEOMEMEE4ucdVOdEfMEEqcQMEOE4MNMEMEMEMNMNmcQ!JMQMMgQMQeMeQicQMMEMNQMQMQ!JdQdME4MEMQE4"
DATA "ecEEeOMQd!JdQMMOOVOVVOOMM4dEcceVO4QM!JdQMQO4KVK!-KKOVO4dO4ecVV4EQNQQMQEMOOKK!-dKOVeOicE4MEQMQ!JMEOdV"
DATA "KKVKOOMEmc4EMMNMOOeVKdVOOqcE4OOgVKVKKucOOVOeVKKycO4OVOV}cOOpc"
DATA ""
DATA 32, 16
DATA "pcE4}cOeEMycMQQNQMdQMucQQ\!J\h!JQQqcOOQ!J\!J\Y\!J\!J\d!JMNmcKKVVMM!J!Je\Ye\!JMM4EicVKK!-KKOOME!JQ\!J"
DATA "\Y\d!JMNO4VVecdOVdKdVOOMMd!J\\!JQ!JMQOOVVOOccMNO4eVOVOOEOMQ!J\!J\!J!JQNE4OVE4ecOOfVeOEMMg!JQQEMOOicV"
DATA "KVVeOMEQQ!JQd!JQ!J!JQ!Jmc4EMMQMQl!Jqcd!JQ!J\!J\\!J!J\!JQucg!Je\ycQ!JQ!J!J\}cEMpc"
DATA ""
DATA 32, 16
DATA "pcMQ}cEMQd!JycMNMQd!J\Q!Juc!J!JQQMMQf!JQQqcQ!J!JQMNEOQM!J\\!J!J\MQmc!JdQMQEEOOEMQ!J\!J!J\!JQ4Oic!J!J"
DATA "QMMNE4OVOOMEQN!J!J\d!JE4VOeceQMM4EdOVOOMMQQ!JQ!J!JQ!JMMeOccQMMQE4OOVOVKVOEOMEMNQMMQMEE4E4ecEEVOdVdKV"
DATA "VOO4EMMNdMeQicK!-!-KK!-VKOVE4MNQMQNQ!J!J\mc!'d!-KKdVOEEMdQ!J!Jqcg!-KKOOMNQ!Juc!'!-!'!-!-KOOMMycK!-VK"
DATA "OO}c4Opc"
DATA ""
DATA 32, 16
DATA "pcMN}cEMQQ!J!JycMEQMd!J\!JQucgM!JQe!J\\qc!JdQMEE4MN!JQ!J\\!J\Ymc!J\!J\!J!JMQEdMQQ!J!J\!J\\Y\ic\Y\Yd\"
DATA "d!JQ!JNQMEMEQM!J!J\Y\Yec!J!Je\i!JMMeOEEQM!J!J\d!JccMEMNMEMEE4MNQME4eVOVOVOOE4E4ecOOeVOVOO4EOOKK!-dKV"
DATA "KKdVicdVhOVV!-Ke!-K!-KKmcgOVVKKi!-qcEdOKK!-K!-K!-K!-KucOOKK!-!-K!-KKycVOVdK}c4Opc"
DATA ""
DATA 32, 16
DATA "pcOO}cVdK!-KycKK!-K!-KK!-VVucKh!-KKVVOOqcd!-!'!'!-!'!'!-!'!-KVVEOMEmcKK!-!-!'e!-d!'!-!-KK4O4EOOicVKV"
DATA "VdK!-!-K!-!-!'!-!-!'K!-OVO4OOE4eckVKKe!-KKVVgOEMEEccO4dO4O4OEVVK!-VKdO4O4O4E4MNMQecQdMEMOEVVKVVO4fOE"
DATA "EMdQicMNOOdVeKhVE4Q!JmcVVjKVVKVV4EqcKVeKVKeVOOuc!-K!-KVKVVOOyc!-dKVO}cOOpc"
DATA ""
DATA 32, 16
DATA "pcMQ}cMQQ!JQQycdQM!JQQNOOucgQ!JdQ4OVOqcMEQNQNQMQNQMMEMNQ!JmcEfMQQ!JfQ!J!J\!J\d!JicMEQNMQMEQM!JQ!JQ!J"
DATA "!Jd\Y!J\e!JecEEjMQQf!Je\!J\!J!JQMEEccMQMNMEE4E4MEQQ!JQ!J\\YY\\!JQ!JO4OVecMNNMEEOOEEQQ!J!J\!J\dY\\QQO"
DATA "OicQ!JMQE4MQ!J\\!J\!JY\\Y\!JQ!JmcQQMM!JQ\!Ji\!J!JqcQMQ!J!JQ!Je\Y\!JucdQMQQ!Jd\ycE4ME!JQ}c4Epc"
DATA ""
DATA 32, 16
DATA "pcE4}cOVOOEMycE4OgVKucQ!JMMVVKKg!-qc!J!JQ!JE4KV!-K!-!'!-!'!-!'!-!'mc\\!J!JQQOOdKd!-!'!'!9d!'!-!-ic!J"
DATA "\\d!JNMO4dVKK!-!'!'!9!'!'!9!-!'!-!-ecQQ!J!J\\!J!JQMEMEE4EOOK!-d!'d!-VKOOccQ!J!J\\!J!JQQMMQQMQMMNOOgV"
DATA "OVE4ec!JQe!JQh!J\!JQQOEOO4dOicQNQ!J!J\\!J\!Jd\Y!J\MEEOE4mcQe!Jd\Ye\!JQQMMqcQQ!JQ\\Y\\Y\!J!J\uc!J!J\"
DATA "!Jg\yc!J!J\!J\Y}cQQpc"
DATA ""
DATA 32, 16
DATA "pcOO}cK!-KVOOyc!-!-K!-KKdVOucd!-dK!-dKOVOOqc!-K!-gK!-VKO4O4O4mcVVKK!-dKVKVVOOgMEMicMEE4VOeKVVOVO4MN"
DATA "!JQ!J!JQMO4ecEEQMQQEMOOVOEE4EMM!JQ\d!JQQ4EMEccE4QMd!J\!JQQMQQ!JQ!J!J\!J\d!JMNMdQec!J!J\\Yd\e!J\\Yd\"
DATA "!J\!JQNMQQic\YY\\Y\!J\!J\eY\\!J!J\Q!Jmci\Y\Y\Ye\!Jqc\\!J\\!J\!J\Y\Y\!JuceQ!J!Je\ycMNQ!J!JQ}cMMpc"
DATA ""
DATA 32, 16
DATA "pcME}c!J!JQQMMycQg!JQQ!Juc!J!JeQe!JQQEEqc!JQQNMEMQ!JQ!J!JMQO4VVmc\!JQQMQQMMQ!JdQMM4EOOKVic!J!JQ!JdQ"
DATA "!JdQNdQ!JQQMEOOKV!-KecMMEdMeQMQMMQQe!JMQOEOOeVccO4EOE4MNMQMEME!JQ\!J\d!JQMMNMEMEec4E4EMMNMEE4EQQh\"
DATA "d!JQQicE4MNQME4EOMQ!Je\Y\Y\!JmcMEMMEMEMQQ!J!Jg\qcdO4MEQ!J\!J\Y\\ucOV4EQQ\!J!J\ycEOMEQM}c4Epc"
DATA ""
DATA 32, 16
DATA "pcMQ}cMdQMQycQ!JQNQ!JQNQQuc!J!JQf!JeQ!JQqc!J\Q!JQ!J!JgQ!JQNNQmc!J!JMM4EMMeQd!JQQMQMQQicQQMEdOVO4MeQ"
DATA "!JQQMQMNMNMEecMM4EOOeVOVEEOEOEEdMOOeVOOccQME4VOKVdK!-KVVKVKeOK!-!-!'!-!-VVecOOdKd!-!'!'!-!'!-!-K!-KK"
DATA "VK!-!-!9!'!'!-ic!-!-!'!-d!'!9d!'e!-K!-K!-!'!'!-mc!-f!'!-!-Kd!-Kd!-K!-qc!-!-K!-dVOOfVucVV4EMMNMEMycME"
DATA "dQ!J}cMMpc"
DATA ""
DATA 32, 16
DATA "pcE4}cKKEEMQyc!-!-K!-OVE4MEucK!-K!-!-KVVEEMMQQqcdVKK!-K!-VKEOMEQ!J!JQmc4dOVOgVOOEEMMeQicMNE4E4MEE4E4"
DATA "iOE4QNQ!JecQQMQMMgQEEOeVdKVVOOEdMccNMQ!J!JQ!J\!J\Q!JO4KV!-K!-!'!-!'!-fKVKecQe!J\!J!JQMOOKK!-!-e!'!-"
DATA "!'!'d!-ic!JQ!J!JQNO4VOVVK!-!-!'!'!-!-!'!-!'mcQQEEVOVVKVdKf!-qcdOdVKVdK!-K!-ucOOVOeVOVyceOE4}c4Epc"
DATA ""
DATA 32, 16
DATA "~c~c~cucIcIc5ycI&II&&_d&Ic5uc66&I&&6I&Id5cIqcI&&Id5I!(_!(II&I&&c5mcIcI65&I5I&6I&6I&55_oc&Id&I!(I&&I&"
DATA "I&5I_I&oc6&6&5I55II&I_55c_ocI5_&!(&56I5II5&Isc5II6dI&&c&wcdI&&IIcI~c~c~cwc"
DATA ""
DATA 32, 16
DATA "~c~c~c~c~c~c~c~c~cqc0yc>gc!H~c|c6~ccc!H~c~c~c{c"
DATA ""
DATA 32, 16
DATA "tcBwcBic+wc+ic99Buc99hc,c+uc,ic,c99tc,gcBc,c,uc,gc+c$$,uc$$fc99cc,|c,dc$${c,~ccc,~ccc$$~c~c~c~czc"
DATA ""
DATA 32, 16
DATA "~c~cxcB~cccBB~cc++wcBfcBc,99vc+fc+c,,cBuc99ec,9$$c+uc,fc,Bc,c,9tc,fc$$c$$,uc$$fc,9cc$${c,~ccc$$~c~c"
DATA "~cxc"
DATA ""
DATA 32, 16
DATA "pc+{cBec99zc+BccB,kcBpc9+cc+,Bjc+pc,99c99+Bic99oc,,cc,$9+ic,pc,,$c,c,99hc,pc$,ccBc,,ic,qc$$c+$$,ic$$"
DATA "sc,9,,}c,cd$|c$$~c~c~c~c}c"
DATA ""
DATA 32, 16
DATA "~c~cuc__JJ__vcg_dJ__vc_JJ!KgJg_qc_J!O!N!N!M!L!LdJ!R!QJJ_lcd_eJ!K!R!Q!P!O!N!M!L!K!K!R!RJ_mc_Jd!P!O!O"
DATA "!N!L!K!R!P!O!N!M!L!L!KJd_kc_Jd!R!Q!Q!P!O!N!M!K!R!Q!O!N!NfJ_icd_fJ!R!Q!P!O!N!M!L!K!RJg_mcd_JJ!R!R!Q!Q"
DATA "!P!O!O!NJ_tc__J!L!K!KgJ_tc__eJh_ucg_~c~cvc"
DATA ""
DATA 32, 16
DATA "~cqc__eJ{c_Jd!M!L!K!Rucf_JJ!P!P!O!O!N!L!Ksc_J!K!Kf!R!Q!Q!P!O!N!M!Kpc__J!O!N!N!M!L!L!K!K!R!R!Q!P!O!N"
DATA "!M!Ljc__fJ!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nic_Jd!P!O!O!N!L!K!R!P!O!N!M!L!Le!KJJd_fc__Jd!R!Q!Q!P!O"
DATA "!N!M!K!R!Q!O!N!Ne!MJJ_hcd_fJ!R!Q!P!O!N!M!L!K!RgJ__lcd_JJ!R!R!Q!Q!P!O!O!NJ__sc__J!L!K!KJJ!K!RJJ_tc__"
DATA "eJh_ucg_~c~cvc"
DATA ""
DATA 32, 16
DATA "~c~cuc__JJ__vcg_eJ_vc_JJ!KgJg_qc_J!O!N!N!M!L!L!KJJ!R!Q!PJJ_kcd_eJ!K!R!Q!P!O!N!M!L!K!K!R!R!QJ_lc_Jd!P"
DATA "!O!O!N!L!K!R!P!O!N!M!L!L!K!KJg_fc__Jd!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Md!NJ_fcd_dJ!K!R!R!Q!P!O!N!M!L!K!R"
DATA "!Qf!P!Q!Qicg_J!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Lpc_J!L!Lf!K!R!R!Q!Q!P!Prc_JJ!Ne!Mf!Nuc__fJ!P!Q!Q|c__J~c"
DATA "qc"
DATA ""
DATA 32, 16
DATA "~cqc_fJ{c_Jd!M!L!K!Rucf_JJ!P!P!O!O!N!L!Ksc_J!K!Kf!R!Q!Q!P!O!N!M!Kpc__J!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M"
DATA "!Ljc__fJ!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nic_Jd!P!O!O!N!L!K!R!P!O!N!M!L!Lf!K!R!R!Q!Qfc__Jd!R!Q!Q!P"
DATA "!O!N!M!K!R!Q!O!N!Ne!Mf!Nfc__eJ!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qicg_J!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Lpc_J"
DATA "!L!Lf!K!R!R!Q!Q!P!Prc_JJ!Ne!Mf!Nuc__fJ!P!Q!Q|c__J~cqc"
DATA ""
DATA 32, 16
DATA "~c~cuc__JJ__vcg_eJ_vc_JJ!K!RfJg_pc__J!O!N!N!M!L!L!KJJ!R!QJJ__kc_gJ!K!R!Q!P!O!N!M!L!K!K!R!RJ_lc_Je!P"
DATA "!O!O!N!L!K!R!P!O!N!M!L!L!KJd_gce_Je!R!Q!Q!P!O!N!M!K!R!Q!O!N!N!MeJ_hc!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L"
DATA "!K!RgJ_jc!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!NJ__pc!R!P!O!N!M!L!Lf!K!R!RJ__rc!R!Q!O!N!N!M!MJg_uc!K!R!Q!P"
DATA "!PJ_|cJ!LJJ_~c__pc"
DATA ""
DATA 32, 16
DATA "~cqc_fJ{c_Jd!M!L!K!Rucf_JJ!P!P!O!O!N!L!Krc_JJ!K!Kf!R!Q!Q!P!O!N!M!Kmc_cc__J!O!N!N!M!L!L!K!K!R!R!Q!P!O"
DATA "!N!M!Lic__fJ!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nhc_Je!P!O!O!N!L!K!R!P!O!N!M!L!Lf!KJd_ccJ__JJe!R!Q"
DATA "!Q!P!O!N!M!K!R!Q!O!N!Ne!M!NJ_gc!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!QgJ_ic!Q!P!O!N!M!L!K!K!R!R!Q!Q!P"
DATA "!O!O!NJ__pc!R!P!O!N!M!L!Lf!K!R!RJJ_rc!R!Q!O!N!N!M!MJJf_uc!K!R!Q!P!PJ_|cJ!LJJ_~c__pc"
DATA ""
DATA 32, 16
DATA "~c~cuc__JJ__vcg_eJ_vc_J!K!K!R!RJ!RJJg_pc__J!O!N!N!M!L!L!K!K!R!R!Q!PJJ_jc__gJ!K!R!Q!P!O!N!M!L!K!K!R!R"
DATA "!QJ_kc_Je!P!O!O!N!L!K!R!P!O!N!M!L!L!K!KJJf_ccJ__JJe!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Md!NJJec!O!N!N!M!L!L"
DATA "!K!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qic!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Lmc!R!P!O!N!M!L!Lf!K!R"
DATA "!R!Q!Q!P!Pqc!R!Q!O!N!Ne!Mf!Nuc!K!R!Qf!P!Q!Qyc!M!L!L!K!K!L}c!P!Ppc"
DATA ""
DATA 32, 16
DATA "~cqc_fJ{c_Jd!M!L!K!Rucf_Jd!P!O!O!N!L!Krc_J!L!K!Kf!R!Q!Q!P!O!N!M!Kmcf_!P!O!N!N!M!L!L!K!K!R!R!Q!P!O!N"
DATA "!M!Lic_gJ!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Ngc__Je!P!O!O!N!L!K!R!P!O!N!M!L!Lf!K!R!R!Q!QccJJ_JJe!R"
DATA "!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Mf!Nec!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qic!Q!P!O!N!M!L!K!K!R!R"
DATA "!Q!Q!P!O!O!N!M!L!L!K!K!Lmc!R!P!O!N!M!L!Lf!K!R!R!Q!Q!P!Pqc!R!Q!O!N!Ne!Mf!Nuc!K!R!Qf!P!Q!Qyc!M!L!L!K!K"
DATA "!L}c!P!Ppc"
DATA ""
DATA 32, 16
DATA "~cqcJf_yc!L!Le!MJJ_vcf!Oe!PJ_tc!M!M!L!L!K!Kf!RJg_mc!Q!R!R!Q!Q!P!O!N!N!M!L!L!K!K!R!R!Q!PJ__jcJJf!M!L"
DATA "!K!R!Q!P!O!N!M!L!K!K!R!RJ__ic__J!Oe!P!O!O!N!L!K!R!P!O!N!M!L!L!KJJ__ic_Jf!R!Q!Q!P!O!N!M!K!R!Q!O!N!N!M"
DATA "eJ_hc__gJ!R!R!Q!P!O!N!M!L!K!RJg_jcg_JJ!R!R!Q!Q!P!O!O!NJ_tc_JJ!L!K!KgJ_tc__eJh_ucg_~c~cvc"
DATA ""
DATA 32, 16
DATA "pc!M!M}c!Q!R!R!Q!Q!Pyc!L!Lf!M!L!K!Rucf!Oe!P!O!O!N!L!Kqc!M!M!L!L!K!Kf!R!Q!Q!P!O!N!M!Kmc!Q!R!R!Q!Q!P!O"
DATA "!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!LicJ!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nfc__J!Oe!P!O!O!N!L!K!R!P"
DATA "!O!N!M!L!Lf!KJd_ec_Jf!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!M!NJ_gc__JJ!L!L!K!K!R!R!Q!P!O!N!M!L!K!RgJ__icg_J!K"
DATA "!R!R!Q!Q!P!O!O!NJ__sc_JJ!Lf!K!R!RJ_tc_fJh_ucg_~c~cvc"
DATA ""
DATA 32, 16
DATA "~cqcJf_yc!L!Lf!MJ_vcf!Oe!PJ_tc!M!M!L!L!K!Kf!RJJf_mc!Q!R!R!Q!Q!P!O!N!N!M!L!L!K!K!R!R!Q!P!OJ_jcJJf!M!L"
DATA "!K!R!Q!P!O!N!M!L!K!K!R!R!QJ_ic__J!Oe!P!O!O!N!L!K!R!P!O!N!M!L!L!K!KJJf_ec_Jf!R!Q!Q!P!O!N!M!K!R!Q!O!N"
DATA "!Ne!Md!NJJec_dJ!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qicf_JJ!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Lpc_J!L!Lf!K"
DATA "!R!R!Q!Q!P!Prc_J!N!Ne!Mf!Nuc__fJ!P!Q!Q|c__J~cqc"
DATA ""
DATA 32, 16
DATA "pc!M!M}c!Q!R!R!Q!Q!Pyc!L!Lf!M!L!K!Rucf!Oe!P!O!O!N!L!Kqc!M!M!L!L!K!Kf!R!Q!Q!P!O!N!M!Kmc!Q!R!R!Q!Q!P!O"
DATA "!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!LicJ!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nfc__J!Oe!P!O!O!N!L!K!R!P"
DATA "!O!N!M!L!Lf!K!R!R!Q!Qec_Jf!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Mf!Nec_JJ!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q"
DATA "!Qicf_JJ!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Loc__J!L!Lf!K!R!R!Q!Q!P!Prc_J!N!Ne!Mf!Nuc_gJ!P!Q!Q|c__J~cqc"
DATA ""
DATA 32, 16
DATA "~cqcJf_yc!L!Lf!MJ_vcf!Oe!PJ_tc!M!M!L!L!K!Kf!RJg_mc!Q!R!R!Q!Q!P!O!N!N!M!L!L!K!K!R!R!Q!PJ__jc!L!Lf!M!L"
DATA "!K!R!Q!P!O!N!M!L!K!K!R!RJ__hcf!Oe!P!O!O!N!L!K!R!P!O!N!M!L!L!KJJ__gc!L!L!K!Kf!R!Q!Q!P!O!N!M!K!R!Q!O!N"
DATA "!N!M!MdJ_hc!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!RgJ_jc!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!NJ__pc!R!P!O!N!M"
DATA "!L!Lf!K!R!RJ__rc!R!Q!O!N!N!M!MJJf_uc!K!R!Q!P!PJ_|c!M!LJJ_~c__pc"
DATA ""
DATA 32, 16
DATA "pc!M!M}c!Q!R!R!Q!Q!Pyc!L!Lf!M!L!K!Rucf!Oe!P!O!O!N!L!Kqc!M!M!L!L!K!Kf!R!Q!Q!P!O!N!M!Kmc!Q!R!R!Q!Q!P!O"
DATA "!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!Lic!L!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Necf!Oe!P!O!O!N!L!K!R!P"
DATA "!O!N!M!L!Lf!KJd_cc!L!L!K!Kf!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!M!NJ_gc!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Q"
DATA "gJ_ic!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N!MJ_pc!R!P!O!N!M!L!Lf!K!R!R!QJ_rc!R!Q!O!N!N!M!MJJf_uc!K!R!Q!P!P"
DATA "J__{c!M!L!LJ__}c__pc"
DATA ""
DATA 32, 16
DATA "~cqcJf_yc!L!Lf!MJ_vcf!Oe!PJ__sc!M!M!L!L!K!Kf!RJJf_mc!Q!R!R!Q!Q!P!O!N!N!M!L!L!K!K!R!R!Q!P!OJ_jc!L!L"
DATA "f!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!QJ_hcf!Oe!P!O!O!N!L!K!R!P!O!N!M!L!L!K!KgJ_cc!L!L!K!Kf!R!Q!Q!P!O!N!M!K"
DATA "!R!Q!O!N!Ne!Me!NJec!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qic!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N!M"
DATA "!L!L!K!K!Lmc!R!P!O!N!M!L!Lf!K!R!R!Q!Q!P!Pqc!R!Q!O!N!Ne!Mf!Nuc!K!R!Qf!P!Q!Qyc!M!L!L!K!K!L}c!P!Ppc"
DATA ""
DATA 32, 16
DATA "pc!M!M}c!Q!R!R!Q!Q!Pyc!L!Lf!M!L!K!Rucf!Oe!P!O!O!N!L!Kqc!M!M!L!L!K!Kf!R!Q!Q!P!O!N!M!Kmc!Q!R!R!Q!Q!P!O"
DATA "!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!Lic!L!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Necf!Oe!P!O!O!N!L!K!R!P"
DATA "!O!N!M!L!Lf!K!R!R!Q!Qcc!L!L!K!Kf!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Mf!Nec!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K"
DATA "!R!Qf!P!Q!Qic!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Lmc!R!P!O!N!M!L!Lf!K!R!R!Q!Q!P!Pqc!R!Q!O!N!N"
DATA "e!Mf!Nuc!K!R!Qf!P!Q!Qyc!M!L!L!K!K!L}c!P!Ppc"
DATA ""
DATA 32, 22
DATA "~c~c~c~czcd!*[{cLUC(!=!0!.!Fyc!6dC(]!0Uatc!6L!*L!0UC((]!3!3L!%sc[!0]!=!0!3U((!0!0!6['accUoc'!0!3](!0"
DATA "U!*!3!3!0!6!*'!7c!6aoc!*]!0((!0L[!.!6!6U'!F!FLU!Foc'!0!0!3(!6L[''!6!*'!F!F!=Laoc']!3!0][C!*'!.d'!.!7"
DATA "'U!Foc[]!0!0](!0!*''LL'!7ca!*aoc['!0!3](!3''!.'!6!Fccaa!Fqc'[[(]d'!*!6U!%xc!*!0L'[[LU!%xc!*][da!F!Fa"
DATA "yca!%~c~c~c{c"
DATA ""
DATA 32, 22
DATA "~csc!F!F~cc'Ua~caL'!%{c!*L]!3!.acL[vc!6!6!=(!=]!3!6a](!*tc!3!=!=CC(]!0!6L!F!0(!*cc!6dajcLUL]!3U!=C(]"
DATA "!0!3!*!.'!0[c!3!=['aic!*](C!=!0!6Ld]!3U!*!.'!.!3UC!F[[aicL]!0!0!3(!3!6[!0!0!3!6['!F!FC(!*'['aicU]]!0"
DATA "!0(!0!6!*'!3!6U'!F!F]!=UL!.''aicL]!3!0!0]!6Ld'UL'!F!F(U!6!*['[aic!*!0!6!3]]!*C!*'!.'L'!F!7(L!6L!*!%a"
DATA "aicL]!0!0]((]!*''UU!F!7caU!6Lamc['!0]((!3L'!.[U!Fcc!%!F!Faaoc''!*(][''[!6!6acavcU!3['[[UU!%xcL!0Lda"
DATA "!F!Fayc!F!F~c~c~c{c"
DATA ""
DATA 32, 22
DATA "~c~c~c~czcd!*[{cLUC(!=!0!.!Fyc!6dC(]!0UagcUdLjcdL'!0U!=C!=]!0!6UaecUCC!=aicLCC(!=]!3L:]]!3!3!*!.!%cc"
DATA "LC6!([aicL!0d](!0!6'!3!3!6UL!F!7cUC!(!(U[aic!*]!3!0]]!0!6!*'!6!3U[!Fa[!=CC!=!*[aic!*!0!3!0((!3U!*['"
DATA "!6L'!Fa!=(!=!='['aic!*!0]!0](!*!=]]!0'['!.!7!=!=CC!.[!*aicL!0f]!0!0!3!0!0U'!7c!*!=(([''ajc[]!3!0(("
DATA "e!0(!3acc!*(([d'akcd[((!3!0!3]]U[ac[!0]'[aaoc[!=!0!3!3!0('[!.aca[aarcL!0!3!3!0([['awcL[!F!F!.(!.'!.a"
DATA "|cL'aa~ca~cqc"
DATA ""
DATA 32, 22
DATA "~csc!F!F~cc'Ua~caL'!%{c!*L]!3!.acL[vc!6!6!=(!=]!3!6a](!*tc!3!=!=CC(]!0!6L!F!0(!*cc!6LUL!F!FhcLL!*(!0"
DATA "!6!=!=((!3!6U''!0'c!6!=C!=[!*agcU!=C((!0!3L((]]!6!*!F'!.!0!6CC!('['agc[d!0]!0!3L!*!3!0!0U[d!FdC!(!3"
DATA "d[agc[]!3!3(!0!3!6''!3!3U'!F!F]!=(C!=e'agcLd!0((UL!*!F'UU'!Fa(!=C!=[['['agcU]d!0(L!=(!0!0'L'a!7(!=("
DATA "!=[['['agcL(!0]]((!3!0!3]!3L'!7c!*((]d[!.[ahc[!0!3:]]!3d!0]!3!Fcc[](e[aajc'[!*](]!6!6!3(!3'!%c']]['a"
DATA "aocL!=]d!3]'!*[!%ca!Fa!%rcL]!3!3!0([''awc!*[!.'](''[!%{caL'!%a~c!%~cqc"
DATA ""
DATA 32, 22
DATA "~c~c~c~czcd!*[{cLUC(!=!0!.!FxcUC!(!((!=(!0U!FtcLLU(!0!6!=C(!0!0!3!*arcLC!=(!=]!0U((]!3!6!*!F!%cc!3!%"
DATA "lc!6!=!(C!=(!0!0!6!*]!0!0UL!.!%c!3Uaic'c!*!=!=C6!=]]!0!3!*'!6!3L[!F!F!*LUaic[!7(CC!(C((!3!3L[['UL'!F"
DATA "!F!=LUaic!*!F!=!=CC((!0UL!=(]!3''!Fa!7(L!*aic[]](]!=!=!0UdCd(!FU'!%caL!*!%ic[(]((]]!6((]!0U'[[!*acca"
DATA "a!Fajc!*!*d(](]((]U'd[!*aqc!*!*]!=(!=]!0]Lf[asc!*[(]((!0!6[[!%a!Fauc[L(]!0!6!*a{cL!*!0U!*a}c'aa~ctc"
DATA ""
DATA 32, 22
DATA "~csc!F!F~cc'Ua~caL'!%{c!*L]!3!.acL[vc!6!6!=(!=]!3!6a](!*tc!3!=!=CC(]!0!6L!F!0(!*cc!6a!F!FjcLUL]!3U!="
DATA "C(]!0!3!*!.'!0[c!3!=['aicUC!(!=!=!0!6Ld]!3U!*!.'!.!3UC!.[[ahcUdC((!0!3U!*!3!3!0U!*!F!F'!=CL['[!FecLc"
DATA "LCC!(6C]]!3UL'!3!3U[!F!F]!=UL[''aec[!7(!=C!(!((]]!3Ud'ULd!F!=!*LL['[aec!*!F](CC!=(!0UL!=(!0!0'L'a!7("
DATA "L!6U[daec!*!0(!=(!=(!0!6!=CC!=]!0[L!F!7cL!*ULahc'((d](!6]](]!6'd[accaLUUaic[!*((]!0((]]!3!6Ld'[acea"
DATA "lcL!*(](e]!6e'[!FscL!*e(!0L!*[eauc!*!*]!=!3!6L!%{c[!*!0U!*a}c''a~ctc"
DATA ""
DATA 32, 22
DATA "~c~c~c~czcd!*[{cLUC(!=!0!.!FxcUC!(!((!=(!0U!FgcULL!*jcUU!*(!0LCC((!3!3L!%ec!6CC!=aic!6C(!=]]!3U((!0"
DATA "!0!6L'!%cc!3CC!(!.ahcU!(C!(C!=]!3U'!3!0!0U[!Fac!6C!=!(!6'aec[c!*C!(!(6!=!=!0!0!6!*'!6!3U[!F!F!*!=dC'"
DATA "'aec!*!7dC!(!(!=(!3!6L[''LL'!Fa(!=!=C''!*aec[!F](!=C!=(]!6U(!=(!6'['!.!7]((!='[[aecL]]((!=(!0UdCd(!0"
DATA "U'!%cLd('[[aec[]](](]!6:]!=(!=]!3]!6acc!*(]d[!.afc!*!*(](!0(]d(]([!6!6[ac[]]'[aaic!*[]!=d]!=(]]!.['["
DATA "!.aca[aamc!*!*(]((!=](d'[!*atc!*!*((]]('[!*[[avc[[(]([''aayc!*[(!.a!%}cLapc"
DATA ""
DATA 32, 22
DATA "~csc!F!F!%~c!.La~ca!*[!%{cL!*]!3'!%cL!*vc!6U!=C(]]U!%]]Ltc!6!=C!((!=]]!6['!0(LccLUL['!FhcULL]!0LC!="
DATA "!=]!3!3U''!3!*cUCC!=!*[ac!*ecU!=d(!0!6U!=((!3!3L!.'!F!0!3C!(!(!.[!.acLaccU!=!(CC(]!0U[!3!0!6!6L!.''"
DATA "dC!(U[''ac!*Lc!*!=C!(6!=]]!3!6['!0!6U'!Fa(C(C!([[''ac[(!7!=CCd!=(!0!3L''!.UU'!Fa(!=C!=!*!.'[[acL!=!F"
DATA ":(C!=C(!0U'd(]'!*'!F!7C!=!=(['['[ac!*((](]((!0!6!=C!=!=((!3!6'!7c!*!=!=][['[[ac!*[]!=]!0](!3(]](](!0"
DATA "]!3!Fcc!*(][d'aaec[!*e]g(!0'!0!6[ac[(]''a!%ic!*L]!0]e(!0(!.!*[[!.aca'a!%mc!*!*!0((]](!0d'[[atc!*!*e]"
DATA "(''!*!*!.avc!*!*d('!*!*aayc[L]'aa}cLapc"
DATA ""
DATA 32, 22
DATA "~coc!*UL[!F|cU!=!0]!6azcU(]!3!3LLayc[!3!6(](!3!%yc'!=C!=(!=(!3!Fwc'!(dC!=!=(!0!6!FpcadcUL(]!0LC!=!=]"
DATA "!0!6!*aoc!*!7aLC(C!=]]U(]!0!3!6!*'!%cc!6ajc!*!FCd!3!6!0]!0!6!*!3!0!6!6[!F!%c!6Lajc[((f!3]!0UL[!6!3!6"
DATA "!*'!FdLajc[((!0!3U!3!0(U!*[''U!*[!Fa(L!*ajc!*!*(]!3!0!6!0('!=[[''[!Fa!7!.U!*!%lcL!0!6!3!6](]!0!*'[!*"
DATA "U'!%caUUalc!*]!3!6!3]((!3!*d[U!Fcceamca!F!F''[(!0d'[LUaxcL!0'[!F[LUaxcL!3[a!%a!F!Fayc'a~c~c~c{c"
DATA ""
DATA 32, 22
DATA "~coc!*UL[!F!FazcU(!0!0!6L[!%xc!6(]!0ULU'!%xcU!0U!=((!0!FacL[uc'!=CCd(!0!6a](!*sc[!=dC(!=]!0!3L!F!3(L"
DATA "ccLaa!Ffcadc!6U((]!6!=!=]]!3!3!*''!0!*c!6!='[afc!*!%!%!6CC!=(]!3U](!0!3!3!*!.!.!F!0!6C['!.afcL!FC!="
DATA "!0!3!3]]!3U[!0!0!3!6[!F!F!.(!=U'['afcL(]!0!6!6!3](!3!6L[!6!3!6'!Fa]CU!*!.'!*afc!*(]!0!3!6!3]!=!6!*['"
DATA "'!6!*'!F!F!=U!6U['[afc!*!=(!3!6!6!0]([(!=!.'[L!Fa!7!=LLU!.dagc[(!0!3!3!6]((!3L[['!6'!7c[L!6Uakc!*!0"
DATA "!3!6!3!0(]!0[''!*!3!Fcc!%!*!F!Falc''!F'[((!0L''!*!*Laca!%tc[!=]!*'[!*!*LaxcL!0L'da!Fayc[aa~c~c~czc"
DATA ""
DATA 32, 22
DATA "~coc!*UL[!F|cU!=!0]!6azcU(]!3!3LLayc[!3!6(](!3!%yc'!=C!=(!=(!3!Fwc'!(dC!=!=(!0!6!Fgc!*U!*!*fcadcUL!="
DATA "]!0UC!=((]!6L!%ec!6!=C!=afcL!7aU!=e(!3L](]!0!6L!.!%cc!3CC!('afc!*a!=!=!3!3!0!=!0!3L!*!3!0!0U[!F!%cLC"
DATA "C!(!6!*afc!*!=(!0d!3]!0!3!6''!3!3U'!F!F!*!=(C!=''afc[((!3!6!3!0(!3!6U''!F!6U'!Fa(C!=C''[afc[!=(]!3!6"
DATA "!3!=!=Ud(]'L'a!7(d!='['agcL(!0d!3(](!0!0!6!3]U'!7cLd(['[ahc!*!3!6!0!3(d]!3!6!3(!6accL(][['[aic'!F!F'"
DATA "L(]!3!0!0!3]!6[ac!*!0][[aaoc!*]!0!3!3!0(!.[[!%ca[aaqc!*(!0!3!6!3(d[awcL''!F!0!0[['a{ca['aa~c!%~cqc"
DATA ""
DATA 32, 22
DATA "~coc!*UL[!F!FazcU(!0!0!6L[!%xc!6(]!0ULU'!%xcU!0U!=((!0!FacL[uc'!=CCd(!0!6a](!*sc[!=dC(!=]!0!3L!F!3(L"
DATA "ccLU!*L!F!Fdc!FdcLU!=(!3U!=!=((!3!6L''!0'c!6C(!='[adc'!7!%LC(!=(]!6!*d]!3!6L!F!.!F!3LC6!(d!*adcL!F!="
DATA "C!6!6!0(]!0L[!3]!6U!*d!F!=C!=CU''[adc[((]!0!6!0!=!0!0U!*[!6UL['a]!=!=C!='!*[!*adc[((!0!3!6!0(]!6L!*'"
DATA "'!6L'!F!F!=!=(!=d[''adc[((!0!6!3]]U[!=d]'['a!7((C(d'!*!*aecL(d!3]d(!0!3!3!0!3U!.!%c!*(]!=!*d'[aec!*]"
DATA "]!3!0]!=(]!0!3!3](Lacc!*]]e[aagc!F!.!F''L!=(!0d!3!=U[ac'!0!0''aaoc!*!=d!3]][['aca!.a!%qcL]!3!6!3!3]'"
DATA "'[awc!*''!F!3]'['a{c[L'!%a~ca~cqc"
DATA ""
DATA 32, 22
DATA "~coc!*UL[!F|cU!=!0]!6azcU(]!3!3LLayc[!3!6(](!3!%yc'!=C!=(!=(!3!Fwc'!(dC!=!=(!0!6!Fmc[ccadcLL((!0!6!="
DATA "!=((!3!6Uaec!6gcL[!F'!7!%UC!=!=]!0!3L](]!3!3L[accLagcL!0'!F!F!=dC((!0!3U!*!3!3!0U!*!F!%c!6L!%gc[(!0"
DATA "!F(CC66C(]!6!6['d!6[a!F!*!0!*agc!*](!F(eC!=]!0!3!*[''!3!*'!Fa!=!*UagcL]!0!F!=(C!=((!3U!*((]!3'!*'!."
DATA "!7!=UUagc!*!=e(]!=(]U(C!=!=(]'U!F!%caULahcL[]((!0(]!3!0((]U'!*'!*acc!%!F!F!%jc!*L]](]]((](U!*['[Laqc"
DATA "['(!=](d]U[!*'[!*!Fsc!*!*(]d(L!*'!%dauc!*[(!=(L[a{c!*!*(ULa}c[a!F~ctc"
DATA ""
DATA 32, 22
DATA "~coc!*UL[!F!FazcU(!0!0!6L[!%xc!6(]!0ULU'!%xcU!0U!=((!0!FacL[uc'!=CCd(!0!6a](!*sc[!=dC(!=]!0!3L!F!3(L"
DATA "ccLa!%!Fcc[ccadcLU!=(]L!=!=(]]U!*''!0!*cUC['acc[[!F[!7!%LC(!=(]!6!*d]!3!6L!F!.!F!3LC['!*accL('!Fa!="
DATA "dC!=(]!3!6[!0!3!6!6!*!.!F!FC!=Ld'accL](!F!=C!=!(6!=!=!0!3!3L!.U!6U''!F!0CLL!*'[acc[](aC!=C!(C!=(]!3L"
DATA "['!.!6L[!Fa!=LUU''[acc!*(!=!F(!=C!=((!0!6L!=((!3'!*!.!.!7([!6!6!*'aacc[g(!=(]!6Cd!=(!0!.U'!7c!*!*UL["
DATA "!%fcL[(!=(]((!6](!=]!6[''[acc[!*UUaic[[d(]d(]]!6[[!*''acad!Flc[L]!=](!0](!6L!.'!*'!Fsc!*[](!0]]UL[ea"
DATA "uc[!*((]LLa{c[[(U!*a}cLa!Fa~csc"
DATA ""
DATA 32, 22
DATA "~coc!*UL[!F|cU!=!0]!6azcU(]!3!3LLayc[!3!6(](!3!%yc'!=C!=(!=(!3!Fwc'!(dC!=!=(!0!6!Fgc!*U!*!*cc[cc!Fdc"
DATA "LU((!3U!=!=((!0!3!*aecUCC!=acc!*!*a[!%!%LCe(]U]((!3!3!*!Facc!3C6C'acc!*]'!FaC!=!(C!=!=]!3U[!3!0!0!6"
DATA "!*'!%c!6dCU'!%cc!*]]!F]CC!(C!=!=]!3U!*[!6!6U'!F!F!*!=dC!*'accL](!F!=CCd!=(!0!3L''!.UU'!Fa(!=C!=!*!.'"
DATA "accL((!F!=(!=C((]!6[(!=(]'!*!.a!7C!=!=(!.!*[acc!*(]]((]!=(!0!6!=C(!=(]]!6!F!7cL]((d[adc!*Le(](!6!0]"
DATA "!=((]!0!0!3!Fcc[(][!*!*'afc!*[((!=!0(d]((]'!3!3[!%c']][[aaic!*!*]!=]d(](]['['[!%ca!Faamc!*!*((]](]("
DATA "d'['!%tc[!*d(:]d'[[avc[L(]!0[''aayc[U([a!%}c!*apc"
DATA ""
DATA 32, 22
DATA "~coc!*UL[!F!FazcU(!0!0!6L[!%xc!6(]!0ULU'!%xcU!0U!=((!0!FacL[uc'!=CCd(!0!6a](!*sc[!=dC(!=]!0!3L!F!3(L"
DATA "ccLU!*L!F!F!*cc!Fdc!6U!=!0!0Ld!=]!3!6L['!3[c!6!=C!=[[aL[a[!%!%!6CC((!0!3L((!0!3U!*d'!0!3!(6!(!*['a!*"
DATA "]!.!F!FC!=dC]!0!3U!*!3!0!0U[d!FeC!6[!*'aL]]a!=C!(CC!=(!0!3U!*'!3!6U['!F]!=dC''[[a!*((a!=eC!=(!3!6U['"
DATA "'UUd!F!=!=(Cd'['aL(!0!F](d!=]!0UL!=(]][[!.a!7!=!=(([[!*[[aL]]!0(!=]((!0UC!=Cd(]U'!7c!*(!=!='[d'acL!*"
DATA "]](!=]!0!3]d(:(!0!3!0accL(]'['[aaec[L](d]e(]([!6!3[!%c']!0''aaic!*L((]!0]]((]!*!*!.'!*aca'a!%mcLL((]"
DATA "(](([!*'!*'!%tcL!*]!0(!0]d['!*avcL!*((]''[aaycL!*][a!%}cLapc"
DATA ""
DATA 16, 24
DATA "~c~c{ciPhcPe>C>C(PfcPi>C!=PdcP>;g>CC!=]PcPf>;e>C!=]]PP;;>;f>CC(]TPPd>;;e>C!=]TTPcPC>!(>!(>C!=(]!6TP"
DATA "dcP(e!=(]TT!3PfcP]dT!*T[TPhcPPWd!&PPjcP_WW!&Pdc  fcPGR!&WPccd fcP_RW!&Pce fcPGG!&!&Pce fcP_RW!&Pf gc"
DATA "PR!&Pec  hcPPxc"
DATA ""
DATA 16, 14
DATA "5IocICIImcC&&I5Ikcd&dI65ic_d&I&&I5Igc_&&Cd&fIgc&__d&I6&I5Igc_g&eI5gc&__d&I&&Iic_g&Ikc&__d&mc_d&oc&_"
DATA "qc"
DATA ""
DATA 16, 14
DATA "oc!0_mce_kc__G__Gicd_dGAGgc__G__GGARAecd_!0G_AGAdRcc_!0G__GGARAARcc_!0_dGA!0!0ARAec]__GG!0RAARgcG_!0"
DATA "GAdRicGARAARkcAARAmc]R~cc"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~ckc!8,!8,~cmcg<!/!/"
DATA "lc,++!8~cjc<F<!G-!G-!G-FF<!/Pjca!7~cjc<-d!Gg-eF!/icP!7~chcF<-!G-!G-!G--!Ge-F<<PPgcP!7~cgc<j!Gf-eF<<P"
DATA "fcP!7~cgc<-k!Ge-F<FF<!/fca!7~cfc<i!G.!G!Ge-dF<<!/!/PecP!7~cec!B-!G-e!G.!G!GZ!G!G-F--F<<F<<!/!FdcP!7"
DATA "~cc<cc<h!G.d!Gf-eF<<!/!/PdcP!7}c<<!G<H--!G-!GZd!G.e!G-FF-F<<!/<<!/!1Pcca!7{c<<--1!@!5!G!G-j!G--eFf<"
DATA "d!/H!)!.!E!7yc<<!G-!G21!(!+-!G!G-!G-!G-!G-!G!Ge-F<FF<!/!/<!/!/!.!+!$!.!7xc<!Ge-1;2!+e-!G!G-!G!Gf-eF"
DATA "d<f!/!E!+!5!F!7xc!/F!G!G-<CC1!=!5--!G-!G-!G-!G-F--eFe<!/<!/H!E!$!BP!7xc!/FF--!GFC!(1!=:Fh-gFe<e!/HH"
DATA "!+!B!B!%!7xcb!D!DeF!=!(!@1!=:!=d-F-FF-dFf<!/!/!B!5H!+!+!$!+P!7xcbe*F<:1!(!@1e!+gF<Fe<!/!/!Bd!.!+!+!B"
DATA "!+!+!%P!Ewcb!D*!D!1F<!=1!(!(C1C!+!B!5!+1d!5!B!+!5::!=H!E!.!F!B!+!B!$!B!5!+PP:HHuc)d*)F<:!@e!(1d!(!E"
DATA "!@22;11!@1!=!+!E!$!$!B!+e!B!+!+!%Pd!5!*!Esc:!D*!D!1F<!51Cd!(!@1!@!(!@1d!@2!(!5!5!+!5!+!$!+!5!+!$!B!$"
DATA "!B!+!$PP!+d!5!EqcR_!(1!@*!1F<!BC1d!(1!@1!@!(!@1!@!(C!+!Bd!+!Be!+d!B!+!$H!B!Bd!+!B!BWWmcGG&!6!(1!(d!@"
DATA "11C!(1!@!(!@1!@1C!(C1!(!=!$!B!5!B!$!B!5!+!5!+!$!+!+!$!6!+!0!5!3!B!6!B!6RRWWicGG5&I&Ce!(1;;!@!(CC!(!("
DATA "1!(1e!(1!=!B!+e!Bd!+!B!B!$!B!+!5!+!+g!BRGG_!&WfcGdI&&_!+!=d!(1!(!@;8!@1C:C!=eC!(!@!(!5!+!$!B!$!B!+!$"
DATA "!)H!6!+!5!5!3d!B!6!B!$!$HR_GL_G!&ecRI&&5I&&_!+Cf!(1!@;!@!(e+:!=C1!(!+!+d!B!+!$H!B!+!5!+!+g!B!$!ERRG"
DATA "d_RW!&fcRR&&I&&I&&!+Cd!(1!(!@;;g+:!(!(!5!+!$!+!+!$!6!+!0!5!3!B!6!B!6!B!$!$HW_GR__WW!&!7icRR&&5dI&&!+"
DATA "CCe!(1!@h+!(!+!B!B!$!B!+!5!+!+g!B!$!EWGdRGRW!&!&mcGR&_II5I&U!+!=d!(1!(!@;f+:!$H!6!+!(!((d!B!6!B!$!$H"
DATA "RG_LGGWW!&aqcGG&&dI5I_!+Cf!(1!@++!BC!(!(1C:g!B!$!ERdG5IRG!&!&ucRR&II&&I&L!+Cd!(1!(!@;;!@!(!((!B!6!B"
DATA "!6!B!$!$HR_RRIIRR!&WycRG&I&I5I&_!+CCe!(1!@!=g!B!$!ERGRI5IRR!7!&}cGG&_I55I&_!+!=d!(1C!B!B!6!B!$!$HWR5"
DATA "5I5GW!&a~cdcGGI&fI_!+C!(!(!=d!B!$!ERG&I&IRR!&!&~chcRR&&Id5I&!+C!=!$!$HR_II5IGG!&a~clcGG&I&I&5I&H!E&I"
DATA "&I&5GGa!&~cpcRR&5II5I&&5II5RR!&a~ctcGG&&I&I&IIRR!&!&~cxcRGI_5&WW!&!7~c|cGR&R!7!&~c~cccGa~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~ckc!8,!8,~cmcg!4``"
DATA "lc,++!8~cjc!4!4!2/DD/D//X!4`Wjca!7~cjc!2/dD/DDd/XX!2!2`icP!7~chc!4!2/D/dD/DD/fX!2`WgcP!7~cgc!4kDe/XX"
DATA "!2!2!4!4WfcP!7~cgc!2/dD^D^eD/XX/XX!2!4!4!,fca!7~cfc!2eDe^0dD//XXe!2!4`WecP!7~cec!B/D/Dd^0d^D//fX!2"
DATA "d!4`!FdcP!7~cc!4cc!4fD^^0^^DD//eX!2!2!4!4``WdcP!7}c!2!4D!4HD/D/D^Dd^D^DD//X!2!2X!2!2!4!4`APcca!7{c!4"
DATA "!2//1!@!5/D/dD^gD//eX!2d!4d`H!)!.!E!7yc!2!4D/D21!(!+//D/dD^eD//X!2XX!2!4!4``!4``!.!+!$!.!7xc!2Dd/X1;"
DATA "2!+d/D/fDd/dXe!2!4!4e`!E!+!5!F!7xc`/D^X!2CC1!=!5X/D/D/D/X//dXf!2!4!4`!4`H!E!$!BP!7xc`dX/DXC!(1!=:j/"
DATA "dXf!2!4!4d`HH!+!B!B!%!7xcb!D!D!2X/!2!=!(!@1!=:!=XX//X/X!2Xf!2!4!4``!B!5H!+!+!$!+P!7xcbe*X!4:1!(!@1"
DATA "e!+hXd!2!4!4``!Bd!.!+!+!B!+!+!%P!Ewcb!D*!D!1!2!2!=1!(!(C1C!+!B!5!+1d!5!B!+!5::!=H!E!.!F!B!+!B!$!B!5"
DATA "!+PP:HHuc)d*)X!2:!@e!(1d!(!E!@22;11!@1!=!+!E!$!$!B!+e!B!+!+!%Pd!5!*!Esc:!D*!D!1!2!2!51Cd!(!@1!@!(!@1"
DATA "d!@2!(!5!5!+!5!+!$!+!5!+!$!B!$!B!+!$PP!+d!5!EqcR_!(1!@*!1X!4!BC1d!(1!@1!@!(!@1!@!(C!+!Bd!+!Be!+d!B!+"
DATA "!$H!B!Bd!+!B!BWWmcGG&!6!(1!(d!@11C!(1!@!(!@1!@1C!(C1!(!=!$!B!5!B!$!B!5!+!5!+!$!+!+!$!6!+!0!5!3!B!6!B"
DATA "!6RRWWicGG5&I&Ce!(1;;!@!(CC!(!(1!(1e!(1!=!B!+e!Bd!+!B!B!$!B!+!5!+!+g!BRGG_!&WfcGdI&&_!+!=d!(1!(!@;8"
DATA "!@1C:C!=eC!(!@!(!5!+!$!B!$!B!+!$!)H!6!+!5!5!3d!B!6!B!$!$HR_GL_G!&ecRI&&5I&&_!+Cf!(1!@;!@!(e+:!=C1!("
DATA "!+!+d!B!+!$H!B!+!5!+!+g!B!$!ERRGd_RW!&fcRR&&I&&I&&!+Cd!(1!(!@;;g+:!(!(!5!+!$!+!+!$!6!+!0!5!3!B!6!B!6"
DATA "!B!$!$HW_GR__WW!&!7icRR&&5dI&&!+CCe!(1!@h+!(!+!B!B!$!B!+!5!+!+g!B!$!EWGdRGRW!&!&mcGR&_II5I&U!+!=d!(1"
DATA "!(!@;f+:!$H!6!+!(!((d!B!6!B!$!$HRG_LGGWW!&aqcGG&&dI5I_!+Cf!(1!@++!BC!(!(1C:g!B!$!ERdG5IRG!&!&ucRR&II"
DATA "&&I&L!+Cd!(1!(!@;;!@!(!((!B!6!B!6!B!$!$HR_RRIIRR!&WycRG&I&I5I&_!+CCe!(1!@!=g!B!$!ERGRI5IRR!7!&}cGG&_"
DATA "I55I&_!+!=d!(1C!B!B!6!B!$!$HWR55I5GW!&a~cdcGGI&fI_!+C!(!(!=d!B!$!ERG&I&IRR!&!&~chcRR&&Id5I&!+C!=!$!$"
DATA "HR_II5IGG!&a~clcGG&I&I&5I&H!E&I&I&5GGa!&~cpcRR&5II5I&&5II5RR!&a~ctcGG&&I&I&IIRR!&!&~cxcRGI_5&WW!&!7"
DATA "~c|cGR&R!7!&~c~cccGa~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cgcg<!/!/~czc<F!G--!Ge-!/!1~cwc<-d!Gg-FFP~cuc<-!G-"
DATA "!G-!G!Gd-FF<<!/~ctc<i!G--dF<P~csc<--d!G.!G--F--FF!/!1P~crc!B--!G!G.d!G--FF<!/P!/-g<!/~ckc!BF--!G!Gd-"
DATA "eF!/P--!G--!Ge-!/!1~cic!5!(CFF-eF<<!/Pf!Gg-FF!/~chc!5!@CC!=!(<<F<<!/!.--d!G-!G!G-!G-FF<<!/~cgc:!@!@"
DATA "!(C!(!(:!5!+!B!)!Ei!Gd-dF<P~cgc!5!@11!(CC:!5!+!+!)d-d!G.!Gf-FF<!1P~cfc!+!@!@11C!=!=!5!5!+!B1--!G!G."
DATA "d!G--dF<!/!/!.~cfc!+!(11!(!=!=:!5!+!B!B1FF-!G!Gd-F-FF<<!/!/!.~cfc!E!(!(!@1!=:e+!B!=!@1-F-fF<<!/!/!$"
DATA "!$!F~cfc!$!=e!(!5e+!B!5!(!@1!(!=<<F<!/!/!+!B!$!$!+!F~cfc:!+!BCC!(!Ee+P!$!(!(11e!=:!5d!+!B!+!+!F~cfc"
DATA "!5!@!5!+!B!+H!E!E!.!.!7!$f!(:CC!=!5!5!$!B!5!+!5!F~cfc:!@!@CC!+!)!E!.!F!%!7!B!(!(CC1!(!=:!5!+d!Bd!+!E"
DATA "~cfc:!@11!(!=H!E!FPP!7!6g!(!=(!5!+!B!$!B!5!+!5!.~cec!5!(e1!=!)H!.!F!%!7!B1!@!(C1!(!=:!5!+d!B!+!B!BP"
DATA "~ccc!5!+!(C!(!@1!(!=!)!E!FPP!7!+!(d1C!(!=(!5!+!B!5!+!$!B!.!%zc!&!Icc!5!5d1!=!(!(11!=HH!.!F!%!7!):!51"
DATA "1!=!=!(!=!=:d!+!$!EH!Fxc!&!#^0!I!#1!@1!@1!(!=!(C!=!+!.!E!.PP!7!$!(!=:!5!B:Cd:!+!5!.!EH!5!Fvc!&!I0d^"
DATA "e!,!@d1C:!=!=!5!B!F!FP!%!F!)!(!(11!B!B!$!B!$!$HH!+!B!+!+!Fuc!&^0^0d!,A!,0^01!@!=C!=!=!5:!=!=!E!E!F!$"
DATA "!(Cd!(!=CC!=(!5!$!B!5!+!5!Fuc!&!I^f!,^0d^::!=!=e:!F!E!E!.!7!Bd!(C1!(::!5!+d!Be!+Wsc!B!,!,!I^W!,0^0^0"
DATA "e:!=C::!F!.!FHPP!7!6!(CC!=!(!(!=(!5!+!B!$!B!5!+!5!B!+Wapc!+!@!@CW!I!,!,d^!,d:!=!=C!=!Ed!FdP!%!7!$!@1"
DATA "CC!(!(!=:!5!+d!B!+d!B!+!5!5WWmcG!(!(1!@;C^!,W^W!,!=:!5!=C!E!.!.!F!&W!B!$!Bd!+!(!(1!(!=C:!5!5!+!B!5!+"
DATA "!$!B!$!B!+!5!+!BLWWicGG&Ce!(!@!@CW!I^W!,:!+!5!E!.!F!&!,!,^0^!5!511CC!(!(!=!=C!=!=:!5!+!+!$!)!B!5e!B"
DATA "d_WWfcG5I&&!=C!(1!(1!@;C^^W^!I!+!.!&W!,0^0^0!,!,1!@11C:!5!5!=!=!5!5!B!$!$!3!(!+d!B!$!$GGRGG!&ecGdI&I"
DATA "_!=Ce!(!@!@CW!&^^!,!,d^0^f!,^!@e1!(CCf!(!+e!B!$HdG_RR!&fcRG&&dI&!=e!(1!@;C!,^W!,!I^0!,Ad!,0^0^01!@1"
DATA "!@1!@1!@!(!+!B!6!B!$!$LRRGLWW!&!7icRRII&I&&!=Ce!(!@!@C!I!,!,!Ie!,d^0^!,!,!&1!@1!@11!+e!B!$HRRI5WR!&"
DATA "!&mcRR&I&&I_!=C!(1!(1!@;C!,^!I!,W^0^0!,!,!&!&!#!51!@!(!+d!B!$!$LWG5IRR!&!7qcGGII5&&_!=Ce!(!@!@C!I!I"
DATA "!,^^!,!,!&!&!#!#_!B!(!+e!B!$HGG5&RR!&!&ucRGII5II&!=e!(1!@;C!,!,!I^!7!#!,_!3!(!+!B!6!B!$!$LRRIIRW!&!7"
DATA "ycRRI&I5I_!=Ce!(!@!@CW!I!#!#!B!(!+e!B!$HRR5&RR!&!&}cRRII5I&I!=C!(1!(1!@;CC!(!+d!B!$!$LWG&&RR!&!7~cdc"
DATA "RG_d&I&!=Ce!(!@!+e!B!$HGR&&RR!&!&~chcGW&&I5&!6!=e!(!6!B!$!$LRR5IRR!&a~clcGG_I5&__!=C!(!B!$HWG5&RR!&"
DATA "!&~cpcRW&&5I&&!=LR_IIRG!&a~ctcGRd&I&dIRW!&!&~cxcGG&I&IRG!&!7~c|cGG&W!&!&~c~cccR!7~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cgc!2g!4`~czc!2!4D/DD/X//`!,~cwc!2/dD/D//dX!2`~cuc"
DATA "!2/fDf/XX!2!,~ctc!4dD^eD//XX!2!4W~csc!4//Dd^DD/XX!2!2!4!4!,W~crc!B//DDd^D/XX!2!2`W!,/!4!2e!4`~ckc!BX"
DATA "X//dDXXd!2`WD/D/DDe/`!,~cic!5!(ChX!2!4``iD//XX!2`~chc!5!@CC!=!(!2!2d!4`!.D/dD/D//D//XX!2!,~cgc:!@!@"
DATA "!(C!(!(:!5!+!B!)!E/iD//XX!2!2W~cgc!5!@11!(CC:!5!+!+!)//DDd^DD/XXd!2!4AW~cfc!+!@!@11C!=!=!5!5!+!B1//D"
DATA "D^^DD/dX!2!4``!.~cfc!+!(11!(!=!=:!5!+!B!B1XX//D/DXXd!2!4!4`A!.~cfc!E!(!(!@1!=:e+!B!=!@1hX!2!4!4``!$"
DATA "!$!F~cfc!$!=e!(!5e+!B!5!(!@1!(!=!2!2!4!4``!+!B!$!$!+!F~cfc:!+!BCC!(!Ee+P!$!(!(11e!=:!5d!+!B!+!+!F~c"
DATA "fc!5!@!5!+!B!+H!E!E!.!.!7!$f!(:CC!=!5!5!$!B!5!+!5!F~cfc:!@!@CC!+!)!E!.!F!%!7!B!(!(CC1!(!=:!5!+d!Bd!+"
DATA "!E~cfc:!@11!(!=H!E!FPP!7!6g!(!=(!5!+!B!$!B!5!+!5!.~cec!5!(e1!=!)H!.!F!%!7!B1!@!(C1!(!=:!5!+d!B!+!B!B"
DATA "P~ccc!5!+!(C!(!@1!(!=!)!E!FPP!7!+!(d1C!(!=(!5!+!B!5!+!$!B!.!%zc!&!Icc!5!5d1!=!(!(11!=HH!.!F!%!7!):!5"
DATA "11!=!=!(!=!=:d!+!$!EH!Fxc!&!#^0!I!#1!@1!@1!(!=!(C!=!+!.!E!.PP!7!$!(!=:!5!B:Cd:!+!5!.!EH!5!Fvc!&!I0d^"
DATA "e!,!@d1C:!=!=!5!B!F!FP!%!F!)!(!(11!B!B!$!B!$!$HH!+!B!+!+!Fuc!&^0^0d!,A!,0^01!@!=C!=!=!5:!=!=!E!E!F!$"
DATA "!(Cd!(!=CC!=(!5!$!B!5!+!5!Fuc!&!I^f!,^0d^::!=!=e:!F!E!E!.!7!Bd!(C1!(::!5!+d!Be!+Wsc!B!,!,!I^W!,0^0^0"
DATA "e:!=C::!F!.!FHPP!7!6!(CC!=!(!(!=(!5!+!B!$!B!5!+!5!B!+Wapc!+!@!@CW!I!,!,d^!,d:!=!=C!=!Ed!FdP!%!7!$!@1"
DATA "CC!(!(!=:!5!+d!B!+d!B!+!5!5WWmcG!(!(1!@;C^!,W^W!,!=:!5!=C!E!.!.!F!&W!B!$!Bd!+!(!(1!(!=C:!5!5!+!B!5!+"
DATA "!$!B!$!B!+!5!+!BLWWicGG&Ce!(!@!@CW!I^W!,:!+!5!E!.!F!&!,!,^0^!5!511CC!(!(!=!=C!=!=:!5!+!+!$!)!B!5e!B"
DATA "d_WWfcG5I&&!=C!(1!(1!@;C^^W^!I!+!.!&W!,0^0^0!,!,1!@11C:!5!5!=!=!5!5!B!$!$!3!(!+d!B!$!$GGRGG!&ecGdI&I"
DATA "_!=Ce!(!@!@CW!&^^!,!,d^0^f!,^!@e1!(CCf!(!+e!B!$HdG_RR!&fcRG&&dI&!=e!(1!@;C!,^W!,!I^0!,Ad!,0^0^01!@1"
DATA "!@1!@1!@!(!+!B!6!B!$!$LRRGLWW!&!7icRRII&I&&!=Ce!(!@!@C!I!,!,!Ie!,d^0^!,!,!&1!@1!@11!+e!B!$HRRI5WR!&"
DATA "!&mcRR&I&&I_!=C!(1!(1!@;C!,^!I!,W^0^0!,!,!&!&!#!51!@!(!+d!B!$!$LWG5IRR!&!7qcGGII5&&_!=Ce!(!@!@C!I!I"
DATA "!,^^!,!,!&!&!#!#_!B!(!+e!B!$HGG5&RR!&!&ucRGII5II&!=e!(1!@;C!,!,!I^!7!#!,_!3!(!+!B!6!B!$!$LRRIIRW!&!7"
DATA "ycRRI&I5I_!=Ce!(!@!@CW!I!#!#!B!(!+e!B!$HRR5&RR!&!&}cRRII5I&I!=C!(1!(1!@;CC!(!+d!B!$!$LWG&&RR!&!7~cdc"
DATA "RG_d&I&!=Ce!(!@!+e!B!$HGR&&RR!&!&~chcGW&&I5&!6!=e!(!6!B!$!$LRR5IRR!&a~clcGG_I5&__!=C!(!B!$HWG5&RR!&"
DATA "!&~cpcRW&&5I&&!=LR_IIRG!&a~ctcGRd&I&dIRW!&!&~cxcGG&I&IRG!&!7~c|cGG&W!&!&~c~cccR!7~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cfce!1"
DATA "~c}cbb!8.dZbb~cycbb..Z.dZ!8!8!1!1~cvcb!De.Z!D!8Z.dZ!8b~cscbf.!D!Df.ZZ!8!8!1!1~cpcb.8d.Z!D..+.Z!D!8Z."
DATA "dZbb~cmc!888d.Zf.!D!D..Z.dZ!8!8!1!1~cjcCe8.!@.8d.Z!De.Z!D!8Z.dZ!8b~cgc!=.e8Z88d.!Df.!D!Df.ZZ!8!8!1!1"
DATA "~ccc!FP!$!)d8!@e8.Z.8d.Z!D..+.Z!D!8Z.dZbb~c!F!.PP88..e8B88d.Zf.!D!D..Z.dZ!8!8!1!1|cP!EP!%.8be8Je8.!@"
DATA ".8d.Z!De.Z!D!8Z.dZ!8bzcP!.PP.b!D!D88!@.e8B88d.!Df.!D!Df.ZZ!1!1PPwc!+H!.P!%bb!8!8.8Ze8Je8.Z.8d.Z!D..+"
DATA ".Z!1!8d!1PbPPscC:1!.!.!%!%!5:!8..Ze8..e8B88d.Zf.h!1PPd!1rc:;8CC!(1!@1!5Z.!D.8.8.e8Je8.!@.8d.!8!1!.!1"
DATA "P!1!FPaP!1!1Prc!5!@!(!@;!(!(!5!+!$Z!De.Ze8J.e8Z88d.e!1bgPb!8!1Pqc!B!(C!(!(!=!$!+!B!E.!De.!D.8.8.e8J"
DATA "e8.!1!1P!1!FPadPaPd!1Poc:!5!(!(1!@!(:!B!$!$b!D!Dd.Ze.Ze8..e8d!1HjPd!1!B!E!FlcG!(!(C!(!@1!@!=!+!$!$b"
DATA "!D.Z..!De.!D.8.8.e8!F!1!)PaPaPaPaPaPP!B!+!B!6!BaicGG_Ce!(!@1!=!+!)!Eb!8dZ!D!Dd.!De.Ze8!1!1!.mP!+!+"
DATA "d!B!$G!&!&fcG&II!6!=11!@1!@!=!)!B!+1C!DZZ!8.Z.Z!De.!D.8.8!1!FdPadPadPaP!B!B!$!$LRGRL!&ecR&&I&&R1C!(1"
DATA "!=1!@e1!(!D!8dZ!8!Dd.Ze.d!1lPd!B!$HWGG_RR!&fcGW&_&_&aC!(!5!(!(!@;!@1!@1C!D!Dd+Z.Z!De.!1b!FPaPaPaPaP"
DATA "!6!B!$!$LWGRLWW!&!%icRG&I&I&W!+Cd!(!@;1!@1h+!8!Dd.e!1hPd!B!$HWdGWR!7!&mcRR&II&&!6!=C!(1!(!@;!@j+Z.Z"
DATA "d!1!FdPaP!B!B!$!$LdRGRR!&!&qcGRI&II&&!=Cd!(!@!@k+f!1dPd!B!$HdG_WR!&!&ucRGI_dI_!=e!(!@!@i+!1!1P!B!5!B"
DATA "!6!B!$!$LRRGGWG!&aycGR&I5II&!=Cd!(!@!@g+!1!B!(!+d!B!$HRGRGRRa!&}cRRI5I5&&!=C!(1!(!@!@d+!B1!+!B!B!$!$"
DATA "LWGeR!&a~cdcRG&&I&I&!=Cd!(!@!@!(!+d!B!$HWRI5RG!&!&~chcRR&I&II!3!=e!(!6!B!$!$LRGI5WR!&!7~clcGRII&II_"
DATA "!=C!(!B!$HWRIIRW!&!&~cpcRR&I5I&!3!=LW&I5WW!&a~ctcRGI&fI&WW!&!&~cxcGG&_IIRW!&a~c|cRRIR!7!&~c~cccR!%~c"
DATA "~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cfcAd!,"
DATA "~c}c_!,!I0d^!#!#~cyc!#_00^0d^!#!#!,!,~cvc_!I^0^0^!I!I^0d^&!#~csc&f0!I!If0^^!#!I!,!,~cpcIf0^!I00^0^!I"
DATA "5^0d^!#!#~cmc!If0^f0!I!I00^0d^!#!#!,!,~cjcIf06f0^!I^0^0^!I!I^0d^&!#~cgcId02h05f0!I!If0^^!#!I!,!,~ccc"
DATA "!FP!$!)0206f06f0^!I00^0^!I5^0d^!#!#~c!F!.PPe02d06f0^f0!I!I00^0d^!#!#!,!,|cP!EP!%00!#00206f06f0^!I^0^"
DATA "0^!I!I^0d^&!#zcP!.PP0_5!I0060202h05f0!I!If0^^!,A!&!&wc!+H!.P!%!#!,!I5f0206f06f0^!I00^0^!,!#!,A!,W!#W"
DATA "!&scC:1!.!.!%!%!5:!#00^g02d06f0^f0Af!,_PPd!,rc:;8CC!(1!@1!5^05h0206f06f0!#!,A!,W_!FPaPA!,Wrc!5!@!(!@"
DATA ";!(!(!5!+!$^!Ie0^e060202h0Ae!,gP!#!#!,!&qc!B!(C!(!(!=!$!+!B!E0!Ie05h0206f0A!,W_!FPadPaPA!,A!&oc:!5!("
DATA "!(1!@!(:!B!$!$!#!I!Id0^e0^g02d0!,!,_HjPd!,!B!E!FlcG!(!(C!(!@1!@!=!+!$!$!#!#0^00!Ie05h020R_!)PaPaPaPa"
DATA "PaPW!B!+!B!6!BaicGG_Ce!(!@1!=!+!)!E!#!#d^!I!Id0!Ie0^e0A!,!.mP!+!+d!B!$G!&!&fcG&II!6!=11!@1!@!=!)!B!+"
DATA "1C!I!I^50^0^!Ie05e0!,WdPadPadPaP!B!B!$!$LRGRL!&ecR&&I&&R1C!(1!=1!@e1!(!I!#d^!#!Id0^e0d!,lPd!B!$HWGG_"
DATA "RR!&fcGW&_&_&aC!(!5!(!(!@;!@1!@1C!I!Id+D0^!Ie0A!#WPaPaPaPaP!6!B!$!$LWGRLWW!&!%icRG&I&I&W!+Cd!(!@;1!@"
DATA "1h+!#!Id0A!,A!,hPd!B!$HWdGWR!7!&mcRR&II&&!6!=C!(1!(!@;!@j+^0^!,A!,WdPaP!B!B!$!$LdRGRR!&!&qcGRI&II&&"
DATA "!=Cd!(!@!@k+_Ad!,dPd!B!$HdG_WR!&!&ucRGI_dI_!=e!(!@!@i+A!,W!B!5!B!6!B!$!$LRRGGWG!&aycGR&I5II&!=Cd!(!@"
DATA "!@g+!,!B!(!+d!B!$HRGRGRRa!&}cRRI5I5&&!=C!(1!(!@!@d+!B1!+!B!B!$!$LWGeR!&a~cdcRG&&I&I&!=Cd!(!@!@!(!+"
DATA "d!B!$HWRI5RG!&!&~chcRR&I&II!3!=e!(!6!B!$!$LRGI5WR!&!7~clcGRII&II_!=C!(!B!$HWRIIRW!&!&~cpcRR&I5I&!3!="
DATA "LW&I5WW!&a~ctcRGI&fI&WW!&!&~cxcGG&_IIRW!&a~c|cRRIR!7!&~c~cccR!%~c~c~cic"
DATA ""
DATA 32, 38
DATA "~c~c~c~c~c~c~c~c~c~c~c~crce<!/!/zcF!G--!G--!/xcFe!G--F<!1vcFg!G-F<!/Puc<e!G--FF<!/Puc<--!G!G--FF<<P"
DATA "uc!5e-dF<!/!/!.uc!51CeF<<!$!+!Euc!+1!(C!=!5!+!B!B!$!+!Fuc!+!(!@C::!+!B!$!+!$!Fuc!$C1!(!(C!=!5!+!+!$P"
DATA "uc!3:!+!(!=!=(!+!+!B!E!Fsc!5!+!@!(!+!B!+!+!$!)!.!.!B!+!E!Eoc!5!5!(!(!@!@C1C!=!=!+!5!B!5!+!+!5!E!ElcC"
DATA "!@11!(C1C!=!=:!5!+!B!$!+!$!+d!5!EjcG1!@;!@1!(!(C!=!=!5!+!BU!$U!B!+!5!+!+!3WgcRR&!(!(!@;;!@!(C:!+!B!B"
DATA "!)!)C!(1C:d!BRW!&dcR5I_!=!(!(11!@;11!(1!(d1!(:!3!B!6!$!*RR_accG&II&&!=C!(!(1;;!@d1!(:d!B!$!)RIIRR!&"
DATA "dcRG_II&U!=d!(1!@;!(:!3!B!6!$!*RI&WR!7!&gcRRe&_!=Cd!(!+!B!B!$!)R&&RR!&!7kcRRII5&I!=!(!=!$!$!*R5IGWa"
DATA "!&ocRGd&I_!5Hd&RG!&!&scGGI&II5IGR!%!&wcRRI&WR!&!&{cGG!7!&~crc"
DATA ""
DATA 32, 38
DATA "~c~c~c~c~c~c~c~c~c~c~c~crc!4!2!4!4``zc!4D/DD/!2`xc!2eD/XX!2!,vc!4Dd^DDX!2!2`Wuc!2DD^^D/X!2!4`Wuc!2//"
DATA "Dd/X!2!4!4Wuc!5//eX!2!4``!.uc!51CXd!2!4!4!$!+!Euc!+1!(C!=!5!+!B!B!$!+!Fuc!+!(!@C::!+!B!$!+!$!Fuc!$C1"
DATA "!(!(C!=!5!+!+!$Puc!3:!+!(!=!=(!+!+!B!E!Fsc!5!+!@!(!+!B!+!+!$!)!.!.!B!+!E!Eoc!5!5!(!(!@!@C1C!=!=!+!5"
DATA "!B!5!+!+!5!E!ElcC!@11!(C1C!=!=:!5!+!B!$!+!$!+d!5!EjcG1!@;!@1!(!(C!=!=!5!+!BU!$U!B!+!5!+!+!3WgcRR&!("
DATA "!(!@;;!@!(C:!+!B!B!)!)C!(1C:d!BRW!&dcR5I_!=!(!(11!@;11!(1!(d1!(:!3!B!6!$!*RR_accG&II&&!=C!(!(1;;!@d1"
DATA "!(:d!B!$!)RIIRR!&dcRG_II&U!=d!(1!@;!(:!3!B!6!$!*RI&WR!7!&gcRRe&_!=Cd!(!+!B!B!$!)R&&RR!&!7kcRRII5&I!="
DATA "!(!=!$!$!*R5IGWa!&ocRGd&I_!5Hd&RG!&!&scGGI&II5IGR!%!&wcRRI&WR!&!&{cGG!7!&~crc"
DATA ""
DATA 16, 16
DATA "~c~crcePdcjP..Z!DPcPe,+,,-F.Z.!DPP!=9,,+,,+--..!Db:P!B!G-!G-!G-!G-F!D!8b!1!5P!+j<!D!D!1!1!BcP!/<!/<"
DATA "!/<!/<!/!8b!1P!5P:!F!.!+!F!F!+!F!+!+!.!+!+P!(PCH!E!5!EH:H:!5!E!5:!F!BP!BPP!)aP!)P!)!)P!)!)!%PcP!7!%"
DATA "!F!%!7!.!7!.!F!%!F!.P ccmPcc"
DATA ""
DATA 16, 16
DATA "~c~crcePdcjP..Z!DPcPe,+,,-F.Z.!DPPH9,,+,,+--..!Db:P!F!G-!G-!G-!G-F!D!8b!1!5P!Fj<!D!D!1!1!BcP!/<!/<!/"
DATA "<!/<!/!8b!1P!5P!E!+!+!F!+!+!.!+!F!F!+!.!.!)!(P!B!5:H:!5!E!5!EH:H!EU!BP!F!)!)P!)!)P!)aP!)Pa!.PcP!F!."
DATA "!7!.!F!%!F!%!7!.!7!%P ccmPcc"
DATA ""
DATA 16, 16
DATA "~cycePjcdP-9+FPhcP!Gb,,+,,FPgcPF!G9+9d,FPecP!+FF!G,,+,!/..Pdc!$!E!+FF!G-.Z.Z!D!8Pcc!.C!E!FFF!G..Zb!1"
DATA "*bPc!F!.C:PFFZ!Db)b)b[PP!F!5!):!B<Z!Dbb!1!1b!%!%cP!B!5!$:!BP!D!1$!$!=!(!@!7ccP!+!.!$!.:CC!(d!B!+!7dc"
DATA "P!F!.!%!7!5!B!$!B!$!BPPecP!F!%!7!+!BePcc"
DATA ""
DATA 16, 16
DATA "~cycePjcdP-9+FPhcP!Gb,,+,,FPgcPF!G9+9d,FPecP!FFF!G,,+,!/..PdcC:PFF!G-.Z.Z!D!8Pcc!5!$:!BFF!G..Zb!1*bP"
DATA "c!B!5!$!E!+FFZ!Db)b)b!%PP!+!.!=!E!F<Z!Dbb!1!1b!E!EcP!F!.C!E!FP!D!1$!$!=!(!@!FccP!F!5C!5:CC!(d!B!+!."
DATA "dcP!B!5!EP!5!B!$!B!$!BPPecP!B!E!F!+!BePcc"
DATA ""
DATA 16, 16
DATA "~cycgPjcP,9+9,9PgcdP,,+d,dPdcP:!5!F+9d,9!.H!)PccP!=!+P+,+d,!F!E!$PccPH!.!7.Z.Z..!7dP cP!=!.P8e.!D!."
DATA "!%!)P cP!=!5!F!D7!87!8b!.H!EP cP!)!.!7!D!87!8!8!1d!%P cPH!.!%!8b!8b!1!1!7!EPP cP!.Ph!(1P!7P cP!.!%:"
DATA "!5:!5:!5:!5!7!%P ccPPi!+PP c"
DATA ""
DATA 16, 16
DATA "~cycgPjcP,9+9,9PgcdP,,+d,dPdcP!*!.!7+9d,9!%dPccP!)!.!7+,+d,!%!%PPccP!=!5!F.Z.Z..!.H!$P cP!)!5!78e.!D"
DATA "!%!EaP cPH!.!7!D7!87!8b!7P!%P cP!=!5!F!D!87!8!8!1!FH!EP cP!=!5!.!8b!8b!1!1!.:!$P cP!5!)h!(1!$!.P cP"
DATA "!5!E:!5:!5:!5:!5!.!.P ccPPi!+PP c"
DATA ""
DATA 16, 16
DATA "~c{cePlcP-9+<dPhcP-d,+,$!/PfcP-9++,9$<!/PecP.!D!/+,,3<!/!/!EPccP!D!8.Z..-<!/P!.!.PcPZ*Z88Zb<!/!/!%!."
DATA "!)!7P!5Z!D*!D*!8!8!/P!%!5!)!7!%!+H!Dbd!D!8b!/!.HP!.!%PP!@CC!=!D!D!8P!E!+P!F!.P P!(dC1!(!(HUP!7!.P cP"
DATA "P!=dC!=1!%P!7!%P ecePC!(!7P!%P dc"
DATA ""
DATA 16, 16
DATA "~c{cePlcP-9+<dPhcP-d,+,$!/PfcP-9++,9$<!/PecP.!D!/+,,3<!/!/!%PccP!D!8.Z..-<!/P!%!+!)cPZ*Z88Zb<!/!/!."
DATA "!5P!.P!.Z!D*!D*!8!8!/P!.!.P!F!.!(:!Dbd!D!8b!/!%!%!)!%!.P!6!@CC!=!D!D!8P!%!.!)!7!%P H!(dC1!(!(H!=!)!."
DATA "!%P cPP!=dC!=1!.!$!F!.P ecePC!(!.!$!.P dc"
DATA ""
DATA 16, 16
DATA "~c~chcePlcP.!D..jPccP!DZ.Z<-d,9++bPc!5e.!/-+,,+,+,!.P!(Z!8!D!8d-!G-!G-!G-!FPCZ!D!8!8j<!.P!(!Db!8b<!/"
DATA "<!/<!/<!/<P !(!.!+!5!F!+!+!.!+!F!F!+!F!F!)P!5!B!5:H:!5!E!5!EH:H!EUPP!F!)!)P!)!)P!)aP!)Pa!.PcP!F!.!7"
DATA "!.!F!%!F!%!7!.!7!%P ccmPcc"
DATA ""
DATA 16, 16
DATA "~c~chcePlcP.!D..jPccP!DZ.Z<-d,9++bPc!5e.!/-+,,+,+,!%P!(Z!8!D!8d-!G-!G-!G-!7PCZ!D!8!8j<!7P!(!Db!8b<!/"
DATA "<!/<!/<!/<P !(!5!.!F!+!F!F!+!F!+!+!.!+!+PP!5CH!E!5!EH:H:!5!E!5:!FPP!BPP!)aP!)P!)!)P!)!)!%PcP!7!%!F!%"
DATA "!7!.!7!.!F!%!F!.P ccmPcc"
DATA ""
DATA 16, 16
DATA "~cicdPkcdP!D!D.PicP.!D!8.ZZ!DPhcPZ!DZ!8!1!1FFPfcP!@!D!8d-+,,FPecP!F!)!8!G,+d,+FP ccC:!F!$F!G9+9+9,FP"
DATA " c!5!$:!BFF!Gb,+b--!/Pc!B!5!$!E!+FF!G-Fe!/!%PP!+!.!=!E!FFFg!/!5!BcP!F!.C!EPF<!/<PP!.H!.ccP!F!5C!5!$"
DATA "dP  PH!.dcP!B!5!*!.Pf PPecP!BH!.P gc"
DATA ""
DATA 16, 16
DATA "~cicdPkcdP!D!D.PicP.!D!8.ZZ!DPhcPZ!DZ!8!1!1FFPfcP!@!D!8d-+,,FPecP!B!)!8!G,+d,+FP cc!$!E!B!$F!G9+9+9,"
DATA "FP c!.C!E!FFF!Gb,+b--!/Pc!F!.C:PFF!G-Fe!/[PP!F!5!):!BFFg!/!EPcP!B!5!$:!+F<!/<PP!%a!%ccP!B!.!$!EeP  P"
DATA "P!7dcP!F!.!%!%Pf PPecP!F!%!7P gc"
DATA ""
DATA 16, 16
DATA "~cicgPjcP!D!Dd.ZPicP.Z.Z..PgcPP!(gb1PPdcP!*!.!7,+,,+9!7P!%PccP!)!.!7,,++,+d!%P cP!=!5!F,+,9,9!.H!$P "
DATA "cP!)!5!7,,++,+!%!EaP cPH!.!7-!Gd-!G!7dP cP!=!+Pg<!F!E!$P cP!=!5!F<F<F<F!.:!$P cP!5!B!.gP!5!B!.P cP!5"
DATA "!)!FPe P!+!$!.P ccdPgcdPcc"
DATA ""
DATA 16, 16
DATA "~cicgPjcP!D!Dd.ZPicP.Z.Z..PgcPP!(gb1PPdcP:!5P,+,,+9!F!E!EPccP!=!5!F,,++,+!FH!EP cPH!.!7,+,9,9!7dP cP"
DATA "!=!.P,,++,+!.!%!)P cP!=!5!F-!Gd-!G!.H!$P cP!)!.!7g<!%!%PP cPH!.!7<F<F<F!7!EPP cP!.!F!%gP!.!F!7P cP!."
DATA "P!7Pe P!Fa!%P ccdPgcdPcc"
DATA ""
DATA 16, 16
DATA "~clcdPmcPd.dPicP8..Z!D!8!1PgcP-!G!8!D!D.b!1PfcP-9,+-F!G!8!1!=PdcP-+d,+$<b!B!%PccP-+,9+93<!/!+!%!+!)c"
DATA "P!G!G-!1,,$<!/!/!.!5P!.P!.dF-!G!G<!/P!.!.P!F!.!(:gF!/!/!%!.!)!%!.P!B!$dPF-FP!%!.!)!7!%P !)!)P  dPC!="
DATA "!)!.!%P cPPfcP!+!$!F!.P jcP!$!$!.P dc"
DATA ""
DATA 16, 16
DATA "~clcdPmcPd.dPicP8..Z!D!8!1PgcP-!G!8!D!D.b!1PfcP-9,+-F!G!8!1!=PdcP-+d,+$<b!B!.PccP-+,9+93<!/!+!E!.PcP"
DATA "!G!G-!1,,$<!/!/!%!.!)!7P!5dF-!G!G<!/P!%!5!)!7!%!+HgF!/!/!.!5P!.!%P!.P!7PPF-FP!.!+P!F!.P PaP  dP!$UP"
DATA "!7!.P cPPfcP!.P!7!%P jcdP!%P dc"
DATA ""
DATA 16, 16
DATA "~c~crcePdcjP00^!IPcPe,+,,//0^0!IPP!=9,,+,,+/X00!I!#:P!B//Dd/D//!I5_A!5P!+!2!4!2!4!2!4!2!4!2!I!I!#!,"
DATA "!BcP`d!4`d!4`!#!IAW!5P:!F!.!+!F!F!+!F!+!+!.!+!+P!(PCH!E!5!EH:H:!5!E!5:!F!BP!BPP!)aP!)P!)!)P!)!)!%PcP"
DATA "!7!%!F!%!7!.!7!.!F!%!F!.P ccmPcc"
DATA ""
DATA 16, 16
DATA "~c~crcePdcjP00^!IPcPe,+,,//0^0!IPPH9,,+,,+/X00!I!#:P!F//Dd/D//!I5_A!5P!F!2!4!2!4!2!4!2!4!2!I!I!#!,!B"
DATA "cP`d!4`d!4`!#!IAW!5P!E!+!+!F!+!+!.!+!F!F!+!.!.!)!(P!B!5:H:!5!E!5!EH:H!EU!BP!F!)!)P!)!)P!)aP!)Pa!.PcP"
DATA "!F!.!7!.!F!%!F!%!7!.!7!%P ccmPcc"
DATA ""
DATA 16, 16
DATA "~cycePjcdP/9+/PhcPDb,,+,,XPgcP/D9+9d,/PecP!+!2/D,,+,!/00Pdc!$!E!+!2/DX0^0^!I5Pcc!.C!E!F!2/D00^!#!,*"
DATA "!#Pc!F!.C:P!2!2^!Ib)b)!#[PP!F!5!):!B!2^!Ibb!,!,_!%!%cP!B!5!$:!BP!#!,W!$!=!(!@!7ccP!+!.!$!.:CC!(d!B!+"
DATA "!7dcP!F!.!%!7!5!B!$!B!$!BPPecP!F!%!7!+!BePcc"
DATA ""
DATA 16, 16
DATA "~cycePjcdP/9+/PhcPDb,,+,,XPgcP/D9+9d,/PecP!F!2/D,,+,!/00PdcC:P!2/DX0^0^!I5Pcc!5!$:!B!2/D00^!#!,*!#Pc"
DATA "!B!5!$!E!+!2!2^!Ib)b)!#!%PP!+!.!=!E!F!2^!Ibb!,!,_!E!EcP!F!.C!E!FP!#!,W!$!=!(!@!FccP!F!5C!5:CC!(d!B!+"
DATA "!.dcP!B!5!EP!5!B!$!B!$!BPPecP!B!E!F!+!BePcc"
DATA ""
DATA 16, 16
DATA "~cycgPjcP,9+9,9PgcdP,,+d,dPdcP:!5!F+9d,9!.H!)PccP!=!+P+,+d,!F!E!$PccPH!.!70^0^00!7dP cP!=!.Pf0!I!.!%"
DATA "!)P cP!=!5!F!I7!87!8!,!.H!EP cP!)!.!7!I!87!8!8_d!%P cPH!.!%!I!I!#!IAA!7!EPP cP!.Ph!(1P!7P cP!.!%:!5:"
DATA "!5:!5:!5!7!%P ccPPi!+PP c"
DATA ""
DATA 16, 16
DATA "~cycgPjcP,9+9,9PgcdP,,+d,dPdcP!*!.!7+9d,9!%dPccP!)!.!7+,+d,!%!%PPccP!=!5!F0^0^00!.H!$P cP!)!5!7f0!I"
DATA "!%!EaP cPH!.!7!I7!87!8!,!7P!%P cP!=!5!F!I!87!8!8_!FH!EP cP!=!5!.!I!I!#!IAA!.:!$P cP!5!)h!(1!$!.P cP"
DATA "!5!E:!5:!5:!5:!5!.!.P ccPPi!+PP c"
DATA ""
DATA 16, 16
DATA "~c{cePlcPD9+!2dPhcPDd,+,$!4PfcPD9++,9$!4`PecP0!I!/+,,3!4``!EPccP!I50^00/!2`!,!.!.PcP^*^00^!#!4`!,!%"
DATA "!.!)!7P!5^!D*!D*!I!#`!,!%!5!)!7!%!+H!I!#!I!D!D!#!#`!.HP!.!%PP!@CC!=!I!I!#P!E!+P!F!.P P!(dC1!(!(HUP!7"
DATA "!.P cPP!=dC!=1!%P!7!%P ecePC!(!7P!%P dc"
DATA ""
DATA 16, 16
DATA "~c{cePlcPD9+!2dPhcPDd,+,$!4PfcPD9++,9$!4`PecP0!I!/+,,3!4``!%PccP!I50^00/!2`!,!%!+!)cP^*^00^!#!4`!,!."
DATA "!5P!.P!.^!D*!D*!I!#`!,!.!.P!F!.!(:!I!#!I!D!D!#!#`!%!%!)!%!.P!6!@CC!=!I!I!#P!%!.!)!7!%P H!(dC1!(!(H!="
DATA "!)!.!%P cPP!=dC!=1!.!$!F!.P ecePC!(!.!$!.P dc"
DATA ""
DATA 16, 16
DATA "~c~chcePlcP0!I00jPccP!I^0^!4Xd,9++bPc!5e0!4/+,,+,+,!.P!(^!I!I!#XX/Dd/D/!FPCe!I!4!2!4!2!4!2!4!2!4!.P"
DATA "!(!I!I!#!#!4`d!4`d!4P !(!.!+!5!F!+!+!.!+!F!F!+!F!F!)P!5!B!5:H:!5!E!5!EH:H!EUPP!F!)!)P!)!)P!)aP!)Pa!."
DATA "PcP!F!.!7!.!F!%!F!%!7!.!7!%P ccmPcc"
DATA ""
DATA 16, 16
DATA "~c~chcePlcP0!I00jPccP!I^0^!4Xd,9++bPc!5e0!4/+,,+,+,!%P!(^!I!I!#XX/Dd/D/!7PCe!I!4!2!4!2!4!2!4!2!4!7P"
DATA "!(!I!I!#!#!4`d!4`d!4P !(!5!.!F!+!F!F!+!F!+!+!.!+!+PP!5CH!E!5!EH:H:!5!E!5:!FPP!BPP!)aP!)P!)!)P!)!)!%P"
DATA "cP!7!%!F!%!7!.!7!.!F!%!F!.P ccmPcc"
DATA ""
DATA 16, 16
DATA "~cicdPkcdP!I!I0PicP^!I50^^!IPhcP^!I^!I__XXPfcP!@!I!IdX+,,/PecP!F!)!ID,+d,+XP ccC:!F!$/D9+9+9,/P c!5"
DATA "!$:!B!2/Db,+b//`Pc!B!5!$!E!+!2/DXX``!4`!%PP!+!.!=!E!F!2/g`!5!BcP!F!.C!EP!2!4`!4PP!.H!.ccP!F!5C!5!$dP"
DATA "  PH!.dcP!B!5!*!.Pf PPecP!BH!.P gc"
DATA ""
DATA 16, 16
DATA "~cicdPkcdP!I!I0PicP^!I50^^!IPhcP^!I^!I__XXPfcP!@!I!IdX+,,/PecP!B!)!ID,+d,+XP cc!$!E!B!$/D9+9+9,/P c"
DATA "!.C!E!F!2/Db,+b//`Pc!F!.C:P!2/DXX``!4`[PP!F!5!):!B!2/g`!EPcP!B!5!$:!+!2!4`!4PP!%a!%ccP!B!.!$!EeP  PP"
DATA "!7dcP!F!.!%!%Pf PPecP!F!%!7P gc"
DATA ""
DATA 16, 16
DATA "~cicgPjcP!I!Id0^PicP0^0^00PgcPP!(g!#1PPdcP!*!.!7,+,,+9!7P!%PccP!)!.!7,,++,+d!%P cP!=!5!F,+,9,9!.H!$P"
DATA " cP!)!5!7,,++,+!%!EaP cPH!.!7/Xd/X!7dP cP!=!+P!2!2!4d!2!F!E!$P cP!=!5!F!2!4!2!4!2!4!.:!$P cP!5!B!.gP"
DATA "!5!B!.P cP!5!)!FPe P!+!$!.P ccdPgcdPcc"
DATA ""
DATA 16, 16
DATA "~cicgPjcP!I!Id0^PicP0^0^00PgcPP!(g!#1PPdcP:!5P,+,,+9!F!E!EPccP!=!5!F,,++,+!FH!EP cPH!.!7,+,9,9!7dP c"
DATA "P!=!.P,,++,+!.!%!)P cP!=!5!F/Xd/X!.H!$P cP!)!.!7!2!2!4d!2!%!%PP cPH!.!7!2!4!2!4!2!4!7!EPP cP!.!F!%gP"
DATA "!.!F!7P cP!.P!7Pe P!Fa!%P ccdPgcdPcc"
DATA ""
DATA 16, 16
DATA "~clcdPmcPd0dPicPd0^!I5!,PgcP/0d!I0!#APfcPD9,+//D!#!,!=PdcPD+d,+$!4!#!B!%PccPD+,9+93!2`!+!%!+!)cPDD/"
DATA "!1,,$!4`!,!.!5P!.P!./XX/0D!4`!,!.!.P!F!.!(:gX``!%!.!)!%!.P!B!$dP!2!2X!,!%!.!)!7!%P !)!)P  dPC!=!)!."
DATA "!%P cPPfcP!+!$!F!.P jcP!$!$!.P dc"
DATA ""
DATA 16, 16
DATA "~clcdPmcPd0dPicPd0^!I5!,PgcP/0d!I0!#APfcPD9,+//D!#!,!=PdcPD+d,+$!4!#!B!.PccPD+,9+93!2`!+!E!.PcPDD/!1"
DATA ",,$!4`!,!%!.!)!7P!5/XX/0D!4`!,!%!5!)!7!%!+HgX``!.!5P!.!%P!.P!7PP!2!2X!,!.!+P!F!.P PaP  dP!$UP!7!.P c"
DATA "PPfcP!.P!7!%P jcdP!%P dc"
DATA ""
DATA 16, 16
DATA "ycPocP!8PmcPZZ!DfPgcP-!8!Db!$!)!)!$!)PfcP!G!=!B!/!%!%PP!FPecP-!G!5:!/<ePgcP-!5!5!/PPicP!.P!/!1PaPicP"
DATA "!)gPcd ecPH!FPP!FaPe ecP!)dP!FPf ecPH!FP P cc hcPP~cmc"
DATA ""
DATA 16, 16
DATA "ycPocP!8PmcPZZ!DfPgcP-!8!Db!$!)!)!$!)PfcP!G!=!B!/!%!%PP!FPecP-!G!5:!/<ePfcPH-!5!5!/PjcPHa!/!1!%!%Pic"
DATA "P!)dP!FPPcd ecPHaPP!.aPe fceP!.PPe gcPcP!.aPd kcPP~cic"
DATA ""
DATA 16, 16
DATA "~ckcdPmcPJ!D!8PkcPZ!D!DbPPicP!@C!1b!/!/PPhcP!@U-<<PP!.PgcP!+FF<!/!/!.!$!FPfcP!F!/!/<!%!%P!)!$PfcP!."
DATA "!%PP!F!FeP ecP!FdPHPP P  ecP!F!%PPH!FPe fcPPcPHPP mcPP~cic"
DATA ""
DATA 16, 16
DATA "~ckcdPmcPJ!D!8PkcPZ!D!DbPPicP!@C!1b!/!/PPhcP!@U-<<PP!.PgcP!+FF<!/!/!.!$!FPgcP!1!/!1dP!)!$PgcPHP!.PPc"
DATA "dP fcP!EP!FPP  P  fcPHdPg fcP!EPPe icPHPPncPPyc"
DATA ""
DATA 16, 16
DATA "~cjcePlcP.J!8bPjcP!@!8!DZbPjcP!=!5bb!/!/PicP-!G-F<!1PPhcPFF<<!/!)!$PhcPH!/!/!1!%!)!$PhcPH!%PP!.!$!$P"
DATA "hcPHdP!.!F!FPd ecP!.!%PP!EPPe fcPPcP!.aPd kcPP~cic"
DATA ""
DATA 16, 16
DATA "~cjcePlcP.J!8bPjcP!@!8!DZbPjcP!=!5bb!/!/PicP-!G-F<!1PPhcPFF<<!/!)!$PhcP!.!%d!/!)!$PhcPHeP!$!$PhcPH!%"
DATA "dP!F!FPd ecPHgPe ecPHaP PPkcPP~cmc"
DATA ""
DATA 16, 16
DATA "~cjcdPmcP.Z!8PkcP1!D!8!8bPjcP!G!G!1b<!/PicP-!G--<!/PicPFF<!/H!)PjcPH!/!$!$aPjcPH!$!$PPe gcP!E!EdPf "
DATA "gcPP!EPPf hcP!.PPf hcP!EPPccd icPPccd rc"
DATA ""
DATA 16, 16
DATA "~cjcdPmcP.Z!8PkcP1!D!8!8bPjcP!G!G!1b<!/PicP-!G--<!/PicP<F<!/H!)PicP!.!%!/!$!$aPicP!.!%!$!$dPd fcP!F"
DATA "!%!EPPaPe ecP!FeP!FPe ecP!F!%PcPPdc  fcPPgcd mcd rc"
DATA ""
DATA 16, 16
DATA "xcPocPZPmcP8!DbPlcP!Db!1!/PkcP-!GF<PgceP--!GF<<PecP!$!$d!)HF<!/PPecPd!F!.d!F!1PaPfceP!FgPcd ecP!F!FP"
DATA "P!FaPe ecPHdP!FPf ecPH!FP Pf gcPP~cmc"
DATA ""
DATA 16, 16
DATA "xcPocPZPmcP8!DbPlcP!Db!1!/PkcP-!GF<PgceP--!GF<<PecP!$!$d!)HF<!/PfcPd!F!.d!F!1!%!%PfceP!)gPcd ecPHaPP"
DATA "!.aPe fceP!.PPe gcP P!.aPd kcPP~cic"
DATA ""
DATA 16, 16
DATA "xcPPjcPdcP.!DPhcP!.PcP.Zb!1PgcP!)!$PP!Db!1!/1PfcP!)!$!)-!G!GF<!/PgcP!)Hd-d<PhcP!)dF<!/!/PicP!F!/!/<"
DATA "!%!%PicP!.!%PP!F!FPdc ecP!FdPHPPe ecP!F!%PPH!FPe fcPPcPHPP mcPP~cic"
DATA ""
DATA 16, 16
DATA "xcPPjcPdcP.!DPhcP!.PcP.Zb!1PgcP!)!$PP!Db!1!/1PfcP!)!$!)-!G!GF<!/PgcP!)Hd-d<PhcP!)dF<!/!/PicPP!1!/!1P"
DATA "PkcPHePcc c fcP!EP!FPPf fcPHdPg fcP!EPPe icPHPPncPPyc"
DATA ""
DATA 16, 16
DATA "xcPPlcdP.!8PjcP!$!$.!@!8!1PicP!$!)Zbb!1:PhcPH-!G!G-!=!=PhcP!F-!G-F<!/PicPdF<!/!/PicPH!/!/!1!%PPicPH"
DATA "!%PP!EPPdc ecPHdP!.aPe ecP!F!%PP!EPPe fcPPcP!.aPd kcPP~cic"
DATA ""
DATA 16, 16
DATA "xcPPlcdP.!8PjcP!$!$.!@!8!1PicP!$!)Zbb!1:PhcPH-!G!G-!=!=PhcP!F-!G-F<!/PicPdF<!/!/PicP!.a!/<!/!%PicPH"
DATA "gPdc ecPHadPaPe ecPHgPe ecPHaP PPkcPP~cmc"
DATA ""
DATA 16, 16
DATA "xcPPncP.!8PlcP.J!8!1PjcP!$!G!8!1bPjcP-!G.-C!@PicP-!G-F!=!+PicPdF<!/!+Pc  gcPH!/!1!/Pc  hcPH!%!EPP  c"
DATA " gcP!E!%!.PPf gcPP!EPPf hcP!.PPd jcP!EPPncPPwc"
DATA ""
DATA 16, 16
DATA "xcPPncP.!8PlcP.J!8!1PjcP!$!G!8!1bPjcP-!G.-C!@PicP-!G-F!=!+PicP<FF<!/!+Pc  fcP!.!%!/<!/!%P  gcP!.!%fP"
DATA " c fcP!F!%dPaPe ecP!FeP!FPe ecP!F!%PcPPkcPP~cmc"
DATA ""
DATA 16, 16
DATA "ycPocP!IPmcP^^!IfPgcP/!I!I!#!$!)!)!$!)PfcPD!=!B`!%!%PP!FPecP//!5:`!4ePgcPX!5!5`PPicP!.P`!4PaPicP!)gP"
DATA "cd ecPH!FPP!FaPe ecP!)dP!FPf ecPH!FP P cc hcPP~cmc"
DATA ""
DATA 16, 16
DATA "ycPocP!IPmcP^^!IfPgcP/!I!I!#!$!)!)!$!)PfcPD!=!B`!%!%PP!FPecP//!5:`!4ePfcPHX!5!5`PjcPHa`!4!%!%PicP!)"
DATA "dP!FPPcd ecPHaPP!.aPe fceP!.PPe gcPcP!.aPd kcPP~cic"
DATA ""
DATA 16, 16
DATA "~ckcdPmcP^!I!IPkcP^!I!I!#PPicP!@C!,!#``PPhcP!@UXX!4!,P!.PgcP!+X!2!4``!.!$!FPfcP!F``!4!%!%P!)!$PfcP!."
DATA "!%PP!F!FeP ecP!FdPHPP P  ecP!F!%PPH!FPe fcPPcPHPP mcPP~cic"
DATA ""
DATA 16, 16
DATA "~ckcdPmcP^!I!IPkcP^!I!I!#PPicP!@C!,!#``PPhcP!@UXX!4!,P!.PgcP!+X!2!4``!.!$!FPgcP!,`AdP!)!$PgcPHP!.PPc"
DATA "dP fcP!EP!FPP  P  fcPHdPg fcP!EPPe icPHPPncPPyc"
DATA ""
DATA 16, 16
DATA "~cjcePlcP0^!I!#PjcP!@!I!I^!#PjcP!=!5_!#``PicP/DX!2!4APPhcPXX!2!4!,!)!$PhcPH``A!%!)!$PhcPH!%PP!.!$!$P"
DATA "hcPHdP!.!F!FPd ecP!.!%PP!EPPe fcPPcP!.aPd kcPP~cic"
DATA ""
DATA 16, 16
DATA "~cjcePlcP0^!I!#PjcP!@!I!I^!#PjcP!=!5_!#``PicP/DX!2!4APPhcPXX!2!4!,!)!$PhcP!.!%`!4`!)!$PhcPHeP!$!$Phc"
DATA "PH!%dP!F!FPd ecPHgPe ecPHaP PPkcPP~cmc"
DATA ""
DATA 16, 16
DATA "~cjcdPmcP0^!#PkcP1!I!I5!#PjcPDD!,!#!4`PicP/D/X!2`PicP!2X!2`H!)PjcPH`!$!$aPjcPH!$!$PPe gcP!E!EdPf gcP"
DATA "P!EPPf hcP!.PPf hcP!EPPccd icPPccd rc"
DATA ""
DATA 16, 16
DATA "~cjcdPmcP0^!#PkcP1!I!I5!#PjcPDD!,!#!4`PicP/D/X!2`PicP!2X!2`H!)PicP!.!%`!$!$aPicP!.!%!$!$dPd fcP!F!%"
DATA "!EPPaPe ecP!FeP!FPe ecP!F!%PcPPdc  fcPPgcd mcd rc"
DATA ""
DATA 16, 16
DATA "xcPocP^PmcP0!I!#PlcP!#!#A`PkcPDDX!4PgcePX/D!2!4!4PecP!$!$d!)H!2!4`PPecPd!F!.d!FAPaPfceP!FgPcd ecP!F"
DATA "!FPP!FaPe ecPHdP!FPf ecPH!FP Pf gcPP~cmc"
DATA ""
DATA 16, 16
DATA "xcPocP^PmcP0!I!#PlcP!#!#A`PkcPDDX!4PgcePX/D!2!4!4PecP!$!$d!)H!2!4`PfcPd!F!.d!FA!%!%PfceP!)gPcd ecPHa"
DATA "PP!.aPe fceP!.PPe gcP P!.aPd kcPP~cic"
DATA ""
DATA 16, 16
DATA "xcPPjcPdcP0!IPhcP!.PcP06!#!,PgcP!)!$PP!I!#A`1PfcP!)!$!)/^DX!2`PgcP!)H//XX!4!4PhcP!)XX!2!4``PicP!F``"
DATA "!4!%!%PicP!.!%PP!F!FPdc ecP!FdPHPPe ecP!F!%PPH!FPe fcPPcPHPP mcPP~cic"
DATA ""
DATA 16, 16
DATA "xcPPjcPdcP0!IPhcP!.PcP06!#!,PgcP!)!$PP!I!#A`1PfcP!)!$!)/^DX!2`PgcP!)H//XX!4!4PhcP!)XX!2!4``PicPP!,`A"
DATA "PPkcPHePcc c fcP!EP!FPPf fcPHdPg fcP!EPPe icPHPPncPPyc"
DATA ""
DATA 16, 16
DATA "xcPPlcdP0!#PjcP!$!$06!#!,PicP!$!)^!#!,!,:PhcPH/^DX!=!=PhcP!F/D/!2!2`PicPdX!4``PicPH``A!%PPicPH!%PP!E"
DATA "PPdc ecPHdP!.aPe ecP!F!%PP!EPPe fcPPcP!.aPd kcPP~cic"
DATA ""
DATA 16, 16
DATA "xcPPlcdP0!#PjcP!$!$06!#!,PicP!$!)^!#!,!,:PhcPH/^DX!=!=PhcP!F/D/!2!2`PicP!2XX!4``PicP!.a`!4`!%PicPHgP"
DATA "dc ecPHadPaPe ecPHgPe ecPHaP PPkcPP~cmc"
DATA ""
DATA 16, 16
DATA "xcPPncP0!#PlcP00!I_PjcP!$Dd!#PjcP/D^/C!@PicP/D/X!=!+PicP!2X!2!2`!+Pc  gcPH`!,`Pc  hcPH!%!EPP  c gcP"
DATA "!E!%!.PPf gcPP!EPPf hcP!.PPd jcP!EPPncPPwc"
DATA ""
DATA 16, 16
DATA "xcPPncP0!#PlcP00!I_PjcP!$Dd!#PjcP/D^/C!@PicP/D/X!=!+PicP!2X!2!2`!+Pc  fcP!.!%`!4`!%P  gcP!.!%fP c fc"
DATA "P!F!%dPaPe ecP!FeP!FPe ecP!F!%PcPPkcPP~cmc"
DATA ""
DATA 16, 16
DATA "~cxcgPjcPZ.Z.Z.PgcdP.Z..!D!D.gP:-FZ.!8!DZ.!1d1!@!(P!=-FZ!8b!Dbb!1!/FF!/PP:!G--bb!8!8!/F-!G-!/PPF<F<F"
DATA "<F<F<F<F!/PP-!/d<!/d<!/e<PP!5!F!F!B!F!F!+!F!+!+!F!+!+!FPPC!*H!=H!*:!*:!=H!=:!.PP!BPP!)aP!)P!)!)P!)!)"
DATA "!%PP!E!7!%!.!%!7!.!7!.!.!%!.!.!7PcoPc"
DATA ""
DATA 16, 16
DATA "~cxcgPjcPZ.Z.Z.PgcdP.Z..!D!D.gP:-FZ.!8!DZ.!1d1!@!(P!=-FZ!8b!Dbb!1!/FF!/PP:!G--bb!8!8!/F-!G-!/PPF<F<F"
DATA "<F<F<F<F!/PP-!/d<!/d<!/e<PP!.!+!B!F!B!B!F!B!F!F!B!F!F!BPP!B!=:!*:!=H!=H!*:!*H!+PP!F!)!)P!)!)P!)aP!)P"
DATA "a!EPP!%!.!.!7!.!.!%!.!%!7!.!7!%!FPcoPc"
DATA ""
DATA 16, 16
DATA "~cjcePkcPPd.bPhcPPZ.Z.Z.!DPfcP!G-.Z..!D!D.PfcPF!G.Z!8!DZZb!.PdcP!BF-ZZb!D!81!1<!%Pcc!)!.!$FF!GF-F!@1"
DATA "-!G!%Pc!E!=!.!FF-!G--F;;FF!.P!.!E!=!5PFF!GF<<;!5!/H!.P!.!E!=!5!BFFg!/!%!.cP!.:!)!5!6F<!/dP!%H!.ccP!5"
DATA ":!$!.!FPPd P!$!.dcP!5!.!)!.Pf PPecP!F!E!.P gc"
DATA ""
DATA 16, 16
DATA "~cjcePkcPPd.bPhcPPZ.Z.Z.!DPfcP!G-.Z..!D!D.PfcPF!G.Z!8!DZZb!%PdcP!FF-ZZb!D!81!1<!.Pcc!=!5PFF!GF-F!@1-"
DATA "!G!.Pc:!$!5!$F-!G--F;;FF!%P!5:!)!.!$FF!GF<<;!5!/P!%P!5:!$!.PFFg!/!E!+cP!+!E!=!.PF<!/dP!EP!%ccP!.!E!="
DATA "!5!BPPd PP!7dcP!.!5a!%Pf PPecP!+!%!7P gc"
DATA ""
DATA 16, 16
DATA "~cjcePkcPPe.PPhcP-.Z.Z.!D!/PfcPPZZ..!D!D.bPPdcP:!+ZZ!8!DZ.!8b!E!+PccP!=!+!GZb1!+bb!/!EHPccP!*!Fd-!@:"
DATA "FF!/!%!%P cPH!F!G--!@!5F-!/!%PP cP:!+d-!@:!/F!/!.!+P cPH!.-<<:!5d!/!%!FP cP!*!.!7!/<!/<!/!/!7!F!FP c"
DATA "P:!+!7gPaH!$P cP!+!)!FPe P!+!.!.P ccdPgcdP c"
DATA ""
DATA 16, 16
DATA "~cjcePkcPPe.PPhcP-.Z.Z.!D!/PfcPPZZ..!D!D.bPPdcP!*!FZZ!8!DZ.!8b!%!FPccPH!.!GZb1!+bb!/!%PPccP:!+d-!@:F"
DATA "F!/!.HP cP!=!+!G--!@!5F-!/!E!EP cP!*!Fd-!@:!/F!/!%!FP cP!=!+-<<:!5d!/!E!+P cP:!+!F!/<!/<!/!/!.!5!+P "
DATA "cP!E!.!.gPHP!FP cdP!7Pe P!.!%!%P ccdPgcdP c"
DATA ""
DATA 16, 16
DATA "~cjcePlcP!Dd.PPicP.Z.Z.!D!DPPgcPZ..!D!D..<<PecP!=Z!D!DZ.!8b<!/PdcPHF!D!(!8!8db!/!/!.PcPH-F1!$!/-FF!/"
DATA "!1!.P!%P!=!G!G!@!+!/--<!/!/!7P!E!7!=!5-1!5!/b!G<!/!1!7!5!B!7!%!B!F<dF<F!/!/!.!5!B!%!%P!B!$!7dP<<!1!."
DATA "!5!F!F!%P !BHPd PP!$!E!F!.!.P cPPfcP!B!+!7!.P jcP!B!B!%P dc"
DATA ""
DATA 16, 16
DATA "~cjcePlcP!Dd.PPicP.Z.Z.!D!DPPgcPZ..!D!D..<<PecPHZ!D!DZ.!8b<!/PdcP!=F!D!(!8!8db!/!/!%PcP!=-F1!$!/-FF"
DATA "!/!1!%!E!EPH!G!G!@!+!/--<!/!/!.!E!%!.H!.-1!5!/b!G<!/!1!.!.!F!F!.C!+<dF<F!/!/!%!.!F!.!.P!.P!FdP<<!1!%"
DATA "!.!B!7!.P !F!%Pd PP!=!=!B!7!%P cPPfcPd!F!%P jcP!FP!.P dc"
DATA ""
DATA 16, 16
DATA "~czcgPjcPZ.Z.!D!DPdcfPZ..!D!D..dPcC1!@1!(Z!8!DZ.Z.<<!+PP!/dF!D!8!D!D!8bb!/<:PP-!G-!G-F!D!D!8b!/F-!+P"
DATA "P-<F<F<F<F<F<F!/PP-!/d<!/d<!/d<!/PP!.!+!B!F!B!B!F!B!F!F!+!F!F!BPP!B!=:!*:!=H!=H!*:!*H!+PP!F!)!)P!)!)"
DATA "P!)aP!)Pa!EPP!%!.!.!7!.!.!%!.!%!7!.!7!%!FPcoPc"
DATA ""
DATA 16, 16
DATA "~czcgPjcPZ.Z.!D!DPdcfPZ..!D!D..dPcC1!@1!(Z!8!DZ.Z.<<!+PP!/dF!D!8!D!D!8bb!/<:PP-!G-!G-F!D!D!8b!/F-!+P"
DATA "P-<F<F<F<F<F<F!/PP-!/d<!/d<!/d<!/PP!5!F!F!+!F!F!+!F!B!B!F!+!+!FPPC!*H!=H!*:!*:!=H!=:!.PP!BPP!)aP!)P"
DATA "!)!)P!)!)!%PP!E!7!%!.!%!7!.!7!.!.!%!.!.!7PcoPc"
DATA ""
DATA 16, 16
DATA "tcPPncP;8PncP;!@fPhcdP!@Z.Z.!DPfcP!G--Z..!D!D..PecPF!GF.!8!DZ.Z.PdcP!FF-!G!8Z..!8bb!.Pcc!=!5PFF!8!D"
DATA "!Dbb!1!/F!.Pc:!$!5!BF-!GFFe-F!%P!5:!)!.!$FF!GFF!=:!E!.P!%P!5:!$!.PFF!+!Ed!.!E!E!+cP!+!E!=!.PF:!.dP!E"
DATA "P!%ccP!.!E!=!5!BPPd PP!7dcP!.!5a!%Pf PPecP!+!%!7P gc"
DATA ""
DATA 16, 16
DATA "tcPPncP;8PncP;!@fPhcdP!@Z.Z.!DPfcP!G--Z..!D!D..PecPF!GF.!8!DZ.Z.PdcP!BF-!G!8Z..!8bb!%Pcc!)!.!6FF!8!D"
DATA "!Dbb!1!/F!%Pc!E!=!.PF-!GFFe-F!.P!.!E!=!5PFF!GFF!=:!E!.H!.P!.!E!=!5!$FF!+!Ed!.!E!%!.cP!.:!)!5!$F:!.dP"
DATA "!%!6!.ccP!5:!$!.dPd P!$!.dcP!5!.!)!.Pf PPecP!F!E!.P gc"
DATA ""
DATA 16, 16
DATA "hcPPncP!@!5PmcP!@!5PkcdP!@!5dPhcP--Z.Z.!G!/PfcPP.Z..!D!D.bPPdcP!*!FZ.!8!DZ.Zb!%!FPccPH!.Z!8Z..db!%!F"
DATA "PccP:!+-b9!Dbb!1!/!.HP cP!=!+!G-dF--!/!E!EP cP!*!Fh-!/!%!%P cP!=!+-!)!)!B!B!)!)!/!E!+P cP:!+!F!)!)!B"
DATA "!$!)!)!.!E!+P cP!E!.!7gP!E!F!FP cdP!7Pe P!.a!%P ccdPgcdP c"
DATA ""
DATA 16, 16
DATA "hcPPncP!@!5PmcP!@!5PkcdP!@!5dPhcP--Z.Z.!G!/PfcPP.Z..!D!D.bPPdcP:!+Z.!8!DZ.Zb!E!+PccP!=!+Z!8Z..db!E!+"
DATA "PccP!*!F-b9!Dbb!1!/!%!%P cPH!F!G-dF--!/!%PP cP:!+h-!/!.HP cPH!.-!)!)!B!B!)!)!/!%!FP cP!*!.!7!)!)!B!$"
DATA "!)!)!7P!FP cP:!+!.gP!5!+!$P cP!+!)!FPe P!+!$!.P ccdPgcdP c"
DATA ""
DATA 16, 16
DATA "|cPPncP!@!5PhcfP1!+PhcP.Z.Z.!$dPfcP.Z..!D!D.--<PecPZ.!8!DZ.Z<<!/PdcP!=Z!8Z.Zbbd!/!%PcP!=-Fb!D!8!8b!8"
DATA "!/!1!%!E!BPH!G!G--eF!/!/!.!E!%!.H!.!+!51CZ!G<!/!1!.P!%!F!.C!+!5!+!B!B!5!F!/!/!7!.!F!.!.P!.P!FdP!+!."
DATA "!1!%!.!B!7!.P !F!%Pd PP!=!=!B!7!%P cPPfcPd!F!%P jcP!FP!.P dc"
DATA ""
DATA 16, 16
DATA "|cPPncP!@!5PhcfP1!+PhcP.Z.Z.!$dPfcP.Z..!D!D.--<PecPZ.!8!DZ.Z<<!/PdcPHZ!8Z.Zbbd!/!.PcPH-Fb!D!8!8b!8!/"
DATA "!1!.!%!FP!=!G!G--eF!/!/!%P!E!7!=!5!+!51CZ!G<!/!1!7!E!E!7!%!B!F!5!+!B!B!5!F!/!/!.!5!B!%!%P!B!$!7dP!+"
DATA "!.!1!.!5!F!F!%P !BHPd PP!$!E!F!.!.P cPPfcP!B!+!7!.P jcP!B!B!%P dc"
DATA ""
DATA 16, 16
DATA "~cxcgPjcP^0^0^0PgcdP0^00!I!I0gP:XX^0!I!I^0_d1!@!(P!=/X^f!#_`XX`PP:/X!2!#_!I_`//D/`PP/m!2`PPXn!4PP!5"
DATA "!F!F!B!F!F!+!F!+!+!F!+!+!FPPC!*H!=H!*:!*:!=H!=:!.PP!BPP!)aP!)P!)!)P!)!)!%PP!E!7!%!.!%!7!.!7!.!.!%!."
DATA "!.!7PcoPc"
DATA ""
DATA 16, 16
DATA "~cxcgPjcP^0^0^0PgcdP0^00!I!I0gP:XX^0!I!I^0_d1!@!(P!=/X^f!#_`XX`PP:/X!2!#_!I_`//D/`PP/m!2`PPXn!4PP!."
DATA "!+!B!F!B!B!F!B!F!F!B!F!F!BPP!B!=:!*:!=H!=H!*:!*H!+PP!F!)!)P!)!)P!)aP!)Pa!EPP!%!.!.!7!.!.!%!.!%!7!.!7"
DATA "!%!FPcoPc"
DATA ""
DATA 16, 16
DATA "~cjcePkcPPd0!#PhcPP^0^0^0!IPfcPD/0^00!I!I0PfcP/D^^!I!I^^_!.PdcP!BX/^^!#!I!#1_`!%Pcc!)!.!$X/D!2X!2!@1"
DATA "/D!%Pc!E!=!.!FX/D//X;;X!2!.P!.!E!=!5PX/D/!2!4;!5`H!.P!.!E!=!5!BXXg`!%!.cP!.:!)!5!6!2!4`dP!%H!.ccP!5:"
DATA "!$!.!FPPd P!$!.dcP!5!.!)!.Pf PPecP!F!E!.P gc"
DATA ""
DATA 16, 16
DATA "~cjcePkcPPd0!#PhcPP^0^0^0!IPfcPD/0^00!I!I0PfcP/D^^!I!I^^_!%PdcP!FX/^^!#!I!#1_`!.Pcc!=!5PX/D!2X!2!@1/"
DATA "D!.Pc:!$!5!$X/D//X;;X!2!%P!5:!)!.!$X/D/!2!4;!5`P!%P!5:!$!.PXXg`!E!+cP!+!E!=!.P!2!4`dP!EP!%ccP!.!E!="
DATA "!5!BPPd PP!7dcP!.!5a!%Pf PPecP!+!%!7P gc"
DATA ""
DATA 16, 16
DATA "~cjcePkcPPe0PPhcP/0^0^0!I`PfcPP^^00!I!I0!#PPdcP:!+^^!I!I^0!#!#!E!+PccP!=!+D!I!#1!+__`!EHPccP!*!F/X!2"
DATA "!@:X/`!%!%P cPH!FD/X!@!5//`!%PP cP:!+/X!2!@:`/`!.!+P cPH!.X!4!4:!5d`!%!FP cP!*!.!7`!4`!4``!7!F!FP cP"
DATA ":!+!7gPaH!$P cP!+!)!FPe P!+!.!.P ccdPgcdP c"
DATA ""
DATA 16, 16
DATA "~cjcePkcPPe0PPhcP/0^0^0!I`PfcPP^^00!I!I0!#PPdcP!*!F^^!I!I^0!#!#!%!FPccPH!.D!I!#1!+__`!%PPccP:!+/X!2"
DATA "!@:X/`!.HP cP!=!+D/X!@!5//`!E!EP cP!*!F/X!2!@:`/`!%!FP cP!=!+X!4!4:!5d`!E!+P cP:!+!F`!4`!4``!.!5!+P "
DATA "cP!E!.!.gPHP!FP cdP!7Pe P!.!%!%P ccdPgcdP c"
DATA ""
DATA 16, 16
DATA "~cjcePlcP!Id0PPicP0^0^0!I!IPPgcP^00!I!I00!4!2PecP!=^5!I^0!#!#``PdcPHX!I!(!I!Id!#``!.PcPH/X1!$`/X!2`"
DATA "!,!.P!%P!=D^!@!+`X/!2``!7P!E!7!=!5!21!5`!#^X`!,!7!5!B!7!%!B!F!2dX!4X``!.!5!B!%!%P!B!$!7dP`!4!,!.!5!F"
DATA "!F!%P !BHPd PP!$!E!F!.!.P cPPfcP!B!+!7!.P jcP!B!B!%P dc"
DATA ""
DATA 16, 16
DATA "~cjcePlcP!Id0PPicP0^0^0!I!IPPgcP^00!I!I00!4!2PecPH^5!I^0!#!#``PdcP!=X!I!(!I!Id!#``!%PcP!=/X1!$`/X!2`"
DATA "!,!%!E!EPHD^!@!+`X/!2``!.!E!%!.H!.!21!5`!#^X`!,!.!.!F!F!.C!+!2dX!4X``!%!.!F!.!.P!.P!FdP`!4!,!%!.!B!7"
DATA "!.P !F!%Pd PP!=!=!B!7!%P cPPfcPd!F!%P jcP!FP!.P dc"
DATA ""
DATA 16, 16
DATA "~czcgPjcP^0^0!I!IPdcfP^00!I!I00dPcC1!@1!(^!I!I^0^0!4!4!+PP`dX!I!#!I!I!#!#_`!4:PP/D/XX!2!I!I!#_`//!+P"
DATA "PXm!2`PPXm!4`PP!.!+!B!F!B!B!F!B!F!F!+!F!F!BPP!B!=:!*:!=H!=H!*:!*H!+PP!F!)!)P!)!)P!)aP!)Pa!EPP!%!.!."
DATA "!7!.!.!%!.!%!7!.!7!%!FPcoPc"
DATA ""
DATA 16, 16
DATA "~czcgPjcP^0^0!I!IPdcfP^00!I!I00dPcC1!@1!(^!I!I^0^0!4!4!+PP`dX!I!#!I!I!#!#_`!4:PP/D/XX!2!I!I!#_`//!+P"
DATA "PXm!2`PPXm!4`PP!5!F!F!+!F!F!+!F!B!B!F!+!+!FPPC!*H!=H!*:!*:!=H!=:!.PP!BPP!)aP!)P!)!)P!)!)!%PP!E!7!%!."
DATA "!%!7!.!7!.!.!%!.!.!7PcoPc"
DATA ""
DATA 16, 16
DATA "tcPPncP;8PncP;!@fPhcdP!@^0^0!IPfcPD//^00!I!I00PecP/D/0!I!I^0^0PdcP!FX/D!I^00!#!#_!.Pcc!=!5PX/e!I!#!#"
DATA "`/!.Pc:!$!5!BX/DeX/X!2!%P!5:!)!.!$X/D/!2!=:!E!.P!%P!5:!$!.PXX!+!Ed!.!E!E!+cP!+!E!=!.P!2:!.dP!EP!%ccP"
DATA "!.!E!=!5!BPPd PP!7dcP!.!5a!%Pf PPecP!+!%!7P gc"
DATA ""
DATA 16, 16
DATA "tcPPncP;8PncP;!@fPhcdP!@^0^0!IPfcPD//^00!I!I00PecP/D/0!I!I^0^0PdcP!BX/D!I^00!#!#_!%Pcc!)!.!6X/e!I!#"
DATA "!#`/!%Pc!E!=!.PX/DeX/X!2!.P!.!E!=!5PX/D/!2!=:!E!.H!.P!.!E!=!5!$XX!+!Ed!.!E!%!.cP!.:!)!5!$!2:!.dP!%!6"
DATA "!.ccP!5:!$!.dPd P!$!.dcP!5!.!)!.Pf PPecP!F!E!.P gc"
DATA ""
DATA 16, 16
DATA "hcPPncP!@!5PmcP!@!5PkcdP!@!5dPhcP//^0^0D`PfcPP0^00!I!I0!#PPdcP!*!F^0!I!I^0^!#!%!FPccPH!.^!#!I00d!#!%"
DATA "!FPccP:!+/!#d!I_!#`!.HP cP!=!+DfX/`!E!EP cP!*!F//X/X/X`!%!%P cP!=!+X!)!)!B!B!)!)`!E!+P cP:!+!F!)!)!B"
DATA "!$!)!)!.!E!+P cP!E!.!7gP!E!F!FP cdP!7Pe P!.a!%P ccdPgcdP c"
DATA ""
DATA 16, 16
DATA "hcPPncP!@!5PmcP!@!5PkcdP!@!5dPhcP//^0^0D`PfcPP0^00!I!I0!#PPdcP:!+^0!I!I^0^!#!E!+PccP!=!+^!#!I00d!#!E"
DATA "!+PccP!*!F/!#d!I_!#`!%!%P cPH!FDfX/`!%PP cP:!+//X/X/X`!.HP cPH!.X!)!)!B!B!)!)`!%!FP cP!*!.!7!)!)!B!$"
DATA "!)!)!7P!FP cP:!+!.gP!5!+!$P cP!+!)!FPe P!+!$!.P ccdPgcdP c"
DATA ""
DATA 16, 16
DATA "|cPPncP!@!5PhcfP1!+PhcP0^0^0!$dPfcP0^00!I!I0X/!2PecP^0!I!I^0^!4X`PdcP!=^!#^0^!#!#!4``!%PcP!=X!2!#!I"
DATA "!I&!#!#`!,!%!E!BPHD^D/dX!2``!.!E!%!.H!.!+!51CD^X`!,!.P!%!F!.C!+!5!+!B!B!5!F``!7!.!F!.!.P!.P!FdP!+!."
DATA "!,!%!.!B!7!.P !F!%Pd PP!=!=!B!7!%P cPPfcPd!F!%P jcP!FP!.P dc"
DATA ""
DATA 16, 16
DATA "|cPPncP!@!5PhcfP1!+PhcP0^0^0!$dPfcP0^00!I!I0X/!2PecP^0!I!I^0^!4X`PdcPH^!#^0^!#!#!4``!.PcPHX!2!#!I!I&"
DATA "!#!#`!,!.!%!FP!=D^D/dX!2``!%P!E!7!=!5!+!51CD^X`!,!7!E!E!7!%!B!F!5!+!B!B!5!F``!.!5!B!%!%P!B!$!7dP!+!."
DATA "!,!.!5!F!F!%P !BHPd PP!$!E!F!.!.P cPPfcP!B!+!7!.P jcP!B!B!%P dc"
DATA ""

MapData:
DATA ".,T,..,.,.,,..,:.....,..,T..:T...,,:...."
DATA "..m...T....,..,.,..........m..,.......,."
DATA ",mmmm...,..........,,::...mm.......,...."
DATA ".mmmmm:,.,,,,.,..,...,T...mm...,,..,.,.."
DATA ".mmmmm...:....###..,,...T.mm..::::.....T"
DATA ",mmmmmT.....#T.#,,..,.....,m.......:...."
DATA ".mmmm.T.....#####T,.T.....,............."
DATA "~...m....TT,#####..m,m.,..T.....,...:..,"
DATA "~~~...T...T.####...mmmm.,,..mmmmm.,....."
DATA ".,~~,,,:....#,###,mmmmmm...mmmmmm.....,,"
DATA ",..~~~..T...,....mmmmmmm..:mmmmmm,.:...."
DATA ".....~~~,.....,..mmmmmmm:,,.mmmm.T.,...."
DATA "T,#####:...T.....,mmmm.,,......m.,.:..,."
DATA "..#####.,..,....:.#mmm..,...,..,........"
DATA "..###.#T..~~......T#.,..T..,.,....###..."
DATA ",,:..:...,:~~~:...##....,..T...:.##.##.,"
DATA ",....,...mm.T~~~..##..,T:.T.,.T.,#####.,"
DATA "......:mmmmm...~~.:,...:...:...::....::."
DATA "..,.:.mmmmmmm...~~~,....,,...,.:....T..."
DATA ":..,..mmmmmm....T...T.,....,:........:.."
DATA ".T....mmmmmmm...T..,~~.#####.T.,.....:.."
DATA ",......mmmm,..T.,.,:.~~#.#.#.,..:.,:...:"
DATA ",..:.:..,,...,,........###.#.:,...mmmm,."
DATA "..,...,...,...:,T..T...#####...::.mmmmmm"
DATA ",..,.,.......m..,..TT..#.#~#~..,Tmmmmmm."
DATA ".,....:....,mmm..,...,..:...~~,...mmmmmm"
DATA "....,.......mmm,.,.......,,,.~~~,:mmmmm."
DATA ":..:..,..,,.mmm.,........TT..:,~~..m:m.T"
DATA ".,..,...,....T:,:,.............:~~~:...."
DATA "..:............T:.,.,,,..,,,...T,.....,."

' The songs: each voice's settings, then a bar to a line (lead, bass, arpeggio), "" the end.
' Voice 1, the lead, is the one DOS's speaker plays. Then the battle's, four strings a bar: bass,
' lead, arpeggio, drums, on voices 2, 4, 5 and 6; voices 1 and 3 are the sound effects'.
Tunes:
DATA "T96MLW1H2,6,11,5V11", "T96MNW2Y1,500,6H0,7,8,4V9", "T96MSW0Q30H0,4,5,3V4"
DATA "O4A4E8A8O5C4O4B8A8", "O2A4A4O3C4O2A4", "O4A8O5C8E8C8O4A8O5C8E8C8"
DATA "O4G4E8G8B4A8G8", "O2G4G4B4G4", "O4G8B8O5D8O4B8G8B8O5D8O4B8"
DATA "O4F4A8O5C8D4C8O4A8", "O2F4F4A4F4", "O4F8A8O5C8O4A8F8A8O5C8O4A8"
DATA "O4E2.P4", "O2E4E4G#4B4", "O4E8G#8B8G#8E8G#8B8G#8"
DATA "O4A4O5C8E8D4C8O4B8", "O2A4A4O3C4O2A4", "O4A8O5C8E8C8O4A8O5C8E8C8"
DATA "O4A4F8A8G4F8E8", "O2F4F4A4F4", "O4F8A8O5C8O4A8F8A8O5C8O4A8"
DATA "O4D4F8A8G#4F8E8", "O2D4F4E4G#4", "O4D8F8A8F8E8G#8B8G#8"
DATA "O4A2.P4", "O2A4E4A2", "O4A8O5C8E8C8O4A2"
DATA "", "", ""
DATA "T150MLW1H1,5,10,4V12", "T150MNW2Y1,700,6H0,6,8,3V9", "T150MSW0Q30H0,3,5,2V5"
DATA "O4C8E8G8O5C4O4G8O5C4", "O2C4G4C4G4", "O4C8E8G8E8C8E8G8E8"
DATA "O5D8C8O4A8O5C8E2", "O2F4F4G4G4", "O4F8A8O5C8O4A8G8B8O5D8O4B8"
DATA "O5C2.P4", "O2C2.P4", "O4C8G8O5C8P8P2"
DATA "", "", ""
DATA "T80MLW1H3,8,9,6V11", "T80MNW2Y1,400,8H0,8,8,5V9", "T80MSW0Q25H0,5,4,4V4"
DATA "O4A4G8E8D4C8O3B8", "O2A4E4D4E4", "O3A8O4C8E8C8O3G#8B8O4E8O3B8"
DATA "O3A2.P4", "O2A2.P4", "O3A8O4C8E8P8P2"
DATA "", "", ""
DATA "T140MNW2Y1,500,7H0,7,7,4V8", "T140MNW0Q25H0,6,10,3V10", "T140MNW2Y1,900,9H0,3,9,2V5", "T140MNW3Y0H0,2,0,1"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "P1", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V10O1C16P16V4O6C32P32P16V10O1C16P16V4O6C32P32P16V10O1C16P16V4O6C32P32P16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "P1", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V10O1C16P16V4O6C32P32P16V10O1C16P16V4O6C32P32P16V10O1C16P16V4O6C32P32P16"
DATA "O2F8F8O3F8O2F8F8O3F8O2F8O3C8", "P1", "O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16", "V10O1C16P16V4O6C32P32P16V10O1C16P16V4O6C32P32P16V10O1C16P16V4O6C32P32P16V10O1C16P16V4O6C32P32P16"
DATA "O2G8G8O3G8O2G8G8O3G8O2G8O3D8", "P1", "O3G16O4D16G16D16O3G16O4D16G16D16O3G16O4D16G16D16O3G16O4D16G16D16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16V6O4C16V8O4C16V6O4C16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "O4A8O5C8E8O4A8O5C8E8A8E8", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "O5G8E8C8O4A8O5C8E8D8C8", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2F8F8O3F8O2F8F8O3F8O2F8O3C8", "O4F8A8O5C8F8E8C8O4A8O5C8", "O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2G8G8O3G8O2G8G8O3G8O2G8O3D8", "O4G8B8O5D8G8F8D8O4B8O5D8", "O3G16O4D16G16D16O3G16O4D16G16D16O3G16O4D16G16D16O3G16O4D16G16D16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "O5E8A8E8C8E8A8O6C8O5A8", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "O5G8E8G8A8G8E8D8C8", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2F8F8O3F8O2F8F8O3F8O2F8O3C8", "O5C8O4A8O5C8F8A8F8E8D8", "O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2E8E8O3E8O2E8E8O3E8O2E8B8", "O5E4O4B8O5E8G#4B4", "O3E16B16O4E16O3B16O3E16B16O4E16O3B16O3E16B16O4E16O3B16O3E16B16O4E16O3B16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16V6O4C16V8O4C16V6O4C16"
DATA "O3C8C8O4C8O3C8C8O4C8O3C8O2G8", "O5C8E8G8E8O6C8O5G8E8G8", "O4C16G16O5C16O4G16O4C16G16O5C16O4G16O4C16G16O5C16O4G16O4C16G16O5C16O4G16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2G8G8O3G8O2G8G8O3G8O2G8O3D8", "O5D8G8B8G8O6D8O5B8G8B8", "O3G16O4D16G16D16O3G16O4D16G16D16O3G16O4D16G16D16O3G16O4D16G16D16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2D8D8O3D8O2D8D8O3D8O2D8A8", "O5D8F8A8F8O6D8O5A8F8A8", "O3D16A16O4D16O3A16O3D16A16O4D16O3A16O3D16A16O4D16O3A16O3D16A16O4D16O3A16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "O5E8A8O6C8O5A8E8C8O4A8O5C8", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2F8F8O3F8O2F8F8O3F8O2F8O3C8", "O5F8E8F8A8O6C8O5A8F8A8", "O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O3C8C8O4C8O3C8C8O4C8O3C8O2G8", "O5E8D8E8G8O6C8O5G8E8G8", "O4C16G16O5C16O4G16O4C16G16O5C16O4G16O4C16G16O5C16O4G16O4C16G16O5C16O4G16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2E8E8O3E8O2E8E8O3E8O2E8B8", "O5E8G#8B8G#8O6E8O5B8G#8B8", "O3E16B16O4E16O3B16O3E16B16O4E16O3B16O3E16B16O4E16O3B16O3E16B16O4E16O3B16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2E8E8O3E8O2E8E8O3E8O2E8B8", "O6E4O5B4G#4E4", "O3E16B16O4E16O3B16O3E16B16O4E16O3B16O3E16B16O4E16O3B16O3E16B16O4E16O3B16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16V6O4C16V8O4C16V6O4C16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "O4A8O5C8E8O4A8O5C8E8A8E8", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "O5G8E8C8O4A8O5C8E8D8C8", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2F8F8O3F8O2F8F8O3F8O2F8O3C8", "O4F8A8O5C8F8E8C8O4A8O5C8", "O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2G8G8O3G8O2G8G8O3G8O2G8O3D8", "O4G8B8O5D8G8F8D8O4B8O5D8", "O3G16O4D16G16D16O3G16O4D16G16D16O3G16O4D16G16D16O3G16O4D16G16D16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "O5E8A8E8C8E8A8O6C8O5A8", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2A8A8O3A8O2A8A8O3A8O2A8O3E8", "O5G8E8G8A8G8E8D8C8", "O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16O3A16O4E16A16E16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2F8F8O3F8O2F8F8O3F8O2F8O3C8", "O5C8O4A8O5C8F8A8F8E8D8", "O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16O3F16O4C16F16C16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16P16V4O6C32P32P16"
DATA "O2E8E8O3E8O2E8E8O3E8O2E8B8", "O5E4O4B8O5E8G#4B4", "O3E16B16O4E16O3B16O3E16B16O4E16O3B16O3E16B16O4E16O3B16O3E16B16O4E16O3B16", "V10O1C16P16V4O6C32P32P16V8O4C16P16V4O6C32P32P16V10O1C16P16V10O1C16P16V8O4C16V6O4C16V8O4C16V6O4C16"
DATA "", "", "", ""

REM $STATIC

SUB AddFx (fx() AS FxT, k AS INTEGER, x AS SINGLE, y AS SINGLE, x2 AS SINGLE, y2 AS SINGLE, life AS SINGLE)
    DIM i AS INTEGER
    FOR i = 1 TO MAXFX
        IF fx(i).alive = 0 THEN
            fx(i).alive = -1: fx(i).kind = k: fx(i).x = x: fx(i).y = y: fx(i).x2 = x2: fx(i).y2 = y2
            fx(i).t = 0: fx(i).life = life
            EXIT SUB
        END IF
    NEXT
END SUB

' A finished building of the computer's, at the first spot near its HQ that takes it.
SUB AIPlace (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fk() AS FieldT)
    DIM h AS INTEGER, r AS INTEGER, x AS INTEGER, y AS INTEGER
    h = FindB%(bl(), 1, BHQ)
    IF h = 0 THEN EXIT SUB
    FOR r = 2 TO 8
        FOR y = bl(h).y - r TO bl(h).y + r
            FOR x = bl(h).x - r TO bl(h).x + r
                IF ABS(x - bl(h).x) = r OR ABS(y - bl(h).y) = r THEN
                    IF CanPlace%(m(), bl(), 1, tm(1).sKind, x, y) THEN
                        PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, tm(1).sKind, x, y
                        EXIT SUB
                    END IF
                END IF
            NEXT
        NEXT
    NEXT
END SUB

' The computer's turn, once a second: it builds, trains, defends and sends waves.
SUB AIThink (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fk() AS FieldT)
    DIM want AS INTEGER, k AS INTEGER, i AS INTEGER, n AS INTEGER, b AS INTEGER, tb AS INTEGER
    DIM d AS SINGLE, best AS SINGLE
    IF FindB%(bl(), 1, BHQ) THEN
        IF tm(1).sKind < 0 THEN
            want = -1
            IF CountB%(bl(), 1, BREF) = 0 THEN
                want = 0
            ELSEIF CountB%(bl(), 1, BBAR) = 0 THEN
                want = 1
            ELSEIF CountB%(bl(), 1, BTUR) < 1 + g.ai.wave \ 2 AND CountB%(bl(), 1, BTUR) < 4 THEN
                want = 2
            END IF
            IF want >= 0 THEN
                IF tm(1).cash >= KCost%(want) THEN
                    tm(1).cash = tm(1).cash - KCost%(want): tm(1).sKind = want: tm(1).sProg = 0
                END IF
            END IF
        ELSEIF tm(1).sReady THEN
            AIPlace g, tm(), m(), u(), bl(), fk()
        END IF
        IF CountU%(u(), 1, UHARV) + tm(1).q3 < 2 AND tm(1).hallKind < 0 AND tm(1).cash >= KCost%(3) THEN
            tm(1).cash = tm(1).cash - KCost%(3): Enqueue tm(1), 3, 1
        END IF
    END IF
    IF FindB%(bl(), 1, BBAR) AND tm(1).q4 + tm(1).q5 < 2 THEN
        k = 4
        IF RND < .3 + g.ai.wave * .06 THEN k = 5
        IF tm(1).cash >= KCost%(k) THEN tm(1).cash = tm(1).cash - KCost%(k): Enqueue tm(1), k, 1
    END IF

    ' defence: the idle answer whoever hit the Legion
    IF g.ai.alarmT > 0 AND g.now - g.ai.alarmT < 4 THEN
        FOR i = 1 TO MAXU
            IF u(i).alive AND u(i).team = 1 AND u(i).kind <> UHARV AND u(i).st = SIDLE THEN
                IF g.ai.alarmK = 1 THEN
                    IF u(g.ai.alarmI).alive AND u(g.ai.alarmI).team = 0 THEN u(i).st = SATTACK: u(i).tk = 1: u(i).ti = g.ai.alarmI: u(i).agg = 0
                ELSEIF g.ai.alarmK = 2 THEN
                    IF bl(g.ai.alarmI).alive AND bl(g.ai.alarmI).team = 0 THEN u(i).st = SATTACK: u(i).tk = 2: u(i).ti = g.ai.alarmI: u(i).agg = 0
                END IF
            END IF
        NEXT
    END IF

    ' a wave, once enough wait at home: on the Coalition's building nearest the Legion
    IF g.now >= g.ai.nextWave THEN
        n = 0
        FOR i = 1 TO MAXU
            IF u(i).alive AND u(i).team = 1 AND u(i).kind <> UHARV AND u(i).st = SIDLE THEN n = n + 1
        NEXT
        IF n >= g.ai.size THEN
            tb = 0: best = 1E+09
            FOR b = 1 TO MAXB
                IF bl(b).alive AND bl(b).team = 0 THEN
                    d = (bl(b).x - 34) ^ 2 + (bl(b).y - 3) ^ 2
                    IF bl(b).kind = BTUR THEN d = d + 40
                    IF d < best THEN best = d: tb = b
                END IF
            NEXT
            IF tb THEN
                FOR i = 1 TO MAXU
                    IF u(i).alive AND u(i).team = 1 AND u(i).kind <> UHARV AND u(i).st = SIDLE THEN
                        u(i).st = SATTACK: u(i).tk = 2: u(i).ti = tb: u(i).agg = -1
                        u(i).gx = bl(tb).x: u(i).gy = bl(tb).y
                    END IF
                NEXT
                g.ai.wave = g.ai.wave + 1
                g.ai.size = 3 + g.ai.wave: IF g.ai.size > 10 THEN g.ai.size = 10
                g.ai.nextWave = g.now + 40
                Say g, "The Scorch Legion is on the march!"
                Sfx g, 8
            END IF
        END IF
    END IF
END SUB

FUNCTION Allowed% (bl() AS BldgT, k AS INTEGER)
    IF k <= 3 THEN Allowed% = FindB%(bl(), 0, BHQ) > 0 ELSE Allowed% = FindB%(bl(), 0, BBAR) > 0
END FUNCTION

FUNCTION Ang! (dx AS SINGLE, dy AS SINGLE)
    DIM a AS SINGLE
    IF ABS(dx) < .0001 THEN
        IF dy >= 0 THEN a = 1.5708 ELSE a = -1.5708
    ELSE
        a = ATN(dy / dx)
        IF dx < 0 THEN a = a + 3.14159
    END IF
    Ang! = a
END FUNCTION

SUB Bar (x AS INTEGER, y AS INTEGER, w AS INTEGER, f AS SINGLE, c AS INTEGER)
    DIM n AS INTEGER
    LINE (x - 1, y - 1)-(x + w, y + 2), 0, BF
    n = INT(f * w)
    IF n > w THEN n = w
    IF n > 0 THEN LINE (x, y)-(x + n - 1, y + 1), c, BF
END SUB

FUNCTION BHp% (k AS INTEGER)
    SELECT CASE k
    CASE BHQ: BHp% = 900
    CASE BTUR: BHp% = 320
    CASE ELSE: BHp% = 450
    END SELECT
END FUNCTION

' The height above its footprint of a building's image: the room for its walls and roof.
FUNCTION BldH% (bt AS INTEGER)
    IF bt = BTUR THEN BldH% = 22 ELSE BldH% = 28
END FUNCTION

' The fog for the mask w by h at mx, 0, drawn at fx, 0: FOG where the mask's picture is.
SUB MaskFog (mx AS INTEGER, fx AS INTEGER, w AS INTEGER, h AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER
    FOR y = 0 TO h - 1: FOR x = 0 TO w - 1
        IF POINT(mx + x, y) THEN PSET (fx + x, y), 0 ELSE PSET (fx + x, y), FOG
    NEXT: NEXT
END SUB

' The next picture from the DATA: drawn at ox, oy, its mask at mx, oy (0 where it is, 255 not), the
' shade it casts at hx, oy (FOG). Its width and height, then strings of symbols to an empty one: a
' colour below 64 CHR$(35 + c), from 64 "!" and CHR$(35 + c - 64), 99 nothing, 32 shade, from 100
' a run of the next symbol, CODE - 97 long.
SUB Unpack (ox AS INTEGER, mx AS INTEGER, hx AS INTEGER, oy AS INTEGER)
    DIM w AS INTEGER, h AS INTEGER, x AS INTEGER, y AS INTEGER, i AS INTEGER, c AS INTEGER, n AS INTEGER
    DIM k AS INTEGER, row AS STRING
    READ w, h
    DO
        READ row
        IF row = "" THEN EXIT DO
        i = 1
        DO WHILE i <= LEN(row)
            c = ASC(MID$(row, i, 1)): n = 1
            IF c >= 100 THEN n = c - 97: i = i + 1: c = ASC(MID$(row, i, 1))
            ' the colour; -1 nothing, -2 shade
            IF c = 33 THEN
                i = i + 1: c = ASC(MID$(row, i, 1)) - 35 + 64
            ELSEIF c = 99 THEN
                c = -1
            ELSEIF c = 32 THEN
                c = -2
            ELSE
                c = c - 35
            END IF
            FOR k = 1 TO n
                IF c < 0 THEN
                    PSET (ox + x, oy + y), 0: PSET (mx + x, oy + y), 255
                ELSE
                    PSET (ox + x, oy + y), c: PSET (mx + x, oy + y), 0
                END IF
                IF c = -2 THEN PSET (hx + x, oy + y), FOG ELSE PSET (hx + x, oy + y), 0
                x = x + 1: IF x = w THEN x = 0: y = y + 1
            NEXT
            i = i + 1
        LOOP
    LOOP
END SUB

FUNCTION BSize% (k AS INTEGER)
    IF k = BTUR THEN BSize% = 1 ELSE BSize% = 2
END FUNCTION

' The tiles, the units in 8 headings and 2 frames each, the buildings by the tile, the icons.
' Units and buildings are drawn at 24 pixels a tile and sampled at TS.
SUB BuildArt (art() AS INTEGER, gl() AS INTEGER)
    DIM k AS INTEGER, x AS INTEGER, y AS INTEGER, tmi AS INTEGER, a AS SINGLE, ca AS SINGLE
    DIM c AS INTEGER, bt AS INTEGER, q AS INTEGER, sz AS INTEGER, n AS INTEGER, total AS INTEGER
    total = 12 + NOVL + 16 + 1 + 2 + 8 + 24
    RESTORE ArtData
    ' the grass at its 3 levels, as diamonds
    FOR k = 0 TO 47
        Unpack 100, 140, 180, 0
        GET (100, 0)-(131, 15), art(0, AGRS + k * 2)
        IF k MOD 4 = 3 THEN n = n + 1: Progress n, total, gl()
    NEXT
    ' the diamond's mask; the fog, OR'd over the land: its colours' dark copies
    FOR y = 0 TO 15: FOR x = 0 TO 31
        IsoUV x, y, a, ca
        IF a >= 0 AND a < 1 AND ca >= 0 AND ca < 1 THEN
            PSET (100 + x, y), 0: PSET (140 + x, y), FOG
        ELSE
            PSET (100 + x, y), 255: PSET (140 + x, y), 0
        END IF
    NEXT: NEXT
    GET (100, 0)-(131, 15), art(0, ADIA)
    GET (140, 0)-(171, 15), art(0, AFOG)
    ' what lies on the grass: image, mask
    FOR k = 0 TO NOVL - 1
        Unpack 100, 140, 180, 0
        GET (100, 0)-(131, 15), art(0, AOVL + k * 4)
        GET (140, 0)-(171, 15), art(0, AOVL + k * 4 + 2)
        n = n + 1: Progress n, total, gl()
    NEXT
    ' rock, raised: image, mask, fog
    FOR k = 0 TO 15
        Unpack 100, 140, 180, 0
        MaskFog 140, 180, 32, 16 + ROCKH
        GET (100, 0)-(131, 15 + ROCKH), art(0, AROK + k * 9)
        GET (140, 0)-(171, 15 + ROCKH), art(0, AROK + k * 9 + 3)
        GET (180, 0)-(211, 15 + ROCKH), art(0, AROK + k * 9 + 6)
        n = n + 1: Progress n, total, gl()
    NEXT
    ' the tree: image, mask, fog, the shade it casts
    Unpack 100, 120, 140, 0
    GET (100, 0)-(115, 23), art(0, ATREE)
    GET (120, 0)-(135, 23), art(0, ATREE + 2)
    GET (140, 0)-(155, 23), art(0, ATREE + 6)
    MaskFog 120, 140, 16, 24
    GET (140, 0)-(155, 23), art(0, ATREE + 4)
    n = n + 1: Progress n, total, gl()
    ' a cliff's step: its left and right faces' image, mask, fog
    FOR k = 0 TO 1
        Unpack 100, 120, 140, 0
        MaskFog 120, 140, 16, 8 + STEPH
        GET (100, 0)-(115, 7 + STEPH), art(0, AFACE + k * 3)
        GET (120, 0)-(135, 7 + STEPH), art(0, AFACE + k * 3 + 1)
        GET (140, 0)-(155, 7 + STEPH), art(0, AFACE + k * 3 + 2)
        ' the shade under a back edge: k 0 the left, 1 the right, every other pixel darkened
        LINE (100, 0)-(115, 11), 0, BF
        FOR x = 0 TO 15
            IF k = 0 THEN c = 8 - (x + 1) \ 2 ELSE c = x \ 2
            FOR y = c TO c + 3
                IF (x + y) MOD 2 = 0 OR y = c THEN PSET (100 + x, y), FOG
            NEXT
        NEXT
        GET (100, 0)-(115, 11), art(0, ASHA + k)
        n = n + 1: Progress n, total, gl()
    NEXT

    ' the buildings: image, mask, fog
    FOR bt = 1 TO 4: FOR tmi = 0 TO 1
        sz = BSize%(bt) * 32: q = BldH%(bt) + sz \ 2
        Unpack 100, 170, 240, 0
        k = ABLD + ((bt - 1) * 2 + tmi) * 3 * BLDW
        GET (100, 0)-(100 + sz - 1, q - 1), art(0, k)
        GET (170, 0)-(170 + sz - 1, q - 1), art(0, k + BLDW)
        ' the sidebar's icons: the Coalition's refinery, barracks and turret, sampled small
        IF tmi = 0 AND bt > 1 THEN
            FOR y = 0 TO TS - 1: FOR x = 0 TO TS - 1
                c = POINT(100 + x * sz \ TS, y * q \ TS)
                IF POINT(170 + x * sz \ TS, y * q \ TS) THEN c = 2
                PSET (250 + x, y), c
            NEXT: NEXT
            GET (250, 0)-(250 + TS - 1, TS - 1), art(0, AICN + bt - 2)
        END IF
        MaskFog 170, 100, sz, q
        GET (100, 0)-(100 + sz - 1, q - 1), art(0, k + 2 * BLDW)
        n = n + 1: Progress n, total, gl()
    NEXT: NEXT
    ' the units: image, mask, the shade they cast
    FOR k = 0 TO 95
        Unpack 100, 120, 140, 0
        GET (100, 0)-(115, 15), art(0, ASPR + k)
        GET (120, 0)-(135, 15), art(0, AMSK + k)
        GET (140, 0)-(155, 15), art(0, ASHD + k)
        IF k MOD 4 = 3 THEN n = n + 1: Progress n, total, gl()
    NEXT
END SUB

' The flow field to goal: each tile's steps to it, FAR where there is no way. Units ignored.
SUB BuildField (m() AS CellT, fd() AS INTEGER, fi AS INTEGER, goal AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER, h AS INTEGER, tl AS INTEGER, d AS INTEGER, nx AS INTEGER, ny AS INTEGER
    DIM v AS INTEGER
    DIM qx(MAPW * MAPH) AS INTEGER, qy(MAPW * MAPH) AS INTEGER
    FOR y = 0 TO MAPH - 1: FOR x = 0 TO MAPW - 1: fd(fi, x, y) = FAR: NEXT: NEXT
    IF goal = OREKEY THEN
        FOR y = 0 TO MAPH - 1
            FOR x = 0 TO MAPW - 1
                IF m(x, y).terr = TORE AND m(x, y).bld = 0 THEN fd(fi, x, y) = 0: qx(tl) = x: qy(tl) = y: tl = tl + 1
            NEXT
        NEXT
    ELSE
        x = goal MOD MAPW: y = goal \ MAPW
        fd(fi, x, y) = 0: qx(0) = x: qy(0) = y: tl = 1
    END IF
    DO WHILE h < tl
        x = qx(h): y = qy(h): h = h + 1
        v = fd(fi, x, y) + 1
        FOR d = 0 TO 7
            nx = x + StepX%(d): ny = y + StepY%(d)
            IF Walk%(m(), x, y, d) THEN
                IF fd(fi, nx, ny) > v THEN fd(fi, nx, ny) = v: qx(tl) = nx: qy(tl) = ny: tl = tl + 1
            END IF
        NEXT
    LOOP
END SUB

' Each character's 8 rows as bits, read back from the screen: text drawn see-through.
SUB BuildGlyphs (gl() AS INTEGER)
    DIM c AS INTEGER, r AS INTEGER, b AS INTEGER, mask AS INTEGER
    SCREEN , , 1, 0
    COLOR 15
    FOR c = 32 TO 126
        LOCATE 1, 1: PRINT CHR$(c);
        FOR r = 0 TO 7
            mask = 0
            FOR b = 0 TO 7
                IF POINT(b, r) THEN mask = mask OR 2 ^ (7 - b)
            NEXT
            gl(c, r) = mask
        NEXT
    NEXT
    CLS
END SUB

' A box of the sidebar, 0 to 5, clicked: build, train, or place what is ready; right cancels.
SUB ButtonClick (g AS GameT, tm() AS TeamT, bl() AS BldgT, k AS INTEGER, rightButton AS INTEGER)
    IF rightButton THEN
        IF k <= 2 THEN
            IF tm(0).sKind = k THEN
                tm(0).cash = tm(0).cash + KCost%(k): tm(0).sKind = -1: tm(0).sReady = 0: g.placeKind = -1
                Say g, "Cancelled"
            END IF
        ELSEIF Queued%(tm(0), k) > 0 THEN
            Enqueue tm(0), k, -1: tm(0).cash = tm(0).cash + KCost%(k): Say g, "Cancelled"
        ELSEIF k = 3 AND tm(0).hallKind = k THEN
            tm(0).hallKind = -1: tm(0).hallProg = 0: tm(0).cash = tm(0).cash + KCost%(k): Say g, "Cancelled"
        ELSEIF k > 3 AND tm(0).denKind = k THEN
            tm(0).denKind = -1: tm(0).denProg = 0: tm(0).cash = tm(0).cash + KCost%(k): Say g, "Cancelled"
        END IF
        Sfx g, 6
        EXIT SUB
    END IF
    IF Allowed%(bl(), k) = 0 THEN
        IF k >= 4 THEN Say g, "Build a Barracks first" ELSE Say g, "Our HQ is gone"
        Sfx g, 7
        EXIT SUB
    END IF
    IF k <= 2 THEN
        IF tm(0).sKind = k AND tm(0).sReady THEN
            g.placeKind = k: Say g, "Left click places the " + KName$(k)
        ELSEIF tm(0).sKind >= 0 THEN
            Say g, "Already building the " + KName$(tm(0).sKind): Sfx g, 7
        ELSEIF tm(0).cash < KCost%(k) THEN
            Say g, "Not enough credits": Sfx g, 7
        ELSE
            tm(0).cash = tm(0).cash - KCost%(k): tm(0).sKind = k: tm(0).sProg = 0: tm(0).sReady = 0
            Say g, "Building the " + KName$(k): Sfx g, 6
        END IF
    ELSEIF tm(0).cash < KCost%(k) THEN
        Say g, "Not enough credits": Sfx g, 7
    ELSEIF Queued%(tm(0), k) >= 9 THEN
        Sfx g, 7
    ELSE
        tm(0).cash = tm(0).cash - KCost%(k): Enqueue tm(0), k, 1
        Say g, "Training a " + KName$(k): Sfx g, 6
    END IF
END SUB

' Where side ti's building of kind fits at x, y: open ground it has seen, near its others,
' blocking no refinery's dock, a refinery's own dock open.
FUNCTION CanPlace% (m() AS CellT, bl() AS BldgT, ti AS INTEGER, kind AS INTEGER, x AS INTEGER, y AS INTEGER)
    DIM bt AS INTEGER, sz AS INTEGER, i AS INTEGER, j AS INTEGER, b AS INTEGER, near AS INTEGER, tt AS INTEGER
    CanPlace% = 0
    bt = KMakes%(kind): sz = BSize%(bt)
    IF x < 0 OR y < 0 OR x + sz > MAPW OR y + sz > MAPH THEN EXIT FUNCTION
    FOR j = y TO y + sz - 1
        FOR i = x TO x + sz - 1
            tt = m(i, j).terr
            IF tt <> TGROUND AND tt <> TGROUND2 AND tt <> TDIRT THEN EXIT FUNCTION
            ' on the flat: one height, not a ramp
            IF m(i, j).hgt <> m(x, y).hgt OR m(i, j).hgt MOD 2 THEN EXIT FUNCTION
            IF m(i, j).bld OR m(i, j).occ THEN EXIT FUNCTION
            IF ti = 0 AND m(i, j).seen = 0 THEN EXIT FUNCTION
        NEXT
    NEXT
    IF bt = BREF THEN
        IF Passable%(m(), x, y + 2) = 0 THEN EXIT FUNCTION
        IF ABS(m(x, y + 2).hgt - m(x, y).hgt) > 1 THEN EXIT FUNCTION
    END IF
    FOR b = 1 TO MAXB
        IF bl(b).alive THEN
            IF bl(b).kind = BREF AND bl(b).x >= x AND bl(b).x < x + sz AND bl(b).y + 2 >= y AND bl(b).y + 2 < y + sz THEN EXIT FUNCTION
            IF bl(b).team = ti THEN
                IF x <= bl(b).x + BSize%(bl(b).kind) + 2 AND x + sz >= bl(b).x - 2 AND y <= bl(b).y + BSize%(bl(b).kind) + 2 AND y + sz >= bl(b).y - 2 THEN near = -1
            END IF
        END IF
    NEXT
    CanPlace% = near
END FUNCTION

' The view centred on tile x, y.
SUB CenterOn (cam AS ViewT, x AS INTEGER, y AS INTEGER)
    cam.camX = (x - y + MAPH) * 16 - VWP \ 2
    cam.camY = (x + y + 1) * 8 - VHP \ 2
    ClampCam cam
END SUB

' The camera kept on the map, by half tiles: whole diamonds then cover the view.
SUB ClampCam (cam AS ViewT)
    IF cam.camX > PLANEW - VWP THEN cam.camX = PLANEW - VWP
    IF cam.camY > PLANEH - VHP THEN cam.camY = PLANEH - VHP
    IF cam.camX < 0 THEN cam.camX = 0
    IF cam.camY < 0 THEN cam.camY = 0
    cam.camX = (cam.camX \ 16) * 16: cam.camY = (cam.camY \ 8) * 8
END SUB

' The tile under screen pixel sx, sy: off the map when outside 0 to MAPW - 1, 0 to MAPH - 1.
' The tile on the ground under screen pixel sx, sy: off the map when outside 0 to MAPW - 1, 0 to
' MAPH - 1.
SUB ScreenTile (cam AS ViewT, sx AS INTEGER, sy AS INTEGER, wx AS INTEGER, wy AS INTEGER)
    DIM x AS SINGLE, y2 AS SINGLE
    x = sx - VX0 + cam.camX - MAPH * 16: y2 = 2 * (sy - VY0 + cam.camY)
    wx = INT((x + y2) / 32): wy = INT((y2 - x) / 32)
END SUB

' The tile seen at screen pixel sx, sy, the land raised: of those whose top is there, the frontmost.
SUB PickTile (g AS GameT, m() AS CellT, sx AS INTEGER, sy AS INTEGER, wx AS INTEGER, wy AS INTEGER)
    DIM h AS INTEGER, x AS INTEGER, y AS INTEGER, front AS INTEGER
    ScreenTile g.vw, sx, sy, wx, wy
    front = -1
    FOR h = 0 TO MAXH
        ScreenTile g.vw, sx, sy + h * STEPH, x, y
        IF x >= 0 AND y >= 0 AND x < MAPW AND y < MAPH THEN
            IF m(x, y).hgt = h AND x + y > front THEN front = x + y: wx = x: wy = y
        END IF
    NEXT
END SUB

' Where unit i's feet are on the screen: raised as the land under it, up or down a ramp as it goes.
SUB UnitScreen (g AS GameT, m() AS CellT, u() AS UnitT, i AS INTEGER, sx AS INTEGER, sy AS INTEGER)
    DIM l AS SINGLE, ln AS SINGLE, t AS SINGLE
    l = Lift%(m(), u(i).x, u(i).y)
    IF u(i).nx <> u(i).x OR u(i).ny <> u(i).y THEN
        ln = Lift%(m(), u(i).nx, u(i).ny)
        t = (ABS(u(i).px - (u(i).x * TS + TS \ 2)) + ABS(u(i).py - (u(i).y * TS + TS \ 2))) / TS
        IF u(i).nx <> u(i).x AND u(i).ny <> u(i).y THEN t = t / 2
        IF t > 1 THEN t = 1
        l = l + (ln - l) * t
    END IF
    sx = u(i).px - u(i).py + MAPH * 16 - g.vw.camX + VX0
    sy = (u(i).px + u(i).py) / 2 - g.vw.camY + VY0 - l
END SUB

' A unit of side team (-1 either) seen near screen pixel sx, sy: the nearest, 0 none.
FUNCTION PickUnit% (g AS GameT, m() AS CellT, u() AS UnitT, sx AS INTEGER, sy AS INTEGER, team AS INTEGER)
    DIM i AS INTEGER, d AS SINGLE, best AS SINGLE, x AS INTEGER, y AS INTEGER
    PickUnit% = 0: best = 9
    FOR i = 1 TO MAXU
        IF u(i).alive AND (team < 0 OR u(i).team = team) THEN
            IF u(i).team = 0 OR m(u(i).x, u(i).y).vis THEN
                UnitScreen g, m(), u(), i, x, y
                d = ABS(x - sx) + ABS(y - 4 - sy)
                IF d < best THEN best = d: PickUnit% = i
            END IF
        END IF
    NEXT
END FUNCTION

' A building seen whose picture is under screen pixel sx, sy: the frontmost, 0 none.
FUNCTION PickBld% (g AS GameT, m() AS CellT, bl() AS BldgT, sx AS INTEGER, sy AS INTEGER)
    DIM b AS INTEGER, n AS INTEGER, x0 AS INTEGER, y0 AS INTEGER, front AS INTEGER
    PickBld% = 0: front = -1
    FOR b = 1 TO MAXB
        IF bl(b).alive THEN
            IF m(bl(b).x, bl(b).y).seen THEN
                n = BSize%(bl(b).kind)
                x0 = (bl(b).x - bl(b).y + MAPH) * 16 - 16 * n - g.vw.camX + VX0
                y0 = (bl(b).x + bl(b).y) * 8 - g.vw.camY + VY0 - Lift%(m(), bl(b).x, bl(b).y)
                IF sx >= x0 + 4 AND sx < x0 + 32 * n - 4 AND sy >= y0 - BldH%(bl(b).kind) \ 2 AND sy < y0 + 16 * n - 2 THEN
                    IF bl(b).x + bl(b).y > front THEN front = bl(b).x + bl(b).y: PickBld% = b
                END IF
            END IF
        END IF
    NEXT
END FUNCTION

' World heading d (0 east, 2 south) as the screen shows it.
FUNCTION IsoDir% (d AS INTEGER)
    DIM c AS SINGLE, sn AS SINGLE
    c = COS(d * .785398): sn = SIN(d * .785398)
    IsoDir% = DirOf%(c - sn, (c + sn) / 2)
END FUNCTION

SUB ClearFields (fk() AS FieldT)
    DIM i AS INTEGER
    FOR i = 0 TO NF - 1: fk(i).goal = NOKEY: NEXT
END SUB

FUNCTION Clock2$ (s AS SINGLE)
    Clock2$ = LTRIM$(STR$(INT(s) \ 60)) + ":" + RIGHT$("0" + LTRIM$(STR$(INT(s) MOD 60)), 2)
END FUNCTION

' The mouse and the screen's edges, each frame: select, order, build, scroll.
SUB Controls (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, dt AS SINGLE)
    DIM wx AS INTEGER, wy AS INTEGER, sdx AS INTEGER, sdy AS INTEGER
    IF g.ms.lHit THEN
        IF g.ms.lX >= SBX THEN
            IF g.ms.lY >= BTNY AND g.ms.lY < BTNY + 6 * BTNH THEN ButtonClick g, tm(), bl(), (g.ms.lY - BTNY) \ BTNH, 0
        ELSEIF g.ms.lX >= VX0 AND g.ms.lY >= VY0 AND g.ms.lY < VIEWB THEN
            PickTile g, m(), g.ms.lX, g.ms.lY, wx, wy
            IF g.placeKind >= 0 THEN
                IF CanPlace%(m(), bl(), 0, g.placeKind, wx, wy) THEN
                    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 0, g.placeKind, wx, wy
                    g.placeKind = -1
                    Sfx g, 5
                ELSE
                    Say g, "It cannot go there": Sfx g, 7
                END IF
            ELSE
                g.ms.dragOn = -1: g.ms.dX0 = g.ms.lX: g.ms.dY0 = g.ms.lY
            END IF
        END IF
    END IF
    ' the minimap follows the button held on it
    IF g.ms.down AND g.ms.x >= MINIX AND g.ms.x < MINIX + MAPW * 2 AND g.ms.y >= MINIY AND g.ms.y < MINIY + MAPH * 2 THEN
        CenterOn g.vw, (g.ms.x - MINIX) \ 2, (g.ms.y - MINIY) \ 2
    END IF
    IF g.ms.dragOn AND g.ms.down = 0 THEN
        g.ms.dragOn = 0
        SelectBox g, m(), u(), g.ms.dX0, g.ms.dY0, g.ms.x, g.ms.y
    END IF
    IF g.ms.rHit THEN
        IF g.placeKind >= 0 THEN
            g.placeKind = -1: Say g, "It waits in the sidebar"
        ELSEIF g.ms.rX >= SBX THEN
            IF g.ms.rY >= BTNY AND g.ms.rY < BTNY + 6 * BTNH THEN ButtonClick g, tm(), bl(), (g.ms.rY - BTNY) \ BTNH, -1
        ELSEIF g.ms.rX >= VX0 AND g.ms.rY >= VY0 AND g.ms.rY < VIEWB THEN
            PickTile g, m(), g.ms.rX, g.ms.rY, wx, wy
            IF wx >= 0 AND wy >= 0 AND wx < MAPW AND wy < MAPH THEN
                Order g, m(), u(), bl(), fx(), wx, wy, PickUnit%(g, m(), u(), g.ms.rX, g.ms.rY, 1), PickBld%(g, m(), bl(), g.ms.rX, g.ms.rY)
            END IF
        END IF
    END IF

    ' the screen's edges scroll, a half tile 18 times a second
    IF g.ms.x <= 4 THEN sdx = -1
    IF g.ms.x >= 315 THEN sdx = 1
    IF g.ms.y <= 3 THEN sdy = -1
    IF g.ms.y >= 196 THEN sdy = 1
    IF sdx OR sdy THEN
        g.vw.scrollAcc = g.vw.scrollAcc + dt * 18
        DO WHILE g.vw.scrollAcc >= 1
            g.vw.scrollAcc = g.vw.scrollAcc - 1
            g.vw.camX = g.vw.camX + sdx * 16: g.vw.camY = g.vw.camY + sdy * 8
        LOOP
    ELSE
        g.vw.scrollAcc = .99
    END IF
    ClampCam g.vw
END SUB

FUNCTION CountB% (bl() AS BldgT, ti AS INTEGER, bt AS INTEGER)
    DIM b AS INTEGER, n AS INTEGER
    FOR b = 1 TO MAXB
        IF bl(b).alive AND bl(b).team = ti AND bl(b).kind = bt THEN n = n + 1
    NEXT
    CountB% = n
END FUNCTION

FUNCTION CountU% (u() AS UnitT, ti AS INTEGER, ut AS INTEGER)
    DIM i AS INTEGER, n AS INTEGER
    FOR i = 1 TO MAXU
        IF u(i).alive AND u(i).team = ti AND u(i).kind = ut THEN n = n + 1
    NEXT
    CountU% = n
END FUNCTION

SUB DepleteOre (g AS GameT, m() AS CellT, fk() AS FieldT, x AS INTEGER, y AS INTEGER)
    DIM i AS INTEGER
    m(x, y).terr = TGROUND
    FOR i = 0 TO NF - 1
        IF fk(i).goal = OREKEY THEN fk(i).goal = NOKEY
    NEXT
    g.vw.miniDirty = -1
END SUB

' The heading of dx, dy: 0 east, 2 south (the screen's y is down), 4 west, 6 north.
FUNCTION DirOf% (dx AS SINGLE, dy AS SINGLE)
    DIM d AS INTEGER
    d = INT(Ang!(dx, dy) / .785398 + .5)
    DirOf% = ((d MOD 8) + 8) MOD 8
END FUNCTION

' Every other pixel of the box in colour c.
SUB Dither (x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, c AS INTEGER)
    DIM y AS INTEGER
    FOR y = y1 TO y2
        IF y MOD 2 THEN LINE (x1, y)-(x2, y), c, , &HAAAA ELSE LINE (x1, y)-(x2, y), c, , &H5555
    NEXT
END SUB

' The pointer: a sight over an enemy while units are selected, else an arrow.
SUB DrawCursor (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT)
    DIM e AS INTEGER, i AS INTEGER, x AS INTEGER, y AS INTEGER
    x = g.ms.x: y = g.ms.y
    IF g.state = GPLAY AND x < SBX AND y < VIEWB AND g.placeKind < 0 THEN
        IF PickUnit%(g, m(), u(), x, y, 1) THEN e = 1
        IF e = 0 THEN
            i = PickBld%(g, m(), bl(), x, y)
            IF i THEN IF bl(i).team = 1 THEN e = 1
        END IF
        IF e THEN
            e = 0
            FOR i = 1 TO MAXU
                IF u(i).sel THEN e = 1
            NEXT
        END IF
    END IF
    IF e THEN
        CIRCLE (x, y), 5, 0: CIRCLE (x, y), 4, 29
        LINE (x - 7, y)-(x - 2, y), 29: LINE (x + 2, y)-(x + 7, y), 29
        LINE (x, y - 6)-(x, y - 2), 29: LINE (x, y + 2)-(x, y + 6), 29
    ELSE
        Arrow x, y
    END IF
END SUB

SUB Arrow (x AS INTEGER, y AS INTEGER)
    LINE (x + 1, y + 1)-(x + 1, y + 9), 0: LINE (x + 1, y + 1)-(x + 7, y + 7), 0
    LINE (x + 1, y + 9)-(x + 7, y + 7), 0
    LINE (x, y)-(x, y + 8), 15: LINE (x, y)-(x + 6, y + 6), 15
    LINE (x, y + 8)-(x + 3, y + 6), 15: LINE (x + 3, y + 6)-(x + 6, y + 6), 15
    LINE (x + 1, y + 3)-(x + 1, y + 6), 11
END SUB

SUB DrawEndBack (g AS GameT, tm() AS TeamT, art() AS INTEGER, gl() AS INTEGER)
    DIM y AS INTEGER, t AS STRING, i AS INTEGER, x AS INTEGER, h AS INTEGER
    IF g.won THEN Ramp SKY, 32, 0, 2, 8, 0, 26, 30 ELSE Ramp SKY, 32, 2, 0, 4, 34, 6, 4
    Ramp LOGOC, 8, 63, 63, 63, 0, 34, 24
    FOR y = 0 TO 199
        LINE (0, y)-(319, y), SKY + y * 32 \ 200
    NEXT
    FOR i = 1 TO 90
        PSET (RND * 319, RND * 120), 15
    NEXT
    ' the land: ore glowing for the Coalition, fires for the Legion
    FOR x = 0 TO 319
        h = 166 + 7 * SIN(x / 30 + 1) + 3 * SIN(x / 11)
        IF g.won THEN LINE (x, h)-(x, 199), 1 ELSE LINE (x, h)-(x, 199), 4
    NEXT
    FOR i = 1 TO 120
        x = RND * 318: y = 172 + RND * 27
        IF g.won THEN PSET (x, y), 8 ELSE PSET (x, y), FIRE + 2 + INT(RND * 6)
    NEXT
    IF g.won THEN
        Logo gl(), "VICTORY", 22, 4, 4
        TxtC gl(), "The Scorch Legion is broken.", 70, 1, 15
        TxtC gl(), "The red ore of Kesh is the Coalition's.", 82, 1, 15
    ELSE
        Logo gl(), "DEFEAT", 22, 4, 4
        TxtC gl(), "The Coalition's line has fallen.", 70, 1, 15
        TxtC gl(), "The Legion takes the ore.", 82, 1, 15
    END IF
    t = "Time " + Clock2$(g.now) + "   Trained" + STR$(tm(0).built) + "   Lost" + STR$(g.losses)
    TxtC gl(), t, 108, 1, 13
    t = "Destroyed" + STR$(g.kills) + "   Ore" + STR$(tm(0).harvested)
    TxtC gl(), t, 120, 1, 13
    GET (0, 0)-(319, 199), art(0, ABACK)
END SUB

SUB DrawFx (g AS GameT, m() AS CellT, fx() AS FxT)
    DIM i AS INTEGER, p AS SINGLE, x AS INTEGER, y AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, r AS INTEGER
    DIM c AS INTEGER, k AS INTEGER, ox AS INTEGER, oy AS INTEGER
    ox = MAPH * 16 - g.vw.camX + VX0: oy = VY0 - g.vw.camY
    FOR i = 1 TO MAXFX
        IF fx(i).alive THEN
            p = fx(i).t / fx(i).life
            x = fx(i).x - fx(i).y + ox: y = (fx(i).x + fx(i).y) / 2 + oy - 4 - Lift%(m(), fx(i).x \ TS, fx(i).y \ TS)
            x2 = ABS(fx(i).x2) - fx(i).y2 + ox
            y2 = (ABS(fx(i).x2) + fx(i).y2) / 2 + oy - 4 - Lift%(m(), ABS(fx(i).x2) \ TS, fx(i).y2 \ TS)
            SELECT CASE fx(i).kind
            CASE FXBOOM, FXBIG
                r = 2 + p * 5: IF fx(i).kind = FXBIG THEN r = 3 + p * 10
                c = FIRE + INT(p * 15)
                CIRCLE (x, y), r, c
                IF p < .6 THEN
                    FOR k = r * .6 TO 1 STEP -1
                        CIRCLE (x, y), k, c - 5 + k
                    NEXT
                    PSET (x, y), FIRE
                END IF
                IF fx(i).kind = FXBIG THEN
                    FOR k = 0 TO 5
                        PSET (x + COS(k * 1.05 + i) * r * 1.3, y + SIN(k * 1.05 + i) * r * 1.3), FIRE + 4
                    NEXT
                END IF
            CASE FXTRACE
                LINE (x, y)-(x2, y2), 13
                PSET (x2, y2), 15
            CASE FXSHELL
                x = x + (x2 - x) * p: y = y + (y2 - y) * p
                LINE (x - 1, y - 1)-(x + 1, y + 1), FIRE + 2, BF
                PSET (x, y), 15
            CASE FXBEAM
                c = 11: IF fx(i).x2 < 0 THEN c = 13
                LINE (x, y)-((x + x2) \ 2 + 2, (y + y2) \ 2 - 1), c
                LINE -(x2, y2), c
                LINE (x, y)-(x2, y2), 15
            CASE FXSMOKE
                CIRCLE (x, y - p * 10), 1 + p * 3, 23
            CASE FXMARK
                r = 6 - p * 4
                LINE (x - r, y - r)-(x + r, y + r), 8, B
            END SELECT
        END IF
    NEXT
END SUB

' The minimap, 2 pixels a tile: the land drawn again when it changed, else put back; the units
' and buildings on it.
SUB DrawMinimap (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, art() AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER, c AS INTEGER, i AS INTEGER, b AS INTEGER, s AS INTEGER
    IF g.vw.miniDirty AND g.vw.miniT <= 0 THEN
        FOR y = 0 TO MAPH - 1
            FOR x = 0 TO MAPW - 1
                c = 0
                IF m(x, y).seen THEN
                    SELECT CASE m(x, y).terr
                    CASE TGROUND, TGROUND2
                        c = 1: IF m(x, y).hgt >= 2 THEN c = 2
                        IF m(x, y).hgt >= 4 THEN c = 16
                    CASE TDIRT: c = 3
                    CASE TROCK: c = 5
                    CASE TWATER: c = 20
                    CASE TORE: c = 22
                    CASE ELSE: c = 27
                    END SELECT
                END IF
                LINE (MINIX + x * 2, MINIY + y * 2)-STEP(1, 1), c, BF
            NEXT
        NEXT
        GET (MINIX, MINIY)-(MINIX + MAPW * 2 - 1, MINIY + MAPH * 2 - 1), art(0, AMINI)
        g.vw.miniDirty = 0: g.vw.miniT = 1
    ELSE
        PUT (MINIX, MINIY), art(0, AMINI), PSET
    END IF
    FOR b = 1 TO MAXB
        IF bl(b).alive THEN
            IF m(bl(b).x, bl(b).y).seen THEN
                s = BSize%(bl(b).kind) * 2
                LINE (MINIX + bl(b).x * 2, MINIY + bl(b).y * 2)-STEP(s - 1, s - 1), 11 + bl(b).team * 2, BF
            END IF
        END IF
    NEXT
    FOR i = 1 TO MAXU
        IF u(i).alive THEN
            IF u(i).team = 0 OR m(u(i).x, u(i).y).vis THEN
                PSET (MINIX + u(i).x * 2, MINIY + u(i).y * 2), 15 - u(i).team * 2
            END IF
        END IF
    NEXT
    ScreenTile g.vw, VX0, VY0, x, y: ScreenTile g.vw, SBX - 1, VY0, i, b
    VIEW SCREEN (MINIX, MINIY)-(MINIX + MAPW * 2 - 1, MINIY + MAPH * 2 - 1)
    LINE (MINIX + x * 2, MINIY + y * 2)-(MINIX + i * 2, MINIY + b * 2), 15
    ScreenTile g.vw, SBX - 1, VIEWB - 1, x, y
    LINE -(MINIX + x * 2, MINIY + y * 2), 15
    ScreenTile g.vw, VX0, VIEWB - 1, i, b
    LINE -(MINIX + i * 2, MINIY + b * 2), 15
    ScreenTile g.vw, VX0, VY0, x, y
    LINE -(MINIX + x * 2, MINIY + y * 2), 15
    VIEW
END SUB

SUB DrawSidebar (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, art() AS INTEGER, gl() AS INTEGER)
    DIM k AS INTEGER, y AS INTEGER, s AS STRING, c AS INTEGER, i AS INTEGER, n AS INTEGER, one AS INTEGER
    DIM ok AS INTEGER, prog AS SINGLE, qn AS INTEGER
    LINE (SBX, 0)-(319, 199), 0, BF
    LINE (0, VIEWH)-(SBX - 1, 199), 0, BF
    LINE (SBX, 0)-(SBX, VIEWH - 1), 23
    Txt gl(), "$", SBX + 3, 1, 1, 13
    s = LTRIM$(STR$(tm(0).cash)): Txt gl(), s, 319 - LEN(s) * 8, 1, 1, 15
    DrawMinimap g, m(), u(), bl(), art()
    LINE (SBX + 1, MINIY - 1)-(319, MINIY - 1), 23
    LINE (SBX + 1, MINIY + MAPH * 2)-(319, MINIY + MAPH * 2), 23
    FOR k = 0 TO 5
        y = BTNY + k * BTNH
        ok = Allowed%(bl(), k)
        LINE (SBX + 1, y)-(319, y + BTNH - 2), 4, B
        IF k <= 2 THEN
            PUT (SBX + 2, y + 1), art(0, AICN + k), PSET
        ELSE
            LINE (SBX + 2, y + 1)-STEP(TS - 1, TS - 1), 2, BF
            Sprite art(), ((KMakes%(k) - 1) * 2) * 16, SBX + 2, y + 1
        END IF
        c = 15
        IF ok = 0 THEN c = 4 ELSE IF tm(0).cash < KCost%(k) THEN c = 14
        Txt gl(), KName$(k), SBX + 20, y + 1, 1, c
        prog = -1
        IF k <= 2 THEN
            IF tm(0).sKind = k THEN prog = tm(0).sProg
        ELSE
            IF k = 3 AND tm(0).hallKind = k THEN prog = tm(0).hallProg
            IF k > 3 AND tm(0).denKind = k THEN prog = tm(0).denProg
            qn = Queued%(tm(0), k)
            IF qn > 0 THEN Txt gl(), "+" + LTRIM$(STR$(qn)), 303, y + 9, 1, 11
        END IF
        IF prog >= 0 AND k <= 2 AND tm(0).sReady THEN
            IF INT(g.clock * 3) MOD 2 = 0 OR g.placeKind = k THEN Txt gl(), "READY", SBX + 20, y + 9, 1, 8
        ELSE
            Txt gl(), LTRIM$(STR$(KCost%(k))), SBX + 20, y + 9, 1, 13
            IF prog >= 0 THEN Bar SBX + 47, y + 12, 28, prog, 11
        END IF
        IF ok = 0 THEN Dither SBX + 2, y + 1, 318, y + BTNH - 3, 0
    NEXT
    Txt gl(), Clock2$(g.now), SBX + 3, 182, 1, 5
    IF g.snd.enabled = 0 THEN Txt gl(), "MUTED", SBX + 40, 182, 1, 5

    ' the status line: a message, the selection, or the army
    FOR i = 1 TO MAXU
        IF u(i).sel THEN n = n + 1: one = i
    NEXT
    IF g.msgT > 0 THEN
        Txt gl(), LEFT$(RTRIM$(g.msg), 39), 2, 192, 1, 15
    ELSEIF n = 1 THEN
        s = KName$(u(one).kind + 2) + "  health" + STR$(u(one).hp) + " of" + STR$(UHp%(u(one).kind))
        IF u(one).kind = UHARV THEN s = s + "  ore" + STR$(u(one).load)
        Txt gl(), s, 2, 192, 1, 11
    ELSEIF n > 1 THEN
        Txt gl(), LTRIM$(STR$(n)) + " selected: right click orders", 2, 192, 1, 11
    ELSE
        Txt gl(), "COALITION", 2, 192, 1, 10
        s = "Rifles" + STR$(CountU%(u(), 0, URIFLE)) + " Tanks" + STR$(CountU%(u(), 0, UTANK)) + " Harvesters" + STR$(CountU%(u(), 0, UHARV))
        Txt gl(), s, 50, 192, 1, 14
    END IF
END SUB

SUB DrawTitleBack (art() AS INTEGER, gl() AS INTEGER)
    DIM y AS INTEGER, x AS INTEGER, i AS INTEGER, h AS INTEGER, c AS INTEGER, r AS INTEGER, l AS SINGLE
    ' the night sky: deep blue into violet
    Ramp SKY, 32, 0, 0, 6, 26, 8, 30
    Ramp LOGOC, 8, 63, 63, 63, 0, 34, 24
    Ramp PLANET, 16, 20, 8, 24, 63, 46, 30
    FOR y = 0 TO 199
        LINE (0, y)-(319, y), SKY + y * 32 \ 200
    NEXT
    FOR i = 1 TO 110
        PSET (RND * 319, RND * 120), 15 - (i MOD 3) * 9
    NEXT
    ' the gas giant behind the name, lit from the upper left, banded
    FOR y = -40 TO 40
        h = SQR(1600 - y * y)
        FOR x = -h TO h
            l = (-x * .5 - y * .6) / 40 + SQR(ABS(1 - (x * x + y * y) / 1600)) * .7
            IF ((y + 40) \ 5) MOD 3 = 0 THEN l = l - .12
            c = INT(l * 15): IF c < 0 THEN c = 0 ELSE IF c > 15 THEN c = 15
            PSET (268 + x, 40 + y), PLANET + c
        NEXT
    NEXT
    ' the hills, far and near
    FOR x = 0 TO 319
        h = 136 + 10 * SIN(x / 35) + 5 * SIN(x / 12 + 1)
        LINE (x, h)-(x, 199), 4
        h = 158 + 8 * SIN(x / 25 + 2) + 3 * SIN(x / 9)
        LINE (x, h)-(x, 199), 1
    NEXT
    ' ore glowing on the near hill
    FOR i = 1 TO 160
        x = RND * 318: y = 168 + RND * 31
        PSET (x, y), 8: IF RND < .3 THEN PSET (x + 1, y), 22
    NEXT
    ' a palm, left
    LINE (34, 112)-(41, 170), 30, BF
    LINE (37, 112)-(38, 170), 3
    FOR r = 32 TO 1 STEP -1
        c = 27: IF r > 28 THEN c = 4
        IF r < 18 THEN c = 5
        CIRCLE (38, 108), r, c, 0, 3.14159, .45
    NEXT
    FOR i = 1 TO 12
        x = 14 + RND * 48: y = 96 + RND * 10
        PSET (x, y), 8: PSET (x + 1, y), 31
    NEXT
    ' the Coalition's units on the hill, the Legion's beyond
    Sprite art(), 32 + 7 * 2, 116, 164
    Sprite art(), 32 + 7 * 2 + 1, 134, 170
    Sprite art(), 7 * 2, 98, 172
    Sprite art(), 5 * 16 + 3 * 2, 262, 158
    Sprite art(), 3 * 16 + 3 * 2, 284, 164
    Logo gl(), "RUSTLINE", 12, 4, 4
    TxtC gl(), "THE WAR FOR THE RED ORE OF KESH", 52, 1, 13
    Dither 14, 70, 305, 108, 0
    TxtC gl(), "Lead the Coalition: mine ore, build,", 74, 1, 15
    TxtC gl(), "and break the Scorch Legion.", 84, 1, 15
    TxtC gl(), "Drag selects, right click orders", 98, 1, 11
    GET (0, 0)-(319, 199), art(0, ABACK)
END SUB

' The units, buildings and the n trees at tx(), ty() in the view, back to front by their feet's
' x + y.
SUB DrawObjects (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, art() AS INTEGER, tx() AS INTEGER, ty() AS INTEGER, nt AS INTEGER)
    DIM id(MAXU + MAXB + 200) AS INTEGER, dp(MAXU + MAXB + 200) AS SINGLE, n AS INTEGER
    DIM i AS INTEGER, j AS INTEGER, k AS INTEGER, b AS INTEGER, sx AS INTEGER, sy AS INTEGER, f AS INTEGER
    DIM c AS INTEGER, mh AS INTEGER, x0 AS INTEGER, y0 AS INTEGER, w AS INTEGER, h AS INTEGER, d AS SINGLE
    DIM ox AS INTEGER, oy AS INTEGER, ax AS SINGLE, ay AS SINGLE
    ox = MAPH * 16 - g.vw.camX + VX0: oy = VY0 - g.vw.camY
    ' units 1 to MAXU, buildings 1000 on, trees 2000 on
    FOR i = 1 TO MAXU
        IF u(i).alive THEN
            IF u(i).team = 0 OR m(u(i).x, u(i).y).vis THEN n = n + 1: id(n) = i: dp(n) = (u(i).px + u(i).py) / TS
        END IF
    NEXT
    FOR b = 1 TO MAXB
        IF bl(b).alive THEN
            IF m(bl(b).x, bl(b).y).seen THEN n = n + 1: id(n) = 1000 + b: dp(n) = bl(b).x + bl(b).y + 2 * BSize%(bl(b).kind) - 1
        END IF
    NEXT
    FOR i = 1 TO nt
        n = n + 1: id(n) = 2000 + i: dp(n) = tx(i) + ty(i) + 1
    NEXT
    FOR i = 2 TO n
        k = id(i): d = dp(i): j = i - 1
        DO WHILE j >= 1
            IF dp(j) <= d THEN EXIT DO
            id(j + 1) = id(j): dp(j + 1) = dp(j): j = j - 1
        LOOP
        id(j + 1) = k: dp(j + 1) = d
    NEXT

    FOR j = 1 TO n
        k = id(j)
        IF k < 1000 THEN
            i = k
            UnitScreen g, m(), u(), i, sx, sy
            x0 = sx - 8: y0 = sy - 12
            IF x0 >= 0 AND y0 >= 0 AND x0 <= 300 AND y0 <= 180 THEN
                f = 0
                IF u(i).nx <> u(i).x OR u(i).ny <> u(i).y OR u(i).st = SGATHER THEN f = INT(u(i).anim * 8) MOD 2
                k = ((u(i).kind - 1) * 2 + u(i).team) * 16 + IsoDir%(u(i).dir) * 2 + f
                PUT (x0, y0), art(0, ASHD + k), OR
                Sprite art(), k, x0, y0
                IF u(i).kind = UHARV AND u(i).load > 0 THEN
                    PSET (sx, sy - 5), 8: PSET (sx + 1, sy - 4), 8
                    IF u(i).load > 5 THEN PSET (sx + 1, sy - 5), 31
                END IF
                IF u(i).sel THEN
                    LINE (x0, y0)-STEP(3, 0), 15: LINE (x0, y0)-STEP(0, 3), 15
                    LINE (x0 + 15, y0)-STEP(-3, 0), 15: LINE (x0 + 15, y0)-STEP(0, 3), 15
                    LINE (x0, y0 + 15)-STEP(3, 0), 15: LINE (x0, y0 + 15)-STEP(0, -3), 15
                    LINE (x0 + 15, y0 + 15)-STEP(-3, 0), 15: LINE (x0 + 15, y0 + 15)-STEP(0, -3), 15
                END IF
                mh = UHp%(u(i).kind)
                IF (u(i).sel OR u(i).hp < mh) AND y0 >= 2 THEN
                    c = 8: IF u(i).hp * 2 < mh THEN c = 13
                    IF u(i).hp * 4 < mh THEN c = 29
                    Bar x0 + 2, y0 - 2, 12, u(i).hp / mh, c
                END IF
            END IF
        ELSEIF k < 2000 THEN
            b = k - 1000
            w = 32 * BSize%(bl(b).kind): h = BldH%(bl(b).kind) + w \ 2
            x0 = (bl(b).x - bl(b).y) * 16 + ox - w \ 2: y0 = (bl(b).x + bl(b).y) * 8 + oy - BldH%(bl(b).kind) - Lift%(m(), bl(b).x, bl(b).y)
            IF x0 >= 0 AND y0 >= 0 AND x0 + w <= 320 AND y0 + h <= 200 THEN
                k = ABLD + ((bl(b).kind - 1) * 2 + bl(b).team) * 3 * BLDW
                PUT (x0, y0), art(0, k + BLDW), AND
                PUT (x0, y0), art(0, k), OR
                IF m(bl(b).x, bl(b).y).vis = 0 THEN PUT (x0, y0), art(0, k + 2 * BLDW), OR
                IF bl(b).kind = BTUR THEN
                    ' the gun turns in the world; drawn as the screen sees it
                    sx = x0 + 16: sy = y0 + 14
                    ax = COS(bl(b).ang): ay = SIN(bl(b).ang)
                    LINE (sx, sy)-(sx + (ax - ay) * 6, sy + (ax + ay) * 3), 0
                    LINE (sx, sy - 1)-(sx + (ax - ay) * 6, sy - 1 + (ax + ay) * 3), 24
                    c = 11 + bl(b).team * 2: IF bl(b).cool > .8 THEN c = 15
                    PSET (sx + (ax - ay) * 6, sy + (ax + ay) * 3), c
                ELSEIF INT(g.clock * 2 + b) MOD 3 = 0 THEN
                    PSET (x0 + w \ 2 + 1, y0 + BldH%(bl(b).kind) - 2), 11 + bl(b).team * 2
                END IF
                mh = BHp%(bl(b).kind)
                IF bl(b).hp < mh AND y0 >= 2 THEN
                    c = 8: IF bl(b).hp * 2 < mh THEN c = 13
                    IF bl(b).hp * 4 < mh THEN c = 29
                    Bar x0 + 6, y0 - 2, w - 12, bl(b).hp / mh, c
                END IF
            END IF
        ELSE
            i = k - 2000
            x0 = (tx(i) - ty(i)) * 16 + ox - 8: y0 = (tx(i) + ty(i)) * 8 + oy - 13 - Lift%(m(), tx(i), ty(i))
            IF x0 >= 0 AND y0 >= 0 AND x0 <= 304 AND y0 <= 176 THEN
                PUT (x0, y0), art(0, ATREE + 6), OR
                PUT (x0, y0), art(0, ATREE + 2), AND
                PUT (x0, y0), art(0, ATREE), OR
                IF m(tx(i), ty(i)).vis = 0 THEN PUT (x0, y0), art(0, ATREE + 4), OR
            END IF
        END IF
    NEXT
END SUB

FUNCTION StepX% (d AS INTEGER)
    SELECT CASE d
    CASE 0, 1, 7: StepX% = 1
    CASE 3, 4, 5: StepX% = -1
    CASE ELSE: StepX% = 0
    END SELECT
END FUNCTION

FUNCTION StepY% (d AS INTEGER)
    SELECT CASE d
    CASE 1, 2, 3: StepY% = 1
    CASE 5, 6, 7: StepY% = -1
    CASE ELSE: StepY% = 0
    END SELECT
END FUNCTION

' A VGA colour: red, green, blue 0 to 63.
FUNCTION Rgb& (r AS INTEGER, g AS INTEGER, b AS INTEGER)
    Rgb& = r + g * 256& + b * 65536
END FUNCTION

' Colour c and its fog's copy, darker and bluer.
SUB Shade (c AS INTEGER, r AS INTEGER, g AS INTEGER, b AS INTEGER)
    PALETTE c, Rgb&(r, g, b)
    PALETTE FOG OR c, Rgb&(r * 4 \ 10, g * 4 \ 10, b * 5 \ 10 + 3)
END SUB

' The water's colours at turn t: WATERC + k a swell's height, a crest every NWATER; the art tool
' draws the water in them.
SUB WaterColours (t AS INTEGER)
    DIM k AS INTEGER, c AS SINGLE
    FOR k = 0 TO NWATER - 1
        c = ((1 + COS((k + t) * 6.28318 / NWATER)) / 2) ^ 2
        Shade WATERC + k, 5 + 11 * c, 11 + 19 * c, 30 + 22 * c
    NEXT
END SUB

' The colours in play: the land's, the art tool's, fire's, and all their copies in the fog.
SUB GamePalette ()
    DIM i AS INTEGER, cr AS INTEGER, cg AS INTEGER, cb AS INTEGER
    Shade 0, 0, 0, 0
    Shade 1, 7, 15, 6       ' ground, dark
    Shade 2, 9, 19, 8     ' ground
    Shade 3, 26, 19, 12     ' dirt
    Shade 4, 12, 10, 12      ' rock's shadow, outlines
    Shade 5, 27, 22, 26     ' rock, palm
    Shade 6, 5, 11, 30      ' deep water
    Shade 7, 20, 32, 54     ' light on water
    Shade 8, 24, 63, 44     ' ore's glow
    Shade 9, 6, 40, 22      ' ore
    Shade 10, 0, 40, 42     ' the Coalition
    Shade 11, 32, 62, 60
    Shade 12, 58, 24, 2     ' the Legion
    Shade 13, 63, 54, 16
    Shade 14, 38, 38, 42    ' metal
    Shade 15, 63, 63, 63
    Shade 16, 12, 23, 9    ' ground, light
    Shade 17, 17, 29, 11    ' grass
    Shade 18, 34, 26, 17    ' dirt, light
    Shade 19, 42, 37, 38    ' rock, light
    Shade 20, 10, 20, 42    ' water
    Shade 21, 44, 54, 63    ' foam
    Shade 22, 14, 52, 32    ' ore, bright
    Shade 23, 22, 22, 28    ' metal, dark
    Shade 24, 50, 50, 56    ' metal, light
    Shade 25, 0, 22, 26     ' the Coalition, dark
    Shade 26, 36, 11, 0     ' the Legion, dark
    Shade 27, 46, 28, 50    ' palm cap, light
    Shade 28, 30, 46, 63    ' glass
    Shade 29, 63, 18, 10    ' a red light
    Shade 30, 17, 11, 6     ' bark
    Shade 31, 44, 63, 54    ' ore's tips
    RESTORE PalData
    FOR i = 32 TO WATERC - 1
        READ cr, cg, cb
        Shade i, cr, cg, cb
    NEXT
    WaterColours 0
    Ramp FIRE, 16, 63, 63, 52, 34, 6, 2
END SUB

' n colours from c0, red, green, blue from r0, g0, b0 to r1, g1, b1.
SUB Ramp (c0 AS INTEGER, n AS INTEGER, r0 AS INTEGER, g0 AS INTEGER, b0 AS INTEGER, r1 AS INTEGER, g1 AS INTEGER, b1 AS INTEGER)
    DIM i AS INTEGER
    FOR i = 0 TO n - 1
        PALETTE c0 + i, Rgb&(r0 + (r1 - r0) * i \ (n - 1), g0 + (g1 - g0) * i \ (n - 1), b0 + (b1 - b0) * i \ (n - 1))
    NEXT
END SUB

SUB Enqueue (t AS TeamT, k AS INTEGER, n AS INTEGER)
    SELECT CASE k
    CASE 3: t.q3 = t.q3 + n
    CASE 4: t.q4 = t.q4 + n
    CASE 5: t.q5 = t.q5 + n
    END SELECT
END SUB

' The field to goal (a tile, or OREKEY), built again only when no field kept is it.
FUNCTION FlowField% (m() AS CellT, fd() AS INTEGER, fk() AS FieldT, goal AS INTEGER, now AS SINGLE)
    DIM i AS INTEGER, old AS INTEGER
    FOR i = 0 TO NF - 1
        IF fk(i).goal = goal THEN fk(i).used = now: FlowField% = i: EXIT FUNCTION
    NEXT
    FOR i = 1 TO NF - 1
        IF fk(old).goal = NOKEY THEN EXIT FOR
        IF fk(i).goal = NOKEY OR fk(i).used < fk(old).used THEN old = i
    NEXT
    BuildField m(), fd(), old, goal
    fk(old).goal = goal: fk(old).used = now
    FlowField% = old
END FUNCTION

FUNCTION FindB% (bl() AS BldgT, ti AS INTEGER, bt AS INTEGER)
    DIM b AS INTEGER
    FOR b = 1 TO MAXB
        IF bl(b).alive AND bl(b).team = ti AND bl(b).kind = bt THEN FindB% = b: EXIT FUNCTION
    NEXT
    FindB% = 0
END FUNCTION

' An open tile round building b for a unit it made, below it first, out to 4 tiles: y * MAPW + x.
FUNCTION FindSpawn% (m() AS CellT, bl() AS BldgT, b AS INTEGER)
    DIM r AS INTEGER, x AS INTEGER, y AS INTEGER, x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER
    FindSpawn% = -1
    FOR r = 1 TO 4
        x1 = bl(b).x - r: y1 = bl(b).y - r
        x2 = bl(b).x + BSize%(bl(b).kind) - 1 + r: y2 = bl(b).y + BSize%(bl(b).kind) - 1 + r
        FOR y = y2 TO y1 STEP -1
            FOR x = x1 TO x2
                IF x = x1 OR x = x2 OR y = y1 OR y = y2 THEN
                    IF Passable%(m(), x, y) THEN
                        IF m(x, y).occ = 0 AND ABS(m(x, y).hgt - m(bl(b).x, bl(b).y).hgt) <= 1 THEN FindSpawn% = y * MAPW + x: EXIT FUNCTION
                    END IF
                END IF
            NEXT
        NEXT
    NEXT
END FUNCTION

' A shot of unit i at its target: the damage now, the look as it flies.
SUB Fire (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, i AS INTEGER)
    DIM tx AS SINGLE, ty AS SINGLE, sx AS SINGLE, sy AS SINGLE, t AS INTEGER, seen AS INTEGER
    t = u(i).ti
    IF u(i).tk = 1 THEN
        tx = u(t).px: ty = u(t).py
    ELSE
        tx = (bl(t).x + BSize%(bl(t).kind) / 2) * TS + RND * 7 - 3
        ty = (bl(t).y + BSize%(bl(t).kind) / 2) * TS + RND * 7 - 3
    END IF
    sx = u(i).px + StepX%(u(i).dir) * 6: sy = u(i).py + StepY%(u(i).dir) * 6
    seen = u(i).team = 0 OR m(u(i).x, u(i).y).vis
    IF u(i).kind = URIFLE THEN
        AddFx fx(), FXTRACE, sx, sy, tx + RND * 4 - 2, ty + RND * 4 - 2, .07
        IF seen THEN Sfx g, 1
    ELSE
        AddFx fx(), FXSHELL, sx, sy, tx, ty, .22
        IF seen THEN Sfx g, 2
    END IF
    u(i).cool = URate!(u(i).kind)
    Hit g, m(), u(), bl(), fx(), fk(), u(i).tk, t, UDmg%(u(i).kind), 1, i
END SUB

SUB Flip (cam AS ViewT)
    SCREEN , , 1 - cam.page, cam.page
    cam.page = 1 - cam.page
END SUB

SUB FxTick (fx() AS FxT, dt AS SINGLE)
    DIM i AS INTEGER
    FOR i = 1 TO MAXFX
        IF fx(i).alive THEN
            fx(i).t = fx(i).t + dt
            IF fx(i).t >= fx(i).life THEN
                fx(i).alive = 0
                IF fx(i).kind = FXSHELL THEN AddFx fx(), FXBOOM, fx(i).x2, fx(i).y2, 0, 0, .3
            END IF
        END IF
    NEXT
END SUB

SUB GoEnd (g AS GameT, tm() AS TeamT, won AS INTEGER, art() AS INTEGER, gl() AS INTEGER)
    g.won = won
    IF won THEN g.state = GWIN ELSE g.state = GLOSE
    g.endT = 0
    SOUND 37, 0
    RANDOMIZE TIMER
    DrawEndBack g, tm(), art(), gl()
    IF won THEN g.snd.song = 2 ELSE g.snd.song = 3
    g.snd.beat = 0
END SUB

SUB GoTitle (g AS GameT, art() AS INTEGER, gl() AS INTEGER)
    g.state = GTITLE
    RANDOMIZE 5
    DrawTitleBack art(), gl()
    RANDOMIZE TIMER
    g.snd.song = 1: g.snd.beat = 0
END SUB

' Damage dmg to a unit (k 1) or a building (k 2), from a unit or a building si (sk 1 or 2).
SUB Hit (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, k AS INTEGER, idx AS INTEGER, dmg AS INTEGER, sk AS INTEGER, si AS INTEGER)
    IF k = 1 THEN
        IF u(idx).alive = 0 THEN EXIT SUB
        u(idx).hp = u(idx).hp - dmg
        IF u(idx).hp <= 0 THEN KillUnit g, m(), u(), fx(), idx: EXIT SUB
        IF u(idx).st = SIDLE AND u(idx).kind <> UHARV THEN u(idx).st = SATTACK: u(idx).tk = sk: u(idx).ti = si: u(idx).agg = 0
        IF u(idx).team = 1 THEN g.ai.alarmK = sk: g.ai.alarmI = si: g.ai.alarmT = g.now
    ELSE
        IF bl(idx).alive = 0 THEN EXIT SUB
        bl(idx).hp = bl(idx).hp - dmg
        IF bl(idx).team = 1 THEN g.ai.alarmK = sk: g.ai.alarmI = si: g.ai.alarmT = g.now
        IF bl(idx).team = 0 AND g.now - g.alarmT > 10 THEN g.alarmT = g.now: g.alertX = bl(idx).x: g.alertY = bl(idx).y: Say g, "Our base is under attack! (Space)": Sfx g, 8
        IF bl(idx).hp <= 0 THEN KillBuilding g, m(), bl(), fx(), fk(), idx
    END IF
END SUB

SUB Keys (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, art() AS INTEGER, gl() AS INTEGER)
    DIM k AS STRING, i AS INTEGER, n AS INTEGER
    DO
        k = UCASE$(INKEY$)
        IF k = "" THEN EXIT DO
        SELECT CASE g.state
        CASE GTITLE
            IF k = CHR$(27) THEN g.quitting = -1 ELSE NewGame g, tm(), m(), u(), bl(), fx(), fk()
        CASE GPLAY
            SELECT CASE k
            CASE CHR$(27)
                IF g.placeKind >= 0 THEN
                    g.placeKind = -1
                ELSEIF g.clock - g.escT < 3 THEN
                    GoEnd g, tm(), 0, art(), gl()
                    EXIT SUB
                ELSE
                    g.escT = g.clock: Say g, "Esc again to surrender"
                END IF
            CASE CHR$(0) + "H": g.vw.camY = g.vw.camY - 16: ClampCam g.vw
            CASE CHR$(0) + "P": g.vw.camY = g.vw.camY + 16: ClampCam g.vw
            CASE CHR$(0) + "K": g.vw.camX = g.vw.camX - 32: ClampCam g.vw
            CASE CHR$(0) + "M": g.vw.camX = g.vw.camX + 32: ClampCam g.vw
            CASE "M"
                g.snd.enabled = NOT g.snd.enabled
                IF g.snd.enabled THEN Say g, "Sound on" ELSE Say g, "Sound off": SOUND 37, 0
            CASE "H"
                i = FindB%(bl(), 0, BHQ)
                IF i THEN CenterOn g.vw, bl(i).x, bl(i).y
            CASE "A"
                n = 0
                FOR i = 1 TO MAXU
                    u(i).sel = u(i).alive AND u(i).team = 0 AND u(i).kind <> UHARV
                    IF u(i).sel THEN n = n + 1
                NEXT
                Say g, LTRIM$(STR$(n)) + " of the army selected"
            CASE "1", "2", "3"
                n = 0
                FOR i = 1 TO MAXU
                    u(i).sel = u(i).alive AND u(i).team = 0 AND u(i).kind = VAL(MID$("231", VAL(k), 1))
                    IF u(i).sel THEN n = n + 1
                NEXT
                Say g, LTRIM$(STR$(n)) + " " + KName$(VAL(MID$("231", VAL(k), 1)) + 2) + "s selected"
            CASE " "
                IF g.alertX >= 0 THEN CenterOn g.vw, g.alertX, g.alertY
            CASE "S"
                FOR i = 1 TO MAXU
                    IF u(i).sel THEN u(i).st = SIDLE: u(i).agg = 0
                NEXT
            END SELECT
        CASE ELSE
            IF g.endT > 1 THEN GoTitle g, art(), gl()
        END SELECT
    LOOP
    IF g.state = GTITLE AND g.ms.lHit THEN
        NewGame g, tm(), m(), u(), bl(), fx(), fk()
        g.ms.lHit = 0
    ELSEIF g.state <> GPLAY AND g.ms.lHit AND g.endT > 1 THEN
        GoTitle g, art(), gl()
        g.ms.lHit = 0
    END IF
END SUB

FUNCTION KCost% (k AS INTEGER)
    SELECT CASE k
    CASE 0: KCost% = 200
    CASE 1, 5: KCost% = 250
    CASE 2: KCost% = 150
    CASE 3: KCost% = 300
    CASE ELSE: KCost% = 100
    END SELECT
END FUNCTION

SUB KillBuilding (g AS GameT, m() AS CellT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, b AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER, sz AS INTEGER
    sz = BSize%(bl(b).kind)
    bl(b).alive = 0
    IF bl(b).team = 0 THEN g.losses = g.losses + 1 ELSE g.kills = g.kills + 1
    FOR y = bl(b).y TO bl(b).y + sz - 1
        FOR x = bl(b).x TO bl(b).x + sz - 1
            m(x, y).bld = 0
            m(x, y).terr = TDIRT
            AddFx fx(), FXBIG, x * TS + TS \ 2, y * TS + TS \ 2, 0, 0, .5 + RND * .5
        NEXT
    NEXT
    ClearFields fk()
    g.vw.miniDirty = -1
    IF m(bl(b).x, bl(b).y).seen THEN Sfx g, 4
    IF bl(b).team = 0 THEN Say g, "We lost a building"
END SUB

SUB KillUnit (g AS GameT, m() AS CellT, u() AS UnitT, fx() AS FxT, i AS INTEGER)
    u(i).alive = 0
    u(i).sel = 0
    IF u(i).team = 0 THEN g.losses = g.losses + 1 ELSE g.kills = g.kills + 1
    IF m(u(i).x, u(i).y).occ = i THEN m(u(i).x, u(i).y).occ = 0
    IF m(u(i).nx, u(i).ny).occ = i THEN m(u(i).nx, u(i).ny).occ = 0
    AddFx fx(), FXBIG, u(i).px, u(i).py, 0, 0, .55
    IF u(i).team = 0 OR m(u(i).x, u(i).y).vis THEN Sfx g, 4
    IF u(i).team = 0 AND u(i).kind = UHARV THEN Say g, "A Harvester was destroyed"
END SUB

FUNCTION KMakes% (k AS INTEGER)
    SELECT CASE k
    CASE 0: KMakes% = BREF
    CASE 1: KMakes% = BBAR
    CASE 2: KMakes% = BTUR
    CASE ELSE: KMakes% = k - 2
    END SELECT
END FUNCTION

FUNCTION KName$ (k AS INTEGER)
    SELECT CASE k
    CASE 0: KName$ = "Refinery"
    CASE 1: KName$ = "Barracks"
    CASE 2: KName$ = "Turret"
    CASE 3: KName$ = "Harvester"
    CASE 4: KName$ = "Rifleman"
    CASE ELSE: KName$ = "Tank"
    END SELECT
END FUNCTION

FUNCTION KSecs! (k AS INTEGER)
    SELECT CASE k
    CASE 0, 3: KSecs! = 6
    CASE 1: KSecs! = 7
    CASE 2, 5: KSecs! = 5
    CASE ELSE: KSecs! = 2.5
    END SELECT
END FUNCTION

' Big letters for the title and the end, each pixel a block of bw by bh: lit from above, shadowed.
SUB Logo (gl() AS INTEGER, t AS STRING, y AS INTEGER, bw AS INTEGER, bh AS INTEGER)
    DIM x0 AS INTEGER, i AS INTEGER, r AS INTEGER, b AS INTEGER, mask AS INTEGER
    DIM x AS INTEGER, yy AS INTEGER, pass AS INTEGER
    x0 = (320 - LEN(t) * 8 * bw) \ 2
    FOR pass = 0 TO 2
        FOR i = 1 TO LEN(t)
            FOR r = 0 TO 7
                mask = gl(ASC(MID$(t, i, 1)), r)
                FOR b = 0 TO 7
                    IF mask AND 2 ^ (7 - b) THEN
                        x = x0 + ((i - 1) * 8 + b) * bw: yy = y + r * bh
                        SELECT CASE pass
                        CASE 0: LINE (x + 3, yy + 3)-STEP(bw - 1, bh - 1), 0, BF
                        CASE 1: LINE (x - 1, yy - 1)-STEP(bw + 1, bh + 1), 25, BF
                        CASE ELSE: LINE (x, yy)-STEP(bw - 1, bh - 1), LOGOC + r, BF
                        END SELECT
                    END IF
                NEXT
            NEXT
        NEXT
    NEXT
END SUB

' One step of unit i on the field to goal: on to the next tile's middle, or the next tile chosen.
' 0: on its way; 1: there (or as near as the others let it, unless exact); 2: no way there.
FUNCTION MoveStep% (g AS GameT, m() AS CellT, u() AS UnitT, fd() AS INTEGER, fk() AS FieldT, i AS INTEGER, goal AS INTEGER, exact AS INTEGER, dt AS SINGLE)
    DIM tx AS SINGLE, ty AS SINGLE, ddx AS SINGLE, ddy AS SINGLE, dist AS SINGLE, stride AS SINGLE
    DIM fi AS INTEGER, cur AS INTEGER, d AS INTEGER, nx AS INTEGER, ny AS INTEGER, v AS INTEGER
    DIM best AS INTEGER, bv AS INTEGER, lower AS INTEGER
    MoveStep% = 0
    IF u(i).nx <> u(i).x OR u(i).ny <> u(i).y THEN
        tx = u(i).nx * TS + TS \ 2: ty = u(i).ny * TS + TS \ 2
        ddx = tx - u(i).px: ddy = ty - u(i).py
        dist = SQR(ddx * ddx + ddy * ddy)
        stride = USpeed!(u(i).kind) * TS * dt
        u(i).anim = u(i).anim + dt
        IF dist <= stride THEN
            u(i).px = tx: u(i).py = ty
            IF m(u(i).x, u(i).y).occ = i THEN m(u(i).x, u(i).y).occ = 0
            u(i).x = u(i).nx: u(i).y = u(i).ny
        ELSE
            u(i).px = u(i).px + ddx / dist * stride: u(i).py = u(i).py + ddy / dist * stride
        END IF
        EXIT FUNCTION
    END IF
    fi = FlowField%(m(), fd(), fk(), goal, g.now)
    cur = fd(fi, u(i).x, u(i).y)
    IF cur = 0 THEN MoveStep% = 1: EXIT FUNCTION
    IF cur >= FAR THEN MoveStep% = 2: EXIT FUNCTION
    best = -1: bv = FAR
    FOR d = 0 TO 7
        nx = u(i).x + StepX%(d): ny = u(i).y + StepY%(d)
        IF Walk%(m(), u(i).x, u(i).y, d) THEN
            v = fd(fi, nx, ny)
            IF v < cur THEN
                lower = -1
                v = v * 2 + (d MOD 2)
                IF m(nx, ny).occ = 0 AND v < bv THEN bv = v: best = d
            END IF
        END IF
    NEXT
    IF best < 0 THEN
        ' the way is taken: wait, or call it there when near
        u(i).stuck = u(i).stuck + dt
        IF exact = 0 AND cur <= 2 AND u(i).stuck > .3 THEN MoveStep% = 1: u(i).stuck = 0: EXIT FUNCTION
        IF lower = 0 OR u(i).stuck > 3 THEN MoveStep% = 2: u(i).stuck = 0
        EXIT FUNCTION
    END IF
    u(i).stuck = 0
    u(i).nx = u(i).x + StepX%(best): u(i).ny = u(i).y + StepY%(best)
    m(u(i).nx, u(i).ny).occ = i
    u(i).dir = best
END FUNCTION

' The songs' next bar, all three voices at once, when the lead's queue runs low; the title's
' comes round again. In battle, the bass line alone, a bar ahead of the clock.
SUB Music (g AS GameT, tune() AS STRING)
    DIM b AS INTEGER
    IF g.snd.enabled = 0 THEN EXIT SUB
    IF g.state = GPLAY THEN
        IF g.snd.due < g.clock THEN g.snd.due = g.clock
        IF g.snd.due - g.clock < .5 THEN
            IF tune(4, g.snd.beat * 4) = "" THEN g.snd.beat = 0
            b = g.snd.beat * 4
            PLAY "", tune(4, b), "", tune(4, b + 1), tune(4, b + 2), tune(4, b + 3)
            g.snd.beat = g.snd.beat + 1
            g.snd.due = g.snd.due + 4 * 60 / 140
        END IF
        EXIT SUB
    END IF
    IF g.snd.song = 0 THEN EXIT SUB
    IF PLAY(0) < 3 THEN
        b = g.snd.beat * 3
        IF tune(g.snd.song, b) = "" THEN
            g.snd.beat = 0
            IF g.snd.song <> 1 THEN g.snd.song = 0: EXIT SUB
            b = 0
        END IF
        PLAY tune(g.snd.song, b), tune(g.snd.song, b + 1), tune(g.snd.song, b + 2)
        g.snd.beat = g.snd.beat + 1
    END IF
END SUB

FUNCTION NearestRef% (u() AS UnitT, bl() AS BldgT, i AS INTEGER)
    DIM b AS INTEGER, best AS INTEGER, d AS INTEGER, bd AS INTEGER
    bd = FAR
    FOR b = 1 TO MAXB
        IF bl(b).alive AND bl(b).team = u(i).team AND bl(b).kind = BREF THEN
            d = ABS(bl(b).x - u(i).x) + ABS(bl(b).y + 2 - u(i).y)
            IF d < bd THEN bd = d: best = b
        END IF
    NEXT
    NearestRef% = best
END FUNCTION

' A building of side ti at x, y: kind -1 the HQ, else the sidebar's kind. A refinery brings a harvester.
SUB PlaceBuilding (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fk() AS FieldT, ti AS INTEGER, kind AS INTEGER, x AS INTEGER, y AS INTEGER)
    DIM b AS INTEGER, bt AS INTEGER, i AS INTEGER, j AS INTEGER, k AS INTEGER
    DIM fresh AS BldgT
    bt = BHQ: IF kind >= 0 THEN bt = KMakes%(kind)
    FOR b = 1 TO MAXB
        IF bl(b).alive = 0 THEN EXIT FOR
    NEXT
    IF b > MAXB THEN EXIT SUB
    bl(b) = fresh
    bl(b).alive = -1: bl(b).kind = bt: bl(b).team = ti: bl(b).hp = BHp%(bt): bl(b).x = x: bl(b).y = y
    bl(b).ang = 1.57
    FOR j = y TO y + BSize%(bt) - 1
        FOR i = x TO x + BSize%(bt) - 1
            m(i, j).bld = b
        NEXT
    NEXT
    IF kind >= 0 AND tm(ti).sKind = kind AND tm(ti).sReady THEN tm(ti).sKind = -1: tm(ti).sReady = 0: tm(ti).sProg = 0
    ClearFields fk()
    g.vw.miniDirty = -1
    IF bt = BREF THEN
        IF m(x, y + 2).occ = 0 THEN k = SpawnUnit%(tm(), m(), u(), ti, UHARV, x, y + 2)
    END IF
    IF ti = 0 AND kind >= 0 THEN Say g, "The " + KName$(kind) + " stands"
END SUB

' Side ti's building and its two factories' units, a step on.
SUB Produce (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, ti AS INTEGER, dt AS SINGLE)
    IF tm(ti).sKind >= 0 AND tm(ti).sReady = 0 AND FindB%(bl(), ti, BHQ) > 0 THEN
        tm(ti).sProg = tm(ti).sProg + dt / KSecs!(tm(ti).sKind)
        IF tm(ti).sProg >= 1 THEN
            tm(ti).sProg = 1: tm(ti).sReady = -1
            IF ti = 0 THEN g.placeKind = tm(ti).sKind: Say g, "Left click places the " + KName$(tm(ti).sKind): Sfx g, 5
        END IF
    END IF
    IF tm(ti).hallKind < 0 AND tm(ti).q3 > 0 THEN tm(ti).hallKind = 3: tm(ti).q3 = tm(ti).q3 - 1: tm(ti).hallProg = 0
    IF tm(ti).denKind < 0 THEN
        IF tm(ti).q4 > 0 THEN
            tm(ti).denKind = 4: tm(ti).q4 = tm(ti).q4 - 1: tm(ti).denProg = 0
        ELSEIF tm(ti).q5 > 0 THEN
            tm(ti).denKind = 5: tm(ti).q5 = tm(ti).q5 - 1: tm(ti).denProg = 0
        END IF
    END IF
    Train g, tm(), m(), u(), bl(), ti, BHQ, tm(ti).hallKind, tm(ti).hallProg, dt
    Train g, tm(), m(), u(), bl(), ti, BBAR, tm(ti).denKind, tm(ti).denProg, dt
END SUB

' The loading screen's bar, on the page shown.
SUB Progress (n AS INTEGER, total AS INTEGER, gl() AS INTEGER)
    SCREEN , , 0, 0
    IF n = 1 THEN
        CLS
        TxtC gl(), "RUSTLINE", 70, 2, 8
        TxtC gl(), "the ore is waiting", 94, 1, 9
        LINE (109, 109)-(210, 116), 4, B
    END IF
    LINE (110, 110)-(110 + 99 * n \ total, 115), 8, BF
    SCREEN , , 1, 0
END SUB

FUNCTION Queued% (t AS TeamT, k AS INTEGER)
    SELECT CASE k
    CASE 3: Queued% = t.q3
    CASE 4: Queued% = t.q4
    CASE 5: Queued% = t.q5
    CASE ELSE: Queued% = 0
    END SELECT
END FUNCTION

SUB ReadMouse (ms AS MouseT)
    ms.x = PEN(4): ms.y = PEN(5): ms.down = PEN(3)
    ms.lHit = PEN(0)
    IF ms.lHit THEN ms.lX = PEN(1): ms.lY = PEN(2)
    ms.rHit = PEN(10)
    IF ms.rHit THEN ms.rX = PEN(11): ms.rY = PEN(12)
END SUB

SUB RenderEnd (g AS GameT, pt() AS PartT, art() AS INTEGER, gl() AS INTEGER)
    DIM i AS INTEGER, c AS INTEGER
    PUT (0, 0), art(0, ABACK), PSET
    FOR i = 1 TO NPART
        IF g.won THEN c = 8 ELSE c = FIRE + 6
        IF i MOD 3 = 0 THEN
            IF g.won THEN c = 31 ELSE c = FIRE + 2
        END IF
        PSET (pt(i).x, pt(i).y), c
    NEXT
    IF g.endT > 1 AND INT(g.clock * 2) MOD 2 THEN TxtC gl(), "CLICK TO CONTINUE", 156, 2, 15
    Arrow g.ms.x, g.ms.y
END SUB

SUB RenderGame (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, art() AS INTEGER, gl() AS INTEGER)
    DIM wx AS INTEGER, wy AS INTEGER, sx AS INTEGER, sy AS INTEGER, t AS INTEGER, lo AS INTEGER, hi AS INTEGER
    DIM k AS INTEGER, sz AS INTEGER, c AS INTEGER, ph AS INTEGER, k0 AS INTEGER, k1 AS INTEGER
    DIM tx(200) AS INTEGER, ty(200) AS INTEGER, nt AS INTEGER, x(3) AS INTEGER, y(3) AS INTEGER
    DIM l AS INTEGER, h AS INTEGER, j AS INTEGER, y0 AS INTEGER
    ' the ore breathes; the water ripples
    ph = INT(g.clock * 2.5) MOD 4
    IF ph <> g.vw.orePh THEN
        g.vw.orePh = ph
        SELECT CASE ph
        CASE 0: Shade 8, 20, 58, 40
        CASE 2: Shade 8, 40, 63, 56
        CASE ELSE: Shade 8, 28, 63, 46
        END SELECT
    END IF
    ' the water's colours turned a step: its swells roll on
    k = INT(g.clock * 6) MOD NWATER
    IF k <> g.vw.waterF THEN g.vw.waterF = k: WaterColours k
    g.vw.miniT = g.vw.miniT - .016

    ' the land a row of equal x + y at a time, from the back; the diamonds a half tile apart
    LINE (0, 0)-(SBX - 1, VIEWH - 1), 0, BF
    k0 = g.vw.camX \ 16 - 1: k1 = (g.vw.camX + VWP) \ 16 - 1
    ' the rows below the view too: their raised tops come up into it
    FOR t = (g.vw.camY - 8) \ 8 TO (g.vw.camY + VHP + MAXH * STEPH) \ 8
        sy = t * 8 - g.vw.camY + VY0
        lo = -INT(-(k0 + t - MAPH + 1) / 2): hi = INT((k1 + t - MAPH + 1) / 2)
        IF lo < t - MAPH + 1 THEN lo = t - MAPH + 1
        IF lo < 0 THEN lo = 0
        IF hi > t THEN hi = t
        IF hi > MAPW - 1 THEN hi = MAPW - 1
        FOR wx = lo TO hi
            wy = t - wx
            sx = (wx - wy + MAPH - 1) * 16 - g.vw.camX + VX0
            l = Lift%(m(), wx, wy)
            ' a cliff's steps down to the land in front, left and right
            IF m(wx, wy).seen AND sx >= 0 THEN
                FOR k = 0 TO 1
                    IF k = 0 THEN h = l - Lift%(m(), wx, wy + 1) ELSE h = l - Lift%(m(), wx + 1, wy)
                    FOR j = 1 TO h \ STEPH
                        y0 = sy + 8 - l + (j - 1) * STEPH
                        IF y0 >= 0 AND y0 + 7 + STEPH <= 199 THEN
                            PUT (sx + k * 16, y0), art(0, AFACE + k * 3 + 1), AND
                            PUT (sx + k * 16, y0), art(0, AFACE + k * 3), OR
                            IF m(wx, wy).vis = 0 THEN PUT (sx + k * 16, y0), art(0, AFACE + k * 3 + 2), OR
                        END IF
                    NEXT
                NEXT
            END IF
            sy = sy - l
            IF m(wx, wy).seen AND sx >= 0 AND sy >= ROCKH AND sy <= 184 THEN
                PUT (sx, sy), art(0, ADIA), AND
                PUT (sx, sy), art(0, AGRS + ((wx MOD 4) + (wy MOD 4) * 4 + 16 * (m(wx, wy).hgt \ 2)) * 2), OR
                k = Overlay%(g, m(), wx, wy)
                IF k >= 0 THEN
                    PUT (sx, sy), art(0, AOVL + k * 4 + 2), AND
                    PUT (sx, sy), art(0, AOVL + k * 4), OR
                END IF
                IF m(wx, wy).vis = 0 THEN PUT (sx, sy), art(0, AFOG), OR
                ' in the shade of a cliff behind
                IF Lift%(m(), wx - 1, wy) > l THEN PUT (sx, sy), art(0, ASHA), OR
                IF Lift%(m(), wx, wy - 1) > l THEN PUT (sx + 16, sy), art(0, ASHA + 1), OR
                IF m(wx, wy).terr = TROCK THEN
                    k = AROK + Sides%(m(), wx, wy) * 9
                    PUT (sx, sy - ROCKH), art(0, k + 3), AND
                    PUT (sx, sy - ROCKH), art(0, k), OR
                    IF m(wx, wy).vis = 0 THEN PUT (sx, sy - ROCKH), art(0, k + 6), OR
                END IF
                IF m(wx, wy).terr = TTREE AND nt < 200 THEN nt = nt + 1: tx(nt) = wx: ty(nt) = wy
            END IF
            sy = sy + l
        NEXT
    NEXT
    DrawObjects g, m(), u(), bl(), art(), tx(), ty(), nt
    VIEW SCREEN (VX0, VY0)-(SBX - 1, VIEWB - 1)
    DrawFx g, m(), fx()
    ' a building's place: its footprint, green where it fits
    IF g.placeKind >= 0 AND g.ms.x >= VX0 AND g.ms.x < SBX AND g.ms.y >= VY0 AND g.ms.y < VIEWB THEN
        PickTile g, m(), g.ms.x, g.ms.y, wx, wy
        sz = BSize%(KMakes%(g.placeKind))
        c = 29: IF CanPlace%(m(), bl(), 0, g.placeKind, wx, wy) THEN c = 8
        sx = (wx - wy + MAPH) * 16 - g.vw.camX + VX0: sy = (wx + wy) * 8 - g.vw.camY + VY0 - Lift%(m(), wx, wy)
        x(0) = sx: y(0) = sy
        x(1) = sx + sz * 16: y(1) = sy + sz * 8
        x(2) = sx: y(2) = sy + sz * 16
        x(3) = sx - sz * 16: y(3) = sy + sz * 8
        FOR k = 0 TO 3
            LINE (x(k), y(k))-(x((k + 1) MOD 4), y((k + 1) MOD 4)), c
        NEXT
        FOR k = 2 TO sz * 16 - 2 STEP 3
            LINE (x(0) - k, y(0) + k \ 2)-(x(1) - k, y(1) + k \ 2), c, , &HAAAA
        NEXT
    END IF
    IF g.ms.dragOn THEN LINE (g.ms.dX0, g.ms.dY0)-(g.ms.x, g.ms.y), 15, B, &HF0F0
    VIEW
    ' the view's frame, over what spilled past it
    LINE (0, 0)-(SBX - 1, VY0 - 2), 0, BF
    LINE (0, VY0 - 1)-(VX0 - 2, VIEWB), 0, BF
    LINE (0, VIEWB + 1)-(SBX - 1, VIEWH - 1), 0, BF
    LINE (VX0 - 1, VY0 - 1)-(SBX, VIEWB), 23, B
    DrawSidebar g, tm(), m(), u(), bl(), art(), gl()
    DrawCursor g, m(), u(), bl()
END SUB

SUB RenderTitle (g AS GameT, pt() AS PartT, art() AS INTEGER, gl() AS INTEGER)
    DIM i AS INTEGER, c AS INTEGER
    PUT (0, 0), art(0, ABACK), PSET
    FOR i = 1 TO NPART
        c = 8: IF i MOD 4 = 0 THEN c = 31
        PSET (pt(i).x, pt(i).y), c
        IF i MOD 3 = 0 THEN PSET (pt(i).x + 1, pt(i).y), 22
    NEXT
    IF INT(g.clock * 2) MOD 2 THEN c = 8 ELSE c = 15
    TxtC gl(), "CLICK TO BEGIN", 128, 2, c
    TxtC gl(), "Sidebar builds  H home  A army  M sound", 150, 1, 14
    TxtC gl(), "Esc leaves", 190, 1, 5
    Arrow g.ms.x, g.ms.y
END SUB

SUB Say (g AS GameT, t AS STRING)
    g.msg = t: g.msgT = 4
END SUB

' Unit i's nearest enemy within radius tiles, a unit before a building: its target. 0: none.
FUNCTION Scan% (u() AS UnitT, bl() AS BldgT, i AS INTEGER, radius AS SINGLE)
    DIM j AS INTEGER, d AS SINGLE, best AS SINGLE, bk AS INTEGER, bi AS INTEGER, half AS SINGLE
    DIM cx AS SINGLE, cy AS SINGLE
    best = (radius * TS) ^ 2
    FOR j = 1 TO MAXU
        IF u(j).alive AND u(j).team <> u(i).team THEN
            d = (u(j).px - u(i).px) ^ 2 + (u(j).py - u(i).py) ^ 2
            IF d < best THEN best = d: bk = 1: bi = j
        END IF
    NEXT
    IF bk = 0 THEN
        FOR j = 1 TO MAXB
            IF bl(j).alive AND bl(j).team <> u(i).team THEN
                half = BSize%(bl(j).kind) * TS / 2
                cx = bl(j).x * TS + half: cy = bl(j).y * TS + half
                d = (cx - u(i).px) ^ 2 + (cy - u(i).py) ^ 2 - half * half
                IF d < best THEN best = d: bk = 2: bi = j
            END IF
        NEXT
    END IF
    IF bk THEN u(i).tk = bk: u(i).ti = bi
    Scan% = bk
END FUNCTION

' The left button let go: a click picks one unit, a box every one of the Coalition's in it.
SUB SelectBox (g AS GameT, m() AS CellT, u() AS UnitT, x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER)
    DIM i AS INTEGER, sx AS INTEGER, sy AS INTEGER, e AS INTEGER, n AS INTEGER, army AS INTEGER
    IF x1 > x2 THEN SWAP x1, x2
    IF y1 > y2 THEN SWAP y1, y2
    FOR i = 1 TO MAXU: u(i).sel = 0: NEXT
    IF x2 - x1 < 5 AND y2 - y1 < 5 THEN
        e = PickUnit%(g, m(), u(), x1, y1, 0)
        IF e THEN
            u(e).sel = -1: Sfx g, 6
            ' twice on one: all of its kind in view
            IF e = g.clickU AND g.clock - g.clickT < .4 THEN
                FOR i = 1 TO MAXU
                    IF u(i).alive AND u(i).team = 0 AND u(i).kind = u(e).kind THEN
                        UnitScreen g, m(), u(), i, sx, sy
                        IF sx >= VX0 AND sx < SBX AND sy >= VY0 AND sy < VIEWB THEN u(i).sel = -1
                    END IF
                NEXT
            END IF
            g.clickU = e: g.clickT = g.clock
        END IF
        EXIT SUB
    END IF
    FOR i = 1 TO MAXU
        IF u(i).alive AND u(i).team = 0 THEN
            UnitScreen g, m(), u(), i, sx, sy
            sy = sy - 4
            IF sx >= x1 AND sx <= x2 AND sy >= y1 AND sy <= y2 THEN
                u(i).sel = -1: n = n + 1
                IF u(i).kind <> UHARV THEN army = -1
            END IF
        END IF
    NEXT
    ' the army before the harvesters, when the box holds both
    IF army THEN
        FOR i = 1 TO MAXU
            IF u(i).kind = UHARV THEN u(i).sel = 0
        NEXT
    END IF
    IF n THEN Sfx g, 6
END SUB

' The screen, its pages, the palette, the music.
SUB Setup (g AS GameT, tune() AS STRING)
    DIM i AS INTEGER, j AS INTEGER, v AS INTEGER, pre(3) AS STRING, bar(3) AS STRING
    SCREEN 13, , 1, 0
    g.vw.page = 1
    g.snd.enabled = -1
    GamePalette
    RESTORE Tunes
    FOR i = 1 TO 3
        READ pre(0), pre(1), pre(2)
        j = 0
        DO
            READ bar(0), bar(1), bar(2)
            FOR v = 0 TO 2
                IF bar(0) = "" THEN tune(i, j + v) = "" ELSE tune(i, j + v) = pre(v) + bar(v)
            NEXT
            j = j + 3
        LOOP WHILE bar(0) <> ""
    NEXT
    READ pre(0), pre(1), pre(2), pre(3)
    j = 0
    DO
        READ bar(0), bar(1), bar(2), bar(3)
        FOR v = 0 TO 3
            IF bar(0) = "" THEN tune(4, j + v) = "" ELSE tune(4, j + v) = pre(v) + bar(v)
        NEXT
        j = j + 4
    LOOP WHILE bar(0) <> ""
END SUB

' A sound for the battle. The guns and blasts are voice 3's noise, one at a time; the
' sidebar's beeps the lead's, when its queue has room: 1 rifle, 2 cannon, 3 beam, 4 blast,
' 5 ready, 6 click, 7 refused, 8 alarm, 9 credits.
SUB Sfx (g AS GameT, k AS INTEGER)
    IF g.snd.enabled = 0 OR g.state <> GPLAY THEN EXIT SUB
    IF k <= 4 THEN
        IF g.clock < g.snd.noise AND k <> 4 THEN EXIT SUB
        IF g.clock < g.snd.noise - .2 THEN EXIT SUB
        SELECT CASE k
        CASE 1: PLAY "", "", "W3H0,1,0,1V5L64O6C": g.snd.noise = g.clock + .04
        CASE 2: PLAY "", "", "W3H0,4,0,3V9L32O3C": g.snd.noise = g.clock + .1
        CASE 3: PLAY "", "", "W2H0,2,0,2V7L64O6CB": g.snd.noise = g.clock + .06
        CASE 4: PLAY "", "", "W3H0,9,0,8V12L8O2C": g.snd.noise = g.clock + .3
        END SELECT
        EXIT SUB
    END IF
    IF PLAY(0) > 6 THEN EXIT SUB
    IF k = 9 AND g.clock - g.snd.last < .07 THEN EXIT SUB
    g.snd.last = g.clock
    SELECT CASE k
    CASE 5: PLAY "T200L32O4CEGO5C"
    CASE 6: SOUND 800, .2
    CASE 7: PLAY "T200L16O2CO1G"
    CASE 8: PLAY "T180L16O4AO3AO4AO3A"
    CASE ELSE: SOUND 1900, .1
    END SELECT
END SUB

FUNCTION SpawnUnit% (tm() AS TeamT, m() AS CellT, u() AS UnitT, ti AS INTEGER, ut AS INTEGER, x AS INTEGER, y AS INTEGER)
    DIM i AS INTEGER, fresh AS UnitT
    SpawnUnit% = 0
    IF x < 0 OR y < 0 THEN EXIT FUNCTION
    FOR i = 1 TO MAXU
        IF u(i).alive = 0 THEN EXIT FOR
    NEXT
    IF i > MAXU THEN EXIT FUNCTION
    u(i) = fresh
    u(i).alive = -1: u(i).kind = ut: u(i).team = ti: u(i).hp = UHp%(ut)
    u(i).x = x: u(i).y = y: u(i).nx = x: u(i).ny = y
    u(i).px = x * TS + TS \ 2: u(i).py = y * TS + TS \ 2
    u(i).dir = 2 + ti * 4
    u(i).scan = RND * .5
    IF ut = UHARV THEN u(i).st = SSEEK
    m(x, y).occ = i
    tm(ti).built = tm(ti).built + 1
    SpawnUnit% = i
END FUNCTION

' One step of the game, 30 a second.
SUB Tick (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fd() AS INTEGER, fk() AS FieldT, art() AS INTEGER, gl() AS INTEGER)
    DIM i AS INTEGER, b AS INTEGER, dt AS SINGLE, pb AS INTEGER, eb AS INTEGER
    dt = STEPT
    g.now = g.now + dt
    g.msgT = g.msgT - dt
    Produce g, tm(), m(), u(), bl(), 0, dt
    Produce g, tm(), m(), u(), bl(), 1, dt
    FOR i = 1 TO MAXU
        IF u(i).alive THEN UnitTick g, tm(), m(), u(), bl(), fx(), fd(), fk(), i, dt
    NEXT
    FOR b = 1 TO MAXB
        IF bl(b).alive THEN
            IF bl(b).kind = BTUR THEN TurretTick g, m(), u(), bl(), fx(), fk(), b, dt
            IF bl(b).hp * 2 < BHp%(bl(b).kind) THEN
                bl(b).smoke = bl(b).smoke - dt
                IF bl(b).smoke <= 0 THEN
                    bl(b).smoke = .35
                    AddFx fx(), FXSMOKE, bl(b).x * TS + 4 + RND * 20, bl(b).y * TS + 9, 0, 0, 1
                END IF
            END IF
        END IF
    NEXT
    FxTick fx(), dt
    ' the Legion's tithe: a few credits a second, so it never stalls
    g.ai.coin = g.ai.coin + dt * 4
    IF g.ai.coin >= 1 THEN tm(1).cash = tm(1).cash + INT(g.ai.coin): g.ai.coin = g.ai.coin - INT(g.ai.coin)
    g.ai.think = g.ai.think - dt
    IF g.ai.think <= 0 THEN g.ai.think = 1: AIThink g, tm(), m(), u(), bl(), fk()
    g.visT = g.visT - dt
    IF g.visT <= 0 THEN g.visT = .25: UpdateVis g, m(), u(), bl()
    g.checkT = g.checkT - dt
    IF g.checkT <= 0 THEN
        g.checkT = .5
        FOR b = 1 TO MAXB
            IF bl(b).alive THEN
                IF bl(b).team = 0 THEN pb = pb + 1 ELSE eb = eb + 1
            END IF
        NEXT
        IF eb = 0 THEN
            GoEnd g, tm(), -1, art(), gl()
        ELSEIF pb = 0 THEN
            GoEnd g, tm(), 0, art(), gl()
        END IF
    END IF
END SUB

' What lies on the grass at x, y: an overlay, -1 none.
' What lies on the grass at x, y: an overlay, -1 none (rock, trees and buildings are drawn apart).
FUNCTION Overlay% (g AS GameT, m() AS CellT, x AS INTEGER, y AS INTEGER)
    SELECT CASE m(x, y).terr
    CASE TDIRT: Overlay% = 0
    CASE TGROUND2: Overlay% = 1
    CASE TORE
        IF m(x, y).amt >= 7 THEN
            Overlay% = 4
        ELSEIF m(x, y).amt >= 4 THEN
            Overlay% = 3
        ELSE
            Overlay% = 2
        END IF
    CASE TWATER: Overlay% = OWATER + Sides%(m(), x, y)
    CASE ELSE: Overlay% = -1
    END SELECT
END FUNCTION

' The sides of x, y where the land goes on as it is (the map's edge too): 1 north, 2 east,
' 4 south, 8 west.
FUNCTION Sides% (m() AS CellT, x AS INTEGER, y AS INTEGER)
    DIM t AS INTEGER, n AS INTEGER
    t = m(x, y).terr
    IF y = 0 THEN n = 1 ELSE IF m(x, y - 1).terr = t THEN n = 1
    IF x = MAPW - 1 THEN n = n OR 2 ELSE IF m(x + 1, y).terr = t THEN n = n OR 2
    IF y = MAPH - 1 THEN n = n OR 4 ELSE IF m(x, y + 1).terr = t THEN n = n OR 4
    IF x = 0 THEN n = n OR 8 ELSE IF m(x - 1, y).terr = t THEN n = n OR 8
    Sides% = n
END FUNCTION

' Side ti's unit of kind ut at the factory's work is done: trained, then placed by the factory.
SUB Train (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, ti AS INTEGER, fb AS INTEGER, kind AS INTEGER, prog AS SINGLE, dt AS SINGLE)
    DIM f AS INTEGER, s AS INTEGER
    IF kind < 0 THEN EXIT SUB
    f = FindB%(bl(), ti, fb)
    IF f = 0 THEN EXIT SUB
    prog = prog + dt / KSecs!(kind)
    IF prog < 1 THEN EXIT SUB
    prog = 1
    s = FindSpawn%(m(), bl(), f)
    IF s < 0 THEN EXIT SUB
    s = SpawnUnit%(tm(), m(), u(), ti, KMakes%(kind), s MOD MAPW, s \ MAPW)
    IF ti = 0 THEN Say g, "The " + KName$(kind) + " is ready": Sfx g, 5
    kind = -1: prog = 0
END SUB

' Text in the glyphs, each pixel sc square, see-through; TxtC centres it over a shadow.
SUB Txt (gl() AS INTEGER, t AS STRING, x AS INTEGER, y AS INTEGER, sc AS INTEGER, c AS INTEGER)
    DIM i AS INTEGER, r AS INTEGER, b AS INTEGER, mask AS INTEGER, px AS INTEGER, py AS INTEGER
    FOR i = 1 TO LEN(t)
        FOR r = 0 TO 7
            mask = gl(ASC(MID$(t, i, 1)), r)
            IF mask THEN
                FOR b = 0 TO 7
                    IF mask AND 2 ^ (7 - b) THEN
                        px = x + ((i - 1) * 8 + b) * sc: py = y + r * sc
                        IF sc = 1 THEN PSET (px, py), c ELSE LINE (px, py)-STEP(sc - 1, sc - 1), c, BF
                    END IF
                NEXT
            END IF
        NEXT
    NEXT
END SUB

SUB TxtC (gl() AS INTEGER, t AS STRING, y AS INTEGER, sc AS INTEGER, c AS INTEGER)
    Txt gl(), t, (320 - LEN(t) * 8 * sc) \ 2 + 1, y + 1, sc, 0
    Txt gl(), t, (320 - LEN(t) * 8 * sc) \ 2, y, sc, c
END SUB

FUNCTION UDmg% (k AS INTEGER)
    SELECT CASE k
    CASE URIFLE: UDmg% = 5
    CASE UTANK: UDmg% = 22
    CASE ELSE: UDmg% = 0
    END SELECT
END FUNCTION

FUNCTION UHp% (k AS INTEGER)
    SELECT CASE k
    CASE UHARV: UHp% = 180
    CASE URIFLE: UHp% = 55
    CASE ELSE: UHp% = 170
    END SELECT
END FUNCTION

' One step of unit i's orders.
SUB UnitTick (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fd() AS INTEGER, fk() AS FieldT, i AS INTEGER, dt AS SINGLE)
    DIM r AS INTEGER, d AS SINGLE, refinery AS INTEGER, tx AS SINGLE, ty AS SINGLE, t AS INTEGER, goal AS INTEGER
    DIM moving AS INTEGER, here AS INTEGER
    u(i).cool = u(i).cool - dt
    moving = u(i).nx <> u(i).x OR u(i).ny <> u(i).y
    here = u(i).y * MAPW + u(i).x
    SELECT CASE u(i).st
    CASE SIDLE
        IF moving THEN r = MoveStep%(g, m(), u(), fd(), fk(), i, here, 0, dt)
        IF u(i).kind = UHARV THEN
            IF u(i).team = 1 THEN u(i).st = SSEEK
        ELSE
            u(i).scan = u(i).scan - dt
            IF u(i).scan <= 0 THEN
                u(i).scan = .5
                IF u(i).team = 0 THEN d = URange!(u(i).kind) + 2.5 ELSE d = USight%(u(i).kind) + 1
                IF Scan%(u(), bl(), i, d) THEN u(i).st = SATTACK
            END IF
        END IF
    CASE SMOVE
        r = MoveStep%(g, m(), u(), fd(), fk(), i, u(i).gy * MAPW + u(i).gx, 0, dt)
        IF r THEN
            u(i).st = SIDLE: u(i).agg = 0
        ELSEIF u(i).agg THEN
            u(i).scan = u(i).scan - dt
            IF u(i).scan <= 0 THEN
                u(i).scan = .5
                IF Scan%(u(), bl(), i, URange!(u(i).kind) + 1.5) THEN u(i).st = SATTACK
            END IF
        END IF
    CASE SATTACK
        d = TargetDist!(u(), bl(), i)
        IF d < 0 THEN
            IF u(i).agg THEN u(i).st = SMOVE ELSE u(i).st = SIDLE
        ELSEIF moving THEN
            r = MoveStep%(g, m(), u(), fd(), fk(), i, here, 0, dt)
        ELSEIF d <= URange!(u(i).kind) THEN
            t = u(i).ti
            IF u(i).tk = 1 THEN
                tx = u(t).px - u(i).px: ty = u(t).py - u(i).py
            ELSE
                tx = (bl(t).x + BSize%(bl(t).kind) / 2) * TS - u(i).px
                ty = (bl(t).y + BSize%(bl(t).kind) / 2) * TS - u(i).py
            END IF
            u(i).dir = DirOf%(tx, ty)
            IF u(i).cool <= 0 THEN Fire g, m(), u(), bl(), fx(), fk(), i
        ELSE
            t = u(i).ti
            IF u(i).tk = 1 THEN goal = u(t).y * MAPW + u(t).x ELSE goal = bl(t).y * MAPW + bl(t).x
            r = MoveStep%(g, m(), u(), fd(), fk(), i, goal, 0, dt)
            IF r = 2 THEN u(i).st = SIDLE
            ' marching on a building, an enemy unit met comes first
            IF u(i).agg AND u(i).tk = 2 THEN
                u(i).scan = u(i).scan - dt
                IF u(i).scan <= 0 THEN
                    u(i).scan = .5
                    t = u(i).ti
                    IF Scan%(u(), bl(), i, URange!(u(i).kind) + 1) <> 1 THEN u(i).tk = 2: u(i).ti = t
                END IF
            END IF
        END IF
    CASE SSEEK
        IF moving = 0 AND m(u(i).x, u(i).y).terr = TORE THEN
            u(i).st = SGATHER: u(i).work = 0
        ELSE
            r = MoveStep%(g, m(), u(), fd(), fk(), i, OREKEY, 0, dt)
            IF r = 2 THEN u(i).st = SIDLE
        END IF
    CASE SGATHER
        u(i).work = u(i).work + dt
        u(i).anim = u(i).anim + dt
        IF u(i).work >= .3 THEN
            u(i).work = 0
            IF m(u(i).x, u(i).y).terr = TORE THEN
                m(u(i).x, u(i).y).amt = m(u(i).x, u(i).y).amt - 1
                u(i).load = u(i).load + 1
                IF m(u(i).x, u(i).y).amt <= 0 THEN DepleteOre g, m(), fk(), u(i).x, u(i).y
            END IF
            IF u(i).load >= LOADMAX THEN
                u(i).st = SRETURN
            ELSEIF m(u(i).x, u(i).y).terr <> TORE THEN
                u(i).st = SSEEK
            END IF
        END IF
    CASE SRETURN
        refinery = NearestRef%(u(), bl(), i)
        IF refinery = 0 THEN
            IF moving THEN r = MoveStep%(g, m(), u(), fd(), fk(), i, here, 0, dt)
            u(i).st = SIDLE
            IF u(i).team = 0 THEN Say g, "The Harvester needs a Refinery"
        ELSE
            r = MoveStep%(g, m(), u(), fd(), fk(), i, (bl(refinery).y + 2) * MAPW + bl(refinery).x, -1, dt)
            IF r = 1 THEN u(i).st = SUNLOAD: u(i).work = 0: u(i).dir = 6
        END IF
    CASE SUNLOAD
        u(i).work = u(i).work + dt
        IF u(i).work >= .12 THEN
            u(i).work = 0
            IF u(i).load > 0 THEN
                u(i).load = u(i).load - 1
                tm(u(i).team).cash = tm(u(i).team).cash + LOADVAL
                tm(u(i).team).harvested = tm(u(i).team).harvested + LOADVAL
                IF u(i).team = 0 THEN Sfx g, 9
            ELSE
                u(i).st = SSEEK
            END IF
        END IF
    END SELECT
END SUB

' What the Coalition sees now (and so has seen): round its units and buildings.
SUB UpdateVis (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT)
    DIM i AS INTEGER, x AS INTEGER, y AS INTEGER, r AS INTEGER, cx AS INTEGER, cy AS INTEGER
    FOR y = 0 TO MAPH - 1: FOR x = 0 TO MAPW - 1: m(x, y).vis = 0: NEXT: NEXT
    FOR i = 1 TO MAXU + MAXB
        r = -1
        IF i <= MAXU THEN
            IF u(i).alive AND u(i).team = 0 THEN cx = u(i).x: cy = u(i).y: r = USight%(u(i).kind)
        ELSEIF bl(i - MAXU).alive AND bl(i - MAXU).team = 0 THEN
            cx = bl(i - MAXU).x: cy = bl(i - MAXU).y: r = 5
        END IF
        IF r >= 0 THEN
            ' the higher, the further
            r = r + m(cx, cy).hgt \ 2
            FOR y = cy - r TO cy + r
                FOR x = cx - r TO cx + r
                    IF x >= 0 AND y >= 0 AND x < MAPW AND y < MAPH THEN
                        IF (x - cx) ^ 2 + (y - cy) ^ 2 <= r * r + r THEN
                            m(x, y).vis = 1
                            IF m(x, y).seen = 0 THEN m(x, y).seen = 1: g.vw.miniDirty = -1
                        END IF
                    END IF
                NEXT
            NEXT
        END IF
    NEXT
END SUB

FUNCTION URange! (k AS INTEGER)
    SELECT CASE k
    CASE URIFLE: URange! = 3.5
    CASE UTANK: URange! = 4.5
    CASE ELSE: URange! = 0
    END SELECT
END FUNCTION

FUNCTION URate! (k AS INTEGER)
    IF k = UTANK THEN URate! = 1.6 ELSE URate! = .7
END FUNCTION

FUNCTION USight% (k AS INTEGER)
    USight% = k + 2
END FUNCTION

FUNCTION USpeed! (k AS INTEGER)
    SELECT CASE k
    CASE UHARV: USpeed! = 1.9
    CASE URIFLE: USpeed! = 3.2
    CASE ELSE: USpeed! = 2.3
    END SELECT
END FUNCTION

' The selected units sent to tile wx, wy: to attack the unit pu or building pb picked there.
SUB Order (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, wx AS INTEGER, wy AS INTEGER, pu AS INTEGER, pb AS INTEGER)
    DIM i AS INTEGER, b AS INTEGER, got AS INTEGER, tk AS INTEGER, ti AS INTEGER
    IF pu THEN tk = 1: ti = pu
    b = pb: IF b = 0 THEN b = m(wx, wy).bld
    IF tk = 0 AND b THEN
        IF bl(b).team = 1 THEN tk = 2: ti = b
    END IF
    FOR i = 1 TO MAXU
        IF u(i).sel THEN
            got = -1
            u(i).agg = 0
            u(i).gx = wx: u(i).gy = wy
            IF u(i).kind = UHARV THEN
                u(i).st = SMOVE
                IF m(wx, wy).terr = TORE THEN u(i).st = SSEEK
                IF b THEN
                    IF bl(b).team = 0 AND bl(b).kind = BREF THEN u(i).st = SRETURN
                END IF
            ELSEIF tk THEN
                u(i).st = SATTACK: u(i).tk = tk: u(i).ti = ti
            ELSE
                u(i).st = SMOVE: u(i).agg = -1
            END IF
        END IF
    NEXT
    IF got THEN
        Sfx g, 6
        AddFx fx(), FXMARK, wx * TS + TS \ 2, wy * TS + TS \ 2, 0, 0, .4
    END IF
END SUB

' Overlay k on black (black left as the grass): 0 dirt, 1 flowers, 2 to 4 ore thin to thick,
' 5 a palm, then rock and water by their sides.
' Where pixel px, py of a 32 by 16 diamond lies on its tile: u east, v south, 0 to 1 inside.
SUB IsoUV (px AS INTEGER, py AS INTEGER, u AS SINGLE, v AS SINGLE)
    DIM ex AS SINGLE, ey AS SINGLE
    ex = (px + .5 - 16) / 16: ey = (py + .5) / 8
    u = (ey + ex) / 2: v = (ey - ex) / 2
END SUB

' A water or rock tile's edges where it meets other land: a shore, a cliff's lip and shadow.
SUB Particles (pt() AS PartT, falling AS INTEGER, dt AS SINGLE)
    DIM i AS INTEGER
    FOR i = 1 TO NPART
        IF pt(i).v = 0 THEN pt(i).x = RND * 318: pt(i).y = RND * 199: pt(i).v = 6 + RND * 18: pt(i).ph = RND * 6
        pt(i).ph = pt(i).ph + dt * 2
        IF falling THEN
            pt(i).y = pt(i).y + pt(i).v * dt
            IF pt(i).y > 199 THEN pt(i).y = 0: pt(i).x = RND * 318
        ELSE
            pt(i).y = pt(i).y - pt(i).v * dt
            IF pt(i).y < 0 THEN pt(i).y = 199: pt(i).x = RND * 318
        END IF
        pt(i).x = pt(i).x + SIN(pt(i).ph) * dt * 7
        IF pt(i).x < 0 THEN pt(i).x = 317
        IF pt(i).x > 317 THEN pt(i).x = 0
    NEXT
END SUB

FUNCTION Passable% (m() AS CellT, x AS INTEGER, y AS INTEGER)
    DIM t AS INTEGER
    Passable% = 0
    IF x < 0 OR y < 0 OR x >= MAPW OR y >= MAPH THEN EXIT FUNCTION
    IF m(x, y).bld THEN EXIT FUNCTION
    t = m(x, y).terr
    IF t = TGROUND OR t = TGROUND2 OR t = TDIRT OR t = TORE THEN Passable% = -1
END FUNCTION

' Whether a unit can step from x, y in heading d: onto open land no more than a ramp's step up or
' down, and on a diagonal past the two tiles it cuts between.
FUNCTION Walk% (m() AS CellT, x AS INTEGER, y AS INTEGER, d AS INTEGER)
    DIM nx AS INTEGER, ny AS INTEGER, h AS INTEGER
    Walk% = 0
    nx = x + StepX%(d): ny = y + StepY%(d): h = m(x, y).hgt
    IF Passable%(m(), nx, ny) = 0 THEN EXIT FUNCTION
    IF ABS(m(nx, ny).hgt - h) > 1 THEN EXIT FUNCTION
    IF d MOD 2 THEN
        IF Passable%(m(), nx, y) = 0 OR Passable%(m(), x, ny) = 0 THEN EXIT FUNCTION
        IF ABS(m(nx, y).hgt - h) > 1 OR ABS(m(x, ny).hgt - h) > 1 THEN EXIT FUNCTION
    END IF
    Walk% = -1
END FUNCTION

' How far tile x, y stands above the ground, in pixels; 0 off the map.
FUNCTION Lift% (m() AS CellT, x AS INTEGER, y AS INTEGER)
    Lift% = 0
    IF x >= 0 AND y >= 0 AND x < MAPW AND y < MAPH THEN Lift% = m(x, y).hgt * STEPH
END FUNCTION

SUB TurretTick (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, b AS INTEGER, dt AS SINGLE)
    DIM j AS INTEGER, d AS SINGLE, best AS SINGLE, cx AS SINGLE, cy AS SINGLE, want AS SINGLE, da AS SINGLE
    cx = bl(b).x * TS + TS \ 2: cy = bl(b).y * TS + TS \ 2
    bl(b).cool = bl(b).cool - dt
    bl(b).scan = bl(b).scan - dt
    IF bl(b).scan <= 0 THEN
        bl(b).scan = .4
        bl(b).ti = 0: best = (5.5 * TS) ^ 2
        FOR j = 1 TO MAXU
            IF u(j).alive AND u(j).team <> bl(b).team THEN
                d = (u(j).px - cx) ^ 2 + (u(j).py - cy) ^ 2
                IF d < best THEN best = d: bl(b).ti = j
            END IF
        NEXT
    END IF
    j = bl(b).ti
    IF j = 0 THEN EXIT SUB
    IF u(j).alive = 0 THEN bl(b).ti = 0: EXIT SUB
    want = Ang!(u(j).px - cx, u(j).py - cy)
    da = want - bl(b).ang
    DO WHILE da > 3.14159: da = da - 6.28318: LOOP
    DO WHILE da < -3.14159: da = da + 6.28318: LOOP
    IF ABS(da) < dt * 6 THEN bl(b).ang = want ELSE bl(b).ang = bl(b).ang + SGN(da) * dt * 6
    IF ABS(da) < .3 AND bl(b).cool <= 0 THEN
        bl(b).cool = 1.1
        ' the beam's colour is the side's: x2 below 0 marks the Legion's
        IF bl(b).team THEN d = -1 ELSE d = 1
        AddFx fx(), FXBEAM, cx + COS(bl(b).ang) * 7, cy + SIN(bl(b).ang) * 7, u(j).px * d, u(j).py, .12
        IF bl(b).team = 0 OR m(bl(b).x, bl(b).y).vis THEN Sfx g, 3
        Hit g, m(), u(), bl(), fx(), fk(), 1, j, 15, 2, b
    END IF
END SUB

FUNCTION TargetDist! (u() AS UnitT, bl() AS BldgT, i AS INTEGER)
    DIM t AS INTEGER, ex AS SINGLE, ey AS SINGLE, x1 AS SINGLE, y1 AS SINGLE, x2 AS SINGLE, y2 AS SINGLE
    t = u(i).ti
    TargetDist! = -1
    IF u(i).tk = 1 THEN
        IF u(t).alive = 0 OR u(t).team = u(i).team THEN EXIT FUNCTION
        TargetDist! = SQR((u(t).px - u(i).px) ^ 2 + (u(t).py - u(i).py) ^ 2) / TS
    ELSEIF u(i).tk = 2 THEN
        IF bl(t).alive = 0 OR bl(t).team = u(i).team THEN EXIT FUNCTION
        x1 = bl(t).x * TS: y1 = bl(t).y * TS
        x2 = x1 + BSize%(bl(t).kind) * TS: y2 = y1 + BSize%(bl(t).kind) * TS
        IF u(i).px < x1 THEN ex = x1 - u(i).px
        IF u(i).px > x2 THEN ex = u(i).px - x2
        IF u(i).py < y1 THEN ey = y1 - u(i).py
        IF u(i).py > y2 THEN ey = u(i).py - y2
        TargetDist! = SQR(ex * ex + ey * ey) / TS
    END IF
END FUNCTION

SUB NewGame (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT)
    DIM x AS INTEGER, y AS INTEGER, r AS STRING, c AS STRING, i AS INTEGER, s AS INTEGER, k AS INTEGER
    DIM blank AS UnitT, noB AS BldgT, noFx AS FxT, side AS TeamT
    SOUND 37, 0
    g.snd.song = 0: g.snd.beat = 0
    GamePalette
    RESTORE MapData
    FOR y = 0 TO MAPH - 1
        READ r
        FOR x = 0 TO MAPW - 1
            c = MID$(r, x + 1, 1)
            m(x, y).amt = 0: m(x, y).seen = 0: m(x, y).vis = 0: m(x, y).occ = 0: m(x, y).bld = 0
            SELECT CASE c
            CASE ",": m(x, y).terr = TGROUND2
            CASE ":": m(x, y).terr = TDIRT
            CASE "#": m(x, y).terr = TROCK
            CASE "~": m(x, y).terr = TWATER
            CASE "m": m(x, y).terr = TORE: m(x, y).amt = 6 + INT(RND * 5)
            CASE "T": m(x, y).terr = TTREE
            CASE ELSE: m(x, y).terr = TGROUND
            END SELECT
        NEXT
    NEXT
    RESTORE HeightData
    FOR y = 0 TO MAPH - 1
        READ r
        FOR x = 0 TO MAPW - 1: m(x, y).hgt = VAL(MID$(r, x + 1, 1)): NEXT
    NEXT
    FOR i = 1 TO MAXU: u(i) = blank: NEXT
    FOR i = 1 TO MAXB: bl(i) = noB: NEXT
    FOR i = 1 TO MAXFX: fx(i) = noFx: NEXT
    side.sKind = -1: side.hallKind = -1: side.denKind = -1
    tm(0) = side: tm(1) = side
    tm(0).cash = 900: tm(1).cash = 700
    ClearFields fk()
    g.alertX = -1: g.clickU = 0
    g.now = 0: g.placeKind = -1: g.ms.dragOn = 0: g.msgT = 0: g.alarmT = -99: g.checkT = 0
    g.ai.think = 3: g.ai.wave = 0: g.ai.size = 3: g.ai.nextWave = 75: g.ai.alarmT = 0: g.ai.coin = 0

    ' the Coalition, south-west; the Legion, north-east
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 0, -1, 3, 24
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 0, 0, 7, 23
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, -1, 34, 3
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, 0, 30, 2
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, 1, 34, 6
    FOR i = 1 TO 2
        FOR k = 0 TO 1
            s = FindSpawn%(m(), bl(), FindB%(bl(), k, BHQ))
            s = SpawnUnit%(tm(), m(), u(), k, URIFLE, s MOD MAPW, s \ MAPW)
        NEXT
    NEXT
    tm(0).built = 0: tm(1).built = 0
    UpdateVis g, m(), u(), bl()
    g.vw.miniDirty = -1: g.vw.miniT = 0
    CenterOn g.vw, 7, 22
    g.state = GPLAY
    Say g, "Mine ore, build, find the Scorch Legion"
    Sfx g, 5
END SUB

SUB Sprite (art() AS INTEGER, idx AS INTEGER, x AS INTEGER, y AS INTEGER)
    PUT (x, y), art(0, AMSK + idx), AND
    PUT (x, y), art(0, ASPR + idx), OR
END SUB
