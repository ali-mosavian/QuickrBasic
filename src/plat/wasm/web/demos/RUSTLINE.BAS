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
' VX0, VY0 to SBX, VIEWB on the screen, the camera in the plane's pixels, by half tiles; PUT clips
' to the VIEW, so what spills past its edges is cut there.
CONST MAPW = 64, MAPH = 48, TS = 16, SBX = 240, VIEWH = 192
CONST VX0 = 0, VY0 = 0, VIEWB = 192, VWP = SBX - VX0, VHP = VIEWB - VY0
' STEPH: a level's height in pixels; MAXH the highest level
CONST PLANEW = (MAPW + MAPH) * 16, PLANEH = (MAPW + MAPH) * 8, ROCKH = 6, STEPH = 12, MAXH = 2
CONST MAXU = 64, MAXB = 32, MAXFX = 64, NF = 24, NPART = 48
CONST STEPT = .0333333
CONST GTITLE = 0, GPLAY = 1, GWIN = 2, GLOSE = 3
CONST TGROUND = 0, TGROUND2 = 1, TDIRT = 2, TROCK = 3, TWATER = 4, TORE = 5, TTREE = 6
CONST UHARV = 1, URIFLE = 2, UTANK = 3, UROCKET = 4, UBUGGY = 5, UARTY = 6, NUNIT = 6
CONST BHQ = 1, BREF = 2, BBAR = 3, BTUR = 4, BPOW = 5, BFAC = 6, NBLD = 6
CONST SIDLE = 0, SMOVE = 1, SATTACK = 2, SSEEK = 3, SGATHER = 4, SRETURN = 5, SUNLOAD = 6
CONST FXBOOM = 1, FXBIG = 2, FXTRACE = 3, FXSHELL = 4, FXBEAM = 5, FXSMOKE = 6, FXMARK = 7
CONST OREKEY = -2, NOKEY = -99, FAR = 32000, LOADMAX = 10, LOADVAL = 25
CONST MINIX = 248, MINIY = 13, BTNY = 72
' the sidebar's items: NBITEM buildings (power plant, refinery, barracks, factory, turret), then
' the units (harvester, rifleman, rocketeer, buggy, tank, artillery), a cell of CELLW by CELLH each
CONST NITEM = 11, NBITEM = 5, CELLW = 26, CELLH = 27
' the palette: 0 to 103 the land's, units' and buildings' (from 32 the art tool's), 104 on the
' water's, turned round to move it (WATERC), 112 on fire's;
' in play 128 to 255 the same in the fog (FOG OR c), on the title and the end their ramps
CONST WATERC = 104, NWATER = 8, FOG = 128, FIRE = 112, SKY = 128, PLANET = 160, LOGOC = 192
' the art, one array of IMG-element slots: images of 16 by 16 (a unit's sprite, mask and shadow),
' the minimap, the full screen.
' Past 64 KB: QB 4.5 needs /AH.
' a unit's sprite, mask and shade, a slot each: by kind, side, heading, frame.
' the land, diamonds of 2 slots: 16 of sand at each of 3 levels, a 4 by 4 piece of it each; the
' diamond's mask; the fog; what lies on the sand, an image and a mask each: dirt, scrub, ore thin
' to thick, water by which of its 4 sides go on (OWATER + sides); rock, raised, by its sides:
' image, mask, fog, 3 slots each; a palm's image, mask, fog and shade, 2 slots each; a cliff's
' left and right faces, a level high: image, mask, fog, 2 slots each; the shade at a cliff's foot
' by its left and right back edges; the ramps (rising north from level 0, 1, then west): image,
' mask, fog, 4 slots each; the buildings' image, mask and fog, BLDW slots each
CONST OWATER = 5, NOVL = 21, BLDW = 15
CONST IMG = 130, NSPR = NUNIT * 32, ASPR = 0, AMSK = NSPR, ASHD = 2 * NSPR, AGRS = 3 * NSPR, ADIA = AGRS + 96, AFOG = ADIA + 2
CONST AOVL = AFOG + 2, AROK = AOVL + NOVL * 4, ATREE = AROK + 16 * 9, AFACE = ATREE + 8, ASHA = AFACE + 12
CONST ARMP = ASHA + 2, ABLD = ARMP + 48, AICN = ABLD + NBLD * 6 * BLDW, AMINI = AICN + NBLD - 1, ABACK = AMINI + 19, NSLOT = ABACK + 247

TYPE CellT
    terr AS INTEGER
    amt AS INTEGER   ' ore left
    seen AS INTEGER  ' the Coalition has seen it
    vis AS INTEGER   ' the Coalition sees it now
    occ AS INTEGER   ' the unit on it, or moving onto it
    bld AS INTEGER   ' the building on it
    hgt AS INTEGER   ' its level, 0 to MAXH
    rmp AS INTEGER   ' a ramp up a level: 1 rising north, 2 west
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

' A side: its credits, the building it builds, its three lines' work and queues, its power.
TYPE TeamT
    cash AS INTEGER
    sKind AS INTEGER ' the building being built, -1 none
    sProg AS SINGLE
    sReady AS INTEGER
    l0Kind AS INTEGER ' what its refinery, barracks and factory make, -1 nothing
    l0Prog AS SINGLE
    l1Kind AS INTEGER
    l1Prog AS SINGLE
    l2Kind AS INTEGER
    l2Prog AS SINGLE
    q5 AS INTEGER    ' the units waiting, by item
    q6 AS INTEGER
    q7 AS INTEGER
    q8 AS INTEGER
    q9 AS INTEGER
    q10 AS INTEGER
    pw AS INTEGER    ' power made, used; low when it uses more
    pu AS INTEGER
    low AS INTEGER
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
DECLARE FUNCTION LiftAt! (m() AS CellT, px AS SINGLE, py AS SINGLE)
DECLARE FUNCTION Corner% (m() AS CellT, x AS INTEGER, y AS INTEGER, c AS INTEGER)
DECLARE FUNCTION Joined% (m() AS CellT, x AS INTEGER, y AS INTEGER, nx AS INTEGER, ny AS INTEGER)
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
DECLARE FUNCTION Allowed% (bl() AS BldgT, ti AS INTEGER, k AS INTEGER)
DECLARE FUNCTION NeedB% (bl() AS BldgT, ti AS INTEGER, k AS INTEGER)
DECLARE FUNCTION BName$ (bt AS INTEGER)
DECLARE FUNCTION BGives% (bt AS INTEGER)
DECLARE FUNCTION BUses% (bt AS INTEGER)
DECLARE FUNCTION KLine% (k AS INTEGER)
DECLARE FUNCTION UItem% (ut AS INTEGER)
DECLARE FUNCTION UClass% (ut AS INTEGER)
DECLARE FUNCTION DmgVs% (ut AS INTEGER, c AS INTEGER)
DECLARE FUNCTION Splash! (ut AS INTEGER)
DECLARE FUNCTION ItemAt% (x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION LineKind% (t AS TeamT, l AS INTEGER)
DECLARE FUNCTION LineProg! (t AS TeamT, l AS INTEGER)
DECLARE FUNCTION NextQ% (t AS TeamT, l AS INTEGER)
DECLARE SUB LineClear (t AS TeamT, l AS INTEGER)
DECLARE SUB AIBuy (tm() AS TeamT, bl() AS BldgT, k AS INTEGER)
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
DIM art(IMG - 1, NSLOT - 1), gl(126, 7), tune(1 TO 4, 511) AS STRING
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
' each tile's level; a, b a ramp rising north from level 0, 1; A, B rising west
DATA "2222B111122222B10000001111111A0111122211000001122B11111111111111"
DATA "2222111122222221100001111111111111112111000011122B11111111111111"
DATA "222111112222222110000111221111111111b111A00011122111111111111111"
DATA "1111111222222221100000112222211111111111100111122111111111111111"
DATA "11111122222222211A0000011222222111110a01111111122111111111111111"
DATA "1111122222222221110110002222222211100000111111122111111111111111"
DATA "1111122222222221110111000112222211100000111111112111111111111111"
DATA "111111222222222B111111101012222211100001111111222111111111111111"
DATA "1111111222222221111111100012221111100001111112222111111111111111"
DATA "11111111112222221111111111111b1111100011111112222111111111111111"
DATA "1111111111111122222211111111111111100111111122222111111111111111"
DATA "1111111111111112222221111100000000111111111122222B11111111111111"
DATA "1111111111111112222221110000000000111111111122222222211111000011"
DATA "111111111111111122221111A000000000011111111112222221111110000011"
DATA "1111111111111111112111111110000000000111111111122211111000000011"
DATA "1111111111111111111111111110001111000011111111111111110000000011"
DATA "1111111111111111111111111111111111000000111111111111A00000000011"
DATA "1111111111111110000011111111111111000000011111111100000000000011"
DATA "1111111111111100000001111222100111000000001110000000000000000011"
DATA "1111111111111100000001112222100011100000001100000000000000000111"
DATA "1111111111111110000001112222100011110000000000000000000000011111"
DATA "1111111111111110000001112222101011111111000000000000000001111111"
DATA "22B11111111111100000011222221111111111111000000000000000111122B1"
DATA "22B1111111112110000001122222111111112111100000000000000011122221"
DATA "2221111111112110000001122222111111222211100000000000000111222222"
DATA "2221111111122111000001122222111111112b11100000011111111111222211"
DATA "22211111112221110000011122b1111111111111000001111111111111122111"
DATA "111111122222221110000011111111010111111A000011111111111111111111"
DATA "1112222222222211100000111111111000100000000011112222222111111100"
DATA "11122222222222211100000aa111121010000000000111122222222211110000"
DATA "111222222222222111000000001112B1000000000001112222222222111a0000"
DATA "1112222222222211110000000011122100000000001111122222222211100000"
DATA "111222222222B1111110000000011b21100000000111111111b2222211100000"
DATA "1111111111111111111000000001112111000001111111111111122211100000"
DATA "aa00000000000000111000000001112B11000111111111100011112111100000"
DATA "00000000000000001111A0000000112111100011111111000001111111100000"
DATA "00000000000000000111A0000000112111111111112B1a000000111111100001"
DATA "0000000000000000111100000000111111111111111110000000111111111111"
DATA "0000000000000000111100000000011111100011111110000000011111111111"
DATA "000000000000000011110000000000110000000111111A000000000011111111"
DATA "0000000000000000111100000000000000000000000111000000000001111111"
DATA "0000000000000000111110000000000000000000000011110000000000111111"
DATA "0000000000000000111110000000000000000000000001111100000000111111"
DATA "0000000000000000111110000000000000000000000000111100000000111111"
DATA "0000000000000000111110000000000000000000000000011110000000111111"
DATA "0000000000000000111110000000000000000000000000001111000001111111"
DATA "0000000000000000111110000000000000011110000000000111110001111111"
DATA "00000000000000001111100000000000001111111A0001100011111111111111"

PalData:
' colours 32 to 103: red, green, blue
DATA 14, 14, 17
DATA 8, 7, 4
DATA 47, 36, 22
DATA 13, 13, 13
DATA 16, 16, 19
DATA 4, 12, 24
DATA 63, 59, 19
DATA 11, 28, 8
DATA 54, 42, 26
DATA 42, 43, 47
DATA 43, 33, 23
DATA 62, 31, 7
DATA 21, 16, 10
DATA 5, 5, 6
DATA 33, 25, 18
DATA 3, 9, 19
DATA 59, 46, 30
DATA 17, 17, 17
DATA 27, 22, 6
DATA 52, 40, 25
DATA 6, 6, 8
DATA 27, 28, 33
DATA 38, 31, 22
DATA 27, 11, 2
DATA 15, 12, 8
DATA 25, 18, 12
DATA 35, 34, 35
DATA 4, 4, 5
DATA 11, 8, 5
DATA 36, 27, 17
DATA 7, 7, 9
DATA 31, 31, 33
DATA 63, 59, 25
DATA 5, 5, 6
DATA 39, 40, 46
DATA 10, 10, 12
DATA 50, 38, 23
DATA 6, 16, 34
DATA 44, 44, 49
DATA 54, 45, 10
DATA 9, 9, 11
DATA 6, 6, 8
DATA 46, 35, 21
DATA 6, 6, 7
DATA 30, 48, 63
DATA 61, 49, 32
DATA 19, 30, 40
DATA 12, 12, 15
DATA 18, 18, 21
DATA 15, 24, 32
DATA 25, 25, 29
DATA 20, 16, 12
DATA 12, 19, 25
DATA 15, 10, 2
DATA 51, 39, 24
DATA 51, 41, 25
DATA 18, 14, 10
DATA 45, 34, 19
DATA 33, 33, 38
DATA 30, 23, 16
DATA 37, 28, 19
DATA 22, 22, 25
DATA 53, 42, 26
DATA 30, 21, 12
DATA 49, 37, 23
DATA 38, 31, 9
DATA 46, 47, 53
DATA 16, 15, 16
DATA 19, 20, 23
DATA 55, 43, 27
DATA 35, 54, 63
DATA 57, 45, 28

ArtData:
' sand by level, what lies on it, rock, the palm, a cliff's faces, the ramps, the buildings,
' the units: width, height, then rows of symbols (Unpack); tools/rustline_art.py writes these
DATA 32, 16
DATA "pc!CE}ce!'!C!'yc!-E!CE!'3!:V!A!Auc!-!-!<!-EE3!9!A!Ae!HqcE!-E!-E!-!C!C!:3!A!Hf!A!HmcdE!C!CE!'!CV3g!A"
DATA "!:!:!Ad!:ic!CE!C!'!C!'!C!'!93!A!:!A!H!A!H!A!:!:V3!9!'3!CEecE!C!'!C!'!'!C!'!C!C3!9d!:e!A!:33!C!CEE!-"
DATA "dEccE!C!CE!C!'E!CEE!C!'!'3!93!93!9!'!C!'E!-!<!-!<5!-!<ecEE!CE!-!-!<!-dE!C!CE!C!C!'!'!C!CEE!-!-!<!<ic"
DATA "EE!-E!-!<!-!-E!-!-EE!C!'!C!9!'!'3!CEmcgEe!-EE!C!'!9!9!'!'qc!CE!CE!CE!CE!CE!CE!C!'uc!'!'!Ce!'d!Cyc!'"
DATA "!C!'3!93}c!C!'pc"
DATA ""
DATA 32, 16
DATA "pc!-!<}cd!<d!-ycE!-!-EE!-E!CEEucd!ChE!CdEqc!CEE!-!-EE!-E!-EE!CEE!CE!Cmc!'!'!C!C!-E!-!-E!-E!-!-EE!CE"
DATA "!CEE!-!-ic3!9!:3!93!C!'fE!-!-EE!-E!-!-E!-dEec3!9Ve3!'d!CdEf!-d!<!-!<!-!-eEcc!CE!CEE!CE!C!-Ef!-!<!<!-"
DATA "!<%!<%!<5!-!<E!-EEece!-!<!<!-!<e!-!<!-!-d!<%d!<!-!<!-!-ic!-!<!<5!<5!<!-!<!-!<!-!-!<!<!-!<%!<%!<5mc!-"
DATA "!-e!<%!<%h!<!-!<qcE!-!<!-!<%!<%!<5!<5!<5ucEE!-!<!-!<!-d!<ycdE!-!-!<}c!C!Cpc"
DATA ""
DATA 32, 16
DATA "pc!CE}c!-dE!CEyc!-!<!-!<!-!<!<!-!<%uce!-g!<e%qc!-!<!-E!-!<!<5!<5!<!<%!<%!<%(mc!-!<d!-d!<%d!<%!<%%!<%"
DATA "!<%!<%ic!<!-!<5!<%!<%!<%%!<%!<%!<%!<%d!<!-!<EEecEEe!-g!<g%!<%!<!<!-!-EE!CE!C!Cccd!CEE!CE!-!-!<!<!-!<"
DATA "%%(%(!<%!-!<E!-E!-E!-E!Cec!'!'!C!CE!C!-E!-!-!<!-!<!<%d!<e!-!<d!-ic!'d!CE!-!-fE!-E!-!<!-!<!<5!<5mc!C"
DATA "!CdE!-EE!CE!CdE!-E!-!-qc!-E!-!<!-!<!-!-E!-E!-E!Cuc!-!<%d!<!-!-EEyc%!<!<%!-!-}c!<!-pc"
DATA ""
DATA 32, 16
DATA "pc!C!'}cEE!C!C3!9yc!-!<!-EE!-!C!'!93ucd!<f!-EE!'!C!'!'qcE!-!-EE!-dE!C!CEE!CdE!-mchEe!C!'!C!CEE!<!-"
DATA "e!<icE!C!CEE!CE!-E!C!C!'!'!C!C!'!CEE!-!<!-!<%!<!<ecE!-e!CEE!-!-EEe!CdE!C!CE!-!-d!<!-E!-cc!'!C!'3!C!C"
DATA "!-E!<5!-!<E!CE!Cd!-EE!CE!-!-EE!C!'!Cec!93!'!C!-!-!<!<!-!<e!-!<e!-EE!-E!-!CEic!'!CEE!-!<!-!<!-!<d!-E"
DATA "!-E!-E!-!<!-!<mc!C!CEd!-!<d!-eE!-!-!<!-qcEE!-E!<!-!-!<dE!-E!-uc!-!-!<d!-eEyc!-!<E!-!CE}cE!-pc"
DATA ""
DATA 32, 16
DATA "pcE!-}c!'d!CE!Cyc!A!:!:!A3V!'!CE!Cucd!A!Hd!A!:!93E!CEEqc3Vd!A!H!A!:3!9!9!'E!C!-EE!-mc!'!'d!:e!A!:!'"
DATA "!9!C!C!-E!<!-EE!C!'ic!C!C!9!'!:!A!A!:!A!:!:V!'!'E!C!-!<!<!-EE!'3!'3ec!C!C!'!C33!A!Ae!H!A!A3!9EE!-!-"
DATA "!<d!-EEe!Ccc!'!C!93!:3!A!H!J!H!J4!J!H!A!:!9!'!CE!-E!<!-!-!<!-EE!-ec3!9!93!A!A!J!H!J!J4!J!J!H!A!A!93E"
DATA "!Cd!-!<!-!-ic!9!'!:V!A!A!H!J!J!H!J!J!H!A!:V!'3EE!-!<mce!:!A!A!H!H!Jd!H!A!A!:!:!'!'qc!:V!:V!A!:d!A!H"
DATA "!A!:!:Vuc!A!Ae!:3!:3!9yc!A!:3V!'!C}c3!'pc"
DATA ""
DATA 32, 16
DATA "pcE!C}c!-!-E!-!-Eyc!-!<!-EE!-E!-!-EucdE!-EE!CfE!-!-qc!'3!CE!'!C!'3!C!'!CE!CE!CEE!Cmc!'!'e!C!'!'!933"
DATA "!93!9!'!'!Ce!'!CicE!CE!-EE!CE!C!C!'3!'3!'3!'3!C!'!'!C!'3!CEecEEe!-Eh!CEEg!-!CEd!'d!Cccd!-EE!C!C!'!'3"
DATA "!C!'E!C!-!<%!<%!<!<%!-EE!CE!C!CEec!-E!C!C3!'3!9!'!'!CE!<!-!<f%!<!<e!-ic!CE!'!C!933!9!C!'!-E!<!-!<%!<"
DATA "%!<!-E!-mc!CEe!'!C!CEd!-!<!-!<!<E!-qcE!-Ee!-!<!-E!-!<!-!<uc!<!-!<!-!<e!-!<yc!-!<!<5!<!-}c!-!-pc"
DATA ""
DATA 32, 16
DATA "pc!C!'}cEEd!C!'ycE!C!CE!C!C!'!C!'3uc!C!C3d!9e!'3d!'qcE!C!9!'!:!A!:!A!:V3V3!9!'!CE!Cmc!-!<EE3!'!:!:"
DATA "h!A!:!'!9EE!-!-icd!<%!<5EE!'3!:3f!A!:3!9!CE!-!-!<%ec!-!-!<!<d%d!<!-E!C!C!9!:d!A!:33!'!'EE!-!<!-!<cc"
DATA "!CE!-!-!<%!<%!<!-!-!<E!-!C!'!:3!:!A!:V3!9!C!'E!-!CEecEE!-d!<!-!<!<d!-EE3!'!93!933!9!'!CE!Cic!-!<!-!<"
DATA "e!-E!-!CE!'!Cd!'!C!'3!'3mc!C!Ce!'!9!93!9!'!9d!'!C!'!'qc!:V!:V!:!A!:!A!:!A!:3!'3uc33!:!:!A!:e!Ayc!'3"
DATA "!93!:V}cEEpc"
DATA ""
DATA 32, 16
DATA "pc!C!'}c!C!'d3!:yc!'!C!'3!:3!:V3!9uc!'!'e!C3!9e!:33qc!C!'!C!'E!C!-!-!CE3!9!:V!:V!'3mc!93!'!'!C!C!-E"
DATA "!<!-!-E!'!C3!93!:!'!'EEic!:V!93!'3!C!'!-E!-EE!C!CE!'33!9!'!'!-E!<!-ece!'g!CeE!Cf!'!9!9!C!'EE!-!<!-!<"
DATA "ccE!CE!CE!-E!-!-E!-!<E!-!CE!'!C!'!C!'3!C!'E!C!-E!-!-ec!-!-!<!<%!<%e!<!-EE!C!C!'!'3e!'!93!'ic%!<%(d%"
DATA "!<!<!-EE!CE!'3!933!9!A!:mch%d!<E!-Ed!C!93qc%!<%!<%(%!<!<!-EE!C!Cuce%!<%!-!-E!Cyc!<5!-!-E!C}cE!Cpc"
DATA ""
DATA 32, 16
DATA "pc!CE}c!-E!C!C3!'ycE!C!CE!'!C!'3!'!Cuc!'!'!C!'!'!Ce!'3!'!93qc!:!93!9!'d!C!'33!9!:3!:V!:VmcV3!:!:!93"
DATA "d!'!C!'!'!933g!:!Aic!:V!:V!:33!9!9!'!'3d!'!C!'!'!93!:V!A!H!A!Hece!:33!'!9g!'!CEe!-!C!C3!'3!:!:!A!:!:"
DATA "cc!'!C!'e!CeE!CE!C!-E!<%!<5!-!<!-!-EE!CE!C!CecdEd!-E!-eE!<!-!<f%!<%%d!<ic!-!-E!-E!CE!CE!C!-!-%!<%(%("
DATA "d%!<mc!-dE!CdE!-!-!<!<g%qcE!-!-E!<5!<%!<%!<5!<%uc!-!-!<d%!<%!<!<yc!<5%!<%!<}c!-!-pc"
DATA ""
DATA 32, 16
DATA "pcE!C}cg!<yc%!<e%!<%!-!<ucf%((d%!<!<!-!-qc%!<d%((%(d%!<%!-!-E!-mc!<!-d!<j%!<%!<d!-!<!-ic!<%!<!-!<5!<"
DATA "5!<%%!<d%!<!<%d!-E!-!<!-!-ec!<!<%j!<d%d!<!-!-gE!CdEcc!-E!-!<!-!<!-!<!-!<!<%%!<!<5E!-E!C!CE!CE!C!'!'"
DATA "!C!'!Cec!C!'E!CEE!-!-d!<%!<!<e!-E!-!CE3!'!93ic!'!CE!-!-d!<%!<%!<!<%!<%!<%E!-!'3mcd!-f!<%e!<%!<!<!-!-"
DATA "qc!-!<!<!-!<!-!-!<!-!<!<!-!-!<uce!<d!-E!CEyc%!<!<5E!-}c!-!-pc"
DATA ""
DATA 32, 16
DATA "pc!C!'}c3!933!'!9yc!'3!'33V!93E!Cuce!C!'!C3d!9E!CEEqcE!-!CE!CE!CE!CEd!CE!'!C3!9mc!'d!CEf!CE!CE!C!'!'"
DATA "!9d3!93ic!'!C!93!9!'!'!C!'!'!93!'3!'33V!:V!93!'3!'3ecEE!C!Cg!'3!933!9d3!Ad!:!93!'!CeEccE!CE!CE!CE!CE"
DATA "!C!C!'3!9!:V!:!A!A!H!H!A!:!A!'!C!-!<!<!-eceEe!-EE3!'!:!:!A!:!H!H!J!H!A!A!'!9!-Eic!C!'E!C!-EE!C!9!'!:"
DATA "V!:!A!A!H!H!A!A!:3!9mc!'d!C!'!C!9!9V3!:!:!Ad!:!93qcd!'3!'3!93!:3!:V3!9uc3!9e!'3d!:yc!C!'!'!C3!9}c!C"
DATA "!Cpc"
DATA ""
DATA 32, 16
DATA "pc!-E}c!<d!-EEyc!C!CEd!-!<!-!<!<uc!93!'!'EE!<!<%!<e%qc!'3!'!CE!C!<!-!<%%!<%!<%!<%!<mcV3!'!'E!C!-!<!<"
DATA "h%(d%!<%ic!:V!:V!9!'!CE!-!<!<%!<%%!<(%(&(%%!<%!<ec3!9!A!:!A!AV3!C!'eE!-!-!<!<%%d(d%!<!<EEcc!93!:!A!A"
DATA "!:!:V!93!'3!'!C!C!'!CE!-!<!<%!<%!<%!<!-EEec!'!'!93!93V3i!:!'!'EEg!-ic!CE!'!'3!9!:!A!A!:d!A!H!:!A!C!'"
DATA "E!CE!Cmc!C!C!'!'33h!A!:!93!'!'qc!CE!'!C!:3!A!:!:!A!:V!:3uc!'!'3!9g!:yc!93!:3!:V}c3!'pc"
DATA ""
DATA 32, 16
DATA "pcE!C}c!<%!<!-EEyc%(%!<!<%!<d!-uck%!<!<!-!-qc!<%%!<!<%!<%%!<!<%!-!<!-!<!-!<mcE!-!<!-5!<!-!<!-d!<!-!-"
DATA "iEicE!CE!C!-!-!<!-!-!<!-eE!C!'!C!9!'!'3!CE!-!<ec!-!-dE!CEEe!-E!C!'!C!'!'!93!:!A!:!:!'!'eEccEE!CE!'3"
DATA "!'3!C!'!CE!C!'!'33V!:!A!A!:!:V!'3!'3!9!'ec!93!A!:!:!A!:!:e!'33!:d!A!:!:!933!'!93ic!H!A!A!H!A!A!:V3!9"
DATA "!:Vd!A!:!:3!93!'3mc!A!H!A!Hj!A!:d3!9qc!A!H!A!:!A!:f!A!:3Vuc3!93!9!:!:e!Ayc!C!C!9!'3V}c!CEpc"
DATA ""
DATA 32, 16
DATA "pc!CE}c!:!:!93!C!'yc!93!:3!:V!:33!9uc!'!'!C!C!'!'!93!:!:!93!C!Cqc!'!C!CEE!-E!C!'3!'3!C!'E!C!-Emc!:!:"
DATA "!'!'!C!CE!CEd!CE!CEE!-Ed!-!<ic!:V3!93!93!9!'3!'!C!C!'!C!'!CEE!-!-!<!<%%!<ecdE!C!'!'3!9d3!9!'!'3!9d3"
DATA "!9!C!C!-!-!<!<%d!<cc!-!<!-dE!C!'!'!C!C!'!'!C!:3d!A!:3!9!'!C!CEE!-E!-ece!-EE!CdE!CE3!9!A!A!Hd!A!:!A3"
DATA "!:!'!'ic!-eE!C!-E!-E!CE3Vd!A!H!A!H!A!:mcdE!CdE!-E!C!'!'!:!:e!AqcdE!-E!C!'!C3!9!:V!:3ucEE!C!C3!'!9d3"
DATA "yc!CE!'!C!'3}c!C!Cpc"
DATA ""
DATA 32, 16
DATA "pcE!C}c!C!C!'!CE!Cyc3!9!93!93!'!C!C!'uc!:!:V3!:!:3!:3!93d!9qc!93!'!C!'3!93!933!93!9!93!'3mc3!'dE!-EE"
DATA "!'!C!'!'!93!933!'!'!9!93ic!'3!CE!-!-!<!-!-!<E!C!CE!C!'!C!'!C!'!C!'!'!C!'!Cecd!'!CEE!-!-d!<i!-dE!-!-"
DATA "d!<!-EEcc!'3!C!'E!-!-!<!<!-!<5!<5!<5!<%!<!-!<!-%!<(%%(!<5ec!-!-!<!<%!<!<f%!<%%d!<%!<%%&d(ic%((%%((%%"
DATA "(d%e!<e%(mce(%(%%!<%%h!<qc%(%!<!<%!<5!-!<!-!<!-!<uc!<!<E!-eE!-Eyc!CE!C!'!C!'}c!'!Cpc"
DATA ""
DATA 32, 16
DATA "pcE!C}c!<%!-!-E!Cycd%!<!<5!-dEuc!<!<%!<!<%!<!<!-dE!C!Cqc!-E!-!-!<5!<5!-!<E!C!CE!'!C!'!Cmce!-Ef!-EE!C"
DATA "Ed!Cf!'icE!C!-E!-EE!-dE!CdE!CE!C!CE!C!'!93!93ec!'!'!C!CEf!CE!C!-!-i!<!-!-d!Cd!'cc!9!'3!93!93!93!9!C"
DATA "!'!-!<%!<%((%%(%!<!<%!<5!-!<ecV3d!:!A!:!:!C!'!-!-!<%(%((&(%((d%ic3V!:V3!9!CE!-E!-!<!<%%((%(%(%mc3!'"
DATA "!C!CEEe!-!<!<%!<e%qcf!-!<!-!<!<!-!<5!<5ucd!<!-!-!<!<d!-yc!<!-!-!<!-E}c!-!-pc"
DATA ""
DATA 32, 16
DATA "pcV!A}ceK!AKyc!9!9V!AK!H!J!HS!Juce!'VV!H!HdS44SqcV!9!9VV!9!AV!J!HS4S!JS!JS4mcgVK!A!J!Hd!JSg!J!H!JicV"
DATA "!A!AVd!AV!H!H!JSS4S!J!JS!J!H!H!JK!HV!AecVVK!AKK!AK!A!A!H!Hi!J!H!HV!AVV!9!9VVccVV!AV!AVV!:V!9!AV!H!A"
DATA "!HKd!HK!A!A!9V!'!C!Cd!'eceV!9!9!'!'!9V!AeV!AKKV!AVV!9!'!'!CicVd!9!'!9!9!'!9V!9!'V!:K!A!HdKV!AmcV!9dV"
DATA "d!9!'!9VV!AK!H!HKKqcV!AV!AV!A!AVV!A!AV!AKucdK!AdKd!AycK!AK!H!HK}c!AKpc"
DATA ""
DATA 32, 16
DATA "pc!'!9}c!C!Cd!'!9yc!9V!9!'!9VV!:V!9uc!AlV!9Vqc!AVd!9!'!9VV!9V!9V!AdV!9mc!AK!A!AVj!9!AV!AVe!9icd!H!J"
DATA "!H!H!AeV!9!9V!9!'d!9V!9!'!9!'VVec!H!H!Je!HK!A!AVV!9V!9!9d!'e!C!'!9!9eVcc!AVV!AV!AV!:!9!'!9!'!'!9!'!C"
DATA "!C!'!CEE!C!C!'!'!9!93V!9ec!9!9d!'!C!'!C!'!'!9d!'!C!'d!CEE!C!'!C!'!'ic!'e!C!'!'!C!'!C!'!C!'!9!C!'!CE"
DATA "!CE!C!'mc!'!'e!CEE!CEe!CE!C!C!'qc!9V!C!'E!C!CEd!C!'!C!Cuc!9!9!C!'!'!C!'d!CycV!9!9V!'!9}c!A!Apc"
DATA ""
DATA 32, 16
DATA "pcV!A}cV!9eVycf!'!C!'!CE!Cuc!9!9!'!'!C!'!C!CE!C!-E5!-qc!'d!9!'!C!C!'!C!'!CEE!-!-E!-5mc!Cd!'!C!'d!CEE"
DATA "!C!CjEic!'e!CE!CEE!CfE!-dE!C!CE!'!CV!9ecVV!9!9!'!'g!CEEe!-EE!C!C!'d!9VV!A!Acc!AVV!AdV!9!'!9!C!'E!C!-"
DATA "5!-5E!C!'!9!9V!9V!9dVec!AKV!AVVd!9d!'E!C!CE!C!C!9!'!9e!'!9icK!A!AV!9V!9!9V!9V!9!9e!'e!CEmc!A!AVVe!9"
DATA "fVd!9!'!'qc!9!9!'!9!'!C!9!'!9VV!9VVuc!C!'!CE!C!C!9!'!9!9ycE!CE!C!'!9}c!'!'pc"
DATA ""
DATA 32, 16
DATA "pc!AV}c!9V!AV!H!Hyc!'!'!9!'!9V!AK!H!Huc!C!Ce!'!9!9VV!A!A!H!Hqc!9V!9!'!9VV!9V!:V!AV!:V!9!9VmcjV!AKd!A"
DATA "VV!'!'e!CicdV!AVV!9dV!AV!AK!AKV!A!93!C!'E!C!CEec!9!9e!A!9V!9!9V!9!A!AV!AgV!9!9!C!C!'!'!9!9cc!AKK!H!A"
DATA "V!9!'!C!'!'!9dV!9!'!9!93dV!9!9!'V!:!AKec!H!H!A!A!'!'E!C!'!C!'!9!'!9!'!'!9!'Vf!9VVicK!A!9V!'!9!'!9!'"
DATA "!9!9!'!93d!9!'!'!C!'!Cmc!A!A!9!9!'!9e!'VV!9Ve!'qcV!9!9!'!C!'!'!C!9VVd!9uc!9!9d!'!9eVyc!'!C!9!9V!A}c"
DATA "!9!9pc"
DATA ""
DATA 32, 16
DATA "pc!9V}cK!AV!A!AVyc!JS!J!J!H!JK!AVVucSS4Se!J!H!H!AV!9!9qc!H!JS!JS4!JS!H!J!HKV!:!9!'V!9mcKK!H!JS!J!JS"
DATA "!J!J!H!HV!A!9!9!'!'!9V!AKic!AV!HK!J!JS!J!JS!J!HK!AV!9d!'!CV!9K!H!H!AecVVK!A!H!HSS44S4S!J!H!HVVf!'!9"
DATA "dV!A!AVccK!A!HK!H!JS4!04!044!0!JS!HK!AV!9!'!C!'!'!C!93!9Vece!H!J!J!04!0!04!0!04S!J!H!H!AVd!'!C!'!'ic"
DATA "!HK!J!HS!J4!0!04!04S4!J!H!H!AV!9!'!Cmce!JS!JS4!04S4S!J!H!J!AKqc!J!H!J!H!JSS!JS4!JS!J!HucSf!J!H!J!H!H"
DATA "yc!JS!H!JK!A}c!HKpc"
DATA ""
DATA 32, 16
DATA "pcV!:}c!'d!9V!9yc!'!C!9!'!9V!9V!9!9uce!9gV!9V!9!9qc!H!A!AV!AKK!H!AK!AVV!AV!AV!:mcKK!A!AV!A!HKg!HdK!A"
DATA "dK!AicVV!9eV!A!AV!H!A!H!A!H!AK!H!AVK!AK!HV!AecVV!'d!9!AV!A!AVd!AVVg!9VVKKe!Acc!9!'!9!'V!9!AKK!H!AdV"
DATA "!'!9EE!-EE!C!9!'V!9V!:V!Aec!9!9!A!Ae!HKKVV!'!'EE!-!-E!-!C!C!9!'!9!9icV!AK!Ad!H!J!AK!9!9!C!'E!CE!C!'"
DATA "!C!9VmcVV!AK!HKVVe!9!'!'!C!C!9!9qc!9V!9V!'!9!'!9!9!'!'!9!'!Cuc!'!'!Cg!'!Cyc!'!9!C!'!C!'}c!'!'pc"
DATA ""
DATA 32, 16
DATA "pc!AV}cVV!AV!AKycVV!AV!AVK!AK!Huc!A!Ae!HeK!A!HKKqcV!:!HK!J!H!JS!J!H!H!J!H!JK!AV!:mc!'!C!9!9!HK!H!JS"
DATA "!JSS!JS!J!JK!HVV!'!9ic!CEEd!CV!9KK!J!HS!JS!J!JS!H!HV!A!9!'!CEec!'!'!C!CEE!CE!'!C!9!9!A!A!J!He!J!H!HK"
DATA "KVV!'!C!'!'ccV!A!9!'!CEE!C!C!'!'!9!9V!AV!H!J!JS!Jd!H!AV!9VV!Aec!9V!'d!C!'!C!'!'!9!'VV!HKg!HK!AVVic!'"
DATA "!9!'!'!9!'!9!'V!9V!A!AKK!AK!AK!HK!Hmc!A!AeKg!HKKd!AKqc!J!H!J!Hd!J!Hd!J!HK!Hucd!He!JS!J!JycKd!H!J!H}c"
DATA "VVpc"
DATA ""
DATA 32, 16
DATA "pc!AK}c!AKd!H!Jyc!AKK!H!J!H!J!H!H!JucKK!A!AVV!H!He!J!H!Hqc!AK!AKV!9!'!9V!A!H!J!J!H!J!HK!Hmc!H!HKK!A"
DATA "!A!9!9!C!'!9!9!A!A!H!H!J!HKK!9Vic!Jd!HK!H!AVd!9!'VV!AVKd!HK!A!93!'!CecK!AKKe!AdV!9VVK!AeK!H!HK!A!9!9"
DATA "!'!C!'!'ccV!AV!A!9V!93!9!'!'!C!9V!AV!AKK!AK!A!AdV!93!9!'ec!'!9!C!CEE!CE!CE!'!'!9VV!A!HK!HdKe!Hic!-E"
DATA "!-5!-dE!'!C!9V!AVK!H!HK!H!J!JSmc5e!-dE!C!C!9!9!AV!A!A!H!Hqc!-E!-E!-5E!-!C!'V!9!AVucEd!-EE!9!'VVyc!C"
DATA "!C!9!'V!9}c!AVpc"
DATA ""
DATA 32, 16
DATA "pc!AV}c!9!9!A!A!HKycdV!AK!AK!H!AKucKK!AK!A!AKK!HK!HK!H!Hqc!H!J!H!HK!A!AVK!Ad!H!J!J!H!J!Hmc!J!H!H!J!H"
DATA "!HdK!AKK!H!H!J!H!Hd!JS!Jic!J!H!J!H!J!H!H!J!HKK!HK!AK!AK!A!HK!J!HS!JS4ec!J!H!J!Je!HKK!HdKVVe!9VV!HK!H"
DATA "!He!JccK!AK!A!AVV!AV!9V!:VV!9!'!CE!CE!'!'!9!'V!9V!A!AVecVVi!9V!9!'!'!-E!-!-E!-EE!CE!C!Cic!9!'!9VV!:"
DATA "dV!:!'!9E!-!-5!-5!-EE!-mc!9!9eV!9!9!'!'!CE!-Ee!-qcd!9!'!CEE!CE!C!CE!CEuc!'!'dE!-EE!C!Cyc!CeE!-}c!9!'"
DATA "pc"
DATA ""
DATA 32, 16
DATA "pcV!:}cg!Cyc!-E!-E!-EE!C!'!9uce!-5!<5d!-!C!C!9!9qcEd!-5!-!<M!<5!-5E!C!'!9!9Vmc!C!'!CE!-E!-E!-!-5!-5"
DATA "!-EE!'!'!9!9!'!'ic!CE!C!'d!CEE!CEEd!-E!CE!'d!9!'!9!9!'ecd!CEg!CE!CeE!C!C!9!9iV!9!9cc!9!'!'!9!'!9!'!9"
DATA "!'!'!CEE!-!C!'!9VV!:V!AV!A!AKK!AK!AecK!A!AV!9V!'!9E!CEE!C!C!'!9!'d!9VVKd!HicK!A!9V!'!C!CEE!-EE!CE!CE"
DATA "!CE!9VK!Hmc!9!9!'!'!C!CEE!CE!CfE!'!9qc!'!9!'!C!C!'!'!9d!'!C!'!9uc!CE!C!C!'!'!9!9VVycE!-!C!C!9V}c!'!'"
DATA "pc"
DATA ""
DATA 32, 16
DATA "pc!AK}ce!HK!HycK!H!H!A!H!J!H!HVVucg!Ae!HVV!9Vqc!9VV!AV!AV!A!AV!AVV!A!AK!H!JmcKe!AVVd!AVV!AVKKe!H!J!H"
DATA "icK!Ad!HKK!AK!A!HKK!HK!H!H!J!Jd!HK!H!H!Aec!9V!A!AgKi!He!J!H!H!A!AVV!9!9ccdV!:dV!:V!A!AK!H!J!J!H!J!HS"
DATA "4S4!JSK!A!'!9!C!'eceVd!9!'!9V!A!H!Hd!JS4!04SSK!H!9!9ic!AKVV!9!'VV!HK!J!H!JSS4S4!JS!H!Jmc!A!AVV!A!A!H"
DATA "!H!J!Hg!J!H!HqcK!AK!AKd!H!J!H!Jd!Huc!H!HKK!HK!Hd!Jyc!AVK!A!H!J}c!A!Apc"
DATA ""
DATA 32, 16
DATA "pc!9!'}c!'!'!9!9V!9yc!AV!9V!'!9!'!C!CEuc!H!H!AKV!9!'!CEE!-E!-!-qcK!HK!AV!9!C!'E!CE!-E!-!-E!-Emc!J!H"
DATA "!HK!AV!C!'dEf!-5d!-EEicd!J!H!HKV!A!'!9E!CE!C!-E!<5%5!<M!-dEec!H!Hf!J!HK!AVV!9!9!'!9!C!C!-!-d!<5!-!-"
DATA "!C!CV!9cc!HK!JS!JS!Jd!HK!HK!A!AKV!A!'!9E!CE!C!CE!C!'V!9ecKKe!H!J!H!J!J!Hd!J!H!JKKV!9!'!9!9d!'icV!AK"
DATA "!A!He!J4S!JS4!J!J!AKdV!:mc!A!AKK!J!Hd!JdS!J!Jd!HKqcV!AK!A!J!H!JS!JS!J!H!H!JucKK!H!H!J!J!H!J!H!Jyc!H"
DATA "!H!J!H!J!H}c!A!Hpc"
DATA ""
DATA 32, 16
DATA "pcV!:}cEE!'!'!9Vyc!-5!-EE!C!'!C!9!'uci!-EE!'!C!'!'qcE!CE!-E!CE!C!-EE!C!'!C!'!9!'!9mc!9!9!'!'d!C!'!C"
DATA "!'!'!C!9!'eV!9VV!9icdV!:!9!'!'!C!'!'!9!'V!9V!AK!A!HK!H!AV!A!'!'ec!9!9eV!9Ve!9VV!A!AKK!H!H!J!J!H!JKKV"
DATA "V!9VccV!9V!AK!HK!H!AKV!A!AK!H!A!H!J!JS!JS!J!HK!HK!H!HKec!H!He!J!H!J!HdK!H!HS!JSd!Jd!HK!H!HicS4S4S!J"
DATA "!J!H!Hd!JS!J!JS!H!J!H!HK!Hmc4dS!JS!J!JSf!Je!HqcS4!JS!JSS!JS!JS!J!H!Juce!H!J!JeSyc!AV!HK!H!J}cVVpc"
DATA ""
DATA 32, 16
DATA "pc!AV}c!H!J!H!H!AKyc!HK!H!J!J!H!J!H!H!Juc!AKV!AKK!H!H!J!J!H!H!A!AqcK!AV!A!9dVK!H!H!A!AdV!9!'mc!H!JKK"
DATA "V!AVV!AVd!AeV!9!'!9!'!Cic!J!H!H!Jf!H!AK!A!AK!AKV!A!9V!'!CE!C!-EecVV!AV!AKg!HKKg!H!AV!'!9!C!CdE!Ccc!'"
DATA "!9!9!'V!9!AVK!A!AKK!A!H!JS!JS!J!H!J!AKV!A!9V!9Vec!9!9!'!9eV!9dV!H!H!JdS!JSd!J!HKKic!93dV!:d!9!'V!A!H"
DATA "!J!JSS4S4!JSmcfVd!9!AV!H!Hd!JSS!JqcV!9!9dV!AK!H!J!J!H!J!Huc!9!9V!A!H!H!Jd!Hyc!AVK!A!H!A}cV!Apc"
DATA ""
DATA 32, 16
DATA "pcV!:}cf!AVyc!H!J!HK!H!HKd!Aucd!J!H!J!H!H!Jg!Hqc!HKK!AKe!HKd!H!J!H!HK!Hmc!HKVV!9!9VV!A!AKKe!H!Af!Hic"
DATA "K!HV!A!9!'!'!C!'!9V!:!AV!AK!AK!AV!AKK!AK!AecdK!AVV!9!9!C!C!'!'d!9d!'e!9!'!'!C!C!'!'!9!9ccK!A!AV!9V!'"
DATA "!9!C!'!CE!C!'!C!'!CE!'!C!C!'!-E!<M!-5!C!'ec!9!9E!CeE!-!-E!-EE!CE!C!CE!C5!-5%!<!<ic!-5!<5!<5!<5!-5!-E"
DATA "E!C!CEE!Cd!-5mc%!<%%!<!<!-!-fEd!CE!Cqc5!-!-E!CE!C!'!'!C!'!C!'!'uc!C!C!9!9VV!9V!9!9ycV!A!AK!AK}cK!Apc"
DATA ""
DATA 32, 16
DATA "pcV!:}c!-E!9!9!AVyc!-5E!-!C!C!9!'!9Vucd!CdE!C!C!9!9VV!A!Aqc!9!'!'!9!C!'!CE!'!9VV!AVK!AK!Amc!'f!9!'!9"
DATA "!'d!9dV!A!AfKicVV!9!'d!9VV!9V!9V!9V!9dV!A!AK!HK!H!HecKK!A!AdVe!AV!9!9!'h!C!9!9!AVK!AKKcc!HK!H!J!H!J"
DATA "!H!J!H!J!AK!'!'E!-!-5!<5!-5E!-!CE!C!'!'!9ec!J!H!Hd!J!H!JK!A!9!'EE5!-%!<5%d!<5!-!-ic!H!J!Jd!HV!A!93!'"
DATA "!'E!C!-5!<5!<5!<Mmc!A!H!A!AVd!9!'!9d!CdE!-!-qc!9!'!'!9!'!9!'!C!C!'!C!'!C!'uc!'!C!'!'!Cd!'!9!9yc!'!C"
DATA "!'!C!9!'}c!9!9pc"
DATA ""
DATA 32, 16
DATA "pc!J!H}ceS!JSyc!H!H!J!HSS!0S!0!0uceK!H!HSSg!0qc!H!J!H!J!H!J!JS!0Si!0mc!Hd!J!H!JS!J!0Sm!0icd!JS!JS!JS"
DATA "Sk!04S!0Sd!Jec!J!JS!JSS!JS!J!JSSi!0SS!J!Jf!H!Jcc!J!H!JS!JS!J!H!H!J!JeS!0S!0S4!JS!H!JKVVdKec!J!J!H!J"
DATA "!H!HKK!H!He!JS!JSS!J!Jd!HK!A!Aic!H!J!HKK!H!HKd!HK!J!HS!JS4S!J!J!Hmci!HKd!H!JdS!JSqcd!J!H!J!H!JS!J!H"
DATA "!JS!JSucSS!JeS!JS!JycS!JdS!0}c!JSpc"
DATA ""
DATA 32, 16
DATA "pcKK}cKVdK!Hycd!HK!H!H!J!H!H!Juch!J!H!J!J!H!J!H!Hqc!JSd!HK!H!J!H!J!H!J!J!H!J!H!H!Jmc!JS!J!Jk!He!J!H"
DATA "!HK!HicS!0!0SS!0!JS!H!J!H!Jd!HKd!H!J!HK!HK!H!JecdS!0!0dS!J!J!H!Je!HKKV!AV!AKK!H!H!J!H!H!Jcc!JS!JS!J"
DATA "!H!J!H!HKK!HK!HK!AKeV!9VdKd!H!Jec!H!HeKV!AgKVKKgV!AKKicKKVKVKKVKVKVKK!AVV!AdVKmcKKV!AgV!A!AV!AVVKKqc"
DATA "!H!HKVV!9dVK!AVVKuc!H!HVdKV!AKVyc!H!J!H!HK!H}cS!Jpc"
DATA ""
DATA 32, 16
DATA "pc!J!H}cd!H!J!H!JychKVV!9uc!HfKeV!9!9!C!CqcKd!HK!A!AVVKV!A!9V!9!'!'!9mcVdKVKVV!9fV!9!9V!9V!9V!9icKVV"
DATA "KdV!AVV!9V!9V!9V!9V!9dVK!A!H!Jec!H!J!H!HKKV!AeV!9!9e!'!9!9VVKe!Hd!Jcc!JS!J!H!J!H!H!JK!H!AVV!9!'!C!'"
DATA "!CV!9KKd!H!J!H!J!J!Hec!JSe!J!H!HeKfV!AgK!HKicS!J!JS!H!Jd!H!J!H!J!HeK!A!AVV!AmcS!Jh!Hd!Je!HKKqc!H!HK"
DATA "!HK!AK!H!H!J!H!J!J!HucKKeV!HK!H!Hyc!9dVK!H}cKKpc"
DATA ""
DATA 32, 16
DATA "pc!JS}c!J!H!J!JSSycKK!HK!H!H!JSS!0ucVVfK!H!J!JS!JSSqc!H!J!HK!H!J!H!J!J!Hd!J!H!H!J!H!Hmc!J!H!J!H!J!H"
DATA "!J!JS!J!JSS!J!J!HKKVVKVic!J!H!J!H!J!H!H!J!J!H!JSS!J!JS!Jd!H!AeVKec!H!He!Jd!HK!H!Hk!J!H!HKVKK!H!HccS"
DATA "!JS!J!JS!HKVKK!H!J!H!H!JK!H!HK!J!H!H!J!HK!J!HS!JecdS!JKKVVKK!HK!HfKg!H!J!JicS!J!H!JfK!HKf!HK!HdK!Amc"
DATA "!J!Jd!HfK!H!H!J!HeKqc!H!J!HKKVK!A!H!J!H!J!H!Huc!H!HKK!HK!J!H!H!JycK!A!H!H!J!H}c!H!Hpc"
DATA ""
DATA 32, 16
DATA "pc!H!J}cSf!Jycd!04!0SS!J!J!Huci!0SS!J!J!H!Hqc!0Sg!0S!0SS!J!H!HK!H!Jmc!JSi!0dS!J!H!HKK!H!H!JSic!JSS4"
DATA "h!04S!J!H!JdKV!H!JS!JSSec!J!JS!JSSi!0SS!H!HfK!H!Hf!JccS!JS!0!0Sd!0BB!#!0B!0!0SS!JS!HK!AVK!A!HK!H!Hec"
DATA "SS!0S!0!0B!0BB!#BBe!0S!J!JKKV!AKKicSSf!0BB!#!0B!0B!0!0SS!H!JKKmci!0Bh!0!JSqc!0S!04j!0Sucg!0S!0SSyc"
DATA "d!0SS!J}cSSpc"
DATA ""
DATA 32, 16
DATA "pc!H!J}c!HKe!HycKK!HKd!H!J!HKuce!H!J!J!H!J!Jf!HqcS!J!JSS!JSS!JS!JS!J!H!J!H!J!Hmc!JSe!JSS!0hS!JeS!Jic"
DATA "!Je!H!J!J!H!JiS!J!JSS!JSd!Jec!J!J!HK!HKj!Jd!HKe!H!JdSd!Jcc!HK!HK!H!J!JdS!JS!J!HK!H!9V!9!'V!9K!H!H!J"
DATA "!Hd!Jec!H!HS!JgS!H!JKK!9!9d!'!9KV!HK!H!Hic!J!HS!JS!0S!0!JS!H!H!AVV!9V!9KV!H!Jmc!H!J!JdS!J!Je!HKK!A!A"
DATA "!H!Hqc!H!J!H!HK!HK!H!HeK!AuckKycKK!AV!AV}cKKpc"
DATA ""
DATA 32, 16
DATA "pc!JS}c!J!Hd!JSyc!J!H!JS!JSS!JS!Juc!J!JeS!JhSqc!J!HS4!04d!0S!0SS!0S!J!H!JmcV!A!H!HSSk!0SS!J!HK!HicV"
DATA "!AV!9VK!H!JS!J!0Sg!0S!0!J!JK!HVVecKKVV!9!9VVKK!H!H!J!JSg!0dS!H!HVdKcc!J!HK!HdV!9!AVKd!H!JS!0S!04!0SS"
DATA "!0!JS!H!J!J!Hec!H!HV!AKVV!AeK!J!HSS!0S!0eSd!JicfK!H!HK!Hd!JS!JS!JS!JdS!JmcS!JkS!JSS!J!JSqc!0S!0Sd!04"
DATA "d!0dSucSj!0ycS!JSS!0S}c!J!Hpc"
DATA ""
DATA 32, 16
DATA "pc!JS}c!JSS!0S!0ycS!JS!J!0S!04S!0ucdSd!JSSe!0SSqc!JS!JS!H!JK!H!J!HS!0!04!04S!Jmc!0eS!J!H!HKK!H!HS!J"
DATA "dS!0SS!J!Hic!0SS!0SS!JSd!HK!J!H!JSS!JS!0S!J!HKKVec!JdS!J!JSd!J!H!Hd!JhS!JS!H!HV!AKKcc!J!H!Jg!HdK!H!H"
DATA "!JSS!JS!JS!J!JS!J!H!HK!HKec!HKKdV!9dVKK!H!H!J!JkSic!9!'!'!C!'!9!9VKV!H!J!JSS!JS4Sd!0mc!'!C!'!'!9!9VV"
DATA "KV!H!He!J!0Sqc!9!'!'!9!'!C!9!'!AV!H!J!JSucd!9!'!9V!HK!J!JycVKK!H!H!J}c!J!Jpc"
DATA ""
DATA 32, 16
DATA "pc!JS}c!H!H!J!JSSyc!J!H!J!HS!JS!JS!JucSS!JSS!JiSqc!0SS!0S!J!JSS!JS!0!0S!04!04mcSe!0eS!JSS!0Si!0icd!0"
DATA "S!0SS!0dS!JS!JS!JS!JSh!0ece!0S!0iS!H!JKd!H!J!JSSg!0ccS!JS!J!JS!J!H!H!J!H!J!J!HK!HdV!AdK!H!H!J!JS!JS"
DATA "ec!Jd!HKh!HKKd!9!'!9!9V!9eVicKd!H!J!H!J!H!J!HK!H!9V!C!'!'!C!9!'!9!9mcd!H!J!H!J!H!HKKVVe!9!'!9qcd!HKV"
DATA "KV!9dV!AVVucKK!9V!9!9V!9KVycV!AV!9!9V}c!HKpc"
DATA ""
DATA 32, 16
DATA "pc!J!H}cKdVKVyc!9!9!'!9!9!'V!9KKuc!9!9!C!'e!C!9!9KV!H!Hqc!9V!'!9!C!'!CE!CE!'!9V!9K!H!H!JmcKKVVf!9!'"
DATA "d!C!'!9!9KK!H!HKKicVVKVVKVKVV!9V!'!9!9!'VVK!H!HdK!HKechVd!AVVd!9VV!AK!H!H!J!Je!H!J!H!Hcc!HjKVV!9VVK"
DATA "!H!J!J!H!J!H!J!H!JSS!JS!Jec!JS!J!Jd!HKdV!9KVK!H!HK!H!H!J!JeSicS!J!H!HKKV!A!9V!9gV!A!H!HS!JmcK!HKKjV"
DATA "!9VVK!HqcdKVKVfKVKKucVVKVKK!H!H!J!Jyc!9VVK!H!H}cKKpc"
DATA ""
DATA 32, 16
DATA "pc!JS}cSS!0dSyceS!0SS!0!J!Huc!J!JS!JS!JeS!J!J!H!Hqc!H!J!J!H!J!H!J!H!JS!JS!J!HS!JS!0mcSh!JSf!JSS!0SS"
DATA "!0!0SicS!JS!0dS!JS!JS!0S!JS!J!0S!0SS!0S!JSSec!H!H!J!JiS!0S!0SSf!0dS!J!H!J!H!Hcc!J!H!J!H!H!J!J!H!J!H"
DATA "!JSS!0!0Sf!0B!04S!JK!H!AVec!H!J!Jd!HKK!H!HSSg!0Bd!0SS!H!Hic!JS!H!J!HK!J!HSS!0Sf!0B!0!0S!0mcSd!JS!JdS"
DATA "i!0SqcS!JS!JS!JS!0!0S!04S!0ucSS!JeSd!0yc!JSS!JS!0}cS!Jpc"
DATA ""
DATA 32, 16
DATA "pc!HK}cdKd!Hyc!JS!H!JK!HKdVucSS!JS!H!HV!AVV!9!9!'!'qcS!JS!J!H!J!AVVe!9!'!9!'!9!'mcS!0SS!J!JKKe!9!'!9"
DATA "d!'!C!9!'!9!9icd!0dS!J!HKKV!9V!9!9!'!CEE!-!CE!9!'!9VecSSe!0S!0!JS!H!J!H!HK!HVV!'!'Ed!C!9!9KV!H!HccS"
DATA "!0!04d!0SS!0S!JS!J!JS!J!HKKV!9V!9VVKV!H!JecSS!0S!0SSd!0Sf!0SSd!HfKic!JSS!JSl!0!JS!J!H!J!HmcS!JSS!0S"
DATA "i!0eSqc!J!JS!J!0Sf!0S!0SuceSg!0ycS!0!0S!04}cSSpc"
DATA ""
DATA 32, 16
DATA "pc!J!H}cV!9KK!J!Hyc!C!'!9!'V!9K!AK!Huc!'!C!9!'!9!9!'!'!9!9V!AKKqcV!9!9VV!9V!9!9!'V!9fK!Hmc!H!HKKV!AK"
DATA "KVK!A!A!HK!H!J!Jf!Hic!J!H!J!HK!HKVKK!HK!H!J!J!HS!JS4SS!J!HKKecd!Hd!Jd!HKK!H!J!JS!JSS!0Se!0SS!H!J!H!H"
DATA "cc!H!J!J!HS!JS!J!JS!J!H!JdS!0S!04d!04dS!JSSecSSe!0S!0eS!0Sg!0gSic!0Bf!0SSi!0SS!0S!Jmcp!0dSqcn!0SuceS"
DATA "g!0yc!JdS!0S}c!H!Jpc"
DATA ""
DATA 32, 16
DATA "pc!JS}c!0!0SS!JSycSS!0S!0S!0SS!0ucSS!J!JSS!0Sd!0S!J!JqcS!J!J!H!H!J!J!HS!JSS!JS!J!H!HKmc!0!0dSj!Jf!HK"
DATA "V!Aic!0SS!0S!0S!0dS!J!JS!JS!J!J!H!JKKVd!9ece!JmS!0dS!J!J!HKV!A!9dVccKK!HK!H!J!JSS!J!JSS!J!0Se!0S!0S"
DATA "d!J!H!J!H!Hecd!HKd!H!Jd!H!JSSi!0S!0SSic!HK!H!J!Je!HK!J!J!0Sf!0B!0!0mc!Hd!Je!H!J!JSSg!0qc!H!J!H!H!J!H"
DATA "S!JS!0!04!0Suc!H!H!J!JSS!0S!0Syc!JSS!JSS}c!J!Jpc"
DATA ""
DATA 32, 16
DATA "pc!J!H}cS!JSd!JycS!0S!0S!0S!J!JSuc!0!0Sd!0S!0gSqcS4S!JS!JS!0S!0S!0S!0S!0S!JmcSS!Je!H!JS!JSS!0hS!0Sic"
DATA "SS!J!JK!HKVKK!J!Hd!JS!JS!JS!JSS!JS!JecdS!J!H!JK!H!A!AKKd!HdKe!HKKV!AKK!H!HccS!J!JS!H!JKK!AVVKVKVKVVK"
DATA "VKV!9!'!CE!C!'!AVecK!HVV!9VV!9!'!9!'d!9gV!C!CdE!Cic!C!'!CE!CEd!C!'!9!'!9eV!9!'!9!C!'mc!CeE!C!'!'V!9"
DATA "iVqc!C!C!'!9dVKK!AK!AKKucVV!H!H!J!H!Jd!Hyc!J!H!JS!JS}cS!Jpc"
DATA ""
DATA 32, 16
DATA "pc!J!H}c!9!9K!H!J!Jyc!'!C!9!'VK!HK!H!JucfV!9V!A!H!H!J!H!J!Jqc!HKK!H!AVV!AK!H!J!H!JSS!JS!Jmc!HK!H!HK"
DATA "!H!HK!HKd!He!JdS!JSic!H!J!HK!HK!H!J!H!J!H!J!H!J!H!J!J!H!J!H!JSS!0S!0ecdSj!J!H!HV!AKVKVV!A!H!Hd!JdScc"
DATA "dS!0S!0S!0S!0!JSKK!9V!'!C!CE!C!'!9VV!A!AVK!HecSh!0!JS!HKV!9!'!CeEe!C!'!'ic!0S!0!0S!0!J!J!H!HKKV!9!C"
DATA "!'!CE!CE!CEmcSS!J!Jf!HK!A!AVVd!9!'qcK!HK!HK!HK!A!AV!AV!AVuc!A!AgK!H!HycKVKK!HK}cK!Hpc"
DATA ""
DATA 32, 16
DATA "~c~c~cuc&c$c&yc!B!B&$&!B$!B&!B&c&uc66$&!B!B6d$d&c$qc$!Bf&$6!B6$!B!B$!B$c&mc$c$6&$$&$!B6$!B6$!B&&!Boc"
DATA "!B$$!B&$6$!B$!B$!B!B&d$!Boc6!B6!B$$&&$&!B$!B&&c!Boc!B&$!B6!B&6!B&!B&&!B!Bsc&$!B6d$!B!Bc$wc!Bd$!B&$c&"
DATA "~c~c~cwc"
DATA ""
DATA 32, 16
DATA "~c~c~c~c~c~c~c~c~cqc>Jxc`Jfc!Byc!>~cccY!B~cc!B~c~c~c{c"
DATA ""
DATA 32, 16
DATA "~c~c~c~c~c~c~chc2fcS,xcS,fc9,ZscBdc9,Zec9,ZrcS,dc9,Zec9,Zrc9,Zcc9,S,dc\!BZrc9,Zcc\!B9,Zwc\!BZec!B\Z"
DATA "~c~c~ctc"
DATA ""
DATA 32, 16
DATA "~c~crcB~ccS,hcS,vc9,ZecSS,,Zuc9,ZecSS,ZZuc\!BZec99,ZZucS,fc\9,Zvc9,Zfc9,Zvc9,Zfc!B\Zvc9,Z~c!B\Z~c~c"
DATA "~c~c~ccc"
DATA ""
DATA 32, 16
DATA "~c~ctcB~ccS,nc2ncS,S9,ZlcS,nc9,Z9,Zlc9,Zmc9,Z9,ZecBgc9,Zmc\!BZ!B\Z,cBS,gc9,ZqcS,,Z,9,Zfc\!BZqc9,ZZ,9"
DATA ",Zyc9,ZZ!B\!BZyc\!BZZ~c~c~c~c|c"
DATA ""
DATA 32, 16
DATA "~c~cuc!B!B;8!B!Bvcg!B;8;!B!Bvc!B;8!K8;8;8;g!Bqc!B8!O!N!N!M!L!L;8;!R!Q8;!Blcd!B8;8;!K!R!Q!P!O!N!M!L!K"
DATA "!K!R!R8!Bmc!B8d!P!O!O!N!L!K!R!P!O!N!M!L!L!K;d!Bkc!B;d!R!Q!Q!P!O!N!M!K!R!Q!O!N!N;8;8;!Bicd!B;8;8;!R!Q"
DATA "!P!O!N!M!L!K!R;g!Bmcd!B8;!R!R!Q!Q!P!O!O!N8!Btc!B!B8!L!K!K8;8;8;!Btc!B!B;8;8h!Bucg!B~c~cvc"
DATA ""
DATA 32, 16
DATA "~cqc!B!B;8;8{c!B;d!M!L!K!Rucf!B8;!P!P!O!O!N!L!Ksc!B;!K!Kf!R!Q!Q!P!O!N!M!Kpc!B!B8!O!N!N!M!L!L!K!K!R!R"
DATA "!Q!P!O!N!M!Ljc!B!B;8;8;!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nic!B8d!P!O!O!N!L!K!R!P!O!N!M!L!Le!K8;d!B"
DATA "fc!B!B;d!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!M;8!Bhcd!B;8;8;!R!Q!P!O!N!M!L!K!R;8;8;8!B!Blcd!B8;!R!R!Q!Q!P!O!O"
DATA "!N8!B!Bsc!B!B8!L!K!K8;!K!R8;!Btc!B!B;8;8h!Bucg!B~c~cvc"
DATA ""
DATA 32, 16
DATA "~c~cuc!B!B;8!B!Bvcg!B;8;8!Bvc!B;8!K8;8;8;g!Bqc!B8!O!N!N!M!L!L!K8;!R!Q!P;8!Bkcd!B8;8;!K!R!Q!P!O!N!M!L"
DATA "!K!K!R!R!Q;!Blc!B8d!P!O!O!N!L!K!R!P!O!N!M!L!L!K!K8g!Bfc!B!B;d!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Md!N8!Bfc"
DATA "d!B;8;!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qicg!B8!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Lpc!B;!L!Lf!K!R!R!Q!Q!P!Prc"
DATA "!B8;!Ne!Mf!Nuc!B!B;8;8;!P!Q!Q|c!B!B;~cqc"
DATA ""
DATA 32, 16
DATA "~cqc!B8;8;8{c!B;d!M!L!K!Rucf!B8;!P!P!O!O!N!L!Ksc!B;!K!Kf!R!Q!Q!P!O!N!M!Kpc!B!B8!O!N!N!M!L!L!K!K!R!R"
DATA "!Q!P!O!N!M!Ljc!B!B;8;8;!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nic!B8d!P!O!O!N!L!K!R!P!O!N!M!L!Lf!K!R!R!Q"
DATA "!Qfc!B!B;d!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Mf!Nfc!B!B8;8;!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qicg!B8!K!R!R!Q!Q"
DATA "!P!O!O!N!M!L!L!K!K!Lpc!B;!L!Lf!K!R!R!Q!Q!P!Prc!B8;!Ne!Mf!Nuc!B!B;8;8;!P!Q!Q|c!B!B;~cqc"
DATA ""
DATA 32, 16
DATA "~c~cuc!B!B;8!B!Bvcg!B;8;8!Bvc!B;8!K!R;8;8;g!Bpc!B!B8!O!N!N!M!L!L!K8;!R!Q8;!B!Bkc!B8;8;8;!K!R!Q!P!O!N"
DATA "!M!L!K!K!R!R8!Blc!B;e!P!O!O!N!L!K!R!P!O!N!M!L!L!K;d!Bgce!B8e!R!Q!Q!P!O!N!M!K!R!Q!O!N!N!M8;8;!Bhc!O!N"
DATA "!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R;8;8;8!Bjc!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N8!B!Bpc!R!P!O!N!M!L!Lf!K!R"
DATA "!R;!B!Brc!R!Q!O!N!N!M!M;g!Buc!K!R!Q!P!P8!B|c8!L8;!B~c!B!Bpc"
DATA ""
DATA 32, 16
DATA "~cqc!B8;8;8{c!B;d!M!L!K!Rucf!B8;!P!P!O!O!N!L!Krc!B8;!K!Kf!R!Q!Q!P!O!N!M!Kmc!Bcc!B!B8!O!N!N!M!L!L!K!K"
DATA "!R!R!Q!P!O!N!M!Lic!B!B8;8;8!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nhc!B;e!P!O!O!N!L!K!R!P!O!N!M!L!Lf!K"
DATA ";d!Bcc8!B!B;8e!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!M!N8!Bgc!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Q8;8;8;!Bic!Q"
DATA "!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N8!B!Bpc!R!P!O!N!M!L!Lf!K!R!R;8!Brc!R!Q!O!N!N!M!M;8f!Buc!K!R!Q!P!P8!B|c"
DATA "8!L8;!B~c!B!Bpc"
DATA ""
DATA 32, 16
DATA "~c~cuc!B!B;8!B!Bvcg!B;8;8!Bvc!B;!K!K!R!R8!R8;g!Bpc!B!B8!O!N!N!M!L!L!K!K!R!R!Q!P;8!Bjc!B!B8;8;8;!K!R"
DATA "!Q!P!O!N!M!L!K!K!R!R!Q;!Bkc!B;e!P!O!O!N!L!K!R!P!O!N!M!L!L!K!K8;f!Bcc8!B!B;8e!R!Q!Q!P!O!N!M!K!R!Q!O!N"
DATA "!Ne!Md!N8;ec!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qic!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N!M!L!L!K"
DATA "!K!Lmc!R!P!O!N!M!L!Lf!K!R!R!Q!Q!P!Pqc!R!Q!O!N!Ne!Mf!Nuc!K!R!Qf!P!Q!Qyc!M!L!L!K!K!L}c!P!Ppc"
DATA ""
DATA 32, 16
DATA "~cqc!B8;8;8{c!B;d!M!L!K!Rucf!B8d!P!O!O!N!L!Krc!B8!L!K!Kf!R!Q!Q!P!O!N!M!Kmcf!B!P!O!N!N!M!L!L!K!K!R!R"
DATA "!Q!P!O!N!M!Lic!B;8;8;8!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Ngc!B!B;e!P!O!O!N!L!K!R!P!O!N!M!L!Lf!K!R"
DATA "!R!Q!Qcc8;!B;8e!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Mf!Nec!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qic!Q!P"
DATA "!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Lmc!R!P!O!N!M!L!Lf!K!R!R!Q!Q!P!Pqc!R!Q!O!N!Ne!Mf!Nuc!K!R!Qf!P"
DATA "!Q!Qyc!M!L!L!K!K!L}c!P!Ppc"
DATA ""
DATA 32, 16
DATA "~cqc;f!Byc!L!Le!M8;!Bvcf!Oe!P8!Btc!M!M!L!L!K!Kf!R;g!Bmc!Q!R!R!Q!Q!P!O!N!N!M!L!L!K!K!R!R!Q!P;!B!Bjc8;"
DATA "f!M!L!K!R!Q!P!O!N!M!L!K!K!R!R8!B!Bic!B!B8!Oe!P!O!O!N!L!K!R!P!O!N!M!L!L!K;8!B!Bic!B;f!R!Q!Q!P!O!N!M!K"
DATA "!R!Q!O!N!N!M8;8;!Bhc!B!B;8;8;8!R!R!Q!P!O!N!M!L!K!R;g!Bjcg!B8;!R!R!Q!Q!P!O!O!N8!Btc!B;8!L!K!K8;8;8;!B"
DATA "tc!B!B;8;8h!Bucg!B~c~cvc"
DATA ""
DATA 32, 16
DATA "pc!M!M}c!Q!R!R!Q!Q!Pyc!L!Lf!M!L!K!Rucf!Oe!P!O!O!N!L!Kqc!M!M!L!L!K!Kf!R!Q!Q!P!O!N!M!Kmc!Q!R!R!Q!Q!P!O"
DATA "!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!Lic8!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nfc!B!B8!Oe!P!O!O!N!L!K!R"
DATA "!P!O!N!M!L!Lf!K;d!Bec!B;f!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!M!N8!Bgc!B!B;8!L!L!K!K!R!R!Q!P!O!N!M!L!K!R;8;8;"
DATA "8!B!Bicg!B8!K!R!R!Q!Q!P!O!O!N8!B!Bsc!B;8!Lf!K!R!R;!Btc!B8;8;8h!Bucg!B~c~cvc"
DATA ""
DATA 32, 16
DATA "~cqc;f!Byc!L!Lf!M;!Bvcf!Oe!P8!Btc!M!M!L!L!K!Kf!R;8f!Bmc!Q!R!R!Q!Q!P!O!N!N!M!L!L!K!K!R!R!Q!P!O8!Bjc8;"
DATA "f!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q;!Bic!B!B8!Oe!P!O!O!N!L!K!R!P!O!N!M!L!L!K!K8;f!Bec!B;f!R!Q!Q!P!O!N!M"
DATA "!K!R!Q!O!N!Ne!Md!N8;ec!B8;8!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qicf!B;8!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K"
DATA "!Lpc!B;!L!Lf!K!R!R!Q!Q!P!Prc!B8!N!Ne!Mf!Nuc!B!B;8;8;!P!Q!Q|c!B!B;~cqc"
DATA ""
DATA 32, 16
DATA "pc!M!M}c!Q!R!R!Q!Q!Pyc!L!Lf!M!L!K!Rucf!Oe!P!O!O!N!L!Kqc!M!M!L!L!K!Kf!R!Q!Q!P!O!N!M!Kmc!Q!R!R!Q!Q!P!O"
DATA "!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!Lic8!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Nfc!B!B8!Oe!P!O!O!N!L!K!R"
DATA "!P!O!N!M!L!Lf!K!R!R!Q!Qec!B;f!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Mf!Nec!B8;!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Q"
DATA "f!P!Q!Qicf!B;8!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Loc!B!B;!L!Lf!K!R!R!Q!Q!P!Prc!B8!N!Ne!Mf!Nuc!B8;8;8;!P!Q"
DATA "!Q|c!B!B;~cqc"
DATA ""
DATA 32, 16
DATA "~cqc;f!Byc!L!Lf!M;!Bvcf!Oe!P8!Btc!M!M!L!L!K!Kf!R;g!Bmc!Q!R!R!Q!Q!P!O!N!N!M!L!L!K!K!R!R!Q!P;!B!Bjc!L"
DATA "!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R8!B!Bhcf!Oe!P!O!O!N!L!K!R!P!O!N!M!L!L!K;8!B!Bgc!L!L!K!Kf!R!Q!Q!P!O!N"
DATA "!M!K!R!Q!O!N!N!M!M;8;!Bhc!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R;8;8;8!Bjc!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O"
DATA "!O!N8!B!Bpc!R!P!O!N!M!L!Lf!K!R!R;!B!Brc!R!Q!O!N!N!M!M;8f!Buc!K!R!Q!P!P8!B|c!M!L8;!B~c!B!Bpc"
DATA ""
DATA 32, 16
DATA "pc!M!M}c!Q!R!R!Q!Q!Pyc!L!Lf!M!L!K!Rucf!Oe!P!O!O!N!L!Kqc!M!M!L!L!K!Kf!R!Q!Q!P!O!N!M!Kmc!Q!R!R!Q!Q!P!O"
DATA "!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!Lic!L!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Necf!Oe!P!O!O!N!L!K!R!P"
DATA "!O!N!M!L!Lf!K;d!Bcc!L!L!K!Kf!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!M!N8!Bgc!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R"
DATA "!Q8;8;8;!Bic!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N!M;!Bpc!R!P!O!N!M!L!Lf!K!R!R!Q8!Brc!R!Q!O!N!N!M!M;8f!Buc"
DATA "!K!R!Q!P!P8!B!B{c!M!L!L;!B!B}c!B!Bpc"
DATA ""
DATA 32, 16
DATA "~cqc;f!Byc!L!Lf!M;!Bvcf!Oe!P8!B!Bsc!M!M!L!L!K!Kf!R;8f!Bmc!Q!R!R!Q!Q!P!O!N!N!M!L!L!K!K!R!R!Q!P!O8!Bjc"
DATA "!L!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q;!Bhcf!Oe!P!O!O!N!L!K!R!P!O!N!M!L!L!K!K8;8;8;!Bcc!L!L!K!Kf!R!Q!Q"
DATA "!P!O!N!M!K!R!Q!O!N!Ne!Me!N;ec!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K!R!Qf!P!Q!Qic!Q!P!O!N!M!L!K!K!R!R!Q!Q"
DATA "!P!O!O!N!M!L!L!K!K!Lmc!R!P!O!N!M!L!Lf!K!R!R!Q!Q!P!Pqc!R!Q!O!N!Ne!Mf!Nuc!K!R!Qf!P!Q!Qyc!M!L!L!K!K!L}c"
DATA "!P!Ppc"
DATA ""
DATA 32, 16
DATA "pc!M!M}c!Q!R!R!Q!Q!Pyc!L!Lf!M!L!K!Rucf!Oe!P!O!O!N!L!Kqc!M!M!L!L!K!Kf!R!Q!Q!P!O!N!M!Kmc!Q!R!R!Q!Q!P!O"
DATA "!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!Lic!L!Lf!M!L!K!R!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!Necf!Oe!P!O!O!N!L!K!R!P"
DATA "!O!N!M!L!Lf!K!R!R!Q!Qcc!L!L!K!Kf!R!Q!Q!P!O!N!M!K!R!Q!O!N!Ne!Mf!Nec!O!N!N!M!L!L!K!K!R!R!Q!P!O!N!M!L!K"
DATA "!R!Qf!P!Q!Qic!Q!P!O!N!M!L!K!K!R!R!Q!Q!P!O!O!N!M!L!L!K!K!Lmc!R!P!O!N!M!L!Lf!K!R!R!Q!Q!P!Pqc!R!Q!O!N!N"
DATA "e!Mf!Nuc!K!R!Qf!P!Q!Qyc!M!L!L!K!K!L}c!P!Ppc"
DATA ""
DATA 32, 22
DATA "~c~c~c~czcO!6OA{c\\E((Q!;[yc\!C!CM((Q!B_tc\dO!>\M(Y`!>!B\Dsc!;$Q%Q$\`(`Q!>!;ADcc!Boc!;Q!B`M!>\O!>!>$"
DATA "\O[Dc\_ocO$!>Q($\!;[!B\!B!;__!6\_oc!;Q!>!>!?\\O!;[\OA__%O_oc!;!>!>`!?!;EO[d!;[_DA\[oc!;Q$!>!?!?!>O!;"
DATA "!;O\[Dc_O_oc!;A!>$`(!>A[!;!;!B[ccd_qcd!;(Q!;!;[O!B\DxcO!>!6[A!;O\DxcO$O__D[[_yc_D~c~c~c{c"
DATA ""
DATA 32, 22
DATA "~csc[_~cc[\_~c[O!;D{cO!6!?$[_cO!;vc\\5(MQ!B\_Q!?!;tc!>!-%!C!C!?!?!>!>OAQ!?Occ!B_[_jcO\O`!B\YM(!?!>!B"
DATA "O[!;!>!;c!BMO!;_ic!6`Q!-M!>!>O!>Q(!>!BOA!;!;!>!B!C!;!;A_icO`d!>!?!B\!;!>!>\\!;[_[!C(Od!;_icO`$QQ($!B"
DATA "O!;d\!;A_$M\\!;[!;_icO$!>Q!>Q\O!;A!;\O!;[[(O\Od!;_ic!6!>!B!>Q`!6MO!;!;[!6[_D!?\!>O!;DD_icOQQ$Q!?($O"
DATA "!;!;\\[Dc_\\O_mc!;AQQ(!?!>\d!;!B_ccDd_Doc[!;O!?Q!;A[!;!B!BDcDvcO!>!;!;A!;!B\DxcO!>ODD_[[Dyc[[~c~c~c"
DATA "{c"
DATA ""
DATA 32, 22
DATA "~c~c~c~czcO!6OA{c\\E((Q!;[yc\!C!CM((Q!B_gc!BO!6Ojcd\!;$\(MMQQ\O_ec\!CEM_ic!6!CE((Q!BO``(!>!>O!;DccO"
DATA "!C3!C!;DicO$`$!?(Q!>!;d!>\O[Dc\!9V3\!;_icO`$Q``!B\O!;!B!B\O__!;MEMEOA_icO`!>Q((!B\O!;!;\O!;__%(!-M["
DATA "!;!;_icOd!>`!?OM(Q!B[O[_^dM!-A!;!;DicO$QQ`QQ$!>\Q!>\!;DcOd(!;A!;_jc!;Q!>`(!?Q!B!>!>!?\_ccO!?(!;A[!;_"
DATA "kcd!;!?(d!>QQ\!;Dc!;``!;!;__oc!;Y!>!B!>!B(A!;A_c_ADDrcOd!>Q!?d!;_wc!6!;A[!;(A!;!;_|cO!;DD~cD~cqc"
DATA ""
DATA 32, 22
DATA "~csc[_~cc[\_~c[O!;D{cO!6!?$[_cO!;vc\\5(MQ!B\_Q!?!;tc!>!-%!C!C!?!?!>!>OAQ!?Occ!B\OOA[hc\\O!?!>\YM(!?"
DATA "!>\O!;[Q!;c\M!CM!;O_gc\M5(!?Q$O`!?(Q\OA[A!>!Bd!CO!;A_gc!;QQ$`!>!>\O!>!>$\O[_[!-!9!C3\!;O!;_gc!;Q$!>`"
DATA "Q!>\O!;\!>\!;__`M%!95!;A!;A_gc\!>!>Q(%\OO[[\\!;[_!?MEMf!;Dgc\Q$Q$!?\%5Q!B!;!6[_D5%%MA!;O!;ADgcO`Q$`"
DATA "!?(!B!>!>Q!>\!;DcO((Qf!;_hc!;!>!>!?!?(!>!B!>$Q\_ccO`!?!;A!;O_Djc!;!;O!?!?`!B!>!B(!>ADc!;`Q!;!;__ocOY"
DATA "`!B!>$QAO!;Dc_!;D_rcOQd!>(d!;_wc!6!;A[$!?A!;!;D{c_O!;__~cD~cqc"
DATA ""
DATA 32, 22
DATA "~c~c~c~czcO!6OA{c\\E((Q!;[xc!BE!C!9M%(!>\_tc\O\!?!>\!?M!?QQ!>O_rc\!C!C((`!B\`((!>!>OADcc!>Dlc!>!-!CE"
DATA "!-!?Q!>!>OQ!>!>\O[Dc\\DicOc!6MEE3(!?Q!>!B!6[!>!>\!;_[!6\ODic!;^(E!C!CM!?!?!>!BO!;!;[\OA__M\\_ic\[MM%"
DATA "MM`!>\!6%``$!;!;[_DMO!6_ic!;``!?`(M!>\d!Cd(A\[Dc_\ODic!;(Q(``!?\!?(!?Q\!;A!;O_cce_jcOO((!?`!?`!?!?Q\"
DATA "f!;_qcO!6QY(MQ`QO!;A!;A!;DscO!;(d!?!>\!;!;e_uc!;!6(Q!>!BOD{cOO!>\!;_}c!;[_~ctc"
DATA ""
DATA 32, 22
DATA "~csc[_~cc[\_~c[O!;D{cO!6!?$[_cO!;vc\\5(MQ!B\_Q!?!;tc!>!-%!C!C!?!?!>!>OAQ!?Occ!B_[[jcO\O`!B\YM(!?!>!B"
DATA "O[!;!>!;c!BMO!;_ic!BE!9MM!>!BO!>Q(!>!BOA!;!;!>!B!C!;!;A_hc\d!CY(!>!>\Od!>\O[_[5!-\!;[!;_ec!6c\!9!9V3"
DATA "E`Q!B\!6[!B!B\!;__$M\OO!;A_ec!;D(MM!C!C(`$!>\!;A!;\\[[_%!;OO!;A!;_ecO[`(EM5!?!B\\%5Q!B!;!6[_D5O!B\A_"
DATA "D_ecOQ!?(!?M!?Q\%!CE!-!?QOO[DcOO\\_hcO(`!?(`(\(`(`!B[!;!;A_ccDd\Dic!;O!?(!?Q!?(Q`!B\Oe!;_c_D__lcO!6("
DATA "`!?(d`!B!;O!;A!;_sc\O`!?((!>\O!;e_ucOO`M!>!BOD{c!;OQ\O_}c[A_~ctc"
DATA ""
DATA 32, 22
DATA "~c~c~c~czcO!6OA{c\\E((Q!;[xc!BE!C!9M%(!>\_gceOjcO\!;!?!>OME!?!?!>!BODec\!CEM_ic!>!CMM(Q\\`($!>!BOADc"
DATA "c!>!C3VA_hcOV!C!CE%!?!B\!;d!>\!;[Dc\!CE!C\[_ec!;c!6!CV33M!?!>!B\!6[\!BO!;__!6M5MM[!;_ecO^E!9d!C(!?!>"
DATA "\O!;A[\O!;[_(5MM!;AODecO[`(M!CY!?$!B\!?Y(!B!;O[_D(!?MMA!;A_ecO``(!?M(!>\!9EEd!?Q\[DcO((!?d!;_ec!;Qd("
DATA "!?Q\!?QY(MQ$Q!B_cc!;!?Q!;O!;!;_fc!;O!?`(Q!?`d(Q(O\\!;_c!;``!;!;__icO!;!?%`QQY(Q`A!;A!;A_c_A__mcOO(`"
DATA "!?((Q!?d!;OODtcO!6(!?QQ(!;!;O!;O_vcO!;(Q!?O!;[D_ycO!;(!;__}cO_pc"
DATA ""
DATA 32, 22
DATA "~csc[_D~c[\_~c[O!;D{cO\`Q[_c!6Ovc\\ME!?Q$\_`$Otc!BME!9Y%QQ!>OA!>!?OccO\O!;O[hcdO`Q\dMQ!>!BO!;!;!B!;c"
DATA "\!9MMO!;_c!;ec\Md(!>!>\!?(`!>!>OA[A!>!>!C3!CA!;!;_cO_cc\M!CMM(QQ\!;!>Q\\Od[!C!C!9V\!;A!;_cO!6c!6ME!C"
DATA "!C(`Q!B\!;[!B\\!;__!?!C(!-MO!;!;O_c!;!?^M!9!CMM((!>!BO!;!;[\\A[_(5EMOe!;_cOY[!?%MM5($\O%(!?!>[O[[D5"
DATA "!-!?(!;!;A!;ODcO((Q(!?Y($\M!-dM!?!>\!;DcO(%!?f!;_cO!;!?%`QQ(!>Q`Q!?`(QQ!>_cc!6(!?!;O!;!;__ec!;O`Q`!?"
DATA "(!?`!?((`!;!>!B!;_c!;`Q!;!;_DicO\`$d!?`(`(!;OA!;!;Dc_A__mcOO$(``Q(Q!;Ad!;_tcOOd`((A!;OO!;DvcOO!?!?(["
DATA "OOD_yc!;!6`O_D}cODpc"
DATA ""
DATA 32, 22
DATA "~cocOO\!;[|c\YQ$\[zc\!?`!>!B\O_yc!;!B!>d(QDyc!;(!C5d(!B[wc[!9!C5!CE%!?!>!>_pc_dc\O(Q$\M((`Q\O_ocOP_O"
DATA "!CMEY`!>\!?Q`!>\OADcc\DjcO[!C!>d!BQQ!>!BO!>Q!>\!;[Dc\ODjc!;!?($d!B$!?!B\!6!;d!>OA[!6O\Djc!;(!?Q!B\!B"
DATA "!>(\O!;!;[\O!;__(\ODjcOO!?$!>!B\!>(O%A!;O!;A[[D!;\\DlcO!>!B!>!>Q!?`!>O!;!;O\ADc_\\DlcO$d!>$(!?!BO!;A"
DATA "!;\_ccDd_mc_[[d!;!?!>!;A!;!;O\Dxc!6!>e!;O\_xc\!>O_D_[[_yc!;_~c~c~c{c"
DATA ""
DATA 32, 22
DATA "~cocOO\!;[[_zcOYQ!B!BO!;_xc\!?`!>\O\!;Dxc\!>!BM!?!?!B[_cO!;uc[5!C!C(M!?Q\D`(Osc!;ME5!C(MQQ!>OA!>`Occ"
DATA "O__[fc_dc\\((Q\YM`Q!>!>!;[[!>Oc\M!;!;_fcO^D!B!CEM!?Q\\Q(`!B!>O!;[AQ!BEA!;!;_fcO_!CM!>!>$`Q!>\!;d!>\"
DATA "!;[_[%MOd!;DfcO``d!B!>`(!B\\!;d!B[A_(!C\O!;!;O_fcO!?`Q\!B!>QM!BO!;!;[!BO!;[_%O\\d!;_fcOY($\!>!>Q(!;("
DATA "([O!;\[[DMOO\A_D_gc!;(!>!>!B!>d!?!BO!;!;A\!;DcO\\O_kcO!Bd!>`!?!?Q!;[AO\_cc_Od_lc[!;[!;OM!?!>O[!;OO\_"
DATA "c__tc!;YQ!6[AO!6ODxcO!>O!;d_[_yc!;__~c~c~czc"
DATA ""
DATA 32, 22
DATA "~cocOO\!;[|c\YQ$\[zc\!?`!>!B\O_yc!;!B!>d(QDyc!;(!C5d(!B[wc[!9!C5!CE%!?!>!>_gc\\!6Ofc_dc\OM`$\M(!?!?Q"
DATA "!B\Dec\M!CM_fcOPDOMY(`!?\OQ((Q\O!;Dcc!>E3!CO_fc!;_MM!>!B!>(!>!>\O!>!>$\O[DcOEV!C\O_fcOM(!B!B\!>`Q!>\"
DATA "O!;\!>\!;__!;M%!95!;A_fc!;((d!>Q(!>!B\!;A[\\A__(E%M!;A!;_fc!;M!?$d!>!?M\d!?$!;!6!;_DeMO!;ADgcO(Q!>!B"
DATA "!B!?`($!>\!B$\[DcO`(!?!;[!;DhcO$!B!>!>``(Q$\$(!B_cc!6(`!;O!;O_ic!;d[O!?Qe!>Q\!;DcO$!?!;!;__ocO(!>\!B"
DATA "$(!;!;ADc_A_DqcO!?$!>\!>(d!;_wc!6!;A[`QOOAD{c_!;!;D_~c_~cqc"
DATA ""
DATA 32, 22
DATA "~cocOO\!;[[_zcOYQ!B!BO!;_xc\!?`!>\O\!;Dxc\!>!BM!?!?!B[_cO!;uc[5!C!C(M!?Q\D`(Osc!;ME5!C(MQQ!>OA!>`Occ"
DATA "O\!6OA[dc_dc\\M(!B\!?(!?(!>\O!;!;!>!;c\!CYM!;!;_dc[PDOEM%!?!?!BOQQ`!>!B\A[A!>O!CV!CdO_dcO_5E!B!BQ!?`"
DATA "!>O!;!>Q\\Od[%!9EM\!;A!;_dc!;(!?$d!>%!>!>\!6!;!>\\!;A_`M5!C!-!;O!;O_dcO((!>!>\!>($!B\O!;[\\!;[_M5(M"
DATA "f!;_dc!;M($\!>Q`\!;M``$!;A!;[DM(M(!;!;OO!;DecO(d!>$!?(!?!B!>!BQ!>\[DcO``(O!;!;A!;_ecOQ$!B!>QM(`d!>`("
DATA "\_ccOQ`d!;A__gc_[[!;!;OM!?Q!>\!>%\!;_c!;$Q!;!;D_ocOY!B$!B$QA!;O_c_!;__qcO`!B!B!>!>`d!;_wc!6!;A[!B`O"
DATA "!;A_{c!;\[__~cD~cqc"
DATA ""
DATA 32, 22
DATA "~cocOO\!;[|c\YQ$\[zc\!?`!>!B\O_yc!;!B!>d(QDyc!;(!C5d(!B[wc[!9!C5!CE%!?!>!>_mc!;cc[dc\\!?!?!>\YM(!?!>"
DATA "\O_ec\gc\!;_!;!.D!B!9EMQQ\O$(d!>OA_ccO_gcO`[__Md!CY(!>!>\Od!>\O[Dc!>\_gc!;!?`_ME3K3EY!?!>\!;[!>\\!;["
DATA "_O!>!6DgcO!?(_(!9dM(Q!>!>Od!;!BOA[_MO\_gc!6Q`[M(MMY(!B\!6(M!?!B!;!6[A^%\O_gcO((d!?`M(`\M!-EM(`A\[Dc_"
DATA "\O_hcO!;`!?(Q!?`!>$(``\!;!;[O_ccD[_DjcOO`!?(`d!?`!?\Od!;O_qc!;!;(M`Q`(`!Be!;O_scOO(`!?(!?OO!;e_ucO!;"
DATA "!?Y!?\!;_{cOO!?\O_}c!;[_~ctc"
DATA ""
DATA 32, 22
DATA "~cocOO\!;[[_zcOYQ!B!BO!;_xc\!?`!>\O\!;Dxc\!>!BM!?!?!B[_cO!;uc[5!C!C(M!?Q\D`(Osc!;ME5!C(MQQ!>OA!>`Occ"
DATA "O__[cc!;cc[dc\\M($OY(!?Q`\O[!;QOc\E!;[_cc!;!;_OPDOEM%!?`!BOQQ`!>!B\A[A!>O!C!;!;O_ccO([[_5EE!C((`!B\"
DATA "!;Q!>!>\!;[_[!CM\!;A!;_cc\Q`[(!9E33(YQ$!B!6[d!B!;A_`EO\O!;O_cc!;!?([EME!CEM!?`!BO!;!;[\\!;[_%d\!;A!;"
DATA "_cc\(M[QM5MY(!B\!6M!?($[!;[_DM!;!B\!;[D_cc!;`((!?e($\EM5M!?Q!;\!;DcO!;\O!;_fcO!;d(`!?!?\`(%Q!B!;!;["
DATA "!;_cc!;O\\Dic!;!;(d!?((!?``\O!;O!;!;_c__[[lc!;\QM`!?Q`(\OA!;O!;[scO!;Q(Q``\\!;e_uc!;O!?!?`OO_{c!;!;("
DATA "\O_}cO[_D~csc"
DATA ""
DATA 32, 22
DATA "~cocOO\!;[|c\YQ$\[zc\!?`!>!B\O_yc!;!B!>d(QDyc!;(!C5d(!B[wc[!9!C5!CE%!?!>!>_gc\\!6Occ!;cc_dc\\!?!?!>\"
DATA "Y(M!?Q!>O_ec\!9MM_cc!6O[!;^DO!CM(!?!?!>\$!?`!B\O!;Dcc!>!C3MADccO!?[__EMVEMM`!>\!;!>Q$\O[Dc\!9!9E\!;D"
DATA "cc!6`(_Q!-!CVE%!?Q$\O!;!>\O!;__!6%E!C3OA_ccOQ!?_M!9!CMM((!>!BO!;!;[\\A[_(5EMO!;!;_cc!6!?!?[Y%MEM($\"
DATA "!6!?Y(!>!;!;[_^5MM(!;OA_ccO!?`!?(!?QM(!>\5!C(%``$\[Dc\!?((d!;_dcO!6g!?\QQY(Q`QQ!>_ccO(`!;OO!;_fcO!;"
DATA "!?((!>!?``Q((!?!;!B!>ODc!;!?Q!;!;__icOO`MQ!?(QQ``A!;!;[!;Dc_!;__mcOO((Q!?(Q(!;A!;!;[Dtc!;Od(!?`A[A!;"
DATA "O_vc!;O(Q`d!;__yc!;\(A_D}cODpc"
DATA ""
DATA 32, 22
DATA "~cocOO\!;[[_zcOYQ!B!BO!;_xc\!?`!>\O\!;Dxc\!>!BM!?!?!B[_cO!;uc[5!C!C(M!?Q\D`(Osc!;ME5!C(MQQ!>OA!>`Occ"
DATA "O\!6OA[Occ_dc\\MQ!>OY(%Q!>!BO!;!;!B!;c\ME(!;!;_!6!;[!;^D!B!C5!?!?Q$\`(`!>!BOA[A!>!>V33O!;A_OQ[[_MM!C"
DATA "E!C`Q!>\O!>!>$\!;[_[M!95!C\O!;!;_\Q(_M!9!CMEMYQ$\O!;!>!B!B!;A_$M!9E!C[A!;O_O(([M!C!-!CEM!?!B!B\!;!;["
DATA "\\[[_!-M(E!;Ad!;_!6($[((MME`!>\!6M%Q!>!;O[_D(M5(O!;O!;A_OQ`Q(M`(!?$\!CME%(!?$\[DcO(MM[e!;_cO!6!?!?("
DATA "!?Q$!>`((d!?!>!>!B_cc!6(Q[A!;A__ec!;OQ(!?`Qd!?(``!;\!>!;Dc!;`!>!;!;D_icO\(!?d`((!?`OO!;[ODc_A__mc\O`"
DATA "!?`!?Q!?(!;O!;!;[_tcOOQ`($Q!;!;A!;O_vcOO!?(!?[[!;__ycOO`A_D}cODpc"
DATA ""
DATA 16, 24
DATA "~c~cwceWcceWcceWJ>JJWWJ>>JWWJ>JDWe>Jd>J>>JJDWcW[>+d>JJ>J[DWdcWWJJ>>JdDWWecWW>>JJ>>JJWWdcW>>JJDJJe>JW"
DATA "cW>JJ[>J>dJ>+>>W>JdDf>JDDJ>DfWDJ>JJDfWfcWWA_WWce gcW!;DWdcd gcWO_Wfc gcW!;_WmcWO_Wdcd gcW!;DWg gcW__"
DATA "Wdcd hcWWdc  sc"
DATA ""
DATA 16, 20
DATA "&(oc55(%mce5&(kc((Ed5((ic%&(&!-5E55(gc((&(&&e5((ec5(%&((&(!-e5(cc55%(&&(&(&e5%&5E!C%%(`((&&(!CE5E(%"
DATA "e5((&(&d(5%`((&55%5&(!?(`(%&(&(&(&e5((&((&cc`d(&&!<!-55&&%&ec(`&(&`Ed5((gc%(e&d5Eice(&(55kc!?&((&(mc"
DATA "(&((oc&(qc"
DATA ""
DATA 16, 20
DATA "oc\\mc\d!Bkc!B\$!B$!Bic\\e!B\\gc!B\\d!BOd\ec\f!Bg\ccO\!B!B\!B\\Od\Ud\e!BO\\dO\\!B!B\!B$!B!B\O\Od\$"
DATA "e!Be\Oe\e!B\\O\O\!B\O\!B!B\!Bk\!B\!B!B\Oe\O\!B\\!B\!BO\!B\!B\cc\\d!B$d\O\Oece!Bd\OO\gc!B!B\Od\OicO\O"
DATA "\!B\kc\O\\mc\\~cc"
DATA ""
DATA 32, 28
DATA "qcK~ccVKVV|cKVKVKVVycfVKeVvcV!9VKVKVKVKVKKtc!9VjK!H!HK!Hqc!9VVgK!H!HK!H!J!JK!H!Hnc!9VVe!HK!H!H!Jd!H"
DATA "e!JKK!HkcVdK!J!H!J!J!H!H!J!J!HK!H!J!H!J!HKd!Hic!HKK!H!Hh!J!HeK!J!HK!H!HK!Bic!HK!J!H!Hf!J!H!HgKVdK!B"
DATA "!Bhc!Je!Hd!J!H!JK!HVV!'V!9VKdV!B!B\gc!H!HdK!HK!J!J!HKVKd!'!C!'!9V!9V\\O\fc!J!HfKd!HK!9!9!CdEd!'dVe\"
DATA "ec!HdKVK!H!HK!HKKV!C!CE!C!C!-!'VVKd\O\ecKVVKKe!HKKV!'!'!CE!C!C!'!9!9Vf\!BdcK!9V!9K!Jd!HdK!9V!'!'!C!C"
DATA "!'!'!9Kd\!B!B\!BccKVV!9KKd!H!J!HdK!'!'!9!9!'!'!9VO!B\e!B\cKKV!9KV!H!H!J!H!H!J!H!JVKVf!9V\!B\!B$\O\KK"
DATA "dVK!Hg!J!HKKeVd!9\d!Bf\cV!9VK!HK!H!Hd!J!H!J!HV!9VV!9V!9\!B\OOe\eceKd!H!J!H!JKfV!9!9!B!B\OOd\iceK!H!J"
DATA "KKVd!9VV!9!B\Oe\mcfKVh!9\\OO\qcK!9VVd!9!'!9!'\OO!Buc!9!9V!9!9!'!9d\ycd!9V\O}cV\pc"
DATA ""
DATA 32, 28
DATA "qc!J~cc!J!JS!J|ch!Jycg!JSd!Jvc!H!J!H!JS!JS!JdS!J!Jtc!H!J!JSS!JhS!0Sqc!Hd!JdS!0dS!0!0S!0!0SSnc!H!J!J"
DATA "fS!0S!0!0SSd!0eSkc!JSS!0!0S!0Sd!0Sd!0S!0SS!0S!0SicS!0S!0Sj!0eS!0eS!Bic!0S!0SSf!0eS!JdS!JS!J!J!B!Bhc"
DATA "!0dSg!0SS!J!JK!J!Hf!J!B!B\gc!0S!0S!0S!0!0S!0d!JK!HKVK!H!J!H!J\\O\fcS!0gS!0S!J!H!JKdVKK!Hd!Je\eceS!JS"
DATA "!0S!0S!JS!HK!9V!9VKK!J!JSd\O\ecSd!JdS!0dS!JdKdVK!H!H!Jf\!BdcS!H!J!HSS!0SS!0S!J!H!J!HVdK!H!HSd\!B!B\"
DATA "!BccS!J!J!HS!JSe!0SS!Je!HK!H!H!JO!B\e!B\cS!J!J!HS!JSe!0S!0!0e!J!H!J!H!H!J\!B\!B$\O\SSd!JSSh!0SSe!J"
DATA "d!H\d!Bf\c!J!H!JSSg!0S!0S!J!H!J!H!H!J!H\!B\OOe\ec!JS!JSSf!0h!J!H!B!B\OOd\icS!JS!J!0SS!J!Jd!H!J!H!H!B"
DATA "\Oe\mcdSe!Jg!H\\OO\qc!J!H!J!J!H!J!H!J!HK\OO!Buc!H!J!H!Jd!Hd\yc!J!J!H!H\O}c!H\pc"
DATA ""
DATA 32, 28
DATA "pc`~c`$``{c$`$`$``xcg$e`uc!?!?`$`d$`$``$rcd!?j`$``pcd`$d`!?`d!?`$`$``mcd`$$g`d!?e`$`$kc$``$`$$`$e`!?"
DATA "`!?!?d`$$`jc%`$`$`$f`!?`e!?f`$ic!C5`$``$g`!?`d!?`!?e`ic&55$m`f!?e`hc`(&5$`!?``!?k`!?`!?`!?`gc(`&(d`"
DATA "!?!?p`!?``fc(&%&f`!?e`$$Q$!>$``$`d!?`ec5%%`&&`!?`d!?d`g$`$d`!?`ecd5(%&``!?``!?!?`$$Qd$!B$`$$`!?``dc("
DATA "e5((&`e!?d`$!>$$!>d$g`cc%((!C55E(d`!?`!?$`$$!>d$!>$``$d`!?cd(%(e5h`l$f`c&`((&!?d5``$$`$`$Q$!>$!>$Q$`"
DATA "$$``ec&&((&(55$$Qd$`j$`$ic&`(%&%5Q$Q$`$`$Q$Q$$`$mc&((&(l$`$qc&%(&$Q$Q$`$Q$Quc&((h$yc&($Q$Q}c&$pc"
DATA ""
DATA 32, 28
DATA "pc(~c`d!?{c`!?``!?(!?xc!?`i!?uc%Y(!?!?(!?(!?(!?(!?rc%%Y(Y(!?(!?d(d!?pc(Y(!?(Y(%(%%Y(!?((!?`mc((f!?d("
DATA "Yd%Ye(!?!?kc!?(`!?`!?((!?(Y(Y%Y%%(Y(!?!?(jc%`d!?`!?f(%Y%Y%%d(!?!?(ic!C5`!?`(!?(!?(Y(Y%Y%Y%Y%Y(!?(ic&"
DATA "55((!?(%h(Yd%Y%e(hc`(&5((Y(Y%Y(Y(Y(Y(Y%%(%(%(gc(`&((!?(Y%Yd(!?(!?f(Y(%%Y(fc(&%&`(!?(Y%Y(!?(d!?`&!?`("
DATA "!?(%%Y(ec5%%`&&(%(%%Y((!?!?f`d!?((%(ecd5(%&`(%(Y%%(!?`!?``$d`!?!?(%(Ydc(e5((&(e%Y(!?`$``$``!?(!?e(cc"
DATA "%((!C55E(!?(Y%Y%!?!?(`$``$$``(!?(Y(Ycd(%(e5!?!?e(d!?d`$d`d!?d(Yc&`((&!?d5`!?(!?(!?(!?!?`&`$`!?!?`!?("
DATA "!?Yec&&((&(55``!?!?`!?`!?!?f`(d!?ic&`(%&%5!?`!?``!?`!?!?`!?!?(!?(mc&((&(`!?e`!?!?``!?`!?qc&%(&`!?`!?"
DATA "`!?`!?`!?uc&((h`yc&(d`!?}c&`pc"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~cjc!D`~c~cec`!D~c~cec!D`~c~ceca^~c~ceca^~c~ceca^~c~ceca^~c~ceca^~c~ceca#"
DATA "~c~ceca^~c~cec!.#~c~cdc<77H~c~c<!(e-<!(~czc!(<i-!(!(~cvc<!(--*d-*d-*-<!(ec!@!3!3C~cjc!(!(q-<:!5!5!E"
DATA "!#!#L!@!GC~cec<!(q-7-!E;!E!E1!#IG1!=!=C~cc!(!4t-7!%dL!%1e!=bG~ccH7*d-*d-*d-*-7-7-7-!%!)!);!E!E!%11!="
DATA "X!2~cc<77--*n-7-7b;22;!E!E!=!=XX!+~ccH**7-7k-7-7-7HR!3!=!E;!)1!=G!&a~cccHd*!177--?i-7hH!3GG!&~cfc<*"
DATA "!1*!1**7-7*d-*--HRdHRHRHRa#~cec!@!377*!1d*!1!177d-7<hHRdH!2WF~cc!5!@]]X!(77!1*!1*!1**7-7iHRHdR!2!.!@"
DATA "'F{c!@!@e1]]b!(77f*C7-jHRRT!3!2!.d1F'wc!5!@616161611]!5!(!(7!1*!1H-7<dHRHRR!+T!G!@!G!2W161!@!@!2FtcY"
DATA "!)116d16d1d]b!(77*C7-gHRT!3!G!@1]!&P!3g!@'sc!@!);6161616161611]!5!(e7HHdRT!G!@!@161]b!5]]161!@!3!+sc"
DATA "!3]1!E;!)6d16g1]]b!(77HRRT!3!G!@l1b!5TT!+sc!3d]1!);6161616161611]X!(!+T]1616161616161]!@!3TGT!+sc!3"
DATA "f]1!E;!)116d16f1]1]d16d16e1b!@gT!+sc!3h]1!);616161616161616161616161]!@!3dTGdT!+qc!;!6k]1!E;!)6d16d1"
DATA "6d16d1611b!5lT__mc!6!6Y!?m]1!);6161616161616161]!@TTGdTGdTGTT!;O_[ic!6!;YMQ\bn]1!E;!)116d16e1b!@oT!F"
DATA "!;d!6[[fcOYYMQY!>!5Xo]1!);61616161]!@!3dTGdTGdTGTT!FF[!;!6O\!6Dec!6Y!?dY!?Y\!5bbi]bbd]1!E;!)611b!5oT"
DATA "!F!FF[!6!;\!6\!;[Dfc!;[!>QYYMM!?Q!5X]bf]d!2bb]]1!);b!@TTGdTGdTGeT!FF!;d!6!;!6[!6__ic!;!6Y!?MMYQ!>!>"
DATA "!5bbe]e!2!&be]!GnT!F!FF!;!6!6d\[!;_Dmc!6!6!>Q!?Y!?M!>\!5Xd]h!2b]bTTGdTGdTGTT!FF[e!6\[[DDqc[!;!?!?Y!?"
DATA "MQ!>\!5bbg!2Pb]bkT!F!FF[[!6!6\!6!;[D_uc!;[!?QYM!?Q!>Q!5!&f!2Pb]b!3dTGeT!FF[!;!;!6!6!;!;[DPyc!;!;!>Q"
DATA "!?!?Y!?!>!+f!2Pb]bgT!F!FF!;!6e\[!;__}c!;!6!>QQdY\d!&!2Pb]bTTGTT!FF[!6\!;!6!6!;!;DD~cdc!6!;!>!?MYY!>"
DATA "!>!+!2Pb]!5TT!F!FF!;!;e!6!;[DD~chc!;[!>!>M!?YY!>P!5X!5T!FF[O\\MQ[!;DP~clc!;!;!>!>dQ!?!?!>!GF\dQMY!;"
DATA "!;_D~cpc!;[YYQ!?QYQ!>YdQ[!;DD~ctc!6[!>!?YMQY!?Q[!;__~cxc!;!6!>QM!?!;!6D_~c|c!6!6!?!;D_~c~ccc!6D~c~c"
DATA "~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~cjc!D`~c~cec`!D~c~cec!D`~c~ceca^~c~ceca^~c~ceca^~c~ceca^~c~ceca^~c~ceca#"
DATA "~c~ceca^~c~cec!.#~c~cdcZ/,Z~c~cU=Nd/U=~czc=Ui/==~cvcU=N/N/N/N/N/N/U=ec!@!3!3C~cjc==/Nd/Nd/Ng/Z:!5!5"
DATA "!E!#!#L!@!GC~cecU=N/N/N/N/N/N/Nd/9/!E;!E!E1!#IG1!=!=C~cc=,d/Nq/!%dL!%1e!=bG~cc=//NN/N/N/N/N/Nd/9/9/"
DATA "!%!)!);!E!E!%11!=X!2~ccZ=/,NN/Nd/Nk/b;22;!E!E!=!=XX!+~cc=**,,//NN/N/Nd/9e/Z!8!3!=!E;!)1!=G!&a~cccZd*"
DATA "!1d/NNj/hZ!3GG!&~cfcZ*!1*!1**,,//NN/N/,ZAZAZ!8Z!8ZAa#~cec!@!3,=*!1d*!1!1//,NN/iZ!8dZ!2WF~cc!5!@]]X=/"
DATA ",!1*!1*!1**,,/ZZAdZAZ!8Z!8ZZ!2!.!@'F{c!@!@e1]]b=,/f*C//jZ!8!8T!3!2!.d1F'wc!5!@616161611]!5=/,!1*!1H="
DATA "/AZAZAZ!8ZZT!G!@!G!2W161!@!@!2FtcY!)116d16d1d]b!D,/*C,/fZ!8!8T!3!G!@1]!&P!3g!@'sc!@!);6161616161611]"
DATA "!5=/,//AZ!8ZZT!G!@!@161]b!5]]161!@!3!+sc!3]1!E;!)6d16g1]]b=,=Z!8!8T!3!G!@l1b!5TT!+sc!3d]1!);61616161"
DATA "61611]X=ZT]1616161616161]!@!3TGT!+sc!3f]1!E;!)116d16f1]1]d16d16e1b!@gT!+sc!3h]1!);616161616161616161"
DATA "616161]!@!3dTGdT!+qc!;!6k]1!E;!)6d16d16d16d1611b!5lT__mc!6!6Y!?m]1!);6161616161616161]!@TTGdTGdTGTT"
DATA "!;O_[ic!6!;YMQ\bn]1!E;!)116d16e1b!@oT!F!;d!6[[fcOYYMQY!>!5Xo]1!);61616161]!@!3dTGdTGdTGTT!FF[!;!6O\"
DATA "!6Dec!6Y!?dY!?Y\!5bbi]bbd]1!E;!)611b!5oT!F!FF[!6!;\!6\!;[Dfc!;[!>QYYMM!?Q!5X]bf]d!2bb]]1!);b!@TTGdTG"
DATA "dTGeT!FF!;d!6!;!6[!6__ic!;!6Y!?MMYQ!>!>!5bbe]e!2!&be]!GnT!F!FF!;!6!6d\[!;_Dmc!6!6!>Q!?Y!?M!>\!5Xd]"
DATA "h!2b]bTTGdTGdTGTT!FF[e!6\[[DDqc[!;!?!?Y!?MQ!>\!5bbg!2Pb]bkT!F!FF[[!6!6\!6!;[D_uc!;[!?QYM!?Q!>Q!5!&"
DATA "f!2Pb]b!3dTGeT!FF[!;!;!6!6!;!;[DPyc!;!;!>Q!?!?Y!?!>!+f!2Pb]bgT!F!FF!;!6e\[!;__}c!;!6!>QQdY\d!&!2Pb]b"
DATA "TTGTT!FF[!6\!;!6!6!;!;DD~cdc!6!;!>!?MYY!>!>!+!2Pb]!5TT!F!FF!;!;e!6!;[DD~chc!;[!>!>M!?YY!>P!5X!5T!FF["
DATA "O\\MQ[!;DP~clc!;!;!>!>dQ!?!?!>!GF\dQMY!;!;_D~cpc!;[YYQ!?QYQ!>YdQ[!;DD~ctc!6[!>!?YMQY!?Q[!;__~cxc!;!6"
DATA "!>QM!?!;!6D_~c|c!6!6!?!;D_~c~ccc!6D~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cgcf!(<HH~czc!(!(f-7--HH~cwc!(i-e7R~cuc!(--?d-*e-7"
DATA "!(!(R~ctc!(--d*e-d7!(!(W~csc<--**?d*-77-7!(<HW~crcG--e?*--d7!(HRR-!(<!(d<H~ckc!377-*-*-d7!(<HRh-7--H"
DATA "R~cic!@!=b77-d7!(!(<HRk-d7R~chc:LbX!5!=7!(!(<<R'd-?f-7--7!(!(R~cgc!@!%!%!=X!=b!@!@!3GF!&--?d*?d-d7!("
DATA "<W~cgc:LL!%!=Xb:!G!GGCd-**?d*-77-7!(<RW~cfc!3L!%!%1X!5!5!@!G!GG1d-d?*--d7!(!(HH!+~cfcG!=L!%!=X:!5!3"
DATA "!3!GG177-*-*-f7<<HR!&~cfc!&!=!=11X!@@,Z!8G!5!%1h7!(!(<HHCG!+~cfc!2X!=!=X!=:,9,!8G:!=11!=!57d!(HHG!3"
DATA "!FG!3!+~cfc!@!G!3bb!=!2,Z!8ZWC!=!=11X!5X!5!5!@!G!3!GG!G!G!+~cfc:L!G!GG!GC!2!&!&!2^Fd!=X!=!5b!5!5:!G"
DATA "!FG!3!G!G!+~cfc!@!%!%bX!G!2!&!+a!.^G!=!=bb1!=!5:!G!G!3GGd!G!&~cfc:d!%!=XC!2!&aP^!3g!=!5!5!@!3GTG!G!G"
DATA "!3!&~cec!G!=!%!%11!5!2!2!+a!.^GL!%!=b1!=!5:!@!GdG!GGG!.~ccc!@!@]b!=11!=!5C!2!&aP^G!=L!%1X!=!5!5!@!3G"
DATA "!G!GTG!+PzcDUcc!@!@11]!5!=!=11X!2!&!+a!.^C!5!@11XX!=X!5:!@!G!3C!&!2!+xcD!D!*0U!D16161bX!=bb!3!2!&!+a"
DATA "P^F!=!5:!@G!5X!5!5!@!G!G!+!&!2!3!+vcDU0d!*e!8e1b!5X!5!GGaaW!.!+C!=!=!%1!3GCGCC!2!2!GG!G!G!+uc^!*0!*0"
DATA "d!8A!80!*016X!5X!5:!G!5:!&!2!+Ff!=!5bb!5!@!GTG!3!G!G!+ucD!D!*f!8!*0d!*::XXe:!+!2!2!&^Gd!=b1!=!5:!@!G"
DATA "!3GGe!G[scT!8!8!D!*_!80!*0!*0e:XX::d!+!&!.P^!3!=bbX!=!=!5!5!@!3GTG!G!G!3G!G[[pc!3L!)b_!D!8!8d!*!8d:"
DATA "eX!2!+!+aWWPP^!F!%!%bX!=!=!5:!@!GdG!GGGT!3!@!@_[mc!;d]L!Eb!*!8D!*_!8!5:!3XX!&!2!&WD_T!FTd!3!=!%1!=Xb"
DATA "!5!5!@!3G!3!GTGFT!@!@!3T!6[!+ic!6!6Qbe]L!)b_!D!*_!8!5!G!G!&!&!+#!8!8!*0!*!G!@11bb!=!=!5!5bX!5!@!G!G"
DATA "!3CFT!@eTd\__fc!;MQ!>\!5be]L!Eb!*!*D!*!D!GaD_!80!*0!*0!8!8161]bX!@!@!5:!@!G!3!FGT]!GdT!F!F[!;d!6Dec"
DATA "!6!?!?YQ!?\!5bbd]L!)b_D!*!*!8!8d!*0!*f!8!*e1]]ebd]!3eT!FF!;!6!6\!;[Dfc[!;!>QYM!?!>!5Xe]L!Eb!8!*D!8!D"
DATA "!*0!8Ad!80!*0!*016161616]!GTGT!F!F!2[d!6[[D!.ic!6[Y!?QY!>!>!5be]L!)b!D!8!8!De!8d!*0!*!8!8#d161]!3eT"
DATA "!FF[[!?M[!;DDmc!;!6!>!?QQ!?!>!5be]L!Eb!8!*!D!8_!*0!*0!8AD#U!G16]!GdT!F!FF[[Y!?[!6_Dqc!6!6dYQ!>\!5bb"
DATA "d]L!)b!D!D!8!*!*!8!8DDUUOT]!3eT!FF!;!6YQ!;!;DDuc!;!6Q!?YM!?!>!5Xe]L!Eb!8!8!D!*#U!8O!3]!GTGT!F!F!2[!;"
DATA "!?Y[!;_!.yc!;[!?QYY!?\!5be]L!)b_!DU!8T]!3eT!FF[!;YQ[[_D}c[!;Y!?MMQ!>!5be]L!Ebb]!GdT!F!FF[[QQ[[_D~cdc"
DATA "!;!6!>QQ!>!?!>!5bbd]L!3eT!FF!;!;QQ[[D_~chc!6!;!>QYM!>Q!5Xd]GT!F!F!2!;[YY[[_D~clc!;!6!>!?YQ!>!>!5bbT"
DATA "!FF[!6YQ[!;DD~cpc!;!;QdYQ!>!5F[\!?M[!;DD~ctc!6[Q!>QY!>!?Y!?!;[__~cxc!;OQ!?QY[[D!.~c|c!;!6Q[__~c~ccc["
DATA "P~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cgcUe=dZ~czc=UN/NN/9/,ZZ~cwc=/dN/Ne/==!8~cuc=/fNe/,"
DATA "/!D=D~ctc=iNe/==!8~cscZ//N!*!J!*NN/9//=UZ!8_~crcG//NNdKN//d=ZZ!8N=U==dZ~ckc!3,d/N9N9//=UZZN/N/NNd/,Z"
DATA "Z~cic!@!=b//,//,!D==Z!8iNd/==!8~chc:LbX!5!=!D=UZZ!8'N/dN/N//N/,/!D=D~cgc!@!%!%!=X!=b!@!@!3GF!&/iNe/="
DATA "U!8~cgc:LL!%!=Xb:!G!GGC,/NN!*!J!*N9/9/==!BZ!8_~cfc!3L!%!%1X!5!5!@!G!GG1/dNKKNN//=,==ZZ!+~cfcG!=L!%!="
DATA "X:!5!3!3!GG1,9//N9N9//=!D=AZ!8!&~cfc!&!=!=11X!@@,Z!8G!5!%1=d/=,==UdZCG!+~cfc!2X!=!=X!=:,9,!8G:!=11!="
DATA "!5!D=U==ZG!3!FG!3!+~cfc!@!G!3bb!=!2,Z!8ZWC!=!=11X!5X!5!5!@!G!3!GG!G!G!+~cfc:L!G!GG!GC!2!&!&!2^Fd!=X"
DATA "!=!5b!5!5:!G!FG!3!G!G!+~cfc!@!%!%bX!G!2!&!+a!.^G!=!=bb1!=!5:!G!G!3GGd!G!&~cfc:d!%!=XC!2!&aP^!3g!=!5"
DATA "!5!@!3GTG!G!G!3!&~cec!G!=!%!%11!5!2!2!+a!.^GL!%!=b1!=!5:!@!GdG!GGG!.~ccc!@!@]b!=11!=!5C!2!&aP^G!=L!%"
DATA "1X!=!5!5!@!3G!G!GTG!+PzcDUcc!@!@11]!5!=!=11X!2!&!+a!.^C!5!@11XX!=X!5:!@!G!3C!&!2!+xcD!D!*0U!D16161bX"
DATA "!=bb!3!2!&!+aP^F!=!5:!@G!5X!5!5!@!G!G!+!&!2!3!+vcDU0d!*e!8e1b!5X!5!GGaaW!.!+C!=!=!%1!3GCGCC!2!2!GG!G"
DATA "!G!+uc^!*0!*0d!8A!80!*016X!5X!5:!G!5:!&!2!+Ff!=!5bb!5!@!GTG!3!G!G!+ucD!D!*f!8!*0d!*::XXe:!+!2!2!&^G"
DATA "d!=b1!=!5:!@!G!3GGe!G[scT!8!8!D!*_!80!*0!*0e:XX::d!+!&!.P^!3!=bbX!=!=!5!5!@!3GTG!G!G!3G!G[[pc!3L!)b_"
DATA "!D!8!8d!*!8d:eX!2!+!+aWWPP^!F!%!%bX!=!=!5:!@!GdG!GGGT!3!@!@_[mc!;d]L!Eb!*!8D!*_!8!5:!3XX!&!2!&WD_T!F"
DATA "Td!3!=!%1!=Xb!5!5!@!3G!3!GTGFT!@!@!3T!6[!+ic!6!6Qbe]L!)b_!D!*_!8!5!G!G!&!&!+#!8!8!*0!*!G!@11bb!=!=!5"
DATA "!5bX!5!@!G!G!3CFT!@eTd\__fc!;MQ!>\!5be]L!Eb!*!*D!*!D!GaD_!80!*0!*0!8!8161]bX!@!@!5:!@!G!3!FGT]!GdT!F"
DATA "!F[!;d!6Dec!6!?!?YQ!?\!5bbd]L!)b_D!*!*!8!8d!*0!*f!8!*e1]]ebd]!3eT!FF!;!6!6\!;[Dfc[!;!>QYM!?!>!5Xe]L"
DATA "!Eb!8!*D!8!D!*0!8Ad!80!*0!*016161616]!GTGT!F!F!2[d!6[[D!.ic!6[Y!?QY!>!>!5be]L!)b!D!8!8!De!8d!*0!*!8"
DATA "!8#d161]!3eT!FF[[!?M[!;DDmc!;!6!>!?QQ!?!>!5be]L!Eb!8!*!D!8_!*0!*0!8AD#U!G16]!GdT!F!FF[[Y!?[!6_Dqc!6"
DATA "!6dYQ!>\!5bbd]L!)b!D!D!8!*!*!8!8DDUUOT]!3eT!FF!;!6YQ!;!;DDuc!;!6Q!?YM!?!>!5Xe]L!Eb!8!8!D!*#U!8O!3]!G"
DATA "TGT!F!F!2[!;!?Y[!;_!.yc!;[!?QYY!?\!5be]L!)b_!DU!8T]!3eT!FF[!;YQ[[_D}c[!;Y!?MMQ!>!5be]L!Ebb]!GdT!F!FF"
DATA "[[QQ[[_D~cdc!;!6!>QQ!>!?!>!5bbd]L!3eT!FF!;!;QQ[[D_~chc!6!;!>QYM!>Q!5Xd]GT!F!F!2!;[YY[[_D~clc!;!6!>!?"
DATA "YQ!>!>!5bbT!FF[!6YQ[!;DD~cpc!;!;QdYQ!>!5F[\!?M[!;DD~ctc!6[Q!>QY!>!?Y!?!;[__~cxc!;OQ!?QY[[D!.~c|c!;!6"
DATA "Q[__~c~ccc[P~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cfc!2R"
DATA "!2!&~c}cCH!4*?.*!7!7~cycC!7d.d?*!4!4!2R~cvcG!1..!I..!1!4?*?.*!4!7~csc!7d!I.!I!1!1d.d?*!4!4!2!&~cpc!G"
DATA "88d!I*!1..!I..!1!4?*?.*!7!7~cmc!5e8!I*f!I!1!1d.d?*!4!4!2R~cjcX82d8*88!I8!I*!1..!I..!1!4?*?.*!4!7~cgc"
DATA "X!I2828?e8!I!=d!I.!I!1!1d.d?*!4!4!2!&~cec!42828!E82d8*88d!I*!1..!I..!1!4?*?.*!7!7~cc!4e8!I!I2828Le8"
DATA "!I*f!I!1!1d.d?*!4!4!2R}c!3!Id8?2828!E82d8*88!I8!I*!1..!I..!1!4?*?.*!4!7zc!71!7!I8!%e8!)!I2828?e8!I!="
DATA "d!I.!I!1!1d.e?!2RW!&wc!G1!%1!%!4!=!Id8?2828!E82d8*88d!I*!1..!I..!2R!2!7!&R!+WWscb!5!E!%1e!E!4!I!I!%"
DATA "e8!I!I2CC8Le8!I*f!I!7!2!2!&R!+Ca!.!2!2Wrc:;81!%d!EL!G*!I1e8?28C7-f8*88!I8!IR!2R!7RCWW!.W!4!7Wrc!@!%"
DATA "!=L!E!%!=!@!3C?!1!I8!I8!%d8!2-7-H28Le8!IR!&R!+Ha!.W!.W!.!2R!2Wqc!3e!=!5T!G!3'*!1.!I8!I1d8C7-7H8!)f8R"
DATA "!&RCWW!.W!.W!.WR!2RWoc!@!@!=!=!%!%!=!@GGC!7!4!1..!I!1d!I8!2!.8-H.!I2d8R!+C!2!.W!.W!.W!.W!.!2!&!+T'!+"
DATA "lc!6]]!=!=!%!%LX!3GF!7!1?..!I!1.d!IC!.d8?!1!128RCCW!.W!.W!.W!.W!.WRT!@TGTDic!6!6!>b]!=!=d!%!5!3C!2!7"
DATA "!4??.!1!1..!I!1!2W!I!I*e!1R!+!&W!.W!.W!.W!.W!.W!.!3!3dT!F!;__fc[Y!?!>\!5!%!%1!%!%X!FT!G1b!1!%?!4?.."
DATA "!I!1C!.!I!IC!18!1!1!2RW!.W!.W!.W!.W!.W!.!.TT!F!FF!;!;\!6_ec!6!>!?QQ!>[!=b!=!%X1!)d1]]!1!4*?.!1!1!2!."
DATA "?!1!4d!I!2!2!+!.W!.W!.W!.W!.WWdT!FF[!6!6!;[!;Dfc!6!6QQM\\_X!=!@b]6!E6161b!1!%?!4?C!.!%!2.d!IR!2CW!.W"
DATA "!.W!.W!.!.GT!F!F!2!;!6!;O[!;DDic!;!;QQ!?!?\!;!3bd]L!E1611]*!4?!2P!7H.!I!IR!2R!+!.W!.W!.WWdT!FF[!6!6\"
DATA "[!;_Dmc[!;g!>!5bd]6!E6161b!1CPR?..!I!2R!7RW!.W!.!.TT!F!FF[O\!6!;[DDqc!6!;!?QYQY!>!5bb]]L!Ed1]!2P!&*?"
DATA ".!4R!2!&!+PWWdT!FF[!;!6!6!;!;__uc!;!;!>\Q!?!?!>!5Xd]6!E]]!+^T!1!%?R!&RT!3TGT!F!F!2[\!;O!;[DDyc!;!6QY"
DATA "M!?QQ!5bd]L!)1]11]*!+T]!3dT!FF[!;!6\[!;DD}c!;!6YdQ!?Q!5bd]6!E6161]!3TT!F!FF[d!6[!;DD~cdc!;[!?!?QY!>"
DATA "!>!5bb]]L!E]!3dT!FF!;[!?M[[D_~chc!;OQ!?QYQ!>!5Xd]GT!F!F!2!;OQM!;[D_~clc!;!6!>!>Y!?!?!>!5bbT!FF[!;YM["
DATA "[__~cpc!6[QQ!?!?Q\!5F!;YQ!>[[_D~ctc[!;!?Q!?MMQYY!;!;_D~cxc!;[Q!?MQ[_DD~c|c!6!;Q!;DD~c~ccc!6_~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cfcAd!8"
DATA "~c}cO!8$0!*!'!*UU~cycUOe0d!*UU!8!8~cvcO!De0!*!D$!*0!*!'!*OU~csc\Ie0!D!Df0!*!*U$!8!8~cpcU!#!#dI!<!De0"
DATA "!*!D$!*0!*!'!*UU~cmc$d!#II!<Ie0!D!De0d!*UU!8!8~cjc`!#Bd!#!*!#!#I0I!<!De0!*!D$!*0!*!'!*OU~cgc$Se!#!J"
DATA "e!#I!*Ie0!D!Df0!*!*U$!8!8~cec$Bf!#Bd!#!*!#!#dI!<!De0!*!D$!*0!*!'!*UU~cc!Be!#!JSe!#!Jd!#II!<Ie0!D!De0"
DATA "d!*UU!8!8}cOe!#!JBf!#Bd!#!*!#!#I0I!<!De0!*!D$!*0!*!'!*\UzcO1$0I!'f!#Se!#!Je!#I!*Ie0!D!Df0!*!*!8AD!8"
DATA "wc!G1!%1!%U!D!#I!#!#!JBf!#Bd!#!*!#!#dI!<!De0!*!8A!8U!8_!8DDscb!5!E!%1e!EU0I!'e!#!JS!#CC!#!Jd!#II!<I"
DATA "e0U!8Ad!8Aa!.!8ADrc:;81!%d!EL!GKI!*e!#!JB!#C/N!#Bd!#!*!#!#I0IA!8AU_AWW!.W\UDrc!@!%!=L!E!%!=!@!3C!*!D"
DATA "dI!#!9d!#!2,//Z!#!#!Je!#IAd!8Ua!.W!.W!.!8A!8Dqc!3e!=!5T!G!3'0!Dd0I!*d!#C/,/Zd!#Bd!#A!8_AWW!.W!.W!.WA"
DATA "!8ADoc!@!@!=!=!%!%!=!@GGCU!D!Dd0!<dI!#!2!.!#/Z!J!Je!#!8!8A!2!.W!.W!.W!.W!.d!8T'!+lc!6]]!=!=!%!%LX!3G"
DATA "FUU0!*00!DI00IC!.I!#!#K%!D!#!#_ACW!.W!.W!.W!.W!.W_T!@TGTDic!6!6!>b]!=!=d!%!5!3C!2UUd!*!D!Dd0!<!2WI!#"
DATA "Ee!DA!8!&W!.W!.W!.W!.W!.W!.!3!3dT!F!;__fc[Y!?!>\!5!%!%1!%!%X!FT!G1b!D!<!*$0!*00!DC!.00U!DI!D%!8_W!.W"
DATA "!.W!.W!.W!.W!.!.TT!F!FF!;!;\!6_ec!6!>!?QQ!>[!=b!=!%X1!)d1]]!DU!*!*0!D!D!2!.!*!D!DdI!8A!8!.W!.W!.W!.W"
DATA "!.WWdT!FF[!6!6!;[!;Dfc!6!6QQM\\_X!=!@b]6!E6161b!D!<!*$0C!.!<A!*00IA!8UW!.W!.W!.W!.!.GT!F!F!2!;!6!;O["
DATA "!;DDic!;!;QQ!?!?\!;!3bd]L!E1611]!*U!*!2PUAd0A!8A!8!.W!.W!.WWdT!FF[!6!6\[!;_Dmc[!;g!>!5bd]6!E6161b!DC"
DATA "PA!*!*00!8AU_W!.W!.!.TT!F!FF[O\!6!;[DDqc!6!;!?QYQY!>!5bb]]L!Ed1]!2P!8!*!*0$Ad!8PWWdT!FF[!;!6!6!;!;__"
DATA "uc!;!;!>\Q!?!?!>!5Xd]6!E]]!+^T!D!<!*A!8_T!3TGT!F!F!2[\!;O!;[DDyc!;!6QYM!?QQ!5bd]L!)1]11]!*!8T]!3dT!F"
DATA "F[!;!6\[!;DD}c!;!6YdQ!?Q!5bd]6!E6161]!3TT!F!FF[d!6[!;DD~cdc!;[!?!?QY!>!>!5bb]]L!E]!3dT!FF!;[!?M[[D_"
DATA "~chc!;OQ!?QYQ!>!5Xd]GT!F!F!2!;OQM!;[D_~clc!;!6!>!>Y!?!?!>!5bbT!FF[!;YM[[__~cpc!6[QQ!?!?Q\!5F!;YQ!>[["
DATA "_D~ctc[!;!?Q!?MMQYY!;!;_D~cxc!;[Q!?MQ[_DD~c|c!6!;Q!;DD~c~ccc!6_~c~c~cic"
DATA ""
DATA 32, 38
DATA "~c~c~c~c~c~c~c~c~c~c~c~crc!(!(<<HRzc7d-*--Rxc!(-**--d7Rvc!(-*??*--7!(HRuc!(-*??*-77!(HRuc!(-*-*--77<"
DATA "Rauc!@--f7<HH!&uc!@!%Xd7!(<HC!G!&uc!3L!=X!5:!GdG!G!+ucG!=1XX:!@!3T!GTaucCb1!=!=b!5!G!G!3CWuc!G:!3!=X"
DATA "X!5!G!3G!&!+sc!@!GL!=!3G!GGG!2!+!&G!G'Foc!@!@]]1!%b1bX:!G!3G!3!3T!@'FlcYL11]b!%bb!5:!GdG!3C!3!G!@!@'"
DATA "kc!@!);61]!=XX:!@!3GGCGTd!@!3!+ic!6!;]]1!E;L]b!5!G!3G!2Cb]1b!5dT__fc!;!?!>Xd]1!);61]]11]1]!@!3TG!F!F"
DATA "[ODec!6!>!?QQXb]]1!E;L11]b!@dT!FF!;!?!;[Dfc!6!;!?Y!?QXd]1!);b!@GTT!F!F[!?[!6DDic[!;QQ!?\Xbd]!GTT!FF["
DATA "!>!;!;__mc!;[!>dQX]Xd!F!;!?[[_!.qc!6!6QQ!>!>!@FQQ!;[DDuc!;!;Q!>QY['_Dyc!;!;Y!;DD}c!;D~csc"
DATA ""
DATA 32, 38
DATA "~c~c~c~c~c~c~c~c~c~c~c~crc==UZZ!8zcUN/NN/=!8xc=eNd/=!8vcUN!*0KNN//=Z!8uc=NN!*NN//==Z!8ucU//N//,,!DZA"
DATA "!8uc!@e/=,!DdZ!&uc!@!%X//=UZAC!G!&uc!3L!=X!5:!GdG!G!+ucG!=1XX:!@!3T!GTaucCb1!=!=b!5!G!G!3CWuc!G:!3!="
DATA "XX!5!G!3G!&!+sc!@!GL!=!3G!GGG!2!+!&G!G'Foc!@!@]]1!%b1bX:!G!3G!3!3T!@'FlcYL11]b!%bb!5:!GdG!3C!3!G!@!@"
DATA "'kc!@!);61]!=XX:!@!3GGCGTd!@!3!+ic!6!;]]1!E;L]b!5!G!3G!2Cb]1b!5dT__fc!;!?!>Xd]1!);61]]11]1]!@!3TG!F"
DATA "!F[ODec!6!>!?QQXb]]1!E;L11]b!@dT!FF!;!?!;[Dfc!6!;!?Y!?QXd]1!);b!@GTT!F!F[!?[!6DDic[!;QQ!?\Xbd]!GTT!F"
DATA "F[!>!;!;__mc!;[!>dQX]Xd!F!;!?[[_!.qc!6!6QQ!>!>!@FQQ!;[DDuc!;!;Q!>QY['_Dyc!;!;Y!;DD}c!;D~csc"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c"
DATA "~c~cec!5!G!G!@aa!G!5!5C~cxcX!5GCGCGCGCGC!5!@~chce!@jc!5LmC!=!G~cec!G!@!#!#61!@!@gc!G!=GdCGdCGdCGC1!@"
DATA "~cc!@!@g!#11!@!@!6dc!G!=oC!=!@}c!@!@I!#I!#I!#I!#I!#616!@!6c!G!)!+CGCGCGCGCGCGP1!5{c!@!3o!#161!@!=!EL"
DATA "!+!3iC!2^!=1'yc!G!@!#!#Id!#Id!#Id!#Id!#]1!%!=!%!E!%!=!+aaWPP:!=!%1G!+wc!@!@v!#]L!%!=!=2g1!E!EbGG!G!+"
DATA "uc!@!@I!#I!#I!#I!#I!#I!#I!#I!#I!#I!#I]]7L!%11!=!=!5!5!@!@!5!@!3G!G!G!3GRrc!@!@]u!#]11!57!%d1X!=!=bXX"
DATA "!G!3Ge!GGRpc!5!561611!#Id!#Id!#Id!#I!#!#]61bT''!(1!=!=b!=db!5!3!3G!G!3!G!3Wqc!3]L!E1611]m!#]11]T!F!F"
DATA "FF7?!=!=XX!=bX!5!GGG!G!G!3<Rqc!Gd]6!E61611!#I!#I!#I!#I]61!)CGCGCG!E!=!(77!)bXX!5!5!GGG!=HH<!&qc!3]b"
DATA "d]L!Ee1]e!#]161LiCL1X!57!(7d!(<<HHCGG'qc!G--be]6!E61611]61]TLdCGdCGCC!)bX!=X!5::!G:!G!3GTdG!+pc!377-"
DATA "7bbd]L!E1611]T!F!F!)!&iCPL!3!3!@d!5:!@:!@!GdG!GG!;[_mc!;]]-7-7-be]6!E]T!F!FC!F!%L!+!&GCGCPP!%1:!@!G"
DATA "!3X!5!@!@!5!@!3G!G!G!3GO\\[[ic!6!;!>bd]7-7-7bbd]Te!F-!=!%!E1f!=!E!GGH!@!3d!@XX!G!3Ge!GG!;!;!6!6!;_["
DATA "fc!;d!?Q[b]]b-7-7-bb]!F!FT!F!F7!%!=!%!=X:!G!3!3!GG!GR!@!3!3:!@!@!5!3!3G!G!3d!G[!;!;d\!>Dec!6YQ!?Y!>"
DATA "!>[bb]b7-7!(b]Te!F-!%!=bX!=b!5!@dG!GH!@!3!3e!@!GGGd!GG!3!;e!6!;!;Dfc!6O!>!?Y!?!>\[X]]b-!(]]e!FC!F7bX"
DATA "XbXX!@TGGH!3!3G!3:!@!G!GGG!F!GCGF[!;!;!6[[DDic!;!;!>eY\[bd]b]TTd!FF!%-77*!5:77HH!GCG!@G!3dG!G!G!3GC"
DATA "!2[\!;[[D_mc!;!6QQYY!>Q[bd]!F!FT!F!;[1!=b7!(!(<<HdGO[!@C!FC!FCTG!FGQ!?Q[[D!.qc!;!;QdY!?!>[bb!F!F[[!;"
DATA "!6!=!%1!=!5!5!@!3d!GG!6!;!6!;!6e!;!6!;\Y!6[D_uc!;!6!?!>YQ!?!>[_!;!?YM!?!=!%!%!=X:!G!3!G!G!3G!;\\!6!;"
DATA "e!6\\!;!;D_yc!6!6!?!>QYY!?YQQYQ!=!%11!5!5!@!3d!G!F[!6!6\!6!6!;!;!6[!;DD}c!6!;dQYMQQ!?!>1d!=X:!@!3!3"
DATA "!GC!3!;!6!6\O\!6!;!;_P~cdc!;!6!?QYMQQ!>bdXbX!5!GCGCC!;!6\\d!;DD~chc!6!;Q!?M`QQ!>:!@X!5!G!3F!2[!>!?YY"
DATA "[__D~clc!;[!>YY!?YYfQ!?!>Q!?Y[!;DD~cpc[[Y!?!?YYQQYYQY!?[[DD~ctc!6!;Q!>dYdM[!;DD~cxc[[QYMY!;!;_D~c|c"
DATA "!;[Q[DD~c~ccc!6D~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c"
DATA "~c~cec!5!G!G!@aa!G!5!5C~cxcX!5GCGCGCGCGC!5!@~chce!@jc!5LmC!=!G~cec!G!@!#!#61!@!@gc!G!=GdCGdCGdCGC1!@"
DATA "~cc!@!@g!#11!@!@!6dc!G!=oC!=!@}c!@!@I!#I!#I!#I!#I!#616!@!6c!G!)!+CGCGCGCGCGCGP1!5{c!@!3o!#161!@!=!EL"
DATA "!+!3iC!2^!=1'yc!G!@!#!#Id!#Id!#Id!#Id!#]1!%!=!%!E!%!=!+aaWPP:!=!%1G!+wc!@!@v!#]L!%!=!=2g1!E!EbGG!G!+"
DATA "uc!@!@I!#I!#I!#I!#I!#I!#I!#I!#I!#I!#I]],L!%11!=!=!5!5!@!@!5!@!3G!G!G!3GDrc!@!@]u!#]11!5/!%d1X!=!=bXX"
DATA "!G!3Ge!GG!8pc!5!561611!#Id!#Id!#Id!#I!#!#]61bT''/1!=!=b!=db!5!3!3G!G!3!G!3!8qc!3]L!E1611]m!#]11]T!F"
DATA "!FFF/N!=!=XX!=bX!5!GGG!G!G!3Z!8qc!Gd]6!E61611!#I!#I!#I!#I]61!)CGCGCG!E!==/,!)bXX!5!5!GGG!=AZA!&qc!3]"
DATA "bd]L!Ee1]e!#]161LiCL1X!5=/d=UeZCGG'qc!G/=be]6!E61611]61]TLdCGdCGCC!)bX!=X!5::!G:!G!3GTdG!+pc!3e/bbd]"
DATA "L!E1611]T!F!F!)!&iCPL!3!3!@d!5:!@:!@!GdG!GG!;[_mc!;]],/,/=be]6!E]T!F!FC!F!%L!+!&GCGCPP!%1:!@!G!3X!5"
DATA "!@!@!5!@!3G!G!G!3GO\\[[ic!6!;!>bd]d/,/bbd]Te!F=!=!%!E1f!=!E!GGZ!@!3d!@XX!G!3Ge!GG!;!;!6!6!;_[fc!;d!?"
DATA "Q[b]]b,/,/=bb]!F!FT!F!F=!%!=!%!=X:!G!3!3!GG!G!8!@!3!3:!@!@!5!3!3G!G!3d!G[!;!;d\!>Dec!6YQ!?Y!>!>[bb]b"
DATA "d/=b]Te!F/!%!=bX!=b!5!@dG!GZ!@!3!3e!@!GGGd!GG!3!;e!6!;!;Dfc!6O!>!?Y!?!>\[X]]b,=]]e!FC!F9bXXbXX!@TGGZ"
DATA "!3!3G!3:!@!G!GGG!F!GCGF[!;!;!6[[DDic!;!;!>eY\[bd]b]TTd!FF!%d/N!5:=,ZZ!GCG!@G!3dG!G!G!3GC!2[\!;[[D_mc"
DATA "!;!6QQYY!>Q[bd]!F!FT!F!;[1!=b,!D=UZAdGO[!@C!FC!FCTG!FGQ!?Q[[D!.qc!;!;QdY!?!>[bb!F!F[[!;!6!=!%1!=!5!5"
DATA "!@!3d!GG!6!;!6!;!6e!;!6!;\Y!6[D_uc!;!6!?!>YQ!?!>[_!;!?YM!?!=!%!%!=X:!G!3!G!G!3G!;\\!6!;e!6\\!;!;D_yc"
DATA "!6!6!?!>QYY!?YQQYQ!=!%11!5!5!@!3d!G!F[!6!6\!6!6!;!;!6[!;DD}c!6!;dQYMQQ!?!>1d!=X:!@!3!3!GC!3!;!6!6\O\"
DATA "!6!;!;_P~cdc!;!6!?QYMQQ!>bdXbX!5!GCGCC!;!6\\d!;DD~chc!6!;Q!?M`QQ!>:!@X!5!G!3F!2[!>!?YY[__D~clc!;[!>Y"
DATA "Y!?YYfQ!?!>Q!?Y[!;DD~cpc[[Y!?!?YYQQYYQY!?[[DD~ctc!6!;Q!>dYdM[!;DD~cxc[[QYMY!;!;_D~c|c!;[Q[DD~c~ccc!6"
DATA "D~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cec!@!5~c~ccc!G!@!%!EX!@~c|c!5!G1"
DATA "f!E!5!G~cvc<!(:!5!E1!%g!E1!G!5mcC!2~cdc!(!(e-d1!E1!E!E!%1!%1!%!@!5jc!2::!2~c<!(*d-*d-!=1!%1;1!%1!%1"
DATA "!%!EX!@hc!2!5:W}cRm-!)e!E!%1f!E!5!5fcC!GCW|c!5!)L!(m-d!E1!%g!E1X!5dc!2!3!2Wzc!@!5!Ed1!=!(m-d1!%1!E!E"
DATA "!%1!%1!%!@!5c!2!GCWxc!G!@!%!E;1!%1!%1!=!(*d-*d-*d-!=!E!%!E;1!%1!%1!%!EX!G!3!2Pvc!@!G1!%f!E!%1!E!E!)"
DATA "!%!(m-!)!E1!E!E!%1d!E!)!3G!&Puc!3!%2d!E1d!E1!%e!E!)L!(m-d!E1!%11!)!%!3G!&!&!2tc!3!=!%;2d!E!%1!%1!%1"
DATA "!E!Ed1!=!(m-e1!)L!=GC!&!+G!&Crc!3!=!%L!%8!E!%1!%1;1!%!E;1!%1!%1!=!(*d-*d-*d-!=11!)LXC!2b!=!%!@'qc!3"
DATA "!=e!%!=L;!E!%1!E1e!E!%1!E!E!)!%7m-!)!)1!)1!=!%!@GG!&qc!@d!%11d!=!%2!%d!E!%!E!E1!%e!E!)L!(m-!E!E!=!3G"
DATA "GT!G'qc!@!%e!=Xd!=!%;21!E!E!%1!E1!%1!E!Ed1!=!(k-HGd!GdG!&qc!@!%!=b!*__!5bb!%L!%8!E;1!%1!%d!E;1!%1!%1"
DATA "!=!(*d-*-H!G!3d!GTGT!Gaqc!@!%!=!=!*!*!8!*!*X1d!%!=L;!E!%1g!E!%1!E!E!)!%7--H!GGG!GG!G!GGd!G!+qc!3e!="
DATA "!2!8_!*!D_!=11d!=!%2!%g!E1!%e!E!)bdGTd!G!3G!G!G!3!Gaqc!3!=!%!%!=d!2!8!*d!8!=Xd!=!%;21!%!E!%1!%1!%!E"
DATA "!E!=!3!G!GGG!GG!GdGe!G!+qc!3!=!%L!%f!2!*_!8!D!D!5bb!%L!%8;;1!E!E!%1L!@!3d!GT!GT!GTGTG!3!G!GG'qcG!="
DATA "d!%h!2_!*!*!8!8X1d!%!=L;1!E!%!@Ge!GGGd!GeG!GdG!+qc!G!%!%1!%j!2!*!D!8!D!D!=11d!=!%!3GGTG!G!GTG!G!G!3"
DATA "!GGGT!GGGT!Gaoc!6!;[!=bb1l!2!8!8!*!*!8f!=!GeG!G!GGGe!GGd!GGG!G!G!&mc!;\Q!?!>\[!5X!&m!2!8!DAbd!=b!G!3"
DATA "GT!G!3GTG!3!G!GG!3d!GT!G!3!3G_[ic!6!6MdYQYQQ[!&n!2PbX!%!%!=!=!GGd!GeG!GdGf!GC!3G[!;!;_[fcOYQQ!?!?YQY"
DATA "Q!GQa!+!2!2!&j!2PXbd!%1G!G!G!3dGT!GGGTG!3!G!F!3GG!;[[d!6\_ec!;QQYYMMYeX!5!>!+!+!&!&i!2PX!=e!%Ge!GG"
DATA "d!GeG!Gd!3[[!;!6!6\!6!;!;Dfc!;!;Q!?QXX::!@:!5XX\a!+!2!2!&f!2P1!%!%L!%!%G!3!GTG!3d!GTG!F!3GG!;[!6\!6"
DATA "!;!6[[_!.ic!;!;Q:!Gh:!G!@!>!+!+!&f!2P!=!%!=!=!%!%G!GdGe!GG!G!3Gd[e!6[[DDmc!;!;!>!G!G::eX!@:!G\a!+!2"
DATA "!2!&!2P!=1d!=bdGTG!3!G!3G!3G_!;!6!;O!;!6!;!6__qc!;!;Q!G:iX!5!>!+!+!&!&P!=!%e!=fG!GGCC[d!;!6\!;!;[DD"
DATA "uc[!;Y!5:XX::!@:!5XX\a!+P!5X!=b!=bGTG!F!3GC[!;[!6!6!;\[d_yc!;!6Y:!Gh:!G!@!>^_X11b!=GGC!3G[[!6!6!;!6\"
DATA "!;!;DD}c[[!?!G!G::eX!@:Q!?\[!=1!%!3!3C[[!;!6!;!6!6!;!;DP~cdc!;!6!?!G:eX!+QY!?YQ!>!;!=C[[!6!;YMY!;[__"
DATA "~chc!;OQ!5:aQ!?Y!?YMY!>Q!>\\Y!?YY!;'D_~clc!6!;!>!>YY!?!?MYdQM!?!?Q!?!;!;_D~cpc!6[!>!>YMYY!?MQ!?!?Y!;"
DATA "[DD~ctc!6!;!>!?!?MMYYQ!;[_D~cxc!;!6Y!?YQ[[_D~c|c!6!6Q!;DD~c~ccc!;_~c~c~cic"
DATA ""
DATA 64, 60
DATA "~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~c~cec!@!5~c~ccc!G!@!%!EX!@~c|c!5!G1"
DATA "f!E!5!G~cvcU=:!5!E1!%g!E1!G!5mcC!2~cdc==/N//d1!E1!E!E!%1!%1!%!@!5jc!2::!2~cU=N/N/N/N/!=1!%1;1!%1!%1"
DATA "!%!EX!@hc!2!5:W}cZd/Nd/Nd/N!)e!E!%1f!E!5!5fcC!GCW|c!5!)L=N/N/N/N/N/N/d!E1!%g!E1X!5dc!2!3!2Wzc!@!5!E"
DATA "d1!==/Nd/Nd/N//d1!%1!E!E!%1!%1!%!@!5c!2!GCWxc!G!@!%!E;1!%1!%1!==N/N/N/N/N/N/!=!E!%!E;1!%1!%1!%!EX!G"
DATA "!3!2Pvc!@!G1!%f!E!%1!E!E!)!%!Dd/Nd/Nd/N!)!E1!E!E!%1d!E!)!3G!&Puc!3!%2d!E1d!E1!%e!E!)L=N/N/N/N/N/N/"
DATA "d!E1!%11!)!%!3G!&!&!2tc!3!=!%;2d!E!%1!%1!%1!E!Ed1!==/Nd/Nd/N//e1!)L!=GC!&!+G!&Crc!3!=!%L!%8!E!%1!%1;"
DATA "1!%!E;1!%1!%1!==N/N/N/N/N/N/!=11!)LXC!2b!=!%!@'qc!3!=e!%!=L;!E!%1!E1e!E!%1!E!E!)!%!Dd/Nd/Nd/N!)!)1!)"
DATA "1!=!%!@GG!&qc!@d!%11d!=!%2!%d!E!%!E!E1!%e!E!)L=N/N/N/N/N/N/!E!E!=!3GGT!G'qc!@!%e!=Xd!=!%;21!E!E!%1!E"
DATA "1!%1!E!Ed1!==/Nd/Nd/NZGd!GdG!&qc!@!%!=b!*__!5bb!%L!%8!E;1!%1!%d!E;1!%1!%1!==N/N/N/A!G!3d!GTGT!Gaqc!@"
DATA "!%!=!=!*!*!8!*!*X1d!%!=L;!E!%1g!E!%1!E!E!)!%!D//Z!GGG!GG!G!GGd!G!+qc!3e!=!2!8_!*!D_!=11d!=!%2!%g!E1"
DATA "!%e!E!)bdGTd!G!3G!G!G!3!Gaqc!3!=!%!%!=d!2!8!*d!8!=Xd!=!%;21!%!E!%1!%1!%!E!E!=!3!G!GGG!GG!GdGe!G!+qc"
DATA "!3!=!%L!%f!2!*_!8!D!D!5bb!%L!%8;;1!E!E!%1L!@!3d!GT!GT!GTGTG!3!G!GG'qcG!=d!%h!2_!*!*!8!8X1d!%!=L;1!E"
DATA "!%!@Ge!GGGd!GeG!GdG!+qc!G!%!%1!%j!2!*!D!8!D!D!=11d!=!%!3GGTG!G!GTG!G!G!3!GGGT!GGGT!Gaoc!6!;[!=bb1l!2"
DATA "!8!8!*!*!8f!=!GeG!G!GGGe!GGd!GGG!G!G!&mc!;\Q!?!>\[!5X!&m!2!8!DAbd!=b!G!3GT!G!3GTG!3!G!GG!3d!GT!G!3!3"
DATA "G_[ic!6!6MdYQYQQ[!&n!2PbX!%!%!=!=!GGd!GeG!GdGf!GC!3G[!;!;_[fcOYQQ!?!?YQYQ!GQa!+!2!2!&j!2PXbd!%1G!G!G"
DATA "!3dGT!GGGTG!3!G!F!3GG!;[[d!6\_ec!;QQYYMMYeX!5!>!+!+!&!&i!2PX!=e!%Ge!GGd!GeG!Gd!3[[!;!6!6\!6!;!;Dfc!;"
DATA "!;Q!?QXX::!@:!5XX\a!+!2!2!&f!2P1!%!%L!%!%G!3!GTG!3d!GTG!F!3GG!;[!6\!6!;!6[[_!.ic!;!;Q:!Gh:!G!@!>!+!+"
DATA "!&f!2P!=!%!=!=!%!%G!GdGe!GG!G!3Gd[e!6[[DDmc!;!;!>!G!G::eX!@:!G\a!+!2!2!&!2P!=1d!=bdGTG!3!G!3G!3G_!;"
DATA "!6!;O!;!6!;!6__qc!;!;Q!G:iX!5!>!+!+!&!&P!=!%e!=fG!GGCC[d!;!6\!;!;[DDuc[!;Y!5:XX::!@:!5XX\a!+P!5X!=b"
DATA "!=bGTG!F!3GC[!;[!6!6!;\[d_yc!;!6Y:!Gh:!G!@!>^_X11b!=GGC!3G[[!6!6!;!6\!;!;DD}c[[!?!G!G::eX!@:Q!?\[!=1"
DATA "!%!3!3C[[!;!6!;!6!6!;!;DP~cdc!;!6!?!G:eX!+QY!?YQ!>!;!=C[[!6!;YMY!;[__~chc!;OQ!5:aQ!?Y!?YMY!>Q!>\\Y!?"
DATA "YY!;'D_~clc!6!;!>!>YY!?!?MYdQM!?!?Q!?!;!;_D~cpc!6[!>!>YMYY!?MQ!?!?Y!;[DD~ctc!6!;!>!?!?MMYYQ!;[_D~cxc"
DATA "!;!6Y!?YQ[[_D~c|c!6!6Q!;DD~c~ccc!;_~c~c~cic"
DATA ""
DATA 16, 16
DATA "~c~crceWdcjWd.!1WcW9d,9,,--d.!1WW!5d,@,,@-7!I.!1!7!@W!37-7-7-7--!1!4!7R!GWGj!(!4!4!2!2GcWj<!4!7!2R!G"
DATA "W:!+!+!G!+!+!3!+!G!3!+!G!G!.!=WX!&!2!@!2!&:!&:!@!2!@:W!3W!3WWC!.WCW!2CWC!2PWcW^P!+P^!&^!&!+P!+!&W cc"
DATA "mWcc"
DATA ""
DATA 16, 16
DATA "~c~crceWdcjWd.!1WcW9d,9,,--d.!1WW!2d,@,,@-7!I.!1!7!@Wa7-7-7-7--!1!4!7R!GWaj!(!4!4!2!2GcWj<!4!7!2R!GW"
DATA "!&!G!3!+!G!G!+!G!+!+!G!+!+!2!=WG!@:!&:!@!2!@!2!&:!&!2G!3W!+CCW!2CWC!.WCW!.!&WcW!+!&^!&!+P!+P^!&^PW c"
DATA "cmWcc"
DATA ""
DATA 16, 16
DATA "~cyceWjcdW-,@-WhcW-d,@,,-WgcW--,@e,-WecW!37--Z,9,Z..Wdc!2!2G7--7e.!1!4Wcc!&X!2!+7-*..*!7H*!7Wca!&X!5"
DATA "!+77?!1!7H!7H!7FWW!+!@C!5!3!(!%!1!7!7R!&!7!.PcW!3!@!2!5!3W!1!+RCb!=L^ccW!3!&!2!&!5bb!=dG!G^dcWa!+P^"
DATA "!GGTdGWWecWa!.^!GGeWcc"
DATA ""
DATA 16, 16
DATA "~cyceWjcdW-,@-WhcW-d,@,,-WgcW--,@e,-WecW!+7--Z,9,Z..Wdc!5!5!+7--7e.!1!4Wcc!@!2!5G7-*..*!7H*!7Wc!3!@"
DATA "!2!2G77?!1!7H!7H!7PWW!3!&X!2!+!(!%!1!7!7R!&!7!2!2cWa!&!5!2aW!1!+RCb!=L!+ccW!+!@X!@!5bb!=dG!G!+dcW!3"
DATA "!GFa!GGTdGWWecWG!2a!GGeWcc"
DATA ""
DATA 16, 16
DATA "~cycgWjcW,,@=,,WgcdW,,9d,dWdcWX!Ga@f,!&!&!2WccWX!@a@,9d,!&!2CWccWC!&^f.?^!.WW cWX!&a8e!I!1!+PCW cW!5"
DATA "!Ga!17!47!4H!+!&!&W cW!2!&^!1e!4CP!.PW cWC!&P!4!7!4!7!2R^'WW cW!&ah!=1W^W cW!+P:!@:!@:!@:!@^PW ccWW"
DATA "d!3!Gd!3!GWW c"
DATA ""
DATA 16, 16
DATA "~cycgWjcW,,@=,,WgcdW,,9d,dWdcWF!&^@f,P!.WWccW!2!+^@,9d,PPaWccW!5!GWf.?!+!&CW cW!2!@^8e!I!1P!2aW cWC"
DATA "!&^!17!47!4H^!.PW cWX!@!+!1e!4C!+!2!&W cW!5!@a!4!7!4!7!2R!+:CW cW!@Ch!=1C!+W cW!3!2:!@:!@:!@:!@!+!&W"
DATA " ccWWd!3!Gd!3!GWW c"
DATA ""
DATA 16, 16
DATA "~c{ceWlcW-=@7dWhcW-d,@,!8HWfcW-,@9,,!8!(HWecW.!4Z9,,Z<HR!&WccW!1!4d.?-!(HR!&!+!.cW?*?88?!7<HRP!&!2^W"
DATA "!@.!1*!1*!4!7HRP!G!2^P!3!2!1!7d!1!4!7R!&!2!.!&PW!+!)bX!5!4!1!4W!&!G!.!+!&W !.!=XbX1!=!=!2!F!.^!&W cW"
DATA "WbXbXb1Pa^PW eceWX!=^aPW dc"
DATA ""
DATA 16, 16
DATA "~c{ceWlcW-=@7dWhcW-d,@,!8HWfcW-,@9,,!8!(HWecW.!4Z9,,Z<HRPWccW!1!4d.?-!(HRP!G!2cW?*?88?!7<HR!&!G!.!&W"
DATA "!&.!1*!1*!4!7HR!&!&!.!+!&!=:!1!7d!1!4!7RP!.!2P!&WG!)bX!5!4!1!4WP!+!2^PW !2!=XbX1!=!=!2X!2!&PW cWWbXb"
DATA "Xb1!+C!+!&W eceWX!=!+C!&W dc"
DATA ""
DATA 16, 16
DATA "~c~chceWlcW!I!1.?jWccW!1d.<7e,@9ZWc!@!Id.<-9,,@,@Z!&W!=*!4!1!177-7-7-7-aWb!%!4!1!4j!(!+W!=!1!7!4!7j<"
DATA "W !=!&!G!G!+!3!G!+!G!+!+!3!+!+CW:G!@:!&:!@!2!@!2!&:!&!2GWW!+CCW!2CWC!.WCW!.!&WcW!+!&^!&!+P!+P^!&^PW "
DATA "ccmWcc"
DATA ""
DATA 16, 16
DATA "~c~chceWlcW!I!1.?jWccW!1d.<7e,@9ZWc!@!Id.<-9,,@,@ZPW!=*!4!1!177-7-7-7-^Wb!%!4!1!4j!(^W!=!1!7!4!7j<W "
DATA "!=!@!+!+!G!+!+!3!+!G!G!+!G!GWW:X!&!2!@!2!&:!&:!@!2!@:dW!3WWC!.WCW!2CWC!2PWcW^P!+P^!&^!&!+P!+!&W ccmW"
DATA "cc"
DATA ""
DATA 16, 16
DATA "~cicdWkcdW!1!1.WicW!I!1!4..?!%WhcW?!1*!4CH77WfcW!)!%!4-77@,,-WecW!+C!4-Z@d,@-W ccb!5aC--,@=@,,-W c!@"
DATA "!2:!37--,,9Z--HWc!3!@!2!2!37--77!(dHPWW!3!&X!2!+7-gH!@GcWa!&!5!2!+7<HHWW!&C!&ccW!+!@X!@CdW  W!2!&dcW"
DATA "!3!GF!&Wf WWecWG!2!+W gc"
DATA ""
DATA 16, 16
DATA "~cicdWkcdW!1!1.WicW!I!1!4..?!%WhcW?!1*!4CH77WfcW!)!%!4-77@,,-WecW!3C!4-Z@d,@-W cc!2!2!3C--,@=@,,-W c"
DATA "!&X!2!+7--,,9Z--HWca!&X!5!+7--77!(dHFWW!+!@C!5G7-gH!&acW!3!@!2!5G7<HHWWPWPccW!3!&!2!&adW  W!.^dcWa!+"
DATA "!.PWf WWecWa!.^W gc"
DATA ""
DATA 16, 16
DATA "~cicgWjcW!1!1d.?WicW!Ie.?WgcWW!=g!71WWdcWF!&^,9,,@=^PPWccW!2!&^,,9@,@P!.PW cW!5!GW,9,=,,!+!&CW cW!2"
DATA "!@^,,9@,@P!2aW cWC!&^-7-7-7^!.WW cWX!@ag!(!&!2CW cW!5!@Wg!(!+:CW cW!@!3!+gW!@G!+W cW!3C!+We W!G!2!&W"
DATA " ccdWgcdWcc"
DATA ""
DATA 16, 16
DATA "~cicgWjcW!1!1d.?WicW!Ie.?WgcWW!=g!71WWdcWX!@!+,9,,@=!+F!&WccWX!G!+,,9@,@!+!2!&W cWC!&^,9,=,,^!.WW cW"
DATA "X!&a,,9@,@!+PCW cW!5!Ga-7-7-7!+!&CW cW!2!+^g!(PPaW cWC!&^g!(^'WW cW!&!+PgW!&a^W cW!+W^We W!+!.PW cc"
DATA "dWgcdWcc"
DATA ""
DATA 16, 16
DATA "~clcdWmcWd.dWicW2!I..!1!4!2WgcW-?!4!4!18!7RWfcW-,,9d-!7!&:WdcW-@d,@!8!(!7GPWccW-9,,@=Z!(HGP!G!2cWd-"
DATA "d,!8<HR!&!G!.!&W!&d7-*?!(HR!&!&!.!+!&!=:g7HRP!&!2P!&W!GC!+WW7-7RP!+!2^PW C!2W  dWXX!2!&PW cWWfcW!GC"
DATA "!+!&W jcWCC!&W dc"
DATA ""
DATA 16, 16
DATA "~clcdWmcWd.dWicW2!I..!1!4!2WgcW-?!4!4!18!7RWfcW-,,9d-!7!&:WdcW-@d,@!8!(!7G!&WccW-9,,@=Z!(HG!&!+!.cW"
DATA "d-d,!8<HRP!&!2^W!@d7-*?!(HRP!G!2^P!3!2g7HR!&!G!.!&PWaW^WW7-7R!&!G!.!+!&W W!.W  dWC!F!.^!&W cWWfcW!+a"
DATA "^PW jcWWaPW dc"
DATA ""
DATA 16, 16
DATA "~c~crceWdcjW00!*!DWcW9d,9,,/,0!*0!DWW!5d,@,,@//00!DU!@W!3//Nd/N/,!D$OA!GWGd=Ud=U=!D!DZ!8GcW=Z=Z=Z=Z="
DATA "U$A_!GW:!+!+!G!+!+!3!+!G!3!+!G!G!.!=WX!&!2!@!2!&:!&:!@!2!@:W!3W!3WWC!.WCW!2CWC!2PWcW^P!+P^!&^!&!+P!+"
DATA "!&W ccmWcc"
DATA ""
DATA 16, 16
DATA "~c~crceWdcjW00!*!DWcW9d,9,,/,0!*0!DWW!2d,@,,@//00!DU!@Wa//Nd/N/,!D$OA!GWad=Ud=U=!D!DZ!8GcW=Z=Z=Z=Z=U"
DATA "$A_!GW!&!G!3!+!G!G!+!G!+!+!G!+!+!2!=WG!@:!&:!@!2!@!2!&:!&!2G!3W!+CCW!2CWC!.WCW!.!&WcW!+!&^!&!+P!+P^"
DATA "!&^PW ccmWcc"
DATA ""
DATA 16, 16
DATA "~cyceWjcdW/,@,WhcWNd,@d,WgcW,N,@f,WecW!3=/NZ,9,Z00Wdc!2!2G=,N90!*0!*!D&Wcc!&X!2!+=,N00!*UA*UWca!&X!5"
DATA "!+=/!*!D!7H!7HUFWW!+!@C!5!3=!*!D!7!7!8!8O!.PcW!3!@!2!5!3WU!8_Cb!=L^ccW!3!&!2!&!5bb!=dG!G^dcWa!+P^!GG"
DATA "TdGWWecWa!.^!GGeWcc"
DATA ""
DATA 16, 16
DATA "~cyceWjcdW/,@,WhcWNd,@d,WgcW,N,@f,WecW!+=/NZ,9,Z00Wdc!5!5!+=,N90!*0!*!D&Wcc!@!2!5G=,N00!*UA*UWc!3!@"
DATA "!2!2G=/!*!D!7H!7HUPWW!3!&X!2!+=!*!D!7!7!8!8O!2!2cWa!&!5!2aWU!8_Cb!=L!+ccW!+!@X!@!5bb!=dG!G!+dcW!3!GF"
DATA "a!GGTdGWWecWG!2a!GGeWcc"
DATA ""
DATA 16, 16
DATA "~cycgWjcW,,@=,,WgcdW,,9d,dWdcWX!Ga@f,!&!&!2WccWX!@a@,9d,!&!2CWccWC!&^0!*0!*00^!.WW cWX!&a!#0I0I!D!+P"
DATA "CW cW!5!Ga!D7!47!4U!+!&!&W cW!2!&^!De!4AP!.PW cWC!&P!D$U$A!8^'WW cW!&ah!=1W^W cW!+P:!@:!@:!@:!@^PW c"
DATA "cWWd!3!Gd!3!GWW c"
DATA ""
DATA 16, 16
DATA "~cycgWjcW,,@=,,WgcdW,,9d,dWdcWF!&^@f,P!.WWccW!2!+^@,9d,PPaWccW!5!GW0!*0!*00!+!&CW cW!2!@^!#0I0I!DP!2"
DATA "aW cWC!&^!D7!47!4U^!.PW cWX!@!+!De!4A!+!2!&W cW!5!@a!D$U$A!8!+:CW cW!@Ch!=1C!+W cW!3!2:!@:!@:!@:!@!+"
DATA "!&W ccWWd!3!Gd!3!GWW c"
DATA ""
DATA 16, 16
DATA "~c{ceWlcWN=@!DdWhcWNd,@,!8ZWfcWN,@9,,!8=ZWecW0!DZ9,,ZUZ!8!&WccW!D&0!*00/UZZ!&!+!.cW!**!*!#!#!*UZZ!8P"
DATA "!&!2^W!@!'!1*!1*!DUZZP!G!2^P!3!2!DU!D!1!1UU!8!&!2!.!&PW!+!)bX!5!D!DUW!&!G!.!+!&W !.!=XbX1!=!=!2!F!.^"
DATA "!&W cWWbXbXb1Pa^PW eceWX!=^aPW dc"
DATA ""
DATA 16, 16
DATA "~c{ceWlcWN=@!DdWhcWNd,@,!8ZWfcWN,@9,,!8=ZWecW0!DZ9,,ZUZ!8PWccW!D&0!*00/UZZP!G!2cW!**!*!#!#!*UZZ!8!&"
DATA "!G!.!&W!&!'!1*!1*!DUZZ!&!&!.!+!&!=:!DU!D!1!1UU!8P!.!2P!&WG!)bX!5!D!DUWP!+!2^PW !2!=XbX1!=!=!2X!2!&PW"
DATA " cWWbXbXb1!+C!+!&W eceWX!=!+C!&W dc"
DATA ""
DATA 16, 16
DATA "~c~chceWlcW0!D00jWccW!D!*0!*Z9e,@9ZWc!@Id0Z/9,,@,@Z!&W!=!*$!DU/9/Nd/N/aWb!<!D!D$Ud=Ud=U!+W!=!D$UUZ=Z"
DATA "=Z=Z=ZW !=!&!G!G!+!3!G!+!G!+!+!3!+!+CW:G!@:!&:!@!2!@!2!&:!&!2GWW!+CCW!2CWC!.WCW!.!&WcW!+!&^!&!+P!+P^"
DATA "!&^PW ccmWcc"
DATA ""
DATA 16, 16
DATA "~c~chceWlcW0!D00jWccW!D!*0!*Z9e,@9ZWc!@Id0Z/9,,@,@ZPW!=!*$!DU/9/Nd/N/^Wb!<!D!D$Ud=Ud=U^W!=!D$UUZ=Z=Z"
DATA "=Z=ZW !=!@!+!+!G!+!+!3!+!G!G!+!G!GWW:X!&!2!@!2!&:!&:!@!2!@:dW!3WWC!.WCW!2CWC!2PWcW^P!+P^!&^!&!+P!+!&"
DATA "W ccmWcc"
DATA ""
DATA 16, 16
DATA "~cicdWkcdW!D!D0WicW0!D&0!*0!<WhcW!*!D!*!DOA,/WfcW!)!<$=/9@d,WecW!+C!DNZ@d,@,W ccb!5aC,N,@=@d,W c!@!2"
DATA ":!3=,N,,9Z//ZWc!3!@!2!2!3=,N/,=dZPWW!3!&X!2!+=/gZ!@GcWa!&!5!2!+=AZAWW!&C!&ccW!+!@X!@CdW  W!2!&dcW!3"
DATA "!GF!&Wf WWecWG!2!+W gc"
DATA ""
DATA 16, 16
DATA "~cicdWkcdW!D!D0WicW0!D&0!*0!<WhcW!*!D!*!DOA,/WfcW!)!<$=/9@d,WecW!3C!DNZ@d,@,W cc!2!2!3C,N,@=@d,W c!&"
DATA "X!2!+=,N,,9Z//ZWca!&X!5!+=,N/,=dZFWW!+!@C!5G=/gZ!&acW!3!@!2!5G=AZAWWPWPccW!3!&!2!&adW  W!.^dcWa!+!.P"
DATA "Wf WWecWa!.^W gc"
DATA ""
DATA 16, 16
DATA "~cicgWjcW!D!Dd0!*WicW0!*0!*0!*WgcWW!=gU1WWdcWF!&^,9,,@=^PPWccW!2!&^,,9@,@P!.PW cW!5!GW,9,=,,!+!&CW c"
DATA "W!2!@^,,9@,@P!2aW cWC!&^/9/9/9^!.WW cWX!@ag=!&!2CW cW!5!@W=U=U=U!+:CW cW!@!3!+gW!@G!+W cW!3C!+We W!G"
DATA "!2!&W ccdWgcdWcc"
DATA ""
DATA 16, 16
DATA "~cicgWjcW!D!Dd0!*WicW0!*0!*0!*WgcWW!=gU1WWdcWX!@!+,9,,@=!+F!&WccWX!G!+,,9@,@!+!2!&W cWC!&^,9,=,,^!.W"
DATA "W cWX!&a,,9@,@!+PCW cW!5!Ga/9/9/9!+!&CW cW!2!+^g=PPaW cWC!&^=U=U=U^'WW cW!&!+PgW!&a^W cW!+W^We W!+!."
DATA "PW ccdWgcdWcc"
DATA ""
DATA 16, 16
DATA "~clcdWmcWd0dWicWB00!*!D$!8WgcW/Sd!D!#UAWfcWN,,9/,NU!8:WdcWN@d,@!8UUGPWccWN9,,@=ZUZGP!G!2cWNN/d,!8UZ"
DATA "!8!&!G!.!&W!&,,//!JNUZZ!&!&!.!+!&!=://,/,/Z!8P!&!2P!&W!GC!+WW=/,ZP!+!2^PW C!2W  dWXX!2!&PW cWWfcW!GC"
DATA "!+!&W jcWCC!&W dc"
DATA ""
DATA 16, 16
DATA "~clcdWmcWd0dWicWB00!*!D$!8WgcW/Sd!D!#UAWfcWN,,9/,NU!8:WdcWN@d,@!8UUG!&WccWN9,,@=ZUZG!&!+!.cWNN/d,!8U"
DATA "Z!8P!&!2^W!@,,//!JNUZZP!G!2^P!3!2//,/,/Z!8!&!G!.!&PWaW^WW=/,Z!&!G!.!+!&W W!.W  dWC!F!.^!&W cWWfcW!+a"
DATA "^PW jcWWaPW dc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW!I!1!7WkcW.!%!1!4dWjcW!>!6WaaCWicW7-eWjcW!(!(WmcW!(!(PWkcW!&WW!.WkcW!+eWf fcW!+eWf fcW!+W W"
DATA "mcW~cmc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW!I!1!7WkcW.!%!1!4dWjcW!>!6WaaCWicW7-eWjcW!(!(WlcWa!(!(WlcW!+WWPWkcWaWWaWf fcW!+WWaWf gcWcWa"
DATA "W  mcW~cjc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW!I!4WlcW!I!I.!7WkcW7Q!6HWkcW-7<RWWjcW-HHR!&!2WhcWC7HRRW!2!.WgcWC!.WW!&!.WWhcW!2dW!&WWe ecW"
DATA "!2!.WW!&WWd gcWWcW!&!.WncWW~cic"
DATA ""
DATA 16, 16
DATA "~ckcWWncW!I!4WlcW!I!I.!7WkcW7Q!6HWkcW-7<RWWjcW-HHR!&!2WicW7HRHW!2!.WicWWPWcWWjcW!+WWg gcWa!.Wf hcW!+"
DATA "Wd lcW~clc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW!I!4WlcW.!I!%!7WkcW7Q!;HWkcW-d7WWjcW!(!(<H!2WjcW!(!(<<!2WjcW!&PC!&WWjcW!+PCWWf fcW!&W!2WWf "
DATA "gcWWCWW  cc icWWfc ~cdc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW!I!4WlcW.!I!%!7WkcW7Q!;HWkcW-d7WWjcW!(!(<H!2WjcW<!(<<!2WjcW!&PeWjcW!&!.!2WWf fcW!&!.!.WWf "
DATA "fcW!&dWfc gcWWhc ~cdc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW.!4WlcW.!%!=!7WkcW7Q!;HWkcW7-*<WkcW77!(!2WkcW77!2HWlcW!2PWmcWaaWg gcW!.!+Wg hcWaWd c jcWec "
DATA "~cfc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW.!4WlcW.!%!=!7WkcW7Q!;HWkcW7-*<WkcW77!(!2WkcW!(7!2H!.WicWC!.!2WaWWicW!2!.WW!&!.We ecW!2dW!&"
DATA "WWe ecW!2WWcWWdc gcWWgc ~cfc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW!I.!4WlcW!I!1!G!2WlcW\!;WjceW7-WicWCC!2!&<!(WjceW!(<PWkcW!.WW!.WkcW!+eWf fcW!+eWf fcW!+W W"
DATA "mcW~cmc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW!I.!4WlcW!I!1!G!2WlcW\!;WjceW7-WicWCC!2!&<!(WjcdWa!(<WlcW!+WWPWkcWaWWaWf fcW!+WWaWf gcWcWaW"
DATA "  mcW~cjc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW!I!I!4WkcWW!I!1!4!7WicWCW7\!6HWjcW!&-7<HWkcW7dHWjcWC7HRHWjcWC!.WW!&WWdc ecW!2dW!&!.We ecW!2"
DATA "!.WW!&WWd gcWWcW!&!.WncWW~cic"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW!I!I!4WkcWW!I!1!4!7WicWCW7\!6HWjcW!&-7<HWkcW7dHWkcW7HRHWlcWWPW ec gcW!+WWg gcWa!.Wf hcW!+W"
DATA "d lcW~clc"
DATA ""
DATA 16, 16
DATA "~cjceWlcW!I!I!4!2WjcWC.!1!4!7WjcWC7\!6HWjcW!2-7-7WkcWe!(WkcW!(!(<!(W jcW!&PC!2W jcW!+PCWWf fcW!&W!2W"
DATA "Wf gcWWCWW  lcWW~cjc"
DATA ""
DATA 16, 16
DATA "~cjceWlcW!I!I!4!2WjcWC.!1!4!7WjcWC7\!6HWjcW!2-7-7WkcWe!(WkcW<!(<!(W jcW!&PdW jcW!&!.!2WWf fcW!&!.!.W"
DATA "Wf fcW!&dWmcWW~clc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW!I!4!2WkcW.!=!7!7WkcW7\!;HWkcW--*<WkcWd7HWkcW-77HWcc  hcW!.PWcc  icWaaWg gcW!.!+Wg hcWaWd "
DATA "lcW~ckc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW!I!4!2WkcW.!=!7!7WkcW7\!;HWkcW--*<WkcWd7HWkcW!(7-H!.Wc  fcWCdWaWW  gcW!2!.WW!&!.We ecW!2dW"
DATA "!&WWe ecW!2!.WcWWkcWW~cmc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW0!DOWkcW!*!:UUdWjcW!>!6WaaCWicW/&eWjcW==WmcW=UPWkcW!&WW!.WkcW!+eWf fcW!+eWf fcW!+W WmcW~cmc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW0!DOWkcW!*!:UUdWjcW!>!6WaaCWicW/&eWjcW==WlcWa=UWlcW!+WWPWkcWaWWaWf fcW!+WWaWf gcWcWaW  mcW"
DATA "~cjc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW0!DWlcW0!0!'UWkcW,Q!6!8WkcW/=UZWWjcW/ZZ!8!&!2WhcWC9Z!8ZW!2!.WgcWC!.WW!&!.WWhcW!2dW!&WWe ecW"
DATA "!2!.WW!&WWd gcWWcW!&!.WncWW~cic"
DATA ""
DATA 16, 16
DATA "~ckcWWncW0!DWlcW0!0!'UWkcW,Q!6!8WkcW/=UZWWjcW/ZZ!8!&!2WicW9Z!8ZW!2!.WicWWPWcWWjcW!+WWg gcWa!.Wf hcW"
DATA "!+Wd lcW~clc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW0$WlcW0S!<UWkcW,Q!;ZWkcW=,//WWjcW=UZZ!2WjcWU=UZ!2WjcW!&PC!&WWjcW!+PCWWf fcW!&W!2WWf gcWWCWW"
DATA "  cc icWWfc ~cdc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW0$WlcW0S!<UWkcW,Q!;ZWkcW=,//WWjcW=UZZ!2WjcWU=UZ!2WjcW!&PeWjcW!&!.!2WWf fcW!&!.!.WWf fcW!&dW"
DATA "fc gcWWhc ~cdc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW0UWlcWI!A!*UWkcW,Q!;ZWkcW//KZWkcW,,=!2WkcW/,!2ZWlcW!2PWmcWaaWg gcW!.!+Wg hcWaWd c jcWec ~c"
DATA "fc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW0UWlcWI!A!*UWkcW,Q!;ZWkcW//KZWkcW,,=!2WkcW!D,!2Z!.WicWC!.!2WaWWicW!2!.WW!&!.We ecW!2dW!&WW"
DATA "e ecW!2WWcWWdc gcWWgc ~cfc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW00UWlcWI!<O!8WlcW\!;WjceW/&WicWCC!2!&U=WjceW=UPWkcW!.WW!.WkcW!+eWf fcW!+eWf fcW!+W WmcW~cmc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW00UWlcWI!<O!8WlcW\!;WjceW/&WicWCC!2!&U=WjcdWa=UWlcW!+WWPWkcWaWWaWf fcW!+WWaWf gcWcWaW  mcW"
DATA "~cjc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW0IUWkcWW!*%\UWicWCW=\!6ZWjcW!&/,UZWkcW/dZWjcWC9ZAZWjcWC!.WW!&WWdc ecW!2dW!&!.We ecW!2!.WW!&"
DATA "WWd gcWWcW!&!.WncWW~cic"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW0IUWkcWW!*%\UWicWCW=\!6ZWjcW!&/,UZWkcW/dZWkcW9ZAZWlcWWPW ec gcW!+WWg gcWa!.Wf hcW!+Wd lcW~c"
DATA "lc"
DATA ""
DATA 16, 16
DATA "~cjceWlcW0I!DAWjcWC!*!<!BUWjcWC,\!6ZWjcW!2,/=/WkcWe=WkcWU=U=W jcW!&PC!2W jcW!+PCWWf fcW!&W!2WWf gcWW"
DATA "CWW  lcWW~cjc"
DATA ""
DATA 16, 16
DATA "~cjceWlcW0I!DAWjcWC!*!<!BUWjcWC,\!6ZWjcW!2,/=/WkcWe=WkcWU=U=W jcW!&PdW jcW!&!.!2WWf fcW!&!.!.WWf fcW"
DATA "!&dWmcWW~clc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcWI!DAWkcW!*!<!BUWkcW,\!;!8WkcW//KZWkcW,/,ZWkcW/,/ZWcc  hcW!.PWcc  icWaaWg gcW!.!+Wg hcWaWd "
DATA "lcW~ckc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcWI!DAWkcW!*!<!BUWkcW,\!;!8WkcW//KZWkcW,/,ZWkcW/,/Z!.Wc  fcWCdWaWW  gcW!2!.WW!&!.We ecW!2dW!&"
DATA "WWe ecW!2!.WcWWkcWW~cmc"
DATA ""
DATA 16, 16
DATA "~cxcgWjcWg.WgcdWe.!1!4.gW:77..!4!1..C1!%1!%!=WX-7*!4!7!4!7!7CH77RWW:7--!7!7!4!GH--*-RWW-m!(HWW7!(<!("
DATA "<!(<!(<!(<!(<RWW!@!+a!3!+!+G!+!3!3a!3!3!+WWbF!2:!2F!5F!5:!2:!5!+WWCWWC!.WCW!2CWC!2PWW!2PP!&PP!&P!&!&"
DATA "P!&!&^WcoWc"
DATA ""
DATA 16, 16
DATA "~cxcgWjcWg.WgcdWe.!1!4.gW:77..!4!1..C1!%1!%!=WX-7*!4!7!4!7!7CH77RWW:7--!7!7!4!GH--*-RWW-m!(HWW7!(<!("
DATA "<!(<!(<!(<!(<RWW!&!3!3!+!3!3a!3!+!+!3!+!+!3WW!3:!5F!5:!2:!2F!5F!2!@WWaCCW!2CWC!.WCW!.!2WWP!&!&P!&!&P"
DATA "!&PP!&PP!+WcoWc"
DATA ""
DATA 16, 16
DATA "~cjceWkcWWd.!7WhcWWg.!4WfcW--e.!1!4.WfcW7-**!4!1.?!7!&WdcWG77?*!7!1!7!%C<PWcc!F!&C77-77!(!%!%-*PWc!&"
DATA "X!&a77d-7;;7!(!&W!&!&b!@a77--!(R;!GH'!&W!&!&X!@G77gHP!+cW!+:!F!@G7RHdWP!2!&ccW!G:C!&aWWd WC!&dcW!G!&"
DATA "C!&Wf WWecW!+'!+W gc"
DATA ""
DATA 16, 16
DATA "~cjceWkcWWd.!7WhcWWg.!4WfcW--e.!1!4.WfcW7-**!4!1.?!7PWdcWa77?*!7!1!7!%C<!&Wccb!@!+77-77!(!%!%-*!&Wc:"
DATA "C!@G77d-7;;7!(PW!G:!F!&C77--!(R;!GHPPW!G:C!&a77gH!&!GcW!G!&b!&!+7RHdW!&!.PccW!&!&X!@GWWd Wa^dcW!&!@"
DATA "!.PWf WWecW!GP^W gc"
DATA ""
DATA 16, 16
DATA "~cjceWkcWWe.WWhcW-f.!4HWfcWW?d.!1!4.!7WWdcWX!G**!4!1..!7!7!2!3WccW!5!3-!1!7!%!G!7!7H!&!2WccWF!+-7-!)"
DATA ":7-HP!.W cW!2!+--7!)!@--HPPW cWX!Gd-!):H7H!2!3W cW!2!+7!(!(!@!@dHP!+W cWF!+^dH<HH^!&!+W cW:!G^gW!.!2"
DATA "CW cWGC!+We W!G'!&W ccdWgcdW c"
DATA ""
DATA 16, 16
DATA "~cjceWkcWWe.WWhcW-f.!4HWfcWW?d.!1!4.!7WWdcWF!+**!4!1..!7!7P!+WccW!2!+-!1!7!%!G!7!7HP!.WccWX!G-7-!):7"
DATA "-H'!2W cW!5!3--7!)!@--H!&'W cWF!+d-!):H7HP!+W cW!5!G7!(!(!@!@dH!&!3W cWX!GWdH<HH!+!3!3W cW'!+!+gW!2"
DATA "!.WW cW!+W^We W!+PPW ccdWgcdW c"
DATA ""
DATA 16, 16
DATA "~cjceWlcW!1d.WWicWf.!4!1WWgcWd.!1!4..<7WecW:*!=!1..!4!7<HWdcW'7!1!=!4!4d!7HR!&WcW'-71!FH-77HH!+PPW:*"
DATA "?!)!3H7-7HR^P!2^:!G-1!GH!7?7HH^!GG^P!3!+!(d7<7HR!&!GGPPWGC^dWH<H!&!Ga!+PW !3!2Wd WWCFa!&!&W cWWfcW!3"
DATA "!3^!&W jcW!3GPW dc"
DATA ""
DATA 16, 16
DATA "~cjceWlcW!1d.WWicWf.!4!1WWgcWd.!1!4..<7WecW'*!=!1..!4!7<HWdcW!57!1!=!4!4d!7HRPWcW:-71!FH-77HHPF!2W'*"
DATA "?!)!3H7-7HR!+'P!&'!&-1!GH!7?7HH!+!&a!+!&b!3!(d7<7HRP!&a!&!&WdadWH<HP!&G^!&W !+!.Wd WWX:G^PW cWWfcWa"
DATA "!+!+PW jcW!+a!&W dc"
DATA ""
DATA 16, 16
DATA "~czcgWjcWe.!4!1WdcfWd.!1!4..dWcb1!%1!=*!4!1e.H<!@WWHd7!1!4!1!1!4!7!7H<!5WWd-d7!1!4!4!7H7-!@WW7m!(HWW"
DATA "-!(<!(<!(<!(<!(<!(<RWW!&!3!3!+!3!3a!3a!+!3!+!+!3WW!3:!5F!5:!2:!2F!5F!2!@WWaCCW!2CWC!.WCW!.!2WWP!&!&P"
DATA "!&!&P!&PP!&PP!+WcoWc"
DATA ""
DATA 16, 16
DATA "~czcgWjcWe.!4!1WdcfWd.!1!4..dWcb1!%1!=*!4!1e.H<!@WWHd7!1!4!1!1!4!7!7H<!5WWd-d7!1!4!4!7H7-!@WW7m!(HWW"
DATA "-!(<!(<!(<!(<!(<!(<RWW!@!+a!3!+!+G!+!3!3a!3!3!+WWbF!2:!2F!5F!5:!2:!5!+WWCWWC!.WCW!2CWC!2PWW!2PP!&PP"
DATA "!&P!&!&P!&!&^WcoWc"
DATA ""
DATA 16, 16
DATA "tcWWncW;8WncW;!%fWhcdW!%e.!4WfcWd-d.!1!4..WecW7--.!4!1e.WdcWa77-!4*!I.!4!7!7!&Wccb!@!+77!4!=!1:!7!2H"
DATA "7!&Wc:C!@G77-77e-!(PW!G:!F!&C77-77::!&!+PPW!G:C!&a77!3!2!+!&!+!2!&!GcW!G!&b!&!+7!G!+dW!&!+PccW!&!&X"
DATA "!@GWWd Wa^dcW!&!@aPWf WWecW!GP^W gc"
DATA ""
DATA 16, 16
DATA "tcWWncW;8WncW;!%fWhcdW!%e.!4WfcWd-d.!1!4..WecW7--.!4!1e.WdcWG77-!4*!I.!4!7!7PWcc!F!&G77!4!=!1:!7!2H7"
DATA "PWc!&X!&a77-77e-!(!&W!&!&b!@a77-77::!&!+!&!&W!&!&X!@G77!3!2!+!&!+!2P!&cW!+:!F!@G7!G!+dWPG!&ccW!G:C!&"
DATA "aWWd WC!&dcW!G!&C!&Wf WWecW!+'!+W gc"
DATA ""
DATA 16, 16
DATA "hcWWncW!)!@WmcW!):WkcdW!)!@dWhcW--e.-HWfcWWe.!1!4.!7WWdcWF!+*.!4!1d.!7P!+WccW!2!+?!4*..d!7P!+WccWX!G"
DATA "-!7!1!1!7!7!2H!2!2W cW!5!3-e7--H!&'W cWF!+--7-7-7HP!.W cW!5!G7!2CGG!2CH!&!3W cWX!GWC!2G!3C!2!+!2!3W "
DATA "cW'!+^gW!&!+!.W cW!+W^We W!+!.PW ccdWgcdW c"
DATA ""
DATA 16, 16
DATA "hcWWncW!)!@WmcW!):WkcdW!)!@dWhcW--e.-HWfcWWe.!1!4.!7WWdcWX!G*.!4!1d.!7!2!3WccW!5!G?!4*..d!7!&!3WccWF"
DATA "!+-!7!1!1!7!7!2HP!.W cW!2!+-e7--HPPW cWX!G--7-7-7H!2!2W cW!2!+7!2CGG!2CHP!+W cWF!+^C!2G!3C!2^!.!+W c"
DATA "W:!G!+gW!@!GCW cWGC!+We W!G!2!&W ccdWgcdW c"
DATA ""
DATA 16, 16
DATA "|cWWncW!)!GWhcfW1!3WhcWf.CdWfcWe.!1!4.7-7WecW..!4!1d.<7HWdcW!5*!4*.*!7!7HHRPWcW:-7!7!1!4!4!7!4HHPFGW"
DATA "'*?-f7HR!+!2P!&'!&G!@LX*?7HH!+PP!+!&b!3!@d!3!@!+HR^!&a!&!&WdadWG!&HP!&G^!&W !+!.Wd WWX:G^PW cWWfcWa"
DATA "!+!+PW jcW!+a!&W dc"
DATA ""
DATA 16, 16
DATA "|cWWncW!)!GWhcfW1!3WhcWf.CdWfcWe.!1!4.7-7WecW..!4!1d.<7HWdcW'*!4*.*!7!7HHR!&WcW'-7!7!1!4!4!7!4HH!&Pa"
DATA "W:*?-f7HRPP!2^:!GG!@LX*?7HH^'!2^P!3!+!@d!3!@!+HR!+!GGPPWGC^dWG!&H!&!Ga!+PW !3!2Wd WWC'a!&!&W cWWfcW"
DATA "!3!3^!&W jcW!3GPW dc"
DATA ""
DATA 16, 16
DATA "~cxcgWjcW!*0!*0!*0WgcdW0!*00!D!D0gW:9,!*0!D!D!*0A1!%1!%!=WX//!*fUAZ,=!8WW:d/UO!DOZ,/N/!8WW,d=!Bd=!B"
DATA "d=!BZWW/=Z=Z=Z=Z=Z=ZAWW!@!+a!3!+!+G!+!3!3a!3!3!+WWbF!2:!2F!5F!5:!2:!5!+WWCWWC!.WCW!2CWC!2PWW!2PP!&PP"
DATA "!&P!&!&P!&!&^WcoWc"
DATA ""
DATA 16, 16
DATA "~cxcgWjcW!*0!*0!*0WgcdW0!*00!D!D0gW:9,!*0!D!D!*0A1!%1!%!=WX//!*fUAZ,=!8WW:d/UO!DOZ,/N/!8WW,d=!Bd=!B"
DATA "d=!BZWW/=Z=Z=Z=Z=Z=ZAWW!&!3!3!+!3!3a!3!+!+!3!+!+!3WW!3:!5F!5:!2:!2F!5F!2!@WWaCCW!2CWC!.WCW!.!2WWP!&"
DATA "!&P!&!&P!&PP!&PP!+WcoWc"
DATA ""
DATA 16, 16
DATA "~cjceWkcWWd0UWhcWW!*0!*0!*0!DWfcWN/0!*00!D!D0WfcW,N!*!*!D!D!*!*O!&WdcWG,/!*!*dU!%AZPWcc!F!&C,,Nd/!%"
DATA "!%/NPWc!&X!&a,/N//=;;/!B!&W!&!&b!@a,,N,=A;!GZ'!&W!&!&X!@G,=gZP!+cW!+:!F!@G=AZdWP!2!&ccW!G:C!&aWWd WC"
DATA "!&dcW!G!&C!&Wf WWecW!+'!+W gc"
DATA ""
DATA 16, 16
DATA "~cjceWkcWWd0UWhcWW!*0!*0!*0!DWfcWN/0!*00!D!D0WfcW,N!*!*!D!D!*!*OPWdcWa,/!*!*dU!%AZ!&Wccb!@!+,,Nd/!%"
DATA "!%/N!&Wc:C!@G,/N//=;;/!BPW!G:!F!&C,,N,=A;!GZPPW!G:C!&a,=gZ!&!GcW!G!&b!&!+=AZdW!&!.PccW!&!&X!@GWWd Wa"
DATA "^dcW!&!@!.PWf WWecW!GP^W gc"
DATA ""
DATA 16, 16
DATA "~cjceWkcWWe0WWhcW/0!*0!*0!DZWfcWW!*!*00!D!D0UWWdcWX!G!*!*!D!D!*0OU!2!3WccW!5!3N!DU!%!GUOZ!&!2WccWF!+"
DATA "//=!):/,ZP!.W cW!2!+N//!)!@,/ZPPW cWX!G//!B!):Z&Z!2!3W cW!2!+/==!@!@dZP!+W cWF!+^ZAZAZA^!&!+W cW:!G^"
DATA "gW!.!2CW cWGC!+We W!G'!&W ccdWgcdW c"
DATA ""
DATA 16, 16
DATA "~cjceWkcWWe0WWhcW/0!*0!*0!DZWfcWW!*!*00!D!D0UWWdcWF!+!*!*!D!D!*0OUP!+WccW!2!+N!DU!%!GUOZP!.WccWX!G//"
DATA "=!):/,Z'!2W cW!5!3N//!)!@,/Z!&'W cWF!+//!B!):Z&ZP!+W cW!5!G/==!@!@dZ!&!3W cWX!GWZAZAZA!+!3!3W cW'!+"
DATA "!+gW!2!.WW cW!+W^We W!+PPW ccdWgcdW c"
DATA ""
DATA 16, 16
DATA "~cjceWlcW!Dd0WWicW0!*0!*0!D!DWWgcW!*00!D!D00Z!DWecW:!*!*!D!*0\UZZWdcW'/!D!=U$dUZ!8!&WcW'/91!FZ,,!BZ"
DATA "!8!+PPW:NS!)!3Z//!DZ!8^P!2^:!G=1!GZ!B!*!DZ!8^!GG^P!3!+=/,,Z/Z!8!&!GGPPWGC^dWdZ!&!Ga!+PW !3!2Wd WWCFa"
DATA "!&!&W cWWfcW!3!3^!&W jcW!3GPW dc"
DATA ""
DATA 16, 16
DATA "~cjceWlcW!Dd0WWicW0!*0!*0!D!DWWgcW!*00!D!D00Z!DWecW'!*!*!D!*0\UZZWdcW!5/!D!=U$dUZ!8PWcW:/91!FZ,,!BZ"
DATA "!8PF!2W'NS!)!3Z//!DZ!8!+'P!&'!&=1!GZ!B!*!DZ!8!+!&a!+!&b!3=/,,Z/Z!8P!&a!&!&WdadWdZP!&G^!&W !+!.Wd WWX"
DATA ":G^PW cWWfcWa!+!+PW jcW!+a!&W dc"
DATA ""
DATA 16, 16
DATA "~czcgWjcW!*0!*0!D!DWdcfW!*00!D!D00dWcb1!%1!=!*!D!D!*0!*0ZZ!@WWZ//,!DU!*!DUUOZZ!5WW/N/N//!D!DUOZ9/!@W"
DATA "W/d=!Bd=!Bd=!BZWW/=Z=Z=Z=Z=Z=Z!8WW!&!3!3!+!3!3a!3a!+!3!+!+!3WW!3:!5F!5:!2:!2F!5F!2!@WWaCCW!2CWC!.WCW"
DATA "!.!2WWP!&!&P!&!&P!&PP!&PP!+WcoWc"
DATA ""
DATA 16, 16
DATA "~czcgWjcW!*0!*0!D!DWdcfW!*00!D!D00dWcb1!%1!=!*!D!D!*0!*0ZZ!@WWZ//,!DU!*!DUUOZZ!5WW/N/N//!D!DUOZ9/!@W"
DATA "W/d=!Bd=!Bd=!BZWW/=Z=Z=Z=Z=Z=Z!8WW!@!+a!3!+!+G!+!3!3a!3!3!+WWbF!2:!2F!5F!5:!2:!5!+WWCWWC!.WCW!2CWC!2"
DATA "PWW!2PP!&PP!&P!&!&P!&!&^WcoWc"
DATA ""
DATA 16, 16
DATA "tcWWncW;8WncW;!%fWhcdW!%!*0!*0!DWfcWN//!*00!D!D00WecW,N,0!D!D!*0!*0WdcWa,/NU!*!#0UUO!&Wccb!@!+,,$!*"
DATA "!D!DUZZ!D!&Wc:C!@G,/N=/,d/!BPW!G:!F!&C,,N!D=::!&!+PPW!G:C!&a,=!3!2!+!&!+!2!&!GcW!G!&b!&!+=!G!+dW!&!+"
DATA "PccW!&!&X!@GWWd Wa^dcW!&!@aPWf WWecW!GP^W gc"
DATA ""
DATA 16, 16
DATA "tcWWncW;8WncW;!%fWhcdW!%!*0!*0!DWfcWN//!*00!D!D00WecW,N,0!D!D!*0!*0WdcWG,/NU!*!#0UUOPWcc!F!&G,,$!*!D"
DATA "!DUZZ!DPWc!&X!&a,/N=/,d/!B!&W!&!&b!@a,,N!D=::!&!+!&!&W!&!&X!@G,=!3!2!+!&!+!2P!&cW!+:!F!@G=!G!+dWPG!&"
DATA "ccW!G:C!&aWWd WC!&dcW!G!&C!&Wf WWecW!+'!+W gc"
DATA ""
DATA 16, 16
DATA "hcWWncW!)!@WmcW!):WkcdW!)!@dWhcW//!*0!*0NZWfcWW0!*00!D!D0UWWdcWF!+!*0!D!D!*0!*UP!+WccW!2!+!*U!<00dUP"
DATA "!+WccWX!G/U!<!D!BOUZ!2!2W cW!5!3N//=/,/Z!&'W cWF!+//9/9/9ZP!.W cW!5!G/!2CGG!2CZ!&!3W cWX!GWC!2G!3C!2"
DATA "!+!2!3W cW'!+^gW!&!+!.W cW!+W^We W!+!.PW ccdWgcdW c"
DATA ""
DATA 16, 16
DATA "hcWWncW!)!@WmcW!):WkcdW!)!@dWhcW//!*0!*0NZWfcWW0!*00!D!D0UWWdcWX!G!*0!D!D!*0!*U!2!3WccW!5!G!*U!<00dU"
DATA "!&!3WccWF!+/U!<!D!BOUZP!.W cW!2!+N//=/,/ZPPW cWX!G//9/9/9Z!2!2W cW!2!+/!2CGG!2CZP!+W cWF!+^C!2G!3C!2"
DATA "^!.!+W cW:!G!+gW!@!GCW cWGC!+We W!G!2!&W ccdWgcdW c"
DATA ""
DATA 16, 16
DATA "|cWWncW!)!GWhcfW1!3WhcW0!*0!*0CdWfcW0!*00!D!D0//!DWecW!*0!D!D!*0!*Z!DZWdcW!5!*U!<0!*UUZZ!8PWcW://U!D"
DATA "$\UUZ!8PFGW'N!*N//=/!DZ!8!+!2P!&'!&G!@LX!J!*!DZ!8!+PP!+!&b!3!@d!3!@!+Z!8^!&a!&!&WdadWG!&ZP!&G^!&W !+"
DATA "!.Wd WWX:G^PW cWWfcWa!+!+PW jcW!+a!&W dc"
DATA ""
DATA 16, 16
DATA "|cWWncW!)!GWhcfW1!3WhcW0!*0!*0CdWfcW0!*00!D!D0//!DWecW!*0!D!D!*0!*Z!DZWdcW'!*U!<0!*UUZZ!8!&WcW'//U!D"
DATA "$\UUZ!8!&PaW:N!*N//=/!DZ!8PP!2^:!GG!@LX!J!*!DZ!8^'!2^P!3!+!@d!3!@!+Z!8!+!GGPPWGC^dWG!&Z!&!Ga!+PW !3"
DATA "!2Wd WWC'a!&!&W cWWfcW!3!3^!&W jcW!3GPW dc"
DATA ""
DATA 16, 16
DATA "~ciciWhcWX!@!I!1!7dXWgcW!2.!%!1!4!2CGWgcW!2!&!>!6eWhcW!+!+7-WkcW!+a!(!(WkcW!+!+!(!(PWkcW!&WW!.Wce fc"
DATA "W!+eWf fcW!+eWf fcW!+W WmcW~cmc"
DATA ""
DATA 16, 16
DATA "~ciciWhcWX!@!I!1!7dXWgcW!2.!%!1!4!2CGWgcW!2!&!>!6eWhcW!+!+7-WkcW!+a!(!(WkcW!+!+!(!(WlcW!+WWPWce fcWa"
DATA "WWaWf fcW!+WWaWf gcWcWaW  mcW~cjc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW!I!4:WkcW!I!I.!7XWicWC7Q!6H!5XWhcW!2-7!(RW!=!#WhcW-dHWW!#WgcWC7HRHWcWhcWC!.WW!&WWcd ecW!2dW"
DATA "!&!.We ecW!2!.WW!&WWd gcWWcW!&!.WncWW~cic"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW!I!4:WkcW!I!I.!7XWicWC7Q!6H!5XWhcW!2-7!(RW!=!#WhcW-dHWW!#WhcW7HRHWcWjcWWPWdcd gcW!+WWg gcWa"
DATA "PWf hcW!+Wd lcW~clc"
DATA ""
DATA 16, 16
DATA "~ckceWlcW!I!4!G!5WjcW.!I!%!7:WjcW7Q!;!2!5WjcW-7!(!G:WjcWd!(!G!#WjcW!(!(<!(WkcW!&PC!2Wccd fcW!+P!2WW"
DATA "f fcW!&W!2WWf gcWWCWW  lcWW~cjc"
DATA ""
DATA 16, 16
DATA "~ckceWlcW!I!4!G!5WjcW.!I!%!7:WjcW7Q!;!2!5WjcW-7!(!G:WjcWd!(!G!#WjcW<!(<!(WkcW!&PdWccd fcW!&!.!2WWf "
DATA "fcW!&!.!.WWf fcW!&dWmcWW~clc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW.!4dWjcW!I!%!=!7X!GWicW7!>[!G!G!2WicW7-!5G!&WjcW7!#!G!+WkcW7!#IHWlcWPPWdcd gcWaaWg gcWW!+W"
DATA "g hcWaWd cc icWfc  ~cdc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW.!4dWjcW!I!%!=!7X!GWicW7!>[!G!G!2WicW7-!5G!&WjcW7!#!G!+WkcW!(!#IHWWicWCdWaWWcd ecW!2dW!&!.W"
DATA "e ecW!2dW!&WWe ecW!2!.WcWWec fcWWhc  ~cdc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW!I.!4WlcW.!1!G!&WhcfW!;[!.aWfcWI:!5!G!3d!G!3WgcfWHHdWkcW!(<WaWjcW!.WW!.WkcW!+eWf fcW!+eWf "
DATA "fcW!+W WmcW~cmc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW!I.!4WlcW.!1!G!&WhcfW!;[!.aWfcWI:!5!G!3d!G!3WgcfWHHdWjcWa!(<WaWjcW!+WWPWkcWaWWaWf fcW!+WWaW"
DATA "f gcWcWaW  mcW~cjc"
DATA ""
DATA 16, 16
DATA "~cgcWWcdWjcW1:W!I!I!4WicW:1!G.!1!4!7WicW:1G!;!;HWjcW:b!+!2!2aWjcW!+!&W!.WWicWC7!2!.W!.WicWC!.fW cc "
DATA "ecW!2!.WWa!.We ecW!2!.WW!&WWe fcWWcW!&!.Wd kcWW~cic"
DATA ""
DATA 16, 16
DATA "~cgcWWcdWjcW1:W!I!I!4WicW:1!G.!1!4!7WicW:1G!;!;HWjcW:b!+!2!2aWjcW!+!&W!.WWjcW7!2!.W!.WkcWWPWW  cc gc"
DATA "W!+WWg gcWa!.Wg gcW!+Wg icW~clc"
DATA ""
DATA 16, 16
DATA "vcWocW:eWkcWX!I!I!4!2WjcWX?!1!4!7WjcWXaO!;HWjcWCa!&!2!2WkcW!+a!+!+WkcWa!+a!+Wc  hcW!&PWPWc  hcW!+PaW"
DATA "Wf fcW!&W!2WWf gcWWCWWd kcWW~cjc"
DATA ""
DATA 16, 16
DATA "vcWocW:eWkcWX!I!I!4!2WjcWX?!1!4!7WjcWXaO!;HWjcWCa!&!2!2WkcW!+a!+!+WkcWa!+a!+Wc  hcW!&^dWc  hcW!&!.!2"
DATA "WWf fcW!&!.!.WWf fcW!&dWcd icWW~clc"
DATA ""
DATA 16, 16
DATA "xcWWncW!@!&WlcW!G!I!4!2WjcW:*!=!7!7WicW!@!2H!6!;HWicW!G!2!2!&!2<WjcW!2!&!2WHWjcW!&!2!&!.HWdc  fcWW!."
DATA "PWdc  hcWaaWg gcWW!+Wg hcWaWe kcW~ckc"
DATA ""
DATA 16, 16
DATA "xcWWncW!@!&WlcW!G!I!4!2WjcW:*!=!7!7WicW!@!2H!6!;HWicW!G!2!2!&!2<WjcW!2!&!2WHWjcW!&!2!&!.H!.Wcc  ecWC"
DATA "!.fWc  fcW!2!.WWa!.We ecW!2dW!&WWe ecW!2WWcWWc  hcWW~cmc"
DATA ""
DATA 16, 16
DATA "~ciciWhcWX!@0!DOdXWgcW!2!*!:UU!2CGWgcW!2!&!>!6eWhcW!+!+/&WkcW!+a==WkcW!+!+=UPWkcW!&WW!.Wce fcW!+eWf "
DATA "fcW!+eWf fcW!+W WmcW~cmc"
DATA ""
DATA 16, 16
DATA "~ciciWhcWX!@0!DOdXWgcW!2!*!:UU!2CGWgcW!2!&!>!6eWhcW!+!+/&WkcW!+a==WkcW!+!+=UWlcW!+WWPWce fcWaWWaWf "
DATA "fcW!+WWaWf gcWcWaW  mcW~cjc"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW0!D:WkcW0!0!'UXWicWC,Q!6!8!5XWhcW!2/==ZW!=!#WhcW/dZWW!#WgcWC9ZAZWcWhcWC!.WW!&WWcd ecW!2dW!&"
DATA "!.We ecW!2!.WW!&WWd gcWWcW!&!.WncWW~cic"
DATA ""
DATA 16, 16
DATA "~ckcdWmcW0!D:WkcW0!0!'UXWicWC,Q!6!8!5XWhcW!2/==ZW!=!#WhcW/dZWW!#WhcW9ZAZWcWjcWWPWdcd gcW!+WWg gcWaPW"
DATA "f hcW!+Wd lcW~clc"
DATA ""
DATA 16, 16
DATA "~ckceWlcW0$!G!5WjcW0S!<U:WjcW,Q!;!2!5WjcW=,,!G:WjcWd=!G!#WjcWU=U=WkcW!&PC!2Wccd fcW!+P!2WWf fcW!&W!2"
DATA "WWf gcWWCWW  lcWW~cjc"
DATA ""
DATA 16, 16
DATA "~ckceWlcW0$!G!5WjcW0S!<U:WjcW,Q!;!2!5WjcW=,,!G:WjcWd=!G!#WjcWU=U=WkcW!&PdWccd fcW!&!.!2WWf fcW!&!.!."
DATA "WWf fcW!&dWmcWW~clc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW0UdWjcWI!A!*UX!GWicW,!>[!G!G!2WicW//!5G!&WjcW,!#!G!+WkcW/!#IZWlcWPPWdcd gcWaaWg gcWW!+Wg hc"
DATA "WaWd cc icWfc  ~cdc"
DATA ""
DATA 16, 16
DATA "~ckcWWncW0UdWjcWI!A!*UX!GWicW,!>[!G!G!2WicW//!5G!&WjcW,!#!G!+WkcW/!#IZWWicWCdWaWWcd ecW!2dW!&!.We ec"
DATA "W!2dW!&WWe ecW!2!.WcWWec fcWWhc  ~cdc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW00UWlcWI!<O!8WhcfW!;[!.aWfcWI:!5!G!3d!G!3WgcfWZZdWkcW=UWaWjcW!.WW!.WkcW!+eWf fcW!+eWf fcW!+"
DATA "W WmcW~cmc"
DATA ""
DATA 16, 16
DATA "~cjcdWmcW00UWlcWI!<O!8WhcfW!;[!.aWfcWI:!5!G!3d!G!3WgcfWZZdWjcWa=UWaWjcW!+WWPWkcWaWWaWf fcW!+WWaWf gc"
DATA "WcWaW  mcW~cjc"
DATA ""
DATA 16, 16
DATA "~cgcWWcdWjcW1:W0IUWicW:1!G!*%\UWicW:1G!;!;ZWjcW:b!+!2!2aWjcW!+!&W!.WWicWC9!2!.W!.WicWC!.fW cc ecW!2"
DATA "!.WWa!.We ecW!2!.WW!&WWe fcWWcW!&!.Wd kcWW~cic"
DATA ""
DATA 16, 16
DATA "~cgcWWcdWjcW1:W0IUWicW:1!G!*%\UWicW:1G!;!;ZWjcW:b!+!2!2aWjcW!+!&W!.WWjcW9!2!.W!.WkcWWPWW  cc gcW!+WW"
DATA "g gcWa!.Wg gcW!+Wg icW~clc"
DATA ""
DATA 16, 16
DATA "vcWocW:eWkcWX0I!DAWjcWX0!<!BUWjcWXaO!;ZWjcWCa!&!2!2WkcW!+a!+!+WkcWa!+a!+Wc  hcW!&PWPWc  hcW!+PaWWf "
DATA "fcW!&W!2WWf gcWWCWWd kcWW~cjc"
DATA ""
DATA 16, 16
DATA "vcWocW:eWkcWX0I!DAWjcWX0!<!BUWjcWXaO!;ZWjcWCa!&!2!2WkcW!+a!+!+WkcWa!+a!+Wc  hcW!&^dWc  hcW!&!.!2WWf "
DATA "fcW!&!.!.WWf fcW!&dWcd icWW~clc"
DATA ""
DATA 16, 16
DATA "xcWWncW!@!&WlcW!GI!DAWjcW:!'!<!BUWicW!@!2Z!6!;!8WicW!G!2!2!&!2ZWjcW!2!&!2WZWjcW!&!2!&!.ZWdc  fcWW!.P"
DATA "Wdc  hcWaaWg gcWW!+Wg hcWaWe kcW~ckc"
DATA ""
DATA 16, 16
DATA "xcWWncW!@!&WlcW!GI!DAWjcW:!'!<!BUWicW!@!2Z!6!;!8WicW!G!2!2!&!2ZWjcW!2!&!2WZWjcW!&!2!&!.Z!.Wcc  ecWC"
DATA "!.fWc  fcW!2!.WWa!.We ecW!2dW!&WWe ecW!2WWcWWc  hcWW~cmc"
DATA ""
DATA 16, 16
DATA "~c~c{cfWjcWW8!E!E1!%fWdcW-7;!E!E!%1!+!&!2!&!&WccW--!=X:!@:H-7-7WccW?--*d-*d-7W cW<Hd<d!(<H<HW cW'!.a"
DATA "^eWa!&!.!.W cWe!.W  We!.W cWd!.W  ccWd!.W ccdWgcdWsc"
DATA ""
DATA 16, 16
DATA "~c~c{cfWjcWW8!E!E1!%fWdcW-7;!E!E!%1!+!&!2!&!&WccW--!=X:!@:H-7-7WccW?--*d-*d-7W cW<Hd<d!(<H<HW cW'!.a"
DATA "^eWa!&!.!.W cWe!.W  We!.W cWd!.W  ccWd!.W ccdWgcdWsc"
DATA ""
DATA 16, 16
DATA "~c~cmcdWkcdW1!%1WicW-7;!E1!%!EWgcWW7-L;b!2GPWecW!+!+a--!@!2!&C7W dcW!+!+!.W7--7!&C-WW ccW!+^WW--77PH"
DATA "!(!+W ccWW WW7-<dHP!.WecW!+!+a<HHWdPWecW!+!+!.dW dW fcW!+^We jcWWic"
DATA ""
DATA 16, 16
DATA "~c~cmcdWkcdW1!%1WicW-7;!E1!%!EWgcWW7-L;b!2GPWecW!+!+a--!@!2!&C7W dcW!+!+!.W7--7!&C-WW ccW!+^WW--77PH"
DATA "!(!+W ccWW WW7-<dHP!.WecW!+!+a<HHWdPWecW!+!+!.dW dW fcW!+^We jcWWic"
DATA ""
DATA 16, 16
DATA "~c~clcgWjcW-1!%1!%7WicW-!E1!%!E<WhcW!+-b!@!@bH^WgcWW-CCWG<^W  ecWP--C!.77^Wd ecW-7!&W-!(We dcW!+-e7H"
DATA "!+W  ecWa7e!(H!+W  eciWaW  fcW  cd Wd rc"
DATA ""
DATA 16, 16
DATA "~c~clcgWjcW-1!%1!%7WicW-!E1!%!E<WhcW!+-b!@!@bH^WgcWW-CCWG<^W  ecWP--C!.77^Wd ecW-7!&W-!(We dcW!+-e7H"
DATA "!+W  ecWa7e!(H!+W  eciWaW  fcW  cd Wd rc"
DATA ""
DATA 16, 16
DATA "~c~cncdWmcW!%1!%dWicW!E1!%!E!%-!(WgcW!&XG2L!G<RWWfcW-GP:G<H^!&PWccWW-GP<-!(HWP^^WcW!+**P<-7HWW^^W W"
DATA "!2!&d7-7RWW WW  Waa^W!(-!(!&!&PW  dcdW dW!&^^W kcWW^W  lcWW fc"
DATA ""
DATA 16, 16
DATA "~c~cncdWmcW!%1!%dWicW!E1!%!E!%-!(WgcW!&XG2L!G<RWWfcW-GP:G<H^!&PWccWW-GP<-!(HWP^^WcW!+**P<-7HWW^^W W"
DATA "!2!&d7-7RWW WW  Waa^W!(-!(!&!&PW  dcdW dW!&^^W kcWW^W  lcWW fc"
DATA ""
DATA 16, 16
DATA "~c~c~cfWgcfW1!%!E!E!GWWdcW!&!2!&!&!+!%1!E!E!G--WccW77-7-!@:!@!5!37-WccW?--*d-*d-7W cW<Hd<d!(<H<HW cW"
DATA "'!.a^eWa!&!.!.W cWe!.W  We!.W cWd!.W  ccWd!.W ccdWgcdWsc"
DATA ""
DATA 16, 16
DATA "~c~c~cfWgcfW1!%!E!E!GWWdcW!&!2!&!&!+!%1!E!E!G--WccW77-7-!@:!@!5!37-WccW?--*d-*d-7W cW<Hd<d!(<H<HW cW"
DATA "'!.a^eWa!&!.!.W cWe!.W  We!.W cWd!.W  ccWd!.W ccdWgcdWsc"
DATA ""
DATA 16, 16
DATA "~cycWocWCWWlcWW!&C-WWicW-77!&C!%!EWgcWW7--;!%1!E!EWecW!+!&a--1!)!=!5GW dcW!+!+!.W7-X!2FH7WWdcW!+^WW-"
DATA "-7d-7!+W ccWW WW7-<dHP!.WecW!+!+a<HHWdPWecW!+!+!.dW dW fcW!+^We jcWWic"
DATA ""
DATA 16, 16
DATA "~cycWocWCWWlcWW!&C-WWicW-77!&C!%!EWgcWW7--;!%1!E!EWecW!+!&a--1!)!=!5GW dcW!+!+!.W7-X!2FH7WWdcW!+^WW-"
DATA "-7d-7!+W ccWW WW7-<dHP!.WecW!+!+a<HHWdPWecW!+!+!.dW dW fcW!+^We jcWWic"
DATA ""
DATA 16, 16
DATA "~c{cWWmcWWCdWjcW--C!.77WicW-!)1!%!E!(WhcW!+-!E!%1!EHPWgcWW-LL!)L<^W  ecWP-e!3H^W  fcW-7-7-7Wd ecW!+-"
DATA "e7H!+Wd dcWa7!(!(<!(H!+Wd dciWaW  fcW  cd Wd rc"
DATA ""
DATA 16, 16
DATA "~c{cWWmcWWCdWjcW--C!.77WicW-!)1!%!E!(WhcW!+-!E!%1!EHPWgcWW-LL!)L<^W  ecWP-e!3H^W  fcW-7-7-7Wd ecW!+-"
DATA "e7H!+Wd dcWa7!(!(<!(H!+Wd dciWaW  fcW  cd Wd rc"
DATA ""
DATA 16, 16
DATA "~c~cWncWWGWkcWW-GPWWicW!E1CP<-7WgcW!E!E!%1!%-7RWWfcWX!E!)!=G<H^!&PWccWW77!@:G<HWP^^WcW!+*--7-!(HWW^^"
DATA "W W!2!&d7-7RWW WW  Waa^W!(-!(!&!&PWd ccdW dW!&^^W kcWW^W  lcWW fc"
DATA ""
DATA 16, 16
DATA "~c~cWncWWGWkcWW-GPWWicW!E1CP<-7WgcW!E!E!%1!%-7RWWfcWX!E!)!=G<H^!&PWccWW77!@:G<HWP^^WcW!+*--7-!(HWW^^"
DATA "W W!2!&d7-7RWW WW  Waa^W!(-!(!&!&PWd ccdW dW!&^^W kcWW^W  lcWW fc"
DATA ""
DATA 16, 16
DATA "~c~c{cfWjcWW8!E!E1!%fWdcWN/;!E!E!%1!+!&!2!&!&WccW//!=X:!@:Z,/,,WccWdN/N/fN/W cWUdZU=U==dZW cW'!.a^eW"
DATA "a!&!.!.W cWe!.W  We!.W cWd!.W  ccWd!.W ccdWgcdWsc"
DATA ""
DATA 16, 16
DATA "~c~c{cfWjcWW8!E!E1!%fWdcWN/;!E!E!%1!+!&!2!&!&WccW//!=X:!@:Z,/,,WccWdN/N/fN/W cWUdZU=U==dZW cW'!.a^eW"
DATA "a!&!.!.W cWe!.W  We!.W cWd!.W  ccWd!.W ccdWgcdWsc"
DATA ""
DATA 16, 16
DATA "~c~cmcdWkcdW1!%1WicWN,;!E1!%!EWgcWW/NL;b!2GPWecW!+!+a,N!@!2!&C,W dcW!+!+!.W/N//!&C/WW ccW!+^WW,N//PZ"
DATA ",!+W ccWW WW//eZP!.WecW!+!+aUZAWdPWecW!+!+!.dW dW fcW!+^We jcWWic"
DATA ""
DATA 16, 16
DATA "~c~cmcdWkcdW1!%1WicWN,;!E1!%!EWgcWW/NL;b!2GPWecW!+!+a,N!@!2!&C,W dcW!+!+!.W/N//!&C/WW ccW!+^WW,N//PZ"
DATA ",!+W ccWW WW//eZP!.WecW!+!+aUZAWdPWecW!+!+!.dW dW fcW!+^We jcWWic"
DATA ""
DATA 16, 16
DATA "~c~clcgWjcWN1!%1!%=WicWN!E1!%!EZWhcW!+/b!@!@bZ^WgcWWNCCWGZ^W  ecWPN/C!./=^Wd ecWN9!&W/!DWe dcW!+/,/,"
DATA ",Z!+W  ecWa/U=U=A!+W  eciWaW  fcW  cd Wd rc"
DATA ""
DATA 16, 16
DATA "~c~clcgWjcWN1!%1!%=WicWN!E1!%!EZWhcW!+/b!@!@bZ^WgcWWNCCWGZ^W  ecWPN/C!./=^Wd ecWN9!&W/!DWe dcW!+/,/,"
DATA ",Z!+W  ecWa/U=U=A!+W  eciWaW  fcW  cd Wd rc"
DATA ""
DATA 16, 16
DATA "~c~cncdWmcW!%1!%dWicW!E1!%!E!%//WgcW!&XG2L!GZ!8WWfcW/GP:GZZ^!&PWccWW/GPZ//ZWP^^WcW!+NKP=//ZWW^^W W!2"
DATA "!&/,d/!8WW WW  Waa^W=/=!&!&PW  dcdW dW!&^^W kcWW^W  lcWW fc"
DATA ""
DATA 16, 16
DATA "~c~cncdWmcW!%1!%dWicW!E1!%!E!%//WgcW!&XG2L!GZ!8WWfcW/GP:GZZ^!&PWccWW/GPZ//ZWP^^WcW!+NKP=//ZWW^^W W!2"
DATA "!&/,d/!8WW WW  Waa^W=/=!&!&PW  dcdW dW!&^^W kcWW^W  lcWW fc"
DATA ""
DATA 16, 16
DATA "~c~c~cfWgcfW1!%!E!E!GWWdcW!&!2!&!&!+!%1!E!E!G//WccW,/,/=!@:!@!5!39/WccWfN/N/dN/W cWUdZU=U==dZW cW'!."
DATA "a^eWa!&!.!.W cWe!.W  We!.W cWd!.W  ccWd!.W ccdWgcdWsc"
DATA ""
DATA 16, 16
DATA "~c~c~cfWgcfW1!%!E!E!GWWdcW!&!2!&!&!+!%1!E!E!G//WccW,/,/=!@:!@!5!39/WccWfN/N/dN/W cWUdZU=U==dZW cW'!."
DATA "a^eWa!&!.!.W cWe!.W  We!.W cWd!.W  ccWd!.W ccdWgcdWsc"
DATA ""
DATA 16, 16
DATA "~cycWocWCWWlcWW!&C/WWicWN//!&C!%!EWgcWW/N/;!%1!E!EWecW!+!&a,N1!)!=!5GW dcW!+!+!.W/NX!2FZ/WWdcW!+^WW,"
DATA "N//N//!+W ccWW WW//eZP!.WecW!+!+aUZAWdPWecW!+!+!.dW dW fcW!+^We jcWWic"
DATA ""
DATA 16, 16
DATA "~cycWocWCWWlcWW!&C/WWicWN//!&C!%!EWgcWW/N/;!%1!E!EWecW!+!&a,N1!)!=!5GW dcW!+!+!.W/NX!2FZ/WWdcW!+^WW,"
DATA "N//N//!+W ccWW WW//eZP!.WecW!+!+aUZAWdPWecW!+!+!.dW dW fcW!+^We jcWWic"
DATA ""
DATA 16, 16
DATA "~c{cWWmcWWCdWjcWN/C!./=WicWN!)1!%!E!DWhcW!+N!E!%1!EZPWgcWWNLL!)LZ^W  ecWP/e!3Z^W  fcWN9d/!DWd ecW!+/"
DATA ",/,,Z!+Wd dcWa/U=U=A!+Wd dciWaW  fcW  cd Wd rc"
DATA ""
DATA 16, 16
DATA "~c{cWWmcWWCdWjcWN/C!./=WicWN!)1!%!E!DWhcW!+N!E!%1!EZPWgcWWNLL!)LZ^W  ecWP/e!3Z^W  fcWN9d/!DWd ecW!+/"
DATA ",/,,Z!+Wd dcWa/U=U=A!+Wd dciWaW  fcW  cd Wd rc"
DATA ""
DATA 16, 16
DATA "~c~cWncWWGWkcWW/GPWWicW!E1CP=//WgcW!E!E!%1!%//!8WWfcWX!E!)!=GZZ^!&PWccWW//!@:GZZWP^^WcW!+N!J/N/!DZWW"
DATA "^^W W!2!&/,d/!8WW WW  Waa^W=/=!&!&PWd ccdW dW!&^^W kcWW^W  lcWW fc"
DATA ""
DATA 16, 16
DATA "~c~cWncWWGWkcWW/GPWWicW!E1CP=//WgcW!E!E!%1!%//!8WWfcWX!E!)!=GZZ^!&PWccWW//!@:GZZWP^^WcW!+N!J/N/!DZWW"
DATA "^^W W!2!&/,d/!8WW WW  Waa^W=/=!&!&PWd ccdW dW!&^^W kcWW^W  lcWW fc"
DATA ""
DATA 16, 16
DATA "~c~ccceWdcjWCGCGccW!E!E!)LeC!2eWcW-!E!E!)1!.WaWa7WWcW-7dL!=GfH!@aWW--!5X!5!@!2H7-7-!+PWW--7-7-7-7-7-"
DATA "!+^WW7!(<!(<!(<!(<!(<!(WW cWa!+GaaGaGG!+G!3W WbF!2!5!2F:F:!5!2!5:aWW!3WWC!.WCW!2CWC!2PWcoW qc"
DATA ""
DATA 16, 16
DATA "~c~ccceWdcjWCGCGccW!E!E!)LeC!2eWcW-!E!E!)1!.WaWa7WWcW-7dL!=GfH!&GWW--!5X!5!@!2H7-7-^'WW--7-7-7-7-7-^"
DATA "!+WW7!(<!(<!(<!(<!(<!(WW cWGGaGG!+Gaa!3a!+W WG!5:F:!5!2!5!2F:F!2CWW!+CCW!2CWC!.WCW!.!&WcoW qc"
DATA ""
DATA 16, 16
DATA "~cxcfWkcW;1!%1!%WicW-;!E!%!3!&1WgcW7-1;!@!2G!3!2WecWWH7-1CF!2a!3!2Wdc!5!2aH7-77!(!.H!=!5W c!&!5!2!+H"
DATA "7--7!(H!G!2GW W!@C!5aH7?7!(dHP!2WcW!@C!5!3H!(eH!+!@!&PccW!&C!2!GfW!&!+WPdcW!&X!2!+Wd W!+P^ecW!&!+!&W"
DATA " dcdWfcWW^W gc"
DATA ""
DATA 16, 16
DATA "~cxcfWkcW;1!%1!%WicW-;!E!%!3!&1WgcW7-1;!@!2G!3!2WecWWH7-1CF!2a!3!2WdcC!5!GH7-77!(!.H!=!5W c!@C!5!3H7"
DATA "--7!(H!G!2aW W!&!5!2!GH7?7!(dH'!.WcW!&X!2!+H!(eH^!&!@!&ccW!@!5!5afW!.!G!2!&dcW!@C!5!3Wd W!3!&!&ecW!@"
DATA "!3PW dcdWfcWC!+W gc"
DATA ""
DATA 16, 16
DATA "~czceWlcW!%1!%1WicWW7!)G!+LHWWfcW!GW7:Ga!3<W'WdcW!2!G!.77GaHHP!2PWccWX!&P--G!+<!(^P!&W cW!5!G!277!5"
DATA "!2H<!+!2!+W cWF!@F7-7-7!(!+FPW cW!2!&!.g!(P!.PW cWX!&hW!&W!&W cW!&!+WW c  W!&!.^W cW!&W^W dcW!+!.PW "
DATA "ccdWgcdWc "
DATA ""
DATA 16, 16
DATA "~czceWlcW!%1!%1WicWW7!)G!+LHWWfcW!&W7:Ga!3<WPWdcW!5!&!277GaHH!+!.!&WccWF!@F--G!+<!(!+'PW cW!2!&!.77"
DATA "!5!2H<P!.^W cWX!&P7-7-7!(^P!&W cW!5!@!2g!(!+!2!&W cWF!@!2gW!GFPW cW:!GCW c  W:!2!+W cW!@C!+W dcW!3!2"
DATA "!&W ccdWgcdWc "
DATA ""
DATA 16, 16
DATA "~c{cfWkcW1!%1!%!EWicW2G!&L!E!G<WgcWGa!&P1!3!3HHWecWG!+!.WG!3!3HHRWWccW!=C7PH<<7HaP!+!2cW!5!3!277H<7H"
DATA "RP!+!2^W!5!&d7*!(-HaP!@!.!+Wd!2aHd7HR!&!G!.!&W !&!+P!+fW!&!+!.^W caa^W ccWG!2!2^W ccdWecW!3a^W kcWWP"
DATA "W ec"
DATA ""
DATA 16, 16
DATA "~c{cfWkcW1!%1!%!EWicW2G!&L!E!G<WgcWGa!&P1!3!3HHWecWG!+!.WG!3!3HHRWWccW!=C7PH<<7Ha!&!@!.cW!2!3!277H<7"
DATA "HR!&!@!.!&W!2!Gd7*!(-Ha!&!+!2^WX!5!.^Hd7HRP!+!2PW !@!G!&^fWP!@!2!+W cCG!+W ccWb!@!.!&W ccdWecWaC!+W "
DATA "kcW!2!&W ec"
DATA ""
DATA 16, 16
DATA "~ctceWmcCGCGjWdcfW!2dC!)d!EWdcWW-WaWWP!@!)!E!EHWcW!2!@fH!%!=dLHHWW!2G7-7-7!5!G!5!5XH<WWa!27-7-7-7-7-"
DATA "7HWcWW<!(<!(<!(<!(<!(<RWcW!3!3aGG!+GaaGaaW WG!5:F:!5!2!5!2F:F!2CWW!+CCW!2CWC!.WCW!.!&WcoW qc"
DATA ""
DATA 16, 16
DATA "~ctceWmcCGCGjWdcfW!2dC!)d!EWdcWW-WaWWP!@!)!E!EHWcW!5!&fH!%!=dLHHWW!5a7-7-7!5!G!5!5XH<WW!3!.7-7-7-7-7"
DATA "-7HWcWW<!(<!(<!(<!(<!(<RWcW!+!+Gaa!3aGG!+GGW WbF!2!5!2F:F:!5!2!5:aWW!3WWC!.WCW!2CWC!2PWcoW qc"
DATA ""
DATA 16, 16
DATA "ccW!%!2WncW!3!2WncW!3!2WncW^WmcW!2!3!&WkcdW!2!3!&WWhcW-77!(!&^!%!)W dcdW7--Le1!EW ccC!5!GH7-b!)!%1!="
DATA "!3!+W c!@C!5!3H7-!=GGCCH!.WcW!&!5!2!GH7-f7H!.WcW!&X!2!+H77fH!2!2ccW!@!5!5aH<HHW!.!&!2!&dcW!@C!5!3dW "
DATA "Wd!&ecW!@!3!&We dWfcWC!+W gc"
DATA ""
DATA 16, 16
DATA "ccW!%!2WncW!3!2WncW!3!2WncW^WmcW!2!3!&WkcdW!2!3!&WWhcW-77!(!&^!%!)W dcdW7--Le1!EW cc!5!2aH7-b!)!%1!="
DATA "!3CW c!&X!2!+H7-!=GGCCH!2WcW!@C!5!+H7-f7H!&WcW!@C!5!3H77fHP!.ccW!&C!2!GH<HHW!&PWPdcW!&!5!2!+dW W!.P^"
DATA "ecW!&!+PWe dWfcWW^W gc"
DATA ""
DATA 16, 16
DATA "gcWG!+WmcWGaWmcWGaWmcWC!.WlcWWGaWWicWW77G!+H<WWcc ccW!&W--Wa<!(W!.Wc cW!5!&!27e1<!+!.!&W cWF!@F7!%1"
DATA "!%1H!+FPW cW!2!&!.7d!@!GHP!.^W cWX!&P7-7-7-^P!&W cW!5!G!2g!(!+!2!&W cWF!@Fd!(<!(!(!+'PW cW:!GCgW:!G"
DATA "!+W cW!@C!+W c  W!3!2!&W ccdWgcdWcc"
DATA ""
DATA 16, 16
DATA "gcWG!+WmcWGaWmcWGaWmcWC!.WlcWWGaWWicWW77G!+H<WWcc ccW!@W--Wa<!(W!2Wc cW!2!@!.7e1<P!2PW cWX!&P7!%1!%1"
DATA "H^P!&W cW!5!G!27d!@!GH!+!2!+W cWF!@F7-7-7-!+'PW cW!2!&!.g!(P!.PW cWX!&Pd!(<!(!(^P!&W cW!&!+hW!&!+^W "
DATA "cW!&W^W c  W!+!.PW ccdWgcdWcc"
DATA ""
DATA 16, 16
DATA "kcWG!GWlcWG!+WlcWGaWmcW!+WmcWC!&WWjcWWG!&eWhcW!EL!+PH<<7WfcW!Ee1G<7HdWccW!2b211TCHHa!&!@PcW!27!5XX!5"
DATA "CHHR!&!@!.!&W!27-?--?!(Ha!&!+!2^WX!@f7-HRP!+!2PW !@!Ga^WH-!(aP!@!2!+W cCG!+W dWX:!.!&W ccdWecW!3C!+W"
DATA " kcWC!2W ec"
DATA ""
DATA 16, 16
DATA "kcWG!GWlcWG!+WlcWGaWmcW!+WmcWC!&WWjcWWG!&eWhcW!EL!+PH<<7WfcW!Ee1G<7HdWccW!5b211TCHHaP!+!2cW!57!5XX!5"
DATA "CHHRP!+!2^W!57-?--?!(HaP!@!.!+W!2'f7-HR!&!G!.!&W !&!+^!+WH-!(a!&!+!.^W caa^W dWG!2!2^W ccdWecWaa^W "
DATA "kcWWPW ec"
DATA ""
DATA 16, 16
DATA "~c~ccceWdcjWCGCGccW!E!E!)LeC!2eWcW=!E!E!)1!.WaWa/WWcWN/dL!=GfZ!@aWWN=!5X!5!@!2=/,/,!+PWWm/!+^WW/=Z=Z"
DATA "=Z=Z=Z=WW cWa!+GaaGaGG!+G!3W WbF!2!5!2F:F:!5!2!5:aWW!3WWC!.WCW!2CWC!2PWcoW qc"
DATA ""
DATA 16, 16
DATA "~c~ccceWdcjWCGCGccW!E!E!)LeC!2eWcW=!E!E!)1!.WaWa/WWcWN/dL!=GfZ!&GWWN=!5X!5!@!2=/,/,^'WWm/^!+WW/=Z=Z="
DATA "Z=Z=Z=WW cWGGaGG!+Gaa!3a!+W WG!5:F:!5!2!5!2F:F!2CWW!+CCW!2CWC!.WCW!.!&WcoW qc"
DATA ""
DATA 16, 16
DATA "~cxcfWkcW;1!%1!%WicW/;!E!%!3!&1WgcW,N1;!@!2G!3!2WecWWZ/N1CF!2a!3!2Wdc!5!2aZ,N9//!.=!=!5W c!&!5!2!+Z,"
DATA "Nd/Z!G!2GW W!@C!5aZ,N/=AZAP!2WcW!@C!5!3Z!DeZ!+!@!&PccW!&C!2!GfW!&!+WPdcW!&X!2!+Wd W!+P^ecW!&!+!&W dc"
DATA "dWfcWW^W gc"
DATA ""
DATA 16, 16
DATA "~cxcfWkcW;1!%1!%WicW/;!E!%!3!&1WgcW,N1;!@!2G!3!2WecWWZ/N1CF!2a!3!2WdcC!5!GZ,N9//!.=!=!5W c!@C!5!3Z,N"
DATA "d/Z!G!2aW W!&!5!2!GZ,N/=AZA'!.WcW!&X!2!+Z!DeZ^!&!@!&ccW!@!5!5afW!.!G!2!&dcW!@C!5!3Wd W!3!&!&ecW!@!3P"
DATA "W dcdWfcWC!+W gc"
DATA ""
DATA 16, 16
DATA "~czceWlcW!%1!%1WicWW/!)G!+LZWWfcW!GW,:Ga!3AW'WdcW!2!G!.//GaZZP!2PWccWX!&P/,G!+Z=^P!&W cW!5!G!2//!5!2"
DATA "ZZ!+!2!+W cWF!@F/,/,/=!+FPW cW!2!&!.g=P!.PW cWX!&hW!&W!&W cW!&!+WW c  W!&!.^W cW!&W^W dcW!+!.PW ccdW"
DATA "gcdWc "
DATA ""
DATA 16, 16
DATA "~czceWlcW!%1!%1WicWW/!)G!+LZWWfcW!&W,:Ga!3AWPWdcW!5!&!2//GaZZ!+!.!&WccWF!@F/,G!+Z=!+'PW cW!2!&!.//!5"
DATA "!2ZZP!.^W cWX!&P/,/,/=^P!&W cW!5!@!2g=!+!2!&W cWF!@!2gW!GFPW cW:!GCW c  W:!2!+W cW!@C!+W dcW!3!2!&W "
DATA "ccdWgcdWc "
DATA ""
DATA 16, 16
DATA "~c{cfWkcW1!%1!%!EWicW2G!&L!E!GZWgcWGa!&P1!3!3ZZWecWG!+!.WG!3!3ZZ!8WWccW!=C/PZ=Z/ZZP!+!2cW!5!3!2/=ZZ/"
DATA "Z!8P!+!2^W!5!&,//N,,Z!8P!@!.!+Wd!2aZ/,/Z!8!&!G!.!&W !&!+P!+fW!&!+!.^W caa^W ccWG!2!2^W ccdWecW!3a^W "
DATA "kcWWPW ec"
DATA ""
DATA 16, 16
DATA "~c{cfWkcW1!%1!%!EWicW2G!&L!E!GZWgcWGa!&P1!3!3ZZWecWG!+!.WG!3!3ZZ!8WWccW!=C/PZ=Z/ZZ!&!@!.cW!2!3!2/=ZZ"
DATA "/Z!8!&!@!.!&W!2!G,//N,,Z!8!&!+!2^WX!5!.^Z/,/Z!8P!+!2PW !@!G!&^fWP!@!2!+W cCG!+W ccWb!@!.!&W ccdWecWa"
DATA "C!+W kcW!2!&W ec"
DATA ""
DATA 16, 16
DATA "~ctceWmcCGCGjWdcfW!2dC!)d!EWdcWW/WaWWP!@!)!E!EZWcW!2!@fZ!%!=dLZZWW!2G/,/,/!5!G!5!5XZZWWa!2l/ZWcWWZ=Z"
DATA "=Z=Z=Z=ZAWcW!3!3aGG!+GaaGaaW WG!5:F:!5!2!5!2F:F!2CWW!+CCW!2CWC!.WCW!.!&WcoW qc"
DATA ""
DATA 16, 16
DATA "~ctceWmcCGCGjWdcfW!2dC!)d!EWdcWW/WaWWP!@!)!E!EZWcW!5!&fZ!%!=dLZZWW!5a/,/,/!5!G!5!5XZZWW!3!.l/ZWcWWZ="
DATA "Z=Z=Z=Z=ZAWcW!+!+Gaa!3aGG!+GGW WbF!2!5!2F:F:!5!2!5:aWW!3WWC!.WCW!2CWC!2PWcoW qc"
DATA ""
DATA 16, 16
DATA "ccW!%!2WncW!3!2WncW!3!2WncW^WmcW!2!3!&WkcdW!2!3!&WWhcWNd/!&^!%!)W dcdW/N/Le1!EW ccC!5!GZ,Nb!)!%1!=!3"
DATA "!+W c!@C!5!3Z,N!=GGCCZ!.WcW!&!5!2!GZ,N=/9/!DZ!.WcW!&X!2!+Z/,fZ!2!2ccW!@!5!5aZ=ZAW!.!&!2!&dcW!@C!5!3"
DATA "dW Wd!&ecW!@!3!&We dWfcWC!+W gc"
DATA ""
DATA 16, 16
DATA "ccW!%!2WncW!3!2WncW!3!2WncW^WmcW!2!3!&WkcdW!2!3!&WWhcWNd/!&^!%!)W dcdW/N/Le1!EW cc!5!2aZ,Nb!)!%1!=!3"
DATA "CW c!&X!2!+Z,N!=GGCCZ!2WcW!@C!5!+Z,N=/9/!DZ!&WcW!@C!5!3Z/,fZP!.ccW!&C!2!GZ=ZAW!&PWPdcW!&!5!2!+dW W!."
DATA "P^ecW!&!+PWe dWfcWW^W gc"
DATA ""
DATA 16, 16
DATA "gcWG!+WmcWGaWmcWGaWmcWC!.WlcWWGaWWicWW//G!+ZZWWcc ccW!&W/,WaZ=W!.Wc cW!5!&!2/e1Z!+!.!&W cWF!@F,!%1!%"
DATA "1A!+FPW cW!2!&!./d!@!GZP!.^W cWX!&P/=/=/,^P!&W cW!5!G!2g=!+!2!&W cWF!@F=U=U=U!+'PW cW:!GCgW:!G!+W cW"
DATA "!@C!+W c  W!3!2!&W ccdWgcdWcc"
DATA ""
DATA 16, 16
DATA "gcWG!+WmcWGaWmcWGaWmcWC!.WlcWWGaWWicWW//G!+ZZWWcc ccW!@W/,WaZ=W!2Wc cW!2!@!./e1ZP!2PW cWX!&P,!%1!%1A"
DATA "^P!&W cW!5!G!2/d!@!GZ!+!2!+W cWF!@F/=/=/,!+'PW cW!2!&!.g=P!.PW cWX!&P=U=U=U^P!&W cW!&!+hW!&!+^W cW!&"
DATA "W^W c  W!+!.PW ccdWgcdWcc"
DATA ""
DATA 16, 16
DATA "kcWG!GWlcWG!+WlcWGaWmcW!+WmcWC!&WWjcWWG!&eWhcW!EL!+PdZ/WfcW!Ee1GZ/ZdWccW!2b211TC=ZZ!&!@PcW!2/!5XX!5C"
DATA "ZZ!8!&!@!.!&W!29NN/=N,ZZ!&!+!2^WX!@//,d/Z!8P!+!2PW !@!Ga^WZ/=ZP!@!2!+W cCG!+W dWX:!.!&W ccdWecW!3C!+"
DATA "W kcWC!2W ec"
DATA ""
DATA 16, 16
DATA "kcWG!GWlcWG!+WlcWGaWmcW!+WmcWC!&WWjcWWG!&eWhcW!EL!+PdZ/WfcW!Ee1GZ/ZdWccW!5b211TC=ZZP!+!2cW!5/!5XX!5C"
DATA "ZZ!8P!+!2^W!59NN/=N,ZZP!@!.!+W!2'//,d/Z!8!&!G!.!&W !&!+^!+WZ/=Z!&!+!.^W caa^W dWG!2!2^W ccdWecWaa^W "
DATA "kcWWPW ec"
DATA ""

MapData:
' a row to a string; tools/rustline_art.py writes these
DATA "...............:~~~:.T..,.....::.:::..,,.....,..............,..."
DATA "...............T:~~~T............:::.......,...................."
DATA ":.......,,........~~~..,,.,.....T..:....,,......................"
DATA ":....,,,.,.........~~~..,,,..,.....:......,....................."
DATA ":......,,,.........,~~~.T,,,..,.........,.m....................."
DATA "....,,.,...,.....,...~~~..,,.,..,.......mmm....................."
DATA "......,,,.,........,,.~~~T,.............mmm....................."
DATA ".......................~T~,.,,,....:....mmm....................."
DATA "...........mm.m........~~~,...,.........,m........,............."
DATA ",,.........mmmm.........,..,,......:....,........,,,............"
DATA ",....::...mmmmm..........,,,.......:............,..,............"
DATA ",,...:.....mmmm.....:.....,.............,.........,,............"
DATA ",...........mm............~~~.........T.......m.mm.....::......."
DATA "............,..............~~T.....,......T...mmmmm....#.#......"
DATA "........,,....,..........::~~~....,...,.##.....mmmm:.:.###......"
DATA "........,..,...,.......:..:~~~...,,,,.,##......mmmm..:.####.,,.,"
DATA "...T.....,....,..........:##T##...,,,.,.#......mmmm..::##.#,,,,."
DATA ".........................######.T..,,.............:::........,.."
DATA "...................::....T#.#~~T................T::::..........."
DATA "..................:.:....,...~~~..................:...T........."
DATA "........:.:.:.....:::....,,,.~~~..::.....,....:::.::.....##,.,.."
DATA "........:T.....::.:::...,,,..~m~m.......................#,#,.,.."
DATA ".......:::::::.::..::...,,,,.mmmmm..............:::....,,,,,,..."
DATA "........:::::.::.::.:...,...mmmmmmm...............:.....,.,,,,.."
DATA ".......:::....:.:::.......,,.mmmmmm,,..............T.,,..,..,,.."
DATA ",....:..:.::::.:.:......,,,,m.mmmmm..................,....#,,..."
DATA ".,....#....:.:.::::..........mmmm.,,.,.......T....T.....,##.,.,,"
DATA ",.....##.....:::.::.T.........~m~T,,,,.....T.........,,,####,,,,"
DATA "......###.....:.:.:::..........~~~,........T.........,..###,.,,,"
DATA ".............m::T:::::.........~T~,,,.,.,............,,,...,,,,."
DATA "........#,,...mmmm..::..........~~~,.,.,,.,.......,...,,,T.,,.,,"
DATA "......###.....mmmm::.T........TT~~T,.,,............T,,.......,.,"
DATA "......###....mmmmm...............~~~....##........,,............"
DATA "..............mm.m.......,,,......~~~....#........,............."
DATA "....................,.,,,,.,.,....~~~TT..#......,,.............."
DATA "..................,,.,.,,,,.,.,...:~~~............,mmmm::......."
DATA "...................,,,..,,.,,......::.......:......mmmm........."
DATA "......,...............,.....,......:.::...::::...,mmmmm........."
DATA ".....,.............................:.:..::...:....mmmmm........."
DATA ".......,,..........................::..:.:::::....mmmmm........."
DATA "......,.,,.....:........:::.......mm:.T:~~~:.T.................."
DATA ".....,.,,...............::...##.mmmm.....~~~...................."
DATA "....................,....:..###..mmm.....T~~~..................."
DATA "..........................:.####.mmmm.T..T.~~~...,,..........,.."
DATA "..................,..,...::::##..m.m..,.,,,,~~~...,..........,,,"
DATA ".........................::::.:.....,,.,...,.~~~,,,........,,,.."
DATA "....................,.................,,,,,...~~~,T............."
DATA "......................,..............,.,,,.,.T.~~~.............."

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
DATA "T140MNW2Y1,700,5H0,8,9,5V13", "T140MLW2Y1,2600,3H1,6,11,5V11", "T140MNW0Q30H0,4,6,3V6", "T140MNY0"
DATA "O1A8A16A16", "P1", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "", "P8A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O2C8O1G8", "", "P8O3A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1A8A16A16", "P1", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "", "P8A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O2C8O1G8", "", "P8O3A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1F8F16F16", "P1", "P8O3F8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16F16F8", "", "P8A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "F8F16F16", "", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8O2C8", "", "P8O3A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1G8G16G16", "P2.", "P8O3G8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16G16G8", "", "P8B8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "G8G16G16", "", "P8O4D8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "B8O2D8", "O3E8G8", "P8O3B8", "W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16"
DATA "O1A8A16A16", "O3A4.", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "O4C8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "E4", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O2C8O1G8", "D8C8", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1A8A16A16", "O3B4", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "A8G8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "A2", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O2C8O1G8", "", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1F8F16F16", "O3A4.", "P8O3F8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16F16F8", "O4C8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "F8F16F16", "F4", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "A8O2C8", "E8C8", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1G8G16G16", "O4D4", "P8O3G8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16G16G8", "O3B8G8", "P8B8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "G8G16G16", "B2", "P8O4D8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "B8O2D8", "", "P8O3B8", "W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16"
DATA "O1A8A16A16", "O3A4.", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "O4C8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "E4", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O2C8O1G8", "D8C8", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1A8A16A16", "O4E4", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "G8E8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "A2", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O2C8O1G8", "", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1F8F16F16", "O4A8G8", "P8O3F8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16F16F8", "F8E8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "F8F16F16", "F4", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "A8O2C8", "C4", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1E8E16E16", "O4E2.", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16E16E8", "", "P8G#8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "E8E16E16", "", "P8B8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "G8B8", "O3B4", "P8G#8", "W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16"
DATA "O2C8C16C16", "O4G4", "P8O3G8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16C16C8", "E8G8", "P8O4C8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "C8C16C16", "O5C4", "P8E8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O1G8B8", "O4G4", "P8C8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1G8G16G16", "O4B4", "P8O3G8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16G16G8", "G8B8", "P8B8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "G8G16G16", "O5D4", "P8O4D8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "B8O2D8", "O4B4", "P8O3B8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O2D8D16D16", "O5D8C8", "P8O3F8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16D16D8", "O4A8F8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "D8D16D16", "A4", "P8O4D8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O1A8O2C8", "O5D4", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1A8A16A16", "O5C4", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "O4A8E8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "A2", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O2C8O1G8", "", "P8O3A8", "W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16"
DATA "O2C8C16C16", "O4G4", "P8O3G8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16C16C8", "E8G8", "P8O4C8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "C8C16C16", "O5C4", "P8E8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O1G8B8", "E4", "P8C8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1G8G16G16", "O5D4", "P8O3G8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16G16G8", "O4B8G8", "P8B8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "G8G16G16", "O5D2", "P8O4D8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "B8O2D8", "", "P8O3B8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1F8F16F16", "O5C8O4A8", "P8O3F8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16F16F8", "F8A8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "F8F16F16", "O5C4", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "A8O2C8", "O4A4", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1E8E16E16", "O4G#4", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16E16E8", "B4", "P8G#8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "E8E16E16", "O5E2", "P8B8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "G8B8", "", "P8G#8", "W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16"
DATA "O1A8A16A16", "O3A4.", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "O4C8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "E4", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O2C8O1G8", "D8C8", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1F8F16F16", "O3A4.", "P8O3F8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16F16F8", "O4C8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "F8F16F16", "F4", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "A8O2C8", "E8C8", "P8O3A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1G8G16G16", "O4D4", "P8O3G8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16G16G8", "O3B8O4D8", "P8B8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "G8G16G16", "G4", "P8O4D8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "B8O2D8", "F8D8", "P8O3B8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1A8A16A16", "O4E8D8", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "C8O3B8", "P8A8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "A2", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "O2C8O1G8", "", "P8O3A8", "W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16"
DATA "O1A8A16A16", "P1", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "", "P8A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O2C8O1G8", "", "P8O3A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1A8A16A16", "P2", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16A16A8", "", "P8A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8A16A16", "O4E8P8", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O2C8O1G8", "E8P8", "P8O3A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1F8F16F16", "P1", "P8O3F8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16F16F8", "", "P8A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "F8F16F16", "", "P8O4C8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "A8O2C8", "", "P8O3A8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "O1E8E16E16", "P2", "P8O3E8", "W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16"
DATA "P16E16E8", "", "P8G#8", "W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16"
DATA "E8E16E16", "O3B8P8", "P8B8", "W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16"
DATA "G8B8", "O4E8G#8", "P8G#8", "W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16"
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
            IF CountB%(bl(), 1, BPOW) = 0 OR tm(1).low THEN
                want = 0
            ELSEIF CountB%(bl(), 1, BREF) = 0 THEN
                want = 1
            ELSEIF CountB%(bl(), 1, BBAR) = 0 THEN
                want = 2
            ELSEIF CountB%(bl(), 1, BFAC) = 0 AND g.now > 50 THEN
                want = 3
            ELSEIF CountB%(bl(), 1, BTUR) < 1 + g.ai.wave \ 2 AND CountB%(bl(), 1, BTUR) < 4 THEN
                want = 4
            END IF
            IF want >= 0 THEN
                IF tm(1).cash >= KCost%(want) THEN
                    tm(1).cash = tm(1).cash - KCost%(want): tm(1).sKind = want: tm(1).sProg = 0
                END IF
            END IF
        ELSEIF tm(1).sReady THEN
            AIPlace g, tm(), m(), u(), bl(), fk()
        END IF
    END IF
    ' its lines: a second harvester; riflemen and rocketeers; buggies, then tanks and guns
    IF tm(1).l0Kind < 0 AND CountU%(u(), 1, UHARV) + Queued%(tm(1), 5) < 2 THEN AIBuy tm(), bl(), 5
    IF tm(1).l1Kind < 0 AND Queued%(tm(1), 6) + Queued%(tm(1), 7) = 0 THEN
        k = 6: IF RND < .35 THEN k = 7
        AIBuy tm(), bl(), k
    END IF
    IF tm(1).l2Kind < 0 AND Queued%(tm(1), 8) + Queued%(tm(1), 9) + Queued%(tm(1), 10) = 0 THEN
        d = RND
        k = 8: IF d < .4 + g.ai.wave * .05 THEN k = 9
        IF d > .85 AND g.ai.wave >= 2 THEN k = 10
        AIBuy tm(), bl(), k
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
                    d = (bl(b).x - 57) ^ 2 + (bl(b).y - 4) ^ 2
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
                g.ai.nextWave = g.now + 45
                Say g, "The Scorch Legion is on the march!"
                Sfx g, 8
            END IF
        END IF
    END IF
END SUB

' Whether side ti has what item k needs.
FUNCTION Allowed% (bl() AS BldgT, ti AS INTEGER, k AS INTEGER)
    Allowed% = NeedB%(bl(), ti, k) = 0
END FUNCTION

' The building side ti lacks for item k, 0 none: a building needs the HQ, and the tech before it.
FUNCTION NeedB% (bl() AS BldgT, ti AS INTEGER, k AS INTEGER)
    DIM r1 AS INTEGER, r2 AS INTEGER
    NeedB% = 0
    SELECT CASE k
    CASE 0: r1 = BHQ
    CASE 1, 2: r1 = BPOW
    CASE 3: r1 = BREF: r2 = BBAR
    CASE 4: r1 = BBAR
    CASE 5: r1 = BREF
    CASE 6, 7: r1 = BBAR
    CASE ELSE: r1 = BFAC
    END SELECT
    IF k < NBITEM AND FindB%(bl(), ti, BHQ) = 0 THEN
        NeedB% = BHQ
    ELSEIF FindB%(bl(), ti, r1) = 0 THEN
        NeedB% = r1
    ELSEIF r2 > 0 THEN
        IF FindB%(bl(), ti, r2) = 0 THEN NeedB% = r2
    END IF
END FUNCTION

FUNCTION BName$ (bt AS INTEGER)
    SELECT CASE bt
    CASE BHQ: BName$ = "HQ"
    CASE BREF: BName$ = "Refinery"
    CASE BBAR: BName$ = "Barracks"
    CASE BTUR: BName$ = "Turret"
    CASE BPOW: BName$ = "Power Plant"
    CASE ELSE: BName$ = "Factory"
    END SELECT
END FUNCTION

' The power a building makes, and uses.
FUNCTION BGives% (bt AS INTEGER)
    BGives% = 0
    IF bt = BHQ THEN BGives% = 50
    IF bt = BPOW THEN BGives% = 100
END FUNCTION

FUNCTION BUses% (bt AS INTEGER)
    SELECT CASE bt
    CASE BREF: BUses% = 30
    CASE BBAR: BUses% = 20
    CASE BFAC: BUses% = 40
    CASE BTUR: BUses% = 25
    CASE ELSE: BUses% = 0
    END SELECT
END FUNCTION

' The line a unit's item is made on: 0 the refinery's, 1 the barracks', 2 the factory's.
FUNCTION KLine% (k AS INTEGER)
    SELECT CASE k
    CASE 5: KLine% = 0
    CASE 6, 7: KLine% = 1
    CASE 8, 9, 10: KLine% = 2
    CASE ELSE: KLine% = -1
    END SELECT
END FUNCTION

FUNCTION UItem% (ut AS INTEGER)
    UItem% = VAL(MID$("050609070810", ut * 2 - 1, 2))
END FUNCTION

' A unit's class as a target: 0 infantry, 1 a vehicle (2 is a building).
FUNCTION UClass% (ut AS INTEGER)
    IF ut = URIFLE OR ut = UROCKET THEN UClass% = 0 ELSE UClass% = 1
END FUNCTION

' The percent of its damage a unit of kind ut does to a target of class c.
FUNCTION DmgVs% (ut AS INTEGER, c AS INTEGER)
    DIM t AS STRING
    SELECT CASE ut
    CASE URIFLE: t = "100040030"
    CASE UROCKET: t = "030130100"
    CASE UBUGGY: t = "110050030"
    CASE UTANK: t = "050100090"
    CASE ELSE: t = "090060150"
    END SELECT
    DmgVs% = VAL(MID$(t, c * 3 + 1, 3))
END FUNCTION

' How far, in tiles, a unit's shot bursts.
FUNCTION Splash! (ut AS INTEGER)
    Splash! = 0
    IF ut = UTANK THEN Splash! = .9
    IF ut = UARTY THEN Splash! = 1.6
END FUNCTION

' The sidebar's item at the screen's x, y, -1 none.
FUNCTION ItemAt% (x AS INTEGER, y AS INTEGER)
    DIM c AS INTEGER, r AS INTEGER
    ItemAt% = -1
    IF x <= SBX OR y < BTNY THEN EXIT FUNCTION
    c = (x - SBX - 1) \ CELLW: r = (y - BTNY) \ CELLH
    IF c <= 2 AND r * 3 + c < NITEM THEN ItemAt% = r * 3 + c
END FUNCTION

FUNCTION LineKind% (t AS TeamT, l AS INTEGER)
    SELECT CASE l
    CASE 0: LineKind% = t.l0Kind
    CASE 1: LineKind% = t.l1Kind
    CASE ELSE: LineKind% = t.l2Kind
    END SELECT
END FUNCTION

FUNCTION LineProg! (t AS TeamT, l AS INTEGER)
    SELECT CASE l
    CASE 0: LineProg! = t.l0Prog
    CASE 1: LineProg! = t.l1Prog
    CASE ELSE: LineProg! = t.l2Prog
    END SELECT
END FUNCTION

SUB LineClear (t AS TeamT, l AS INTEGER)
    SELECT CASE l
    CASE 0: t.l0Kind = -1: t.l0Prog = 0
    CASE 1: t.l1Kind = -1: t.l1Prog = 0
    CASE ELSE: t.l2Kind = -1: t.l2Prog = 0
    END SELECT
END SUB

' The next item waiting for line l, taken off its queue; -1 none.
FUNCTION NextQ% (t AS TeamT, l AS INTEGER)
    DIM k AS INTEGER
    NextQ% = -1
    FOR k = NBITEM TO NITEM - 1
        IF KLine%(k) = l AND Queued%(t, k) > 0 THEN Enqueue t, k, -1: NextQ% = k: EXIT FUNCTION
    NEXT
END FUNCTION

' The Legion queues item k when it may and can pay.
SUB AIBuy (tm() AS TeamT, bl() AS BldgT, k AS INTEGER)
    IF Allowed%(bl(), 1, k) = 0 OR tm(1).cash < KCost%(k) THEN EXIT SUB
    tm(1).cash = tm(1).cash - KCost%(k): Enqueue tm(1), k, 1
END SUB

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
    CASE BPOW: BHp% = 350
    CASE BFAC: BHp% = 550
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
    total = 12 + NOVL + 16 + 1 + 2 + 4 + NBLD * 2 + NSPR \ 4
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
        GET (100, 0)-(115, 7 + STEPH), art(0, AFACE + k * 6)
        GET (120, 0)-(135, 7 + STEPH), art(0, AFACE + k * 6 + 2)
        GET (140, 0)-(155, 7 + STEPH), art(0, AFACE + k * 6 + 4)
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
    ' the ramps: image, mask, fog
    FOR k = 0 TO 3
        Unpack 100, 140, 180, 0
        MaskFog 140, 180, 32, 16 + STEPH
        GET (100, 0)-(131, 15 + STEPH), art(0, ARMP + k * 12)
        GET (140, 0)-(171, 15 + STEPH), art(0, ARMP + k * 12 + 4)
        GET (180, 0)-(211, 15 + STEPH), art(0, ARMP + k * 12 + 8)
        n = n + 1: Progress n, total, gl()
    NEXT

    ' the buildings: image, mask, fog
    FOR bt = 1 TO NBLD: FOR tmi = 0 TO 1
        sz = BSize%(bt) * 32: q = BldH%(bt) + sz \ 2
        Unpack 100, 170, 240, 0
        k = ABLD + ((bt - 1) * 2 + tmi) * 3 * BLDW
        GET (100, 0)-(100 + sz - 1, q - 1), art(0, k)
        GET (170, 0)-(170 + sz - 1, q - 1), art(0, k + BLDW)
        ' the sidebar's icons: the Coalition's buildings but its HQ, sampled small
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
    FOR k = 0 TO NSPR - 1
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
    DIM l AS INTEGER
    l = KLine%(k)
    IF rightButton THEN
        IF k < NBITEM THEN
            IF tm(0).sKind = k THEN
                tm(0).cash = tm(0).cash + KCost%(k): tm(0).sKind = -1: tm(0).sReady = 0: g.placeKind = -1
                Say g, "Cancelled"
            END IF
        ELSEIF Queued%(tm(0), k) > 0 THEN
            Enqueue tm(0), k, -1: tm(0).cash = tm(0).cash + KCost%(k): Say g, "Cancelled"
        ELSEIF LineKind%(tm(0), l) = k THEN
            LineClear tm(0), l: tm(0).cash = tm(0).cash + KCost%(k): Say g, "Cancelled"
        END IF
        Sfx g, 6
        EXIT SUB
    END IF
    IF Allowed%(bl(), 0, k) = 0 THEN
        Say g, "The " + KName$(k) + " needs a " + BName$(NeedB%(bl(), 0, k))
        Sfx g, 7
        EXIT SUB
    END IF
    IF k < NBITEM THEN
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
        Say g, "Ordered: " + KName$(k): Sfx g, 6
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
            IF m(i, j).hgt <> m(x, y).hgt OR m(i, j).rmp THEN EXIT FUNCTION
            IF m(i, j).bld OR m(i, j).occ THEN EXIT FUNCTION
            IF ti = 0 AND m(i, j).seen = 0 THEN EXIT FUNCTION
        NEXT
    NEXT
    IF bt = BREF THEN
        IF Passable%(m(), x, y + 2) = 0 THEN EXIT FUNCTION
        IF m(x, y + 2).hgt <> m(x, y).hgt OR m(x, y + 2).rmp THEN EXIT FUNCTION
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
    ' by half levels: a ramp's middle stands a half above its foot
    FOR h = 0 TO MAXH * 2
        ScreenTile g.vw, sx, sy + h * STEPH \ 2, x, y
        IF x >= 0 AND y >= 0 AND x < MAPW AND y < MAPH THEN
            IF m(x, y).hgt * 2 + SGN(m(x, y).rmp) = h AND x + y > front THEN front = x + y: wx = x: wy = y
        END IF
    NEXT
END SUB

' Where unit i's feet are on the screen: raised as the land under it, up or down a ramp as it goes.
SUB UnitScreen (g AS GameT, m() AS CellT, u() AS UnitT, i AS INTEGER, sx AS INTEGER, sy AS INTEGER)
    sx = u(i).px - u(i).py + MAPH * 16 - g.vw.camX + VX0
    sy = (u(i).px + u(i).py) / 2 - g.vw.camY + VY0 - LiftAt!(m(), u(i).px, u(i).py)
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
            IF ItemAt%(g.ms.lX, g.ms.lY) >= 0 THEN ButtonClick g, tm(), bl(), ItemAt%(g.ms.lX, g.ms.lY), 0
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
    IF g.ms.down AND g.ms.x >= MINIX AND g.ms.x < MINIX + MAPW AND g.ms.y >= MINIY AND g.ms.y < MINIY + MAPH THEN
        CenterOn g.vw, g.ms.x - MINIX, g.ms.y - MINIY
    END IF
    IF g.ms.dragOn AND g.ms.down = 0 THEN
        g.ms.dragOn = 0
        SelectBox g, m(), u(), g.ms.dX0, g.ms.dY0, g.ms.x, g.ms.y
    END IF
    IF g.ms.rHit THEN
        IF g.placeKind >= 0 THEN
            g.placeKind = -1: Say g, "It waits in the sidebar"
        ELSEIF g.ms.rX >= SBX THEN
            IF ItemAt%(g.ms.rX, g.ms.rY) >= 0 THEN ButtonClick g, tm(), bl(), ItemAt%(g.ms.rX, g.ms.rY), -1
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
    Ramp LOGOC, 8, 63, 60, 50, 50, 16, 6
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
        IF g.won THEN PSET (x, y), 22 ELSE PSET (x, y), FIRE + 2 + INT(RND * 6)
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
            x = fx(i).x - fx(i).y + ox: y = (fx(i).x + fx(i).y) / 2 + oy - 4 - LiftAt!(m(), fx(i).x, fx(i).y)
            x2 = ABS(fx(i).x2) - fx(i).y2 + ox
            y2 = (ABS(fx(i).x2) + fx(i).y2) / 2 + oy - 4 - LiftAt!(m(), ABS(fx(i).x2), fx(i).y2)
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
                        c = 1: IF m(x, y).hgt >= 1 THEN c = 2
                        IF m(x, y).hgt >= 2 THEN c = 16
                    CASE TDIRT: c = 3
                    CASE TROCK: c = 5
                    CASE TWATER: c = 20
                    CASE TORE: c = 22
                    CASE ELSE: c = 27
                    END SELECT
                END IF
                PSET (MINIX + x, MINIY + y), c
            NEXT
        NEXT
        GET (MINIX, MINIY)-(MINIX + MAPW - 1, MINIY + MAPH - 1), art(0, AMINI)
        g.vw.miniDirty = 0: g.vw.miniT = 1
    ELSE
        PUT (MINIX, MINIY), art(0, AMINI), PSET
    END IF
    FOR b = 1 TO MAXB
        IF bl(b).alive THEN
            IF m(bl(b).x, bl(b).y).seen THEN
                s = BSize%(bl(b).kind)
                LINE (MINIX + bl(b).x, MINIY + bl(b).y)-STEP(s - 1, s - 1), 11 + bl(b).team * 2, BF
            END IF
        END IF
    NEXT
    FOR i = 1 TO MAXU
        IF u(i).alive THEN
            IF u(i).team = 0 OR m(u(i).x, u(i).y).vis THEN
                PSET (MINIX + u(i).x, MINIY + u(i).y), 15 - u(i).team * 2
            END IF
        END IF
    NEXT
    ScreenTile g.vw, VX0, VY0, x, y: ScreenTile g.vw, SBX - 1, VY0, i, b
    VIEW SCREEN (MINIX, MINIY)-(MINIX + MAPW - 1, MINIY + MAPH - 1)
    LINE (MINIX + x, MINIY + y)-(MINIX + i, MINIY + b), 15
    ScreenTile g.vw, SBX - 1, VIEWB - 1, x, y
    LINE -(MINIX + x, MINIY + y), 15
    ScreenTile g.vw, VX0, VIEWB - 1, i, b
    LINE -(MINIX + i, MINIY + b), 15
    ScreenTile g.vw, VX0, VY0, x, y
    LINE -(MINIX + x, MINIY + y), 15
    VIEW
END SUB

SUB DrawSidebar (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, art() AS INTEGER, gl() AS INTEGER)
    DIM k AS INTEGER, y AS INTEGER, s AS STRING, c AS INTEGER, i AS INTEGER, n AS INTEGER, one AS INTEGER
    DIM ok AS INTEGER, prog AS SINGLE, qn AS INTEGER, x AS INTEGER, hov AS INTEGER, inf AS INTEGER, veh AS INTEGER
    LINE (SBX, 0)-(319, 199), 0, BF
    LINE (0, VIEWH)-(SBX - 1, 199), 0, BF
    LINE (SBX, 0)-(SBX, VIEWH - 1), 23
    Txt gl(), "$", SBX + 3, 1, 1, 13
    s = LTRIM$(STR$(tm(0).cash)): Txt gl(), s, 319 - LEN(s) * 8, 1, 1, 15
    DrawMinimap g, m(), u(), bl(), art()
    LINE (SBX + 1, MINIY - 1)-(319, MINIY - 1), 23
    LINE (SBX + 1, MINIY + MAPH)-(319, MINIY + MAPH), 23
    hov = -1
    FOR k = 0 TO NITEM - 1
        x = SBX + 1 + (k MOD 3) * CELLW: y = BTNY + (k \ 3) * CELLH
        ok = Allowed%(bl(), 0, k)
        LINE (x, y)-(x + CELLW - 2, y + CELLH - 2), 4, B
        IF k < NBITEM THEN
            PUT (x + 4, y + 1), art(0, AICN + KMakes%(k) - 2), PSET
        ELSE
            LINE (x + 4, y + 1)-STEP(TS - 1, TS - 1), 2, BF
            Sprite art(), ((KMakes%(k) - 1) * 2) * 16, x + 4, y + 1
        END IF
        c = 13: IF tm(0).cash < KCost%(k) THEN c = 23
        s = LTRIM$(STR$(KCost%(k))): Txt gl(), s, x + 12 - LEN(s) * 4, y + 18, 1, c
        prog = -1
        IF k < NBITEM THEN
            IF tm(0).sKind = k THEN prog = tm(0).sProg
        ELSE
            IF LineKind%(tm(0), KLine%(k)) = k THEN prog = LineProg!(tm(0), KLine%(k))
            qn = Queued%(tm(0), k)
            IF qn > 0 THEN Txt gl(), LTRIM$(STR$(qn)), x + 17, y + 1, 1, 11
        END IF
        IF g.ms.x >= x AND g.ms.x < x + CELLW - 1 AND g.ms.y >= y AND g.ms.y < y + CELLH - 1 THEN
            hov = k: LINE (x, y)-(x + CELLW - 2, y + CELLH - 2), 15, B
        END IF
        IF prog >= 0 THEN
            IF k < NBITEM AND tm(0).sReady THEN
                ' ready to place: its frame flashes
                IF INT(g.clock * 3) MOD 2 = 0 OR g.placeKind = k THEN
                    LINE (x, y)-(x + CELLW - 2, y + CELLH - 2), 8, B
                    LINE (x + 1, y + 1)-(x + CELLW - 3, y + CELLH - 3), 8, B
                END IF
            ELSE
                Bar x + 2, y + 14, 20, prog, 11
            END IF
        END IF
        IF ok = 0 THEN Dither x + 1, y + 1, x + CELLW - 3, y + CELLH - 3, 0
    NEXT
    Txt gl(), Clock2$(g.now), SBX + 3, 182, 1, 23
    IF g.snd.enabled = 0 THEN Txt gl(), "MUTED", SBX + 40, 182, 1, 23

    ' the status line: a message, the item under the mouse, the selection, or the army and power
    FOR i = 1 TO MAXU
        IF u(i).sel THEN n = n + 1: one = i
        IF u(i).alive AND u(i).team = 0 AND u(i).kind <> UHARV THEN
            IF UClass%(u(i).kind) = 0 THEN inf = inf + 1 ELSE veh = veh + 1
        END IF
    NEXT
    IF g.msgT > 0 THEN
        Txt gl(), LEFT$(RTRIM$(g.msg), 39), 2, 192, 1, 15
    ELSEIF hov >= 0 THEN
        s = MID$("QWERTZXCVBN", hov + 1, 1) + " " + KName$(hov) + STR$(KCost%(hov))
        IF Allowed%(bl(), 0, hov) THEN
            Txt gl(), s, 2, 192, 1, 15
        ELSE
            Txt gl(), s + ": needs a " + BName$(NeedB%(bl(), 0, hov)), 2, 192, 1, 29
        END IF
    ELSEIF n = 1 THEN
        s = KName$(UItem%(u(one).kind)) + "  health" + STR$(u(one).hp) + " of" + STR$(UHp%(u(one).kind))
        IF u(one).kind = UHARV THEN s = s + "  ore" + STR$(u(one).load)
        Txt gl(), s, 2, 192, 1, 11
    ELSEIF n > 1 THEN
        Txt gl(), LTRIM$(STR$(n)) + " selected: right click orders", 2, 192, 1, 11
    ELSE
        s = "Infantry" + STR$(inf) + "  Vehicles" + STR$(veh) + "  Power" + STR$(tm(0).pu) + "/" + LTRIM$(STR$(tm(0).pw))
        c = 14: IF tm(0).low THEN c = 29
        Txt gl(), s, 2, 192, 1, c
    END IF
END SUB

SUB DrawTitleBack (art() AS INTEGER, gl() AS INTEGER)
    DIM y AS INTEGER, x AS INTEGER, i AS INTEGER, h AS INTEGER, c AS INTEGER, r AS INTEGER, l AS SINGLE
    ' dusk over the desert: deep blue down into the sun's orange
    Ramp SKY, 32, 2, 3, 14, 60, 30, 10
    Ramp LOGOC, 8, 63, 60, 50, 50, 16, 6
    Ramp PLANET, 16, 44, 12, 6, 63, 58, 34
    FOR y = 0 TO 199
        LINE (0, y)-(319, y), SKY + y * 32 \ 200
    NEXT
    FOR i = 1 TO 60
        PSET (RND * 319, RND * 60), 15 - (i MOD 3) * 9
    NEXT
    ' the sun going down behind the dunes, brightest at its heart
    FOR y = -34 TO 34
        h = SQR(1156 - y * y)
        FOR x = -h TO h
            l = 1 - (x * x + y * y) / 1156
            c = INT(l * 12 + 4): IF c > 15 THEN c = 15
            PSET (236 + x, 126 + y), PLANET + c
        NEXT
    NEXT
    ' the hills, far and near
    FOR x = 0 TO 319
        h = 136 + 10 * SIN(x / 35) + 5 * SIN(x / 12 + 1)
        LINE (x, h)-(x, 199), 3
        h = 158 + 8 * SIN(x / 25 + 2) + 3 * SIN(x / 9)
        LINE (x, h)-(x, 199), 1
    NEXT
    ' ore glowing on the near hill
    FOR i = 1 TO 160
        x = RND * 318: y = 168 + RND * 31
        PSET (x, y), 9: IF RND < .3 THEN PSET (x + 1, y), 22
    NEXT
    ' the Coalition's units on the hill, the Legion's beyond
    Sprite art(), 32 + 7 * 2, 116, 164
    Sprite art(), 32 + 7 * 2 + 1, 134, 170
    Sprite art(), 7 * 2, 98, 172
    Sprite art(), 4 * 16 + 7 * 2, 150, 176
    Sprite art(), 9 * 16 + 3 * 2, 244, 166
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
            IF x0 > -64 AND y0 > -64 AND x0 < SBX AND y0 < VIEWB THEN
                f = 0
                IF u(i).nx <> u(i).x OR u(i).ny <> u(i).y OR u(i).st = SGATHER THEN f = INT(u(i).anim * 8) MOD 2
                k = ((u(i).kind - 1) * 2 + u(i).team) * 16 + IsoDir%(u(i).dir) * 2 + f
                PUT (x0, y0), art(0, ASHD + k), OR
                Sprite art(), k, x0, y0
                IF u(i).kind = UHARV AND u(i).load > 0 THEN
                    PSET (sx, sy - 5), 22: PSET (sx + 1, sy - 4), 22
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
            IF x0 > -64 AND y0 > -64 AND x0 < SBX AND y0 < VIEWB THEN
                k = ABLD + ((bl(b).kind - 1) * 2 + bl(b).team) * 3 * BLDW
                PUT (x0, y0), art(0, k + BLDW), AND
                PUT (x0, y0), art(0, k), OR
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
            IF x0 > -64 AND y0 > -64 AND x0 < SBX AND y0 < VIEWB THEN
                PUT (x0, y0), art(0, ATREE + 6), OR
                PUT (x0, y0), art(0, ATREE + 2), AND
                PUT (x0, y0), art(0, ATREE), OR
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
    Shade 1, 34, 25, 14       ' sand, dark
    Shade 2, 42, 32, 19     ' sand
    Shade 3, 38, 26, 15     ' hard earth
    Shade 4, 12, 10, 12      ' rock's shadow, outlines
    Shade 5, 40, 29, 19     ' sandstone
    Shade 6, 5, 11, 30      ' deep water
    Shade 7, 20, 32, 54     ' light on water
    Shade 8, 24, 63, 44     ' the display's green
    Shade 9, 40, 10, 8      ' ore, dark
    Shade 10, 10, 28, 58     ' the Coalition
    Shade 11, 30, 48, 63
    Shade 12, 58, 24, 2     ' the Legion
    Shade 13, 63, 54, 16
    Shade 14, 38, 38, 42    ' metal
    Shade 15, 63, 63, 63
    Shade 16, 50, 40, 25    ' sand, light
    Shade 17, 58, 49, 33    ' sand, lit
    Shade 18, 46, 33, 20    ' sandstone, lit
    Shade 19, 48, 42, 36    ' stone
    Shade 20, 10, 20, 42    ' water
    Shade 21, 44, 54, 63    ' foam
    Shade 22, 58, 22, 14    ' ore
    Shade 23, 22, 22, 28    ' metal, dark
    Shade 24, 50, 50, 56    ' metal, light
    Shade 25, 4, 12, 30     ' the Coalition, dark
    Shade 26, 36, 11, 0     ' the Legion, dark
    Shade 27, 16, 36, 12    ' palm
    Shade 28, 30, 46, 63    ' glass
    Shade 29, 63, 18, 10    ' a red light
    Shade 30, 17, 11, 6     ' bark
    Shade 31, 63, 52, 44    ' ore's glint
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
    CASE 5: t.q5 = t.q5 + n
    CASE 6: t.q6 = t.q6 + n
    CASE 7: t.q7 = t.q7 + n
    CASE 8: t.q8 = t.q8 + n
    CASE 9: t.q9 = t.q9 + n
    CASE 10: t.q10 = t.q10 + n
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
                        IF m(x, y).occ = 0 AND m(x, y).hgt = m(bl(b).x, bl(b).y).hgt AND m(x, y).rmp = 0 THEN FindSpawn% = y * MAPW + x: EXIT FUNCTION
                    END IF
                END IF
            NEXT
        NEXT
    NEXT
END FUNCTION

' A shot of unit i at its target: the damage now, the look as it flies.
SUB Fire (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, i AS INTEGER)
    DIM tx AS SINGLE, ty AS SINGLE, sx AS SINGLE, sy AS SINGLE, t AS INTEGER, seen AS INTEGER
    DIM k AS INTEGER, c AS INTEGER, j AS INTEGER, r AS SINGLE
    t = u(i).ti: k = u(i).kind
    IF u(i).tk = 1 THEN
        tx = u(t).px: ty = u(t).py: c = UClass%(u(t).kind)
    ELSE
        tx = (bl(t).x + BSize%(bl(t).kind) / 2) * TS + RND * 7 - 3
        ty = (bl(t).y + BSize%(bl(t).kind) / 2) * TS + RND * 7 - 3
        c = 2
    END IF
    sx = u(i).px + StepX%(u(i).dir) * 6: sy = u(i).py + StepY%(u(i).dir) * 6
    seen = u(i).team = 0 OR m(u(i).x, u(i).y).vis
    SELECT CASE k
    CASE URIFLE, UBUGGY
        AddFx fx(), FXTRACE, sx, sy, tx + RND * 4 - 2, ty + RND * 4 - 2, .07
        IF seen THEN Sfx g, 1
    CASE UARTY
        AddFx fx(), FXSHELL, sx, sy, tx, ty, .45
        AddFx fx(), FXBIG, tx, ty, 0, 0, .4
        IF seen THEN Sfx g, 4
    CASE ELSE
        AddFx fx(), FXSHELL, sx, sy, tx, ty, .22
        IF seen THEN Sfx g, 2
    END SELECT
    u(i).cool = URate!(k)
    Hit g, m(), u(), bl(), fx(), fk(), u(i).tk, t, UDmg%(k) * DmgVs%(k, c) \ 100, 1, i
    ' its burst: half to the foes around
    r = (Splash!(k) * TS) ^ 2
    IF r = 0 THEN EXIT SUB
    FOR j = 1 TO MAXU
        IF u(j).alive AND u(j).team <> u(i).team AND (u(i).tk <> 1 OR j <> t) THEN
            IF (u(j).px - tx) ^ 2 + (u(j).py - ty) ^ 2 < r THEN
                Hit g, m(), u(), bl(), fx(), fk(), 1, j, UDmg%(k) * DmgVs%(k, UClass%(u(j).kind)) \ 200, 1, i
            END IF
        END IF
    NEXT
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
                ' the infantry, the vehicles, the harvesters
                n = 0
                FOR i = 1 TO MAXU
                    u(i).sel = 0
                    IF u(i).alive AND u(i).team = 0 THEN
                        IF k = "3" THEN
                            u(i).sel = u(i).kind = UHARV
                        ELSEIF u(i).kind <> UHARV THEN
                            u(i).sel = UClass%(u(i).kind) = VAL(k) - 1
                        END IF
                    END IF
                    IF u(i).sel THEN n = n + 1
                NEXT
                Say g, LTRIM$(STR$(n)) + " selected"
            CASE " "
                IF g.alertX >= 0 THEN CenterOn g.vw, g.alertX, g.alertY
            CASE "S"
                FOR i = 1 TO MAXU
                    IF u(i).sel THEN u(i).st = SIDLE: u(i).agg = 0
                NEXT
            CASE ELSE
                ' an item's key
                i = INSTR("QWERTZXCVBN", k)
                IF i > 0 THEN ButtonClick g, tm(), bl(), i - 1, 0
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
    KCost% = VAL(MID$("150300200350200300080140180320300", k * 3 + 1, 3))
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

' The building or unit kind item k makes.
FUNCTION KMakes% (k AS INTEGER)
    KMakes% = VAL(MID$("52364124536", k + 1, 1))
END FUNCTION

FUNCTION KName$ (k AS INTEGER)
    SELECT CASE k
    CASE 5: KName$ = "Harvester"
    CASE 6: KName$ = "Rifleman"
    CASE 7: KName$ = "Rocketeer"
    CASE 8: KName$ = "Buggy"
    CASE 9: KName$ = "Tank"
    CASE 10: KName$ = "Artillery"
    CASE ELSE: KName$ = BName$(KMakes%(k))
    END SELECT
END FUNCTION

FUNCTION KSecs! (k AS INTEGER)
    KSecs! = VAL(MID$("5.07.06.08.05.06.02.53.54.06.06.0", k * 3 + 1, 3))
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
        ' a beat at a time, once the bass (voice 1, the one PLAY(0) counts) plays its last note:
        ' a voice holds 18 notes before PLAY waits, and the synth's clock is not the program's
        IF PLAY(0) = 0 THEN
            IF tune(4, g.snd.beat * 4) = "" THEN g.snd.beat = 0
            b = g.snd.beat * 4
            PLAY tune(4, b), "", "", tune(4, b + 1), tune(4, b + 2), tune(4, b + 3)
            g.snd.beat = g.snd.beat + 1
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

' Side ti's power, its building and its three lines' units, a step on.
SUB Produce (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, ti AS INTEGER, dt AS SINGLE)
    DIM b AS INTEGER, low AS INTEGER, rate AS SINGLE
    tm(ti).pw = 0: tm(ti).pu = 0
    FOR b = 1 TO MAXB
        IF bl(b).alive AND bl(b).team = ti THEN
            tm(ti).pw = tm(ti).pw + BGives%(bl(b).kind): tm(ti).pu = tm(ti).pu + BUses%(bl(b).kind)
        END IF
    NEXT
    low = tm(ti).pu > tm(ti).pw
    IF low AND tm(ti).low = 0 AND ti = 0 THEN Say g, "Low power: build a Power Plant": Sfx g, 7
    tm(ti).low = low
    ' low on power, all is made at half speed
    rate = dt: IF low THEN rate = dt / 2
    IF tm(ti).sKind >= 0 AND tm(ti).sReady = 0 AND FindB%(bl(), ti, BHQ) > 0 THEN
        tm(ti).sProg = tm(ti).sProg + rate / KSecs!(tm(ti).sKind)
        IF tm(ti).sProg >= 1 THEN
            tm(ti).sProg = 1: tm(ti).sReady = -1
            IF ti = 0 THEN g.placeKind = tm(ti).sKind: Say g, "Left click places the " + KName$(tm(ti).sKind): Sfx g, 5
        END IF
    END IF
    IF tm(ti).l0Kind < 0 THEN tm(ti).l0Kind = NextQ%(tm(ti), 0)
    IF tm(ti).l1Kind < 0 THEN tm(ti).l1Kind = NextQ%(tm(ti), 1)
    IF tm(ti).l2Kind < 0 THEN tm(ti).l2Kind = NextQ%(tm(ti), 2)
    Train g, tm(), m(), u(), bl(), ti, BREF, tm(ti).l0Kind, tm(ti).l0Prog, rate
    Train g, tm(), m(), u(), bl(), ti, BBAR, tm(ti).l1Kind, tm(ti).l1Prog, rate
    Train g, tm(), m(), u(), bl(), ti, BFAC, tm(ti).l2Kind, tm(ti).l2Prog, rate
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
    CASE 5: Queued% = t.q5
    CASE 6: Queued% = t.q6
    CASE 7: Queued% = t.q7
    CASE 8: Queued% = t.q8
    CASE 9: Queued% = t.q9
    CASE 10: Queued% = t.q10
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
        IF g.won THEN c = 22 ELSE c = FIRE + 6
        IF i MOD 3 = 0 THEN
            IF g.won THEN c = 13 ELSE c = FIRE + 2
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
    DIM l AS INTEGER, h AS INTEGER, j AS INTEGER, y0 AS INTEGER, lip(1) AS INTEGER
    ' the water's colours turned a step: its swells roll on
    k = INT(g.clock * 6) MOD NWATER
    IF k <> g.vw.waterF THEN g.vw.waterF = k: WaterColours k
    g.vw.miniT = g.vw.miniT - .016

    ' the land a row of equal x + y at a time, from the back; the diamonds a half tile apart
    VIEW SCREEN (VX0, VY0)-(SBX - 1, VIEWB - 1)
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
            IF m(wx, wy).seen AND sx > -32 AND sx < SBX THEN
                FOR k = 0 TO 1
                    IF k = 0 THEN h = l - Lift%(m(), wx, wy + 1) ELSE h = l - Lift%(m(), wx + 1, wy)
                    lip(k) = h > 0
                    FOR j = 1 TO h \ STEPH
                        y0 = sy + 8 - l + (j - 1) * STEPH
                        IF y0 < VIEWB THEN
                            PUT (sx + k * 16, y0), art(0, AFACE + k * 6 + 2), AND
                            PUT (sx + k * 16, y0), art(0, AFACE + k * 6), OR
                        END IF
                    NEXT
                NEXT
            END IF
            sy = sy - l
            IF m(wx, wy).seen AND sx > -32 AND sx < SBX AND sy > -16 AND sy < VIEWB + STEPH AND m(wx, wy).rmp > 0 THEN
                k = ARMP + ((m(wx, wy).rmp - 1) * 2 + m(wx, wy).hgt) * 12
                PUT (sx, sy - STEPH), art(0, k + 4), AND
                PUT (sx, sy - STEPH), art(0, k), OR
            ELSEIF m(wx, wy).seen AND sx > -32 AND sx < SBX AND sy > -16 AND sy < VIEWB + ROCKH THEN
                PUT (sx, sy), art(0, ADIA), AND
                PUT (sx, sy), art(0, AGRS + ((wx MOD 4) + (wy MOD 4) * 4 + 16 * m(wx, wy).hgt) * 2), OR
                k = Overlay%(g, m(), wx, wy)
                IF k >= 0 THEN
                    PUT (sx, sy), art(0, AOVL + k * 4 + 2), AND
                    PUT (sx, sy), art(0, AOVL + k * 4), OR
                END IF
                ' in the shade of a cliff behind
                IF Lift%(m(), wx - 1, wy) > l THEN PUT (sx, sy), art(0, ASHA), OR
                IF Lift%(m(), wx, wy - 1) > l THEN PUT (sx + 16, sy), art(0, ASHA + 1), OR
                ' a cliff's lip, where the light catches it
                c = 17
                IF lip(0) THEN LINE (sx + 1, sy + 8)-(sx + 15, sy + 15), c
                IF lip(1) THEN LINE (sx + 16, sy + 15)-(sx + 30, sy + 8), c
                IF m(wx, wy).terr = TROCK THEN
                    k = AROK + Sides%(m(), wx, wy) * 9
                    PUT (sx, sy - ROCKH), art(0, k + 3), AND
                    PUT (sx, sy - ROCKH), art(0, k), OR
                END IF
                IF m(wx, wy).terr = TTREE AND nt < 200 THEN nt = nt + 1: tx(nt) = wx: ty(nt) = wy
            END IF
            sy = sy + l
        NEXT
    NEXT
    DrawObjects g, m(), u(), bl(), art(), tx(), ty(), nt
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
    DrawSidebar g, tm(), m(), u(), bl(), art(), gl()
    DrawCursor g, m(), u(), bl()
END SUB

SUB RenderTitle (g AS GameT, pt() AS PartT, art() AS INTEGER, gl() AS INTEGER)
    DIM i AS INTEGER, c AS INTEGER
    PUT (0, 0), art(0, ABACK), PSET
    FOR i = 1 TO NPART
        ' embers on the wind
        c = 22: IF i MOD 4 = 0 THEN c = 13
        PSET (pt(i).x, pt(i).y), c
    NEXT
    IF INT(g.clock * 2) MOD 2 THEN c = 13 ELSE c = 15
    TxtC gl(), "CLICK TO BEGIN", 128, 2, c
    TxtC gl(), "QWERT build  ZXCVBN train  A army  H HQ", 150, 1, 14
    TxtC gl(), "Esc leaves", 190, 1, 23
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
    ' on voice 2, spaced so its queue never fills
    IF g.clock - g.snd.last < .1 THEN EXIT SUB
    g.snd.last = g.clock
    SELECT CASE k
    CASE 5: PLAY "", "T200W0Q50V7L32O4CEGO5C"
    CASE 6: PLAY "", "T200W0Q50V6L64O5G"
    CASE 7: PLAY "", "T200W0Q50V8L16O2CO1G"
    CASE 8: PLAY "", "T180W0Q50V9L16O4AO3AO4AO3A"
    CASE ELSE: PLAY "", "T200W1V7L64O6A#"
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
            IF bl(b).kind = BTUR AND tm(bl(b).team).low = 0 THEN TurretTick g, m(), u(), bl(), fx(), fk(), b, dt
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
    g.ai.coin = g.ai.coin + dt * 6
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
    UDmg% = VAL(MID$("000005024018006034", k * 3 - 2, 3))
END FUNCTION

FUNCTION UHp% (k AS INTEGER)
    UHp% = VAL(MID$("220055220050090080", k * 3 - 2, 3))
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
            r = r + m(cx, cy).hgt
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
    URange! = VAL(MID$("0.03.54.55.03.57.5", k * 3 - 2, 3))
END FUNCTION

FUNCTION URate! (k AS INTEGER)
    URate! = VAL(MID$("1.00.71.61.8.353.2", k * 3 - 2, 3))
END FUNCTION

FUNCTION USight% (k AS INTEGER)
    USight% = VAL(MID$("345475", k, 1))
END FUNCTION

FUNCTION USpeed! (k AS INTEGER)
    USpeed! = VAL(MID$("1.93.22.32.84.61.7", k * 3 - 2, 3))
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
' Whether a unit can step from x, y in heading d: onto open land the ground runs on to, and on a
' diagonal round both the tiles it cuts between.
FUNCTION Walk% (m() AS CellT, x AS INTEGER, y AS INTEGER, d AS INTEGER)
    DIM nx AS INTEGER, ny AS INTEGER
    Walk% = 0
    nx = x + StepX%(d): ny = y + StepY%(d)
    IF Passable%(m(), nx, ny) = 0 THEN EXIT FUNCTION
    IF d MOD 2 = 0 THEN Walk% = Joined%(m(), x, y, nx, ny): EXIT FUNCTION
    IF Passable%(m(), nx, y) = 0 OR Passable%(m(), x, ny) = 0 THEN EXIT FUNCTION
    IF Joined%(m(), x, y, nx, y) = 0 OR Joined%(m(), nx, y, nx, ny) = 0 THEN EXIT FUNCTION
    Walk% = Joined%(m(), x, y, x, ny) AND Joined%(m(), x, ny, nx, ny)
END FUNCTION

' The level of tile x, y's corner c: 0 north (its diamond's top), 1 east, 2 south, 3 west.
FUNCTION Corner% (m() AS CellT, x AS INTEGER, y AS INTEGER, c AS INTEGER)
    DIM h AS INTEGER
    h = m(x, y).hgt
    IF m(x, y).rmp = 1 AND c <= 1 THEN h = h + 1
    IF m(x, y).rmp = 2 AND (c = 0 OR c = 3) THEN h = h + 1
    Corner% = h
END FUNCTION

' Whether the ground runs on from tile x, y to nx, ny beside it: no cliff, no ramp's side.
FUNCTION Joined% (m() AS CellT, x AS INTEGER, y AS INTEGER, nx AS INTEGER, ny AS INTEGER)
    IF nx > x THEN
        Joined% = Corner%(m(), x, y, 1) = Corner%(m(), nx, ny, 0) AND Corner%(m(), x, y, 2) = Corner%(m(), nx, ny, 3)
    ELSEIF nx < x THEN
        Joined% = Corner%(m(), nx, ny, 1) = Corner%(m(), x, y, 0) AND Corner%(m(), nx, ny, 2) = Corner%(m(), x, y, 3)
    ELSEIF ny > y THEN
        Joined% = Corner%(m(), x, y, 3) = Corner%(m(), nx, ny, 0) AND Corner%(m(), x, y, 2) = Corner%(m(), nx, ny, 1)
    ELSE
        Joined% = Corner%(m(), nx, ny, 3) = Corner%(m(), x, y, 0) AND Corner%(m(), nx, ny, 2) = Corner%(m(), x, y, 1)
    END IF
END FUNCTION

' How far the ground stands above level 0 at the map's pixel px, py: on a ramp, by how far up it.
FUNCTION LiftAt! (m() AS CellT, px AS SINGLE, py AS SINGLE)
    DIM x AS INTEGER, y AS INTEGER, l AS SINGLE
    x = INT(px / TS): y = INT(py / TS)
    LiftAt! = 0
    IF x < 0 OR y < 0 OR x >= MAPW OR y >= MAPH THEN EXIT FUNCTION
    l = m(x, y).hgt
    IF m(x, y).rmp = 1 THEN l = l + 1 - (py - y * TS) / TS
    IF m(x, y).rmp = 2 THEN l = l + 1 - (px - x * TS) / TS
    LiftAt! = l * STEPH
END FUNCTION

' How far tile x, y stands above the ground, in pixels; 0 off the map.
' How far tile x, y stands above level 0, in pixels (a ramp's foot); 0 off the map.
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
        FOR x = 0 TO MAPW - 1
            i = ASC(MID$(r, x + 1, 1))
            m(x, y).rmp = 0
            IF i >= 97 THEN
                m(x, y).hgt = i - 97: m(x, y).rmp = 1
            ELSEIF i >= 65 THEN
                m(x, y).hgt = i - 65: m(x, y).rmp = 2
            ELSE
                m(x, y).hgt = i - 48
            END IF
        NEXT
    NEXT
    FOR i = 1 TO MAXU: u(i) = blank: NEXT
    FOR i = 1 TO MAXB: bl(i) = noB: NEXT
    FOR i = 1 TO MAXFX: fx(i) = noFx: NEXT
    side.sKind = -1: side.l0Kind = -1: side.l1Kind = -1: side.l2Kind = -1
    tm(0) = side: tm(1) = side
    tm(0).cash = 1000: tm(1).cash = 800
    ClearFields fk()
    g.alertX = -1: g.clickU = 0
    g.now = 0: g.placeKind = -1: g.ms.dragOn = 0: g.msgT = 0: g.alarmT = -99: g.checkT = 0
    g.ai.think = 3: g.ai.wave = 0: g.ai.size = 3: g.ai.nextWave = 110: g.ai.alarmT = 0: g.ai.coin = 0

    ' the Coalition, south-west; the Legion, north-east
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 0, -1, 4, 41
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 0, 1, 8, 40
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, -1, 57, 4
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, 1, 53, 3
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, 2, 57, 7
    FOR i = 1 TO 2
        FOR k = 0 TO 1
            s = FindSpawn%(m(), bl(), FindB%(bl(), k, BHQ))
            s = SpawnUnit%(tm(), m(), u(), k, URIFLE, s MOD MAPW, s \ MAPW)
        NEXT
    NEXT
    tm(0).built = 0: tm(1).built = 0
    UpdateVis g, m(), u(), bl()
    g.vw.miniDirty = -1: g.vw.miniT = 0
    CenterOn g.vw, 8, 39
    g.state = GPLAY
    Say g, "Mine ore, build, find the Scorch Legion"
    Sfx g, 5
END SUB

SUB Sprite (art() AS INTEGER, idx AS INTEGER, x AS INTEGER, y AS INTEGER)
    PUT (x, y), art(0, AMSK + idx), AND
    PUT (x, y), art(0, ASPR + idx), OR
END SUB
