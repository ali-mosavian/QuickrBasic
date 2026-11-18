' IRONMOSS: the war for the glowing moss of Veyra.
' Lead the Thornwake Lodge: harvest moss, build a base, destroy the Ash Clade.
' Mouse: left drags a box to select, right orders (move, attack, harvest); the sidebar builds.
' Arrows or the screen's edges scroll; H home, A all army, S stop, M sound, Esc leaves.
' PEN reads the mouse. SCREEN 13, 256 colours, on two pages: one drawn while the other is shown
' (QB 4.5's SCREEN 13 has one; QuickrBasic's has two).
' Driven by TIMER: the same speed on any machine.
'
' No shared state: the game is a GameT and the collections the main module owns, passed down.
' The rules (hit points, costs) are functions of a kind.
DEFINT A-Z

CONST MAPW = 40, MAPH = 30, TS = 16, VW = 15, VH = 12, SBX = 240, VIEWH = 192
CONST MAXU = 64, MAXB = 32, MAXFX = 64, NF = 24, NPART = 48
CONST STEPT = .0333333
CONST GTITLE = 0, GPLAY = 1, GWIN = 2, GLOSE = 3
CONST TGROUND = 0, TGROUND2 = 1, TDIRT = 2, TROCK = 3, TWATER = 4, TMOSS = 5, TTREE = 6
CONST UCRAWL = 1, UWARD = 2, URAM = 3
CONST BHALL = 1, BMILL = 2, BDEN = 3, BSPIRE = 4
CONST SIDLE = 0, SMOVE = 1, SATTACK = 2, SSEEK = 3, SGATHER = 4, SRETURN = 5, SUNLOAD = 6
CONST FXBOOM = 1, FXBIG = 2, FXTRACE = 3, FXSHELL = 4, FXBEAM = 5, FXSMOKE = 6, FXMARK = 7
CONST MOSSKEY = -2, NOKEY = -99, FAR = 32000, LOADMAX = 10, LOADVAL = 25
CONST MINIX = 240, MINIY = 11, BTNY = 72, BTNH = 18
' the palette: 0 to 31 the land's, units' and buildings'; 32 to 63 the same in the fog (FOG OR c);
' from 64 the screens' ramps
CONST FOG = 32, SKY = 64, PLANET = 96, FIRE = 112, LOGOC = 128
' the art, one array of IMG-element slots: images of 16 by 16 (a unit's sprite, mask and shadow),
' the minimap, the full screen.
' Past 64 KB: QB 4.5 needs /AH.
' the land: 16 tiles of grass, a 4 by 4 piece of it each; then what is drawn on the grass, an
' image and a mask each: dirt, flowers, moss thin to thick, a tree, rock and water's two frames
' by which of their 4 sides go on (OROCK + sides, OWATER + frame * 16 + sides)
CONST OROCK = 6, OWATER = 22, NOVL = 54
CONST IMG = 130, ASPR = 0, AMSK = 96, ASHD = 192, ATIL = 288, AOVL = ATIL + 16
CONST ABLD = AOVL + NOVL * 2, AICN = ABLD + 32
CONST AFOG = AICN + 3, AMINI = AFOG + 1, ABACK = AMINI + 19, NSLOT = ABACK + 247

TYPE CellT
    terr AS INTEGER
    amt AS INTEGER   ' moss left
    seen AS INTEGER  ' the Lodge has seen it
    vis AS INTEGER   ' the Lodge sees it now
    occ AS INTEGER   ' the unit on it, or moving onto it
    bld AS INTEGER   ' the building on it
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
    ti AS INTEGER    ' a spire's target unit
    ang AS SINGLE    ' a spire's gun
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

' A flow field's goal (a tile, or MOSSKEY) and when it was last used.
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
    q3 AS INTEGER    ' crawlers, wardens, rams waiting
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
    camX AS INTEGER
    camY AS INTEGER
    page AS INTEGER  ' the page being drawn
    mossPh AS INTEGER
    waterF AS INTEGER
    miniDirty AS INTEGER
    miniT AS SINGLE
    scrollAcc AS SINGLE
END TYPE

TYPE AiT
    think AS SINGLE
    wave AS INTEGER
    size AS INTEGER
    nextWave AS SINGLE
    alarmK AS INTEGER ' who last hit the Clade
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
    losses AS INTEGER ' the Lodge's units and buildings lost
    kills AS INTEGER  ' the Clade's
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
DECLARE SUB PaintOverlay (k AS INTEGER)
DECLARE SUB GrassField ()
DECLARE FUNCTION Overlay% (g AS GameT, m() AS CellT, x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION Tex% (kind AS INTEGER, x AS INTEGER, y AS INTEGER, ph AS INTEGER)
DECLARE FUNCTION Sides% (m() AS CellT, x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION LandPix% (k AS INTEGER, same AS INTEGER, f AS INTEGER, x AS INTEGER, y AS INTEGER)
DECLARE SUB NewGame (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT)
DECLARE SUB GoTitle (g AS GameT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB GoEnd (g AS GameT, tm() AS TeamT, won AS INTEGER, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB ReadMouse (ms AS MouseT)
DECLARE SUB Keys (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB Controls (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, dt AS SINGLE)
DECLARE SUB Tick (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fd() AS INTEGER, fk() AS FieldT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB UnitTick (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fd() AS INTEGER, fk() AS FieldT, i AS INTEGER, dt AS SINGLE)
DECLARE SUB SpireTick (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, b AS INTEGER, dt AS SINGLE)
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
DECLARE SUB DepleteMoss (g AS GameT, m() AS CellT, fk() AS FieldT, x AS INTEGER, y AS INTEGER)
DECLARE SUB AddFx (fx() AS FxT, k AS INTEGER, x AS SINGLE, y AS SINGLE, x2 AS SINGLE, y2 AS SINGLE, life AS SINGLE)
DECLARE SUB FxTick (fx() AS FxT, dt AS SINGLE)
DECLARE SUB Order (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, wx AS INTEGER, wy AS INTEGER)
DECLARE SUB SelectBox (g AS GameT, m() AS CellT, u() AS UnitT, x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER)
DECLARE SUB ButtonClick (g AS GameT, tm() AS TeamT, bl() AS BldgT, k AS INTEGER, rightButton AS INTEGER)
DECLARE SUB CenterOn (cam AS ViewT, x AS INTEGER, y AS INTEGER)
DECLARE SUB RenderGame (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, art() AS INTEGER, gl() AS INTEGER)
DECLARE SUB DrawUnits (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, art() AS INTEGER)
DECLARE SUB DrawFx (g AS GameT, fx() AS FxT)
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
DECLARE SUB Ramp (c0 AS INTEGER, n AS INTEGER, r0 AS INTEGER, g0 AS INTEGER, b0 AS INTEGER, r1 AS INTEGER, g1 AS INTEGER, b1 AS INTEGER)
DECLARE SUB Shade (c AS INTEGER, r AS INTEGER, g AS INTEGER, b AS INTEGER)
DECLARE FUNCTION UnitPix% (ut AS INTEGER, tmi AS INTEGER, u AS SINGLE, v AS SINGLE, fr AS INTEGER)
DECLARE FUNCTION BldPix% (bt AS INTEGER, tmi AS INTEGER, x AS INTEGER, y AS INTEGER)
DECLARE FUNCTION Passable% (m() AS CellT, x AS INTEGER, y AS INTEGER)
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
DECLARE FUNCTION NearestMill% (u() AS UnitT, bl() AS BldgT, i AS INTEGER)
DECLARE FUNCTION TargetDist! (u() AS UnitT, bl() AS BldgT, i AS INTEGER)
DECLARE FUNCTION Scan% (u() AS UnitT, bl() AS BldgT, i AS INTEGER, radius AS SINGLE)
DECLARE FUNCTION UnitAt% (m() AS CellT, u() AS UnitT, wx AS INTEGER, wy AS INTEGER)
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
DIM art(IMG - 1, NSLOT - 1), gl(126, 7), tune(1 TO 4, 47) AS STRING
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

' The map, a row to a string: . , ground, : dirt, # rock, ~ water, m moss, T fungus.
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
' Voice 1, the lead, is the one DOS's speaker plays. The battle's is a bass line alone.
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
DATA "T90MSW2Y1,420,8H0,6,6,5V7"
DATA "O2A4P4O2A4O2E4", "O2F4P4O2F4O2C4", "O2D4P4O2D4O2A4", "O2E4P4O2E4O2G#4", ""

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

' A finished building of the computer's, at the first spot near its hall that takes it.
SUB AIPlace (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fk() AS FieldT)
    DIM h AS INTEGER, r AS INTEGER, x AS INTEGER, y AS INTEGER
    h = FindB%(bl(), 1, BHALL)
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
    IF FindB%(bl(), 1, BHALL) THEN
        IF tm(1).sKind < 0 THEN
            want = -1
            IF CountB%(bl(), 1, BMILL) = 0 THEN
                want = 0
            ELSEIF CountB%(bl(), 1, BDEN) = 0 THEN
                want = 1
            ELSEIF CountB%(bl(), 1, BSPIRE) < 1 + g.ai.wave \ 2 AND CountB%(bl(), 1, BSPIRE) < 4 THEN
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
        IF CountU%(u(), 1, UCRAWL) + tm(1).q3 < 2 AND tm(1).hallKind < 0 AND tm(1).cash >= KCost%(3) THEN
            tm(1).cash = tm(1).cash - KCost%(3): Enqueue tm(1), 3, 1
        END IF
    END IF
    IF FindB%(bl(), 1, BDEN) AND tm(1).q4 + tm(1).q5 < 2 THEN
        k = 4
        IF RND < .3 + g.ai.wave * .06 THEN k = 5
        IF tm(1).cash >= KCost%(k) THEN tm(1).cash = tm(1).cash - KCost%(k): Enqueue tm(1), k, 1
    END IF

    ' defence: the idle answer whoever hit the Clade
    IF g.ai.alarmT > 0 AND g.now - g.ai.alarmT < 4 THEN
        FOR i = 1 TO MAXU
            IF u(i).alive AND u(i).team = 1 AND u(i).kind <> UCRAWL AND u(i).st = SIDLE THEN
                IF g.ai.alarmK = 1 THEN
                    IF u(g.ai.alarmI).alive AND u(g.ai.alarmI).team = 0 THEN u(i).st = SATTACK: u(i).tk = 1: u(i).ti = g.ai.alarmI: u(i).agg = 0
                ELSEIF g.ai.alarmK = 2 THEN
                    IF bl(g.ai.alarmI).alive AND bl(g.ai.alarmI).team = 0 THEN u(i).st = SATTACK: u(i).tk = 2: u(i).ti = g.ai.alarmI: u(i).agg = 0
                END IF
            END IF
        NEXT
    END IF

    ' a wave, once enough wait at home: on the Lodge's building nearest the Clade
    IF g.now >= g.ai.nextWave THEN
        n = 0
        FOR i = 1 TO MAXU
            IF u(i).alive AND u(i).team = 1 AND u(i).kind <> UCRAWL AND u(i).st = SIDLE THEN n = n + 1
        NEXT
        IF n >= g.ai.size THEN
            tb = 0: best = 1E+09
            FOR b = 1 TO MAXB
                IF bl(b).alive AND bl(b).team = 0 THEN
                    d = (bl(b).x - 34) ^ 2 + (bl(b).y - 3) ^ 2
                    IF bl(b).kind = BSPIRE THEN d = d + 40
                    IF d < best THEN best = d: tb = b
                END IF
            NEXT
            IF tb THEN
                FOR i = 1 TO MAXU
                    IF u(i).alive AND u(i).team = 1 AND u(i).kind <> UCRAWL AND u(i).st = SIDLE THEN
                        u(i).st = SATTACK: u(i).tk = 2: u(i).ti = tb: u(i).agg = -1
                        u(i).gx = bl(tb).x: u(i).gy = bl(tb).y
                    END IF
                NEXT
                g.ai.wave = g.ai.wave + 1
                g.ai.size = 3 + g.ai.wave: IF g.ai.size > 10 THEN g.ai.size = 10
                g.ai.nextWave = g.now + 55
                Say g, "The Ash Clade is on the march!"
                Sfx g, 8
            END IF
        END IF
    END IF
END SUB

FUNCTION Allowed% (bl() AS BldgT, k AS INTEGER)
    IF k <= 3 THEN Allowed% = FindB%(bl(), 0, BHALL) > 0 ELSE Allowed% = FindB%(bl(), 0, BDEN) > 0
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
    CASE BHALL: BHp% = 900
    CASE BSPIRE: BHp% = 320
    CASE ELSE: BHp% = 450
    END SELECT
END FUNCTION

' A building's look, a pixel at a time: 48 by 48, the spire 24 by 24 (drawn at two thirds).
FUNCTION BldPix% (bt AS INTEGER, tmi AS INTEGER, x AS INTEGER, y AS INTEGER)
    DIM c AS INTEGER, mc AS INTEGER, lc AS INTEGER, dc AS INTEGER, d AS SINGLE, sz AS INTEGER
    mc = 10 + tmi * 2: lc = 11 + tmi * 2: dc = 25 + tmi
    sz = 48: IF bt = BSPIRE THEN sz = 24
    ' the pad: slabs of metal with seams
    c = 14
    IF x MOD 12 = 0 OR y MOD 12 = 0 THEN c = 23
    IF x MOD 12 = 1 OR y MOD 12 = 1 THEN c = 24
    IF x = 3 OR y = 3 OR x = sz - 4 OR y = sz - 4 THEN c = 4
    IF (x = 6 OR x = sz - 7) AND (y = 6 OR y = sz - 7) THEN c = 15
    ' the earth round the pad, its shadow below and right
    IF x < 3 OR y < 3 OR x > sz - 4 OR y > sz - 4 THEN
        c = 3: IF RND < .3 THEN c = 18
        IF (x > sz - 4 AND y > 4) OR (y > sz - 4 AND x > 4) THEN c = 30
    END IF
    SELECT CASE bt
    CASE BHALL
        IF y >= 36 AND y <= 46 AND x >= 17 AND x <= 30 THEN
            c = 0
            IF y = 36 OR (x + y) MOD 4 = 0 THEN c = lc
        END IF
        d = SQR((x - 24) ^ 2 + (y - 23) ^ 2)
        IF d <= 16 THEN c = 4
        IF d <= 15 THEN
            c = mc: IF d > 12 THEN c = dc
            IF SQR((x - 19) ^ 2 + (y - 17) ^ 2) < 6 THEN c = lc
            IF ABS(d - 9) < .8 THEN c = dc
        END IF
        IF d < 3 THEN c = 15
        d = SQR((x - 38) ^ 2 + (y - 9) ^ 2)
        IF d <= 6 THEN c = 0
        IF d <= 5 THEN
            c = 28: IF x + y < 46 THEN c = 15
        END IF
        IF d <= 1.5 THEN c = 29
    CASE BMILL
        IF y >= 33 AND y <= 46 AND x >= 4 AND x <= 43 THEN
            c = 4
            IF y >= 43 THEN
                c = 0: IF (x \ 4) MOD 2 = 0 THEN c = 13
            END IF
            IF y = 33 THEN c = 0
        END IF
        d = SQR((x - 14) ^ 2 + (y - 16) ^ 2)
        IF d <= 11 THEN c = 0
        IF d <= 10 THEN
            c = 14: IF x + y < 26 THEN c = 24
        END IF
        IF d <= 8 AND d >= 6 THEN c = mc
        IF d <= 4.5 THEN c = 22
        IF d <= 2 THEN c = 31
        d = SQR((x - 34) ^ 2 + (y - 16) ^ 2)
        IF d <= 11 THEN c = 0
        IF d <= 10 THEN
            c = 14: IF x + y < 46 THEN c = 24
        END IF
        IF d <= 8 AND d >= 6 THEN c = mc
        IF d <= 4.5 THEN c = 9
        IF d <= 2 THEN c = 8
        IF y >= 26 AND y <= 30 AND x >= 20 AND x <= 28 THEN c = 23
    CASE BDEN
        IF x >= 4 AND x <= 43 AND y >= 4 AND y <= 38 THEN
            c = mc
            IF (y - 4) MOD 6 < 2 THEN c = lc
            IF (y - 4) MOD 6 = 5 THEN c = dc
            IF x = 4 OR x = 43 OR y = 38 THEN c = 0
            IF y >= 8 AND y <= 15 AND ((x >= 8 AND x <= 15) OR (x >= 32 AND x <= 39)) THEN
                c = 0: IF (x + y) MOD 2 THEN c = 28
            END IF
        END IF
        IF x >= 14 AND x <= 33 AND y >= 30 AND y <= 45 THEN
            c = 0
            IF y MOD 3 = 0 THEN c = 23
            IF y = 30 OR x = 14 OR x = 33 THEN c = 13
        END IF
    CASE BSPIRE
        d = SQR((x - 11.5) ^ 2 + (y - 11.5) ^ 2)
        IF d <= 11 THEN c = 0
        IF d <= 10 THEN
            c = mc: IF x + y < 18 THEN c = lc
        END IF
        IF d <= 10 AND d >= 8 THEN c = dc
        IF d <= 5 THEN c = 14
        IF d <= 3 THEN c = 0
    END SELECT
    BldPix% = c
END FUNCTION

FUNCTION BSize% (k AS INTEGER)
    IF k = BSPIRE THEN BSize% = 1 ELSE BSize% = 2
END FUNCTION

' The tiles, the units in 8 headings and 2 frames each, the buildings by the tile, the icons.
' Units and buildings are drawn at 24 pixels a tile and sampled at TS.
SUB BuildArt (art() AS INTEGER, gl() AS INTEGER)
    DIM k AS INTEGER, x AS INTEGER, y AS INTEGER, ut AS INTEGER, tmi AS INTEGER, d AS INTEGER
    DIM f AS INTEGER, idx AS INTEGER, a AS SINGLE, ca AS SINGLE, sa AS SINGLE
    DIM px AS SINGLE, py AS SINGLE, c AS INTEGER, bt AS INTEGER, q AS INTEGER, sz AS INTEGER
    DIM n AS INTEGER, total AS INTEGER
    DIM px2(TS - 1, TS - 1) AS INTEGER
    total = 4 + NOVL + 96 + 8 + 3
    RANDOMIZE 11
    GrassField
    FOR k = 0 TO 15
        x = (k MOD 4) * TS: y = (k \ 4) * TS
        GET (x, y)-(x + TS - 1, y + TS - 1), art(0, ATIL + k)
        IF k MOD 4 = 3 THEN n = n + 1: Progress n, total, gl()
    NEXT
    FOR k = 0 TO NOVL - 1
        LINE (0, 0)-(TS - 1, TS - 1), 0, BF
        PaintOverlay k
        GET (0, 0)-(TS - 1, TS - 1), art(0, AOVL + k * 2)
        FOR y = 0 TO TS - 1: FOR x = 0 TO TS - 1
            IF POINT(x, y) THEN PSET (x, y), 0 ELSE PSET (x, y), 255
        NEXT: NEXT
        GET (0, 0)-(TS - 1, TS - 1), art(0, AOVL + k * 2 + 1)
        n = n + 1: Progress n, total, gl()
    NEXT
    ' the fog: OR'd over the land, its colours' dark copies
    LINE (0, 0)-(TS - 1, TS - 1), FOG, BF
    GET (0, 0)-(TS - 1, TS - 1), art(0, AFOG)

    FOR ut = 1 TO 3: FOR tmi = 0 TO 1: FOR d = 0 TO 7: FOR f = 0 TO 1
        a = d * .785398: ca = COS(a): sa = SIN(a)
        FOR y = 0 TO TS - 1: FOR x = 0 TO TS - 1
            px = (x - (TS - 1) / 2) * 24 / TS: py = (y - (TS - 1) / 2) * 24 / TS
            px2(x, y) = UnitPix%(ut, tmi, px * ca + py * sa, -px * sa + py * ca, f)
        NEXT: NEXT
        ' a black outline round the shape: -2
        FOR y = 0 TO TS - 1: FOR x = 0 TO TS - 1
            IF px2(x, y) = -1 THEN
                c = 0
                IF x > 0 THEN IF px2(x - 1, y) >= 1 THEN c = 1
                IF x < TS - 1 THEN IF px2(x + 1, y) >= 1 THEN c = 1
                IF y > 0 THEN IF px2(x, y - 1) >= 1 THEN c = 1
                IF y < TS - 1 THEN IF px2(x, y + 1) >= 1 THEN c = 1
                IF c THEN px2(x, y) = -2
            END IF
        NEXT: NEXT
        idx = ((ut - 1) * 2 + tmi) * 16 + d * 2 + f
        LINE (0, 0)-(TS - 1, TS - 1), 0, BF
        FOR y = 0 TO TS - 1: FOR x = 0 TO TS - 1
            IF px2(x, y) >= 0 THEN PSET (x, y), px2(x, y)
        NEXT: NEXT
        GET (0, 0)-(TS - 1, TS - 1), art(0, ASPR + idx)
        LINE (0, 0)-(TS - 1, TS - 1), 255, BF
        FOR y = 0 TO TS - 1: FOR x = 0 TO TS - 1
            IF px2(x, y) <> -1 THEN PSET (x, y), 0
        NEXT: NEXT
        GET (0, 0)-(TS - 1, TS - 1), art(0, AMSK + idx)
        LINE (0, 0)-(TS - 1, TS - 1), 0, BF
        FOR y = 0 TO TS - 1: FOR x = 0 TO TS - 1
            IF px2(x, y) >= 0 THEN PSET (x, y), FOG
        NEXT: NEXT
        GET (0, 0)-(TS - 1, TS - 1), art(0, ASHD + idx)
        n = n + 1: Progress n, total, gl()
    NEXT: NEXT: NEXT: NEXT

    FOR bt = 1 TO 4: FOR tmi = 0 TO 1
        sz = BSize%(bt) * TS
        FOR y = 0 TO sz - 1: FOR x = 0 TO sz - 1
            PSET (x, y), BldPix%(bt, tmi, x * 24 \ TS, y * 24 \ TS)
        NEXT: NEXT
        FOR q = 0 TO 3
            IF sz = 2 * TS OR q = 0 THEN
                x = (q MOD 2) * TS: y = (q \ 2) * TS
                GET (x, y)-(x + TS - 1, y + TS - 1), art(0, ABLD + (((bt - 1) * 2 + tmi) * 4 + q))
            END IF
        NEXT
        n = n + 1: Progress n, total, gl()
    NEXT: NEXT

    ' the sidebar's icons: the mill, the den, the spire
    FOR k = 0 TO 2
        bt = k + 2
        FOR y = 0 TO TS - 1: FOR x = 0 TO TS - 1
            PSET (x, y), BldPix%(bt, 0, x * 24 * BSize%(bt) \ TS, y * 24 * BSize%(bt) \ TS)
        NEXT: NEXT
        GET (0, 0)-(TS - 1, TS - 1), art(0, AICN + k)
        n = n + 1: Progress n, total, gl()
    NEXT
END SUB

' The flow field to goal: each tile's steps to it, FAR where there is no way. Units ignored.
SUB BuildField (m() AS CellT, fd() AS INTEGER, fi AS INTEGER, goal AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER, h AS INTEGER, tl AS INTEGER, d AS INTEGER, nx AS INTEGER, ny AS INTEGER
    DIM v AS INTEGER
    DIM qx(MAPW * MAPH) AS INTEGER, qy(MAPW * MAPH) AS INTEGER
    FOR y = 0 TO MAPH - 1: FOR x = 0 TO MAPW - 1: fd(fi, x, y) = FAR: NEXT: NEXT
    IF goal = MOSSKEY THEN
        FOR y = 0 TO MAPH - 1
            FOR x = 0 TO MAPW - 1
                IF m(x, y).terr = TMOSS AND m(x, y).bld = 0 THEN fd(fi, x, y) = 0: qx(tl) = x: qy(tl) = y: tl = tl + 1
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
            IF Passable%(m(), nx, ny) THEN
                IF fd(fi, nx, ny) > v THEN
                    IF d MOD 2 = 0 OR (Passable%(m(), nx, y) AND Passable%(m(), x, ny)) THEN
                        fd(fi, nx, ny) = v: qx(tl) = nx: qy(tl) = ny: tl = tl + 1
                    END IF
                END IF
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
        IF k >= 4 THEN Say g, "Build a Den first" ELSE Say g, "Our Hall is gone"
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
' blocking no mill's dock, a mill's own dock open.
FUNCTION CanPlace% (m() AS CellT, bl() AS BldgT, ti AS INTEGER, kind AS INTEGER, x AS INTEGER, y AS INTEGER)
    DIM bt AS INTEGER, sz AS INTEGER, i AS INTEGER, j AS INTEGER, b AS INTEGER, near AS INTEGER, tt AS INTEGER
    CanPlace% = 0
    bt = KMakes%(kind): sz = BSize%(bt)
    IF x < 0 OR y < 0 OR x + sz > MAPW OR y + sz > MAPH THEN EXIT FUNCTION
    FOR j = y TO y + sz - 1
        FOR i = x TO x + sz - 1
            tt = m(i, j).terr
            IF tt <> TGROUND AND tt <> TGROUND2 AND tt <> TDIRT THEN EXIT FUNCTION
            IF m(i, j).bld OR m(i, j).occ THEN EXIT FUNCTION
            IF ti = 0 AND m(i, j).seen = 0 THEN EXIT FUNCTION
        NEXT
    NEXT
    IF bt = BMILL THEN
        IF Passable%(m(), x, y + 2) = 0 THEN EXIT FUNCTION
    END IF
    FOR b = 1 TO MAXB
        IF bl(b).alive THEN
            IF bl(b).kind = BMILL AND bl(b).x >= x AND bl(b).x < x + sz AND bl(b).y + 2 >= y AND bl(b).y + 2 < y + sz THEN EXIT FUNCTION
            IF bl(b).team = ti THEN
                IF x <= bl(b).x + BSize%(bl(b).kind) + 2 AND x + sz >= bl(b).x - 2 AND y <= bl(b).y + BSize%(bl(b).kind) + 2 AND y + sz >= bl(b).y - 2 THEN near = -1
            END IF
        END IF
    NEXT
    CanPlace% = near
END FUNCTION

SUB CenterOn (cam AS ViewT, x AS INTEGER, y AS INTEGER)
    cam.camX = x - VW \ 2: cam.camY = y - VH \ 2
    IF cam.camX < 0 THEN cam.camX = 0
    IF cam.camY < 0 THEN cam.camY = 0
    IF cam.camX > MAPW - VW THEN cam.camX = MAPW - VW
    IF cam.camY > MAPH - VH THEN cam.camY = MAPH - VH
END SUB

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
        ELSEIF g.ms.lY < VIEWH THEN
            wx = g.vw.camX + g.ms.lX \ TS: wy = g.vw.camY + g.ms.lY \ TS
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
        ELSEIF g.ms.rY < VIEWH THEN
            Order g, m(), u(), bl(), fx(), g.vw.camX + g.ms.rX \ TS, g.vw.camY + g.ms.rY \ TS
        END IF
    END IF

    ' the screen's edges scroll, ten tiles a second
    IF g.ms.x <= 1 THEN sdx = -1
    IF g.ms.x >= 318 THEN sdx = 1
    IF g.ms.y <= 1 THEN sdy = -1
    IF g.ms.y >= 198 THEN sdy = 1
    IF sdx OR sdy THEN
        g.vw.scrollAcc = g.vw.scrollAcc + dt * 10
        DO WHILE g.vw.scrollAcc >= 1
            g.vw.scrollAcc = g.vw.scrollAcc - 1
            g.vw.camX = g.vw.camX + sdx: g.vw.camY = g.vw.camY + sdy
        LOOP
    ELSE
        g.vw.scrollAcc = .99
    END IF
    CenterOn g.vw, g.vw.camX + VW \ 2, g.vw.camY + VH \ 2
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

SUB DepleteMoss (g AS GameT, m() AS CellT, fk() AS FieldT, x AS INTEGER, y AS INTEGER)
    DIM i AS INTEGER
    m(x, y).terr = TGROUND
    FOR i = 0 TO NF - 1
        IF fk(i).goal = MOSSKEY THEN fk(i).goal = NOKEY
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
    DIM e AS INTEGER, i AS INTEGER, wx AS INTEGER, wy AS INTEGER, x AS INTEGER, y AS INTEGER
    x = g.ms.x: y = g.ms.y
    IF g.state = GPLAY AND x < SBX AND y < VIEWH AND g.placeKind < 0 THEN
        wx = g.vw.camX + x \ TS: wy = g.vw.camY + y \ TS
        i = UnitAt%(m(), u(), wx, wy)
        IF i THEN
            IF u(i).team = 1 AND m(wx, wy).vis THEN e = 1
        ELSEIF m(wx, wy).bld THEN
            IF bl(m(wx, wy).bld).team = 1 THEN e = 1
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
    FOR y = 0 TO 199
        LINE (0, y)-(319, y), SKY + y * 32 \ 200
    NEXT
    FOR i = 1 TO 90
        PSET (RND * 319, RND * 120), 15
    NEXT
    ' the land: moss glowing for the Lodge, fires for the Clade
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
        TxtC gl(), "The Ash Clade is broken.", 70, 1, 15
        TxtC gl(), "The moss of Veyra glows for the Lodge.", 82, 1, 15
    ELSE
        Logo gl(), "DEFEAT", 22, 4, 4
        TxtC gl(), "The Thornwake Lodge has fallen.", 70, 1, 15
        TxtC gl(), "The Clade takes the moss.", 82, 1, 15
    END IF
    t = "Time " + Clock2$(g.now) + "   Trained" + STR$(tm(0).built) + "   Lost" + STR$(g.losses)
    TxtC gl(), t, 108, 1, 13
    t = "Destroyed" + STR$(g.kills) + "   Moss" + STR$(tm(0).harvested)
    TxtC gl(), t, 120, 1, 13
    GET (0, 0)-(319, 199), art(0, ABACK)
END SUB

SUB DrawFx (g AS GameT, fx() AS FxT)
    DIM i AS INTEGER, p AS SINGLE, x AS INTEGER, y AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, r AS INTEGER
    DIM c AS INTEGER, k AS INTEGER, ox AS INTEGER, oy AS INTEGER
    ox = g.vw.camX * TS: oy = g.vw.camY * TS
    FOR i = 1 TO MAXFX
        IF fx(i).alive THEN
            p = fx(i).t / fx(i).life
            x = fx(i).x - ox: y = fx(i).y - oy: x2 = fx(i).x2 - ox: y2 = fx(i).y2 - oy
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
                    CASE TGROUND, TGROUND2: c = 2
                    CASE TDIRT: c = 3
                    CASE TROCK: c = 5
                    CASE TWATER: c = 20
                    CASE TMOSS: c = 22
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
    LINE (MINIX + g.vw.camX * 2, MINIY + g.vw.camY * 2)-STEP(VW * 2 - 1, VH * 2 - 1), 15, B
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
        IF u(one).kind = UCRAWL THEN s = s + "  moss" + STR$(u(one).load)
        Txt gl(), s, 2, 192, 1, 11
    ELSEIF n > 1 THEN
        Txt gl(), LTRIM$(STR$(n)) + " selected: right click orders", 2, 192, 1, 11
    ELSE
        Txt gl(), "LODGE", 2, 192, 1, 10
        s = "Wardens" + STR$(CountU%(u(), 0, UWARD)) + " Rams" + STR$(CountU%(u(), 0, URAM)) + " Crawlers" + STR$(CountU%(u(), 0, UCRAWL))
        Txt gl(), s, 50, 192, 1, 14
    END IF
END SUB

SUB DrawTitleBack (art() AS INTEGER, gl() AS INTEGER)
    DIM y AS INTEGER, x AS INTEGER, i AS INTEGER, h AS INTEGER, c AS INTEGER, r AS INTEGER, l AS SINGLE
    ' the night sky: deep blue into violet
    Ramp SKY, 32, 0, 0, 6, 26, 8, 30
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
    ' moss glowing on the near hill
    FOR i = 1 TO 160
        x = RND * 318: y = 168 + RND * 31
        PSET (x, y), 8: IF RND < .3 THEN PSET (x + 1, y), 22
    NEXT
    ' a fungus tree, left
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
    ' the Lodge's units on the hill, the Clade's beyond
    Sprite art(), 32 + 7 * 2, 116, 164
    Sprite art(), 32 + 7 * 2 + 1, 134, 170
    Sprite art(), 7 * 2, 98, 172
    Sprite art(), 5 * 16 + 3 * 2, 262, 158
    Sprite art(), 3 * 16 + 3 * 2, 284, 164
    Logo gl(), "IRONMOSS", 12, 4, 4
    TxtC gl(), "THE WAR FOR THE GLOWING MOSS OF VEYRA", 52, 1, 13
    Dither 14, 70, 305, 108, 0
    TxtC gl(), "Lead the Lodge: harvest moss, build,", 74, 1, 15
    TxtC gl(), "and break the Ash Clade.", 84, 1, 15
    TxtC gl(), "Drag selects, right click orders", 98, 1, 11
    GET (0, 0)-(319, 199), art(0, ABACK)
END SUB

SUB DrawUnits (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, art() AS INTEGER)
    DIM k AS INTEGER, i AS INTEGER, sx AS INTEGER, sy AS INTEGER, f AS INTEGER, c AS INTEGER, mh AS INTEGER
    DIM ox AS INTEGER, oy AS INTEGER, b AS INTEGER, cx AS INTEGER, cy AS INTEGER
    ox = g.vw.camX * TS: oy = g.vw.camY * TS
    FOR i = 1 TO MAXU
        IF u(i).alive THEN
            sx = INT(u(i).px) - ox - TS \ 2: sy = INT(u(i).py) - oy - TS \ 2
            IF sx >= 0 AND sy >= 0 AND sx <= SBX - TS - 2 AND sy <= VIEWH - TS - 2 THEN
                IF u(i).team = 0 OR m(u(i).x, u(i).y).vis THEN
                    f = 0
                    IF u(i).nx <> u(i).x OR u(i).ny <> u(i).y OR u(i).st = SGATHER THEN f = INT(u(i).anim * 8) MOD 2
                    k = ((u(i).kind - 1) * 2 + u(i).team) * 16 + u(i).dir * 2 + f
                    PUT (sx + 2, sy + 2), art(0, ASHD + k), OR
                    Sprite art(), k, sx, sy
                    IF u(i).kind = UCRAWL AND u(i).load > 0 THEN
                        PSET (sx + 7, sy + 7), 8: PSET (sx + 8, sy + 8), 8
                        IF u(i).load > 5 THEN PSET (sx + 8, sy + 7), 31
                    END IF
                    IF u(i).sel THEN
                        LINE (sx, sy)-STEP(3, 0), 15: LINE (sx, sy)-STEP(0, 3), 15
                        LINE (sx + 15, sy)-STEP(-3, 0), 15: LINE (sx + 15, sy)-STEP(0, 3), 15
                        LINE (sx, sy + 15)-STEP(3, 0), 15: LINE (sx, sy + 15)-STEP(0, -3), 15
                        LINE (sx + 15, sy + 15)-STEP(-3, 0), 15: LINE (sx + 15, sy + 15)-STEP(0, -3), 15
                    END IF
                    mh = UHp%(u(i).kind)
                    IF (u(i).sel OR u(i).hp < mh) AND sy >= 2 THEN
                        c = 8: IF u(i).hp * 2 < mh THEN c = 13
                        IF u(i).hp * 4 < mh THEN c = 29
                        Bar sx + 2, sy - 2, 12, u(i).hp / mh, c
                    END IF
                END IF
            END IF
        END IF
    NEXT
    ' the spires' guns, the lights, the buildings' health
    FOR b = 1 TO MAXB
        IF bl(b).alive THEN
            cx = bl(b).x * TS - ox: cy = bl(b).y * TS - oy
            IF cx >= 0 AND cy >= 4 AND cx <= SBX - TS AND cy <= VIEWH - TS THEN
                IF m(bl(b).x, bl(b).y).seen THEN
                    IF bl(b).kind = BSPIRE THEN
                        sx = cx + 8 + COS(bl(b).ang) * 7: sy = cy + 8 + SIN(bl(b).ang) * 7
                        LINE (cx + 8, cy + 8)-(sx, sy), 0
                        LINE (cx + 7, cy + 8)-(sx - 1, sy), 14
                        LINE (cx + 8, cy + 7)-(sx, sy - 1), 24
                        c = 11 + bl(b).team * 2: IF bl(b).cool > .8 THEN c = 15
                        PSET (sx, sy), c
                    ELSEIF INT(g.clock * 2 + b) MOD 3 = 0 THEN
                        PSET (cx + 2, cy + 2), 11 + bl(b).team * 2
                    END IF
                    mh = BHp%(bl(b).kind)
                    IF bl(b).hp < mh THEN
                        c = 8: IF bl(b).hp * 2 < mh THEN c = 13
                        IF bl(b).hp * 4 < mh THEN c = 29
                        Bar cx + 2, cy - 3, BSize%(bl(b).kind) * TS - 4, bl(b).hp / mh, c
                    END IF
                END IF
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

' The field to goal (a tile, or MOSSKEY), built again only when no field kept is it.
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
                        IF m(x, y).occ = 0 THEN FindSpawn% = y * MAPW + x: EXIT FUNCTION
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
    IF u(i).kind = UWARD THEN
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
        IF u(idx).st = SIDLE AND u(idx).kind <> UCRAWL THEN u(idx).st = SATTACK: u(idx).tk = sk: u(idx).ti = si: u(idx).agg = 0
        IF u(idx).team = 1 THEN g.ai.alarmK = sk: g.ai.alarmI = si: g.ai.alarmT = g.now
    ELSE
        IF bl(idx).alive = 0 THEN EXIT SUB
        bl(idx).hp = bl(idx).hp - dmg
        IF bl(idx).team = 1 THEN g.ai.alarmK = sk: g.ai.alarmI = si: g.ai.alarmT = g.now
        IF bl(idx).team = 0 AND g.now - g.alarmT > 10 THEN g.alarmT = g.now: Say g, "Our base is under attack!": Sfx g, 8
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
            CASE CHR$(0) + "H": g.vw.camY = g.vw.camY - 1
            CASE CHR$(0) + "P": g.vw.camY = g.vw.camY + 1
            CASE CHR$(0) + "K": g.vw.camX = g.vw.camX - 1
            CASE CHR$(0) + "M": g.vw.camX = g.vw.camX + 1
            CASE "M"
                g.snd.enabled = NOT g.snd.enabled
                IF g.snd.enabled THEN Say g, "Sound on" ELSE Say g, "Sound off": SOUND 37, 0
            CASE "H"
                i = FindB%(bl(), 0, BHALL)
                IF i THEN CenterOn g.vw, bl(i).x, bl(i).y
            CASE "A"
                n = 0
                FOR i = 1 TO MAXU
                    u(i).sel = u(i).alive AND u(i).team = 0 AND u(i).kind <> UCRAWL
                    IF u(i).sel THEN n = n + 1
                NEXT
                Say g, LTRIM$(STR$(n)) + " of the army selected"
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
    IF u(i).team = 0 AND u(i).kind = UCRAWL THEN Say g, "A Crawler was destroyed"
END SUB

FUNCTION KMakes% (k AS INTEGER)
    SELECT CASE k
    CASE 0: KMakes% = BMILL
    CASE 1: KMakes% = BDEN
    CASE 2: KMakes% = BSPIRE
    CASE ELSE: KMakes% = k - 2
    END SELECT
END FUNCTION

FUNCTION KName$ (k AS INTEGER)
    SELECT CASE k
    CASE 0: KName$ = "Mill"
    CASE 1: KName$ = "Den"
    CASE 2: KName$ = "Spire"
    CASE 3: KName$ = "Crawler"
    CASE 4: KName$ = "Warden"
    CASE ELSE: KName$ = "Ram"
    END SELECT
END FUNCTION

FUNCTION KSecs! (k AS INTEGER)
    SELECT CASE k
    CASE 0, 3: KSecs! = 10
    CASE 1: KSecs! = 12
    CASE 2, 5: KSecs! = 8
    CASE ELSE: KSecs! = 4
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
        IF Passable%(m(), nx, ny) THEN
            v = fd(fi, nx, ny)
            IF v < cur THEN
                IF d MOD 2 = 0 OR (Passable%(m(), nx, u(i).y) AND Passable%(m(), u(i).x, ny)) THEN
                    lower = -1
                    v = v * 2 + (d MOD 2)
                    IF m(nx, ny).occ = 0 AND v < bv THEN bv = v: best = d
                END IF
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
            IF tune(4, g.snd.beat) = "" THEN g.snd.beat = 0
            PLAY "", tune(4, g.snd.beat)
            g.snd.beat = g.snd.beat + 1
            g.snd.due = g.snd.due + 4 * 60 / 90
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

FUNCTION NearestMill% (u() AS UnitT, bl() AS BldgT, i AS INTEGER)
    DIM b AS INTEGER, best AS INTEGER, d AS INTEGER, bd AS INTEGER
    bd = FAR
    FOR b = 1 TO MAXB
        IF bl(b).alive AND bl(b).team = u(i).team AND bl(b).kind = BMILL THEN
            d = ABS(bl(b).x - u(i).x) + ABS(bl(b).y + 2 - u(i).y)
            IF d < bd THEN bd = d: best = b
        END IF
    NEXT
    NearestMill% = best
END FUNCTION

' A building of side ti at x, y: kind -1 the hall, else the sidebar's kind. A mill brings a crawler.
SUB PlaceBuilding (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fk() AS FieldT, ti AS INTEGER, kind AS INTEGER, x AS INTEGER, y AS INTEGER)
    DIM b AS INTEGER, bt AS INTEGER, i AS INTEGER, j AS INTEGER, k AS INTEGER
    DIM fresh AS BldgT
    bt = BHALL: IF kind >= 0 THEN bt = KMakes%(kind)
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
    IF bt = BMILL THEN
        IF m(x, y + 2).occ = 0 THEN k = SpawnUnit%(tm(), m(), u(), ti, UCRAWL, x, y + 2)
    END IF
    IF ti = 0 AND kind >= 0 THEN Say g, "The " + KName$(kind) + " stands"
END SUB

' Side ti's building and its two factories' units, a step on.
SUB Produce (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, ti AS INTEGER, dt AS SINGLE)
    IF tm(ti).sKind >= 0 AND tm(ti).sReady = 0 AND FindB%(bl(), ti, BHALL) > 0 THEN
        tm(ti).sProg = tm(ti).sProg + dt / KSecs!(tm(ti).sKind)
        IF tm(ti).sProg >= 1 THEN
            tm(ti).sProg = 1: tm(ti).sReady = -1
            IF ti = 0 THEN Say g, "The " + KName$(tm(ti).sKind) + " is ready to place": Sfx g, 5
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
    Train g, tm(), m(), u(), bl(), ti, BHALL, tm(ti).hallKind, tm(ti).hallProg, dt
    Train g, tm(), m(), u(), bl(), ti, BDEN, tm(ti).denKind, tm(ti).denProg, dt
END SUB

' The loading screen's bar, on the page shown.
SUB Progress (n AS INTEGER, total AS INTEGER, gl() AS INTEGER)
    SCREEN , , 0, 0
    IF n = 1 THEN
        CLS
        TxtC gl(), "IRONMOSS", 70, 2, 8
        TxtC gl(), "the moss is growing", 94, 1, 9
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
    DIM tx AS INTEGER, ty AS INTEGER, wx AS INTEGER, wy AS INTEGER, sx AS INTEGER, sy AS INTEGER
    DIM b AS INTEGER, k AS INTEGER, sz AS INTEGER, c AS INTEGER, ph AS INTEGER
    ' the moss breathes; the water ripples
    ph = INT(g.clock * 2.5) MOD 4
    IF ph <> g.vw.mossPh THEN
        g.vw.mossPh = ph
        SELECT CASE ph
        CASE 0: Shade 8, 20, 58, 40
        CASE 2: Shade 8, 40, 63, 56
        CASE ELSE: Shade 8, 28, 63, 46
        END SELECT
    END IF
    g.vw.waterF = INT(g.clock * 1.6) MOD 2
    g.vw.miniT = g.vw.miniT - .016

    FOR ty = 0 TO VH - 1
        wy = g.vw.camY + ty: sy = ty * TS
        FOR tx = 0 TO VW - 1
            wx = g.vw.camX + tx: sx = tx * TS
            IF m(wx, wy).seen = 0 THEN
                LINE (sx, sy)-STEP(TS - 1, TS - 1), 0, BF
            ELSE
                b = m(wx, wy).bld
                IF b THEN
                    k = (wx - bl(b).x) + (wy - bl(b).y) * 2
                    PUT (sx, sy), art(0, ABLD + (((bl(b).kind - 1) * 2 + bl(b).team) * 4 + k)), PSET
                ELSE
                    PUT (sx, sy), art(0, ATIL + (wx MOD 4) + (wy MOD 4) * 4), PSET
                    k = Overlay%(g, m(), wx, wy)
                    IF k >= 0 THEN
                        PUT (sx, sy), art(0, AOVL + k * 2 + 1), AND
                        PUT (sx, sy), art(0, AOVL + k * 2), OR
                    END IF
                END IF
                IF m(wx, wy).vis = 0 THEN PUT (sx, sy), art(0, AFOG), OR
            END IF
        NEXT
    NEXT
    VIEW SCREEN (0, 0)-(SBX - 1, VIEWH - 1)
    DrawUnits g, m(), u(), bl(), art()
    DrawFx g, fx()
    ' a building's place, green where it fits
    IF g.placeKind >= 0 AND g.ms.x < SBX AND g.ms.y < VIEWH THEN
        wx = g.vw.camX + g.ms.x \ TS: wy = g.vw.camY + g.ms.y \ TS
        sz = BSize%(KMakes%(g.placeKind)) * TS
        c = 29: IF CanPlace%(m(), bl(), 0, g.placeKind, wx, wy) THEN c = 8
        sx = (wx - g.vw.camX) * TS: sy = (wy - g.vw.camY) * TS
        LINE (sx, sy)-STEP(sz - 1, sz - 1), c, B
        FOR k = 4 TO sz * 2 - 4 STEP 4
            IF k < sz THEN LINE (sx + k, sy)-(sx, sy + k), c, , &HAAAA ELSE LINE (sx + sz - 1, sy + k - sz + 1)-(sx + k - sz + 1, sy + sz - 1), c, , &HAAAA
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

' The left button let go: a click picks one unit, a box every one of the Lodge's in it.
SUB SelectBox (g AS GameT, m() AS CellT, u() AS UnitT, x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER)
    DIM i AS INTEGER, sx AS INTEGER, sy AS INTEGER, e AS INTEGER, n AS INTEGER, army AS INTEGER
    IF x1 > x2 THEN SWAP x1, x2
    IF y1 > y2 THEN SWAP y1, y2
    FOR i = 1 TO MAXU: u(i).sel = 0: NEXT
    IF x2 - x1 < 5 AND y2 - y1 < 5 THEN
        e = UnitAt%(m(), u(), g.vw.camX + x1 \ TS, g.vw.camY + y1 \ TS)
        IF e THEN
            IF u(e).team = 0 THEN u(e).sel = -1: Sfx g, 6
        END IF
        EXIT SUB
    END IF
    FOR i = 1 TO MAXU
        IF u(i).alive AND u(i).team = 0 THEN
            sx = u(i).px - g.vw.camX * TS: sy = u(i).py - g.vw.camY * TS
            IF sx >= x1 AND sx <= x2 AND sy >= y1 AND sy <= y2 THEN
                u(i).sel = -1: n = n + 1
                IF u(i).kind <> UCRAWL THEN army = -1
            END IF
        END IF
    NEXT
    ' the army before the crawlers, when the box holds both
    IF army THEN
        FOR i = 1 TO MAXU
            IF u(i).kind = UCRAWL THEN u(i).sel = 0
        NEXT
    END IF
    IF n THEN Sfx g, 6
END SUB

' The screen, its pages, the palette, the music.
SUB Setup (g AS GameT, tune() AS STRING)
    DIM i AS INTEGER, j AS INTEGER, v AS INTEGER, pre(2) AS STRING, bar(2) AS STRING
    SCREEN 13, , 1, 0
    g.vw.page = 1
    g.snd.enabled = -1
    Shade 0, 0, 0, 0
    Shade 1, 7, 15, 6       ' ground, dark
    Shade 2, 9, 19, 8     ' ground
    Shade 3, 26, 19, 12     ' dirt
    Shade 4, 12, 10, 12      ' rock's shadow, outlines
    Shade 5, 27, 22, 26     ' rock, fungus
    Shade 6, 5, 11, 30      ' deep water
    Shade 7, 20, 32, 54     ' light on water
    Shade 8, 24, 63, 44     ' moss's glow
    Shade 9, 6, 40, 22      ' moss
    Shade 10, 0, 40, 42     ' the Lodge
    Shade 11, 32, 62, 60
    Shade 12, 58, 24, 2     ' the Clade
    Shade 13, 63, 54, 16
    Shade 14, 38, 38, 42    ' metal
    Shade 15, 63, 63, 63
    Shade 16, 12, 23, 9    ' ground, light
    Shade 17, 17, 29, 11    ' grass
    Shade 18, 34, 26, 17    ' dirt, light
    Shade 19, 42, 37, 38    ' rock, light
    Shade 20, 10, 20, 42    ' water
    Shade 21, 44, 54, 63    ' foam
    Shade 22, 14, 52, 32    ' moss, bright
    Shade 23, 22, 22, 28    ' metal, dark
    Shade 24, 50, 50, 56    ' metal, light
    Shade 25, 0, 22, 26     ' the Lodge, dark
    Shade 26, 36, 11, 0     ' the Clade, dark
    Shade 27, 46, 28, 50    ' fungus cap, light
    Shade 28, 30, 46, 63    ' glass
    Shade 29, 63, 18, 10    ' a red light
    Shade 30, 17, 11, 6     ' bark
    Shade 31, 44, 63, 54    ' moss's tips
    Ramp FIRE, 16, 63, 63, 52, 34, 6, 2
    Ramp LOGOC, 8, 63, 63, 63, 0, 34, 24
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
    READ pre(0)
    j = 0
    DO
        READ bar(0)
        IF bar(0) = "" THEN tune(4, j) = "" ELSE tune(4, j) = pre(0) + bar(0)
        j = j + 1
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
        CASE 3: PLAY "", "", "W2H0,2,0,2V7L64O6CO7C": g.snd.noise = g.clock + .06
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
    IF ut = UCRAWL THEN u(i).st = SSEEK
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
            IF bl(b).kind = BSPIRE THEN SpireTick g, m(), u(), bl(), fx(), fk(), b, dt
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
    ' the Clade's tithe: a few credits a second, so it never stalls
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
FUNCTION Overlay% (g AS GameT, m() AS CellT, x AS INTEGER, y AS INTEGER)
    SELECT CASE m(x, y).terr
    CASE TDIRT: Overlay% = 0
    CASE TGROUND2: Overlay% = 1
    CASE TMOSS
        IF m(x, y).amt >= 7 THEN
            Overlay% = 4
        ELSEIF m(x, y).amt >= 4 THEN
            Overlay% = 3
        ELSE
            Overlay% = 2
        END IF
    CASE TTREE: Overlay% = 5
    CASE TROCK: Overlay% = OROCK + Sides%(m(), x, y)
    CASE TWATER: Overlay% = OWATER + ((g.vw.waterF + x + y) MOD 2) * 16 + Sides%(m(), x, y)
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

' Rock (k 0) or water (k 1, frame f) at x, y of a tile whose land goes on at the sides same: a colour,
' 0 the grass. Where a side ends the edge is ragged; the rock is boulders lit from the upper
' left with a shadow below and right, the water has a muddy bank and foam.
FUNCTION LandPix% (k AS INTEGER, same AS INTEGER, f AS INTEGER, x AS INTEGER, y AS INTEGER)
    DIM e AS SINGLE, es AS SINGLE, r AS SINGLE, i AS INTEGER, j AS INTEGER, cx AS SINGLE, cy AS SINGLE
    DIM d AS SINGLE, d1 AS SINGLE, d2 AS SINGLE, bx AS SINGLE, by AS SINGLE, c AS INTEGER
    ' each open side's distance, inset by a ragged margin; two meeting round their corner
    e = 99: es = 99: r = 2.5 - k
    IF (same AND 1) = 0 THEN e = y - (r + SIN(x * 1.1 + k))
    IF (same AND 8) = 0 THEN d = x - (r + SIN(y * 1.3 + 2)): e = (e + d - SQR((e - d) ^ 2 + 9)) / 2
    IF (same AND 4) = 0 THEN es = (TS - 1 - y) - (r + .5 + SIN(x * .9 + 1))
    IF (same AND 2) = 0 THEN d = (TS - 1 - x) - (r + .5 + SIN(y * 1.2)): es = (es + d - SQR((es - d) ^ 2 + 9)) / 2
    d = (e + es - SQR((e - es) ^ 2 + 9)) / 2
    LandPix% = 0
    IF k = 1 THEN
        IF d < -1 THEN EXIT FUNCTION
        IF d < 0 THEN LandPix% = 30: EXIT FUNCTION
        IF d < 1 THEN
            IF (x + y + f * 2) MOD 4 THEN LandPix% = 21 ELSE LandPix% = 7
            EXIT FUNCTION
        END IF
        LandPix% = Tex%(1, x, y, f)
        EXIT FUNCTION
    END IF
    IF d < 0 THEN
        IF es < 0 AND es >= -2 AND e >= 0 THEN LandPix% = 1
        EXIT FUNCTION
    END IF
    ' boulders: the nearest of four jittered middles a tile, and the next nearest for the cracks
    d1 = 99: d2 = 99
    FOR j = -1 TO 2
        FOR i = -1 TO 2
            r = VAL(MID$("31524031", ((i + 2) MOD 2) * 2 + ((j + 2) MOD 2) * 4 + 1, 1))
            cx = i * 8 + 4 + r - 2: cy = j * 8 + 4 + (r * 3 MOD 5) - 2
            d = SQR((x - cx) ^ 2 + (y - cy) ^ 2)
            IF d < d1 THEN
                d2 = d1: d1 = d: bx = x - cx: by = y - cy
            ELSEIF d < d2 THEN
                d2 = d
            END IF
        NEXT
    NEXT
    IF d2 - d1 < 1.3 THEN LandPix% = 4: EXIT FUNCTION
    r = (bx + by) / (d1 + d2)
    c = 5
    IF r < -.25 THEN c = 19
    IF r > .3 THEN c = 23
    IF r > .55 OR e < 1 OR es < 1 THEN c = 4
    IF (x * 5 + y * 3) MOD 7 = 0 AND c = 5 THEN c = 23
    LandPix% = c
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
    CASE UWARD: UDmg% = 5
    CASE URAM: UDmg% = 22
    CASE ELSE: UDmg% = 0
    END SELECT
END FUNCTION

FUNCTION UHp% (k AS INTEGER)
    SELECT CASE k
    CASE UCRAWL: UHp% = 180
    CASE UWARD: UHp% = 55
    CASE ELSE: UHp% = 170
    END SELECT
END FUNCTION

FUNCTION UnitAt% (m() AS CellT, u() AS UnitT, wx AS INTEGER, wy AS INTEGER)
    DIM i AS INTEGER
    UnitAt% = 0
    IF wx < 0 OR wy < 0 OR wx >= MAPW OR wy >= MAPH THEN EXIT FUNCTION
    FOR i = 1 TO MAXU
        IF u(i).alive THEN
            IF u(i).x = wx AND u(i).y = wy THEN UnitAt% = i: EXIT FUNCTION
        END IF
    NEXT
    UnitAt% = m(wx, wy).occ
END FUNCTION

' A unit's look at u ahead and v to its right, in pixels from its middle: a colour, -1 none.
FUNCTION UnitPix% (ut AS INTEGER, tmi AS INTEGER, u AS SINGLE, v AS SINGLE, fr AS INTEGER)
    DIM c AS INTEGER, mc AS INTEGER, lc AS INTEGER, av AS SINGLE, au AS SINGLE, fs AS SINGLE
    mc = 10 + tmi * 2: lc = 11 + tmi * 2
    c = -1: av = ABS(v): au = ABS(u)
    SELECT CASE ut
    CASE UCRAWL
        IF av >= 6 AND av <= 10 AND au <= 10 THEN
            c = 14: IF (INT(u + 40) + fr * 2) MOD 4 < 2 THEN c = 4
        END IF
        IF u > 9 AND u <= 11.5 AND av <= 7 THEN c = 3
        IF u >= -10 AND u < 3 AND av < 6 THEN
            c = 14
            IF INT(u + 40) MOD 3 = 0 THEN c = 3
            IF av <= 3.5 AND u > -8.5 AND u < 1.5 THEN
                c = 9: IF (INT(u * 2) + INT(v * 2) + fr + 40) MOD 5 = 0 THEN c = 8
            END IF
            IF av >= 4.5 THEN c = mc
        END IF
        IF u >= 3 AND u <= 9 AND av < 6 THEN
            c = lc
            IF u >= 5.5 AND u <= 7.5 AND av <= 3.5 THEN c = 6
        END IF
    CASE UWARD
        IF fr THEN fs = -3 ELSE fs = 3
        IF ABS(u - fs) <= 2 AND ABS(v + 4) <= 1.5 THEN c = 14
        IF ABS(u + fs) <= 2 AND ABS(v - 4) <= 1.5 THEN c = 14
        IF u * u + v * v <= 20 THEN c = mc
        IF (u - 1) ^ 2 + v * v <= 6 THEN c = lc
        IF u >= 2.5 AND u <= 4.2 AND av <= 1.5 THEN c = 15
        IF u >= 2 AND u <= 10 AND v >= 1.5 AND v <= 3.2 THEN c = 14
        IF u >= 9 AND u <= 10 AND v >= 1.5 AND v <= 3.2 THEN c = 0
    CASE URAM
        IF av >= 5 AND av <= 9 AND au <= 9.5 THEN
            c = 14: IF (INT(u + 40) + fr * 2) MOD 4 < 2 THEN c = 4
        END IF
        IF au <= 8 AND av < 5.5 THEN
            c = mc
            IF u > 5.5 THEN c = lc
            IF u < -6.5 THEN c = 4
        END IF
        IF u >= 1 AND u <= 12 AND av <= 1.2 THEN c = 14
        IF u > 11 AND u <= 12 AND av <= 1.2 THEN c = 15
        IF (u + 1) ^ 2 + v * v <= 13 THEN
            c = lc: IF (u + 1) ^ 2 + v * v >= 9 THEN c = mc
        END IF
        IF (u + 2) ^ 2 + (v + 1) ^ 2 <= 1.5 THEN c = 15
    END SELECT
    UnitPix% = c
END FUNCTION

' One step of unit i's orders.
SUB UnitTick (g AS GameT, tm() AS TeamT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fd() AS INTEGER, fk() AS FieldT, i AS INTEGER, dt AS SINGLE)
    DIM r AS INTEGER, d AS SINGLE, mill AS INTEGER, tx AS SINGLE, ty AS SINGLE, t AS INTEGER, goal AS INTEGER
    DIM moving AS INTEGER, here AS INTEGER
    u(i).cool = u(i).cool - dt
    moving = u(i).nx <> u(i).x OR u(i).ny <> u(i).y
    here = u(i).y * MAPW + u(i).x
    SELECT CASE u(i).st
    CASE SIDLE
        IF moving THEN r = MoveStep%(g, m(), u(), fd(), fk(), i, here, 0, dt)
        IF u(i).kind = UCRAWL THEN
            IF u(i).team = 1 THEN u(i).st = SSEEK
        ELSE
            u(i).scan = u(i).scan - dt
            IF u(i).scan <= 0 THEN
                u(i).scan = .5
                IF u(i).team = 0 THEN d = URange!(u(i).kind) + 1 ELSE d = USight%(u(i).kind) + 1
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
        IF moving = 0 AND m(u(i).x, u(i).y).terr = TMOSS THEN
            u(i).st = SGATHER: u(i).work = 0
        ELSE
            r = MoveStep%(g, m(), u(), fd(), fk(), i, MOSSKEY, 0, dt)
            IF r = 2 THEN u(i).st = SIDLE
        END IF
    CASE SGATHER
        u(i).work = u(i).work + dt
        u(i).anim = u(i).anim + dt
        IF u(i).work >= .45 THEN
            u(i).work = 0
            IF m(u(i).x, u(i).y).terr = TMOSS THEN
                m(u(i).x, u(i).y).amt = m(u(i).x, u(i).y).amt - 1
                u(i).load = u(i).load + 1
                IF m(u(i).x, u(i).y).amt <= 0 THEN DepleteMoss g, m(), fk(), u(i).x, u(i).y
            END IF
            IF u(i).load >= LOADMAX THEN
                u(i).st = SRETURN
            ELSEIF m(u(i).x, u(i).y).terr <> TMOSS THEN
                u(i).st = SSEEK
            END IF
        END IF
    CASE SRETURN
        mill = NearestMill%(u(), bl(), i)
        IF mill = 0 THEN
            IF moving THEN r = MoveStep%(g, m(), u(), fd(), fk(), i, here, 0, dt)
            u(i).st = SIDLE
            IF u(i).team = 0 THEN Say g, "The Crawler needs a Mill"
        ELSE
            r = MoveStep%(g, m(), u(), fd(), fk(), i, (bl(mill).y + 2) * MAPW + bl(mill).x, -1, dt)
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

' What the Lodge sees now (and so has seen): round its units and buildings.
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
    CASE UWARD: URange! = 3.5
    CASE URAM: URange! = 4.5
    CASE ELSE: URange! = 0
    END SELECT
END FUNCTION

FUNCTION URate! (k AS INTEGER)
    IF k = URAM THEN URate! = 1.6 ELSE URate! = .7
END FUNCTION

FUNCTION USight% (k AS INTEGER)
    USight% = k + 2
END FUNCTION

FUNCTION USpeed! (k AS INTEGER)
    SELECT CASE k
    CASE UCRAWL: USpeed! = 1.3
    CASE UWARD: USpeed! = 2.4
    CASE ELSE: USpeed! = 1.7
    END SELECT
END FUNCTION

SUB Order (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, wx AS INTEGER, wy AS INTEGER)
    DIM i AS INTEGER, e AS INTEGER, b AS INTEGER, got AS INTEGER, tk AS INTEGER, ti AS INTEGER
    e = UnitAt%(m(), u(), wx, wy)
    IF e THEN
        IF u(e).team = 1 AND m(wx, wy).vis THEN tk = 1: ti = e
    END IF
    b = m(wx, wy).bld
    IF tk = 0 AND b THEN
        IF bl(b).team = 1 THEN tk = 2: ti = b
    END IF
    FOR i = 1 TO MAXU
        IF u(i).sel THEN
            got = -1
            u(i).agg = 0
            u(i).gx = wx: u(i).gy = wy
            IF u(i).kind = UCRAWL THEN
                u(i).st = SMOVE
                IF m(wx, wy).terr = TMOSS THEN u(i).st = SSEEK
                IF b THEN
                    IF bl(b).team = 0 AND bl(b).kind = BMILL THEN u(i).st = SRETURN
                END IF
            ELSEIF tk THEN
                u(i).st = SATTACK: u(i).tk = tk: u(i).ti = ti
            ELSE
                u(i).st = SMOVE
            END IF
        END IF
    NEXT
    IF got THEN
        Sfx g, 6
        AddFx fx(), FXMARK, wx * TS + TS \ 2, wy * TS + TS \ 2, 0, 0, .4
    END IF
END SUB

' A texture's colour at x, y of a tile: kind 1 water (ph its frame), 2 rock. Its waves repeat
' every tile, so like tiles join without a seam; shades by ordered dithering.
FUNCTION Tex% (kind AS INTEGER, x AS INTEGER, y AS INTEGER, ph AS INTEGER)
    CONST TAU = 6.28318 / TS
    DIM n AS SINGLE, b AS SINGLE, i AS INTEGER
    b = VAL("&H" + MID$("08A2C4E63B19F7D5", (y MOD 4) * 4 + x MOD 4 + 1, 1)) / 16
    SELECT CASE kind
    CASE 1
        ' swells running east, a glint on their crests
        n = SIN((y * 2 + x * .5 + ph * 4) * TAU) + .4 * SIN((x - ph * 4) * TAU)
        i = INT((n + 1.4) / 2.8 * 1.8 + b)
        IF i < 0 THEN i = 0 ELSE IF i > 1 THEN i = 1
        Tex% = VAL(MID$("0620", i * 2 + 1, 2))
        IF n > 1.2 AND (x + y + ph * 3) MOD 5 = 0 THEN Tex% = 7
    CASE ELSE
        n = SIN(x * TAU * 2 + .7) * SIN(y * TAU + 1.1) + .8 * COS((x - 2 * y) * TAU) + .5 * SIN((x + y) * TAU * 2)
        i = INT((n + 2.3) / 4.6 * 3 + b)
        IF i < 0 THEN i = 0 ELSE IF i > 2 THEN i = 2
        Tex% = VAL(MID$("040519", i * 2 + 1, 2))
        IF ABS(SIN((x * 3 + y * 5) * TAU * .5) + COS((x * 2 - y * 3) * TAU * .5)) < .12 THEN Tex% = 4
    END SELECT
END FUNCTION

' Grass, 4 by 4 tiles at 0, 0: Perlin noise in three octaves, each repeating every 4 tiles,
' so the 16 pieces join every way; four shades, ordered dithering, and a few blades.
SUB GrassField ()
    DIM ga(2, 15, 15) AS SINGLE, o AS INTEGER, l AS INTEGER, i AS INTEGER, j AS INTEGER
    DIM px AS INTEGER, py AS INTEGER, gx AS SINGLE, gy AS SINGLE, fx AS SINGLE, fy AS SINGLE
    DIM i1 AS INTEGER, j1 AS INTEGER, u AS SINGLE, v AS SINGLE, a AS SINGLE, b AS SINGLE
    DIM n AS SINGLE, amp AS SINGLE, n00 AS SINGLE, n10 AS SINGLE, n01 AS SINGLE, n11 AS SINGLE
    DIM c AS INTEGER, w AS INTEGER
    w = 4 * TS
    FOR o = 0 TO 2
        l = 4 * 2 ^ o
        FOR j = 0 TO l - 1: FOR i = 0 TO l - 1: ga(o, i, j) = RND * 6.28318: NEXT: NEXT
    NEXT
    FOR py = 0 TO w - 1
        FOR px = 0 TO w - 1
            n = 0: amp = 1
            FOR o = 0 TO 2
                l = 4 * 2 ^ o
                gx = (px + .5) * l / w: gy = (py + .5) * l / w
                i = INT(gx): j = INT(gy): fx = gx - i: fy = gy - j
                i1 = (i + 1) MOD l: j1 = (j + 1) MOD l
                n00 = COS(ga(o, i, j)) * fx + SIN(ga(o, i, j)) * fy
                n10 = COS(ga(o, i1, j)) * (fx - 1) + SIN(ga(o, i1, j)) * fy
                n01 = COS(ga(o, i, j1)) * fx + SIN(ga(o, i, j1)) * (fy - 1)
                n11 = COS(ga(o, i1, j1)) * (fx - 1) + SIN(ga(o, i1, j1)) * (fy - 1)
                u = fx * fx * fx * (fx * (fx * 6 - 15) + 10)
                v = fy * fy * fy * (fy * (fy * 6 - 15) + 10)
                a = n00 + u * (n10 - n00): b = n01 + u * (n11 - n01)
                n = n + amp * (a + v * (b - a))
                amp = amp * .5
            NEXT
            b = VAL("&H" + MID$("08A2C4E63B19F7D5", (py MOD 4) * 4 + px MOD 4 + 1, 1)) / 16
            c = INT(n * 2.6 + 2 + b - .5 + (RND - .5) * .4)
            IF c < 0 THEN c = 0 ELSE IF c > 3 THEN c = 3
            PSET (px, py), VAL(MID$("01021617", c * 2 + 1, 2))
        NEXT
    NEXT
    ' blades, lit at the tip
    FOR i = 1 TO 70
        px = RND * (w - 1): py = 2 + RND * (w - 3)
        PSET (px, py), 1: PSET (px, py - 1), 17: PSET (px, py - 2), 16
    NEXT
END SUB

' Overlay k on black (black left as the grass): 0 dirt, 1 flowers, 2 to 4 moss thin to thick,
' 5 a fungus tree, then rock and water by their sides.
SUB PaintOverlay (k AS INTEGER)
    DIM x AS INTEGER, y AS INTEGER, c AS INTEGER, i AS INTEGER, j AS INTEGER, d AS SINGLE
    DIM cx AS INTEGER, cy AS INTEGER, h AS INTEGER
    IF k >= OROCK THEN
        FOR y = 0 TO TS - 1
            FOR x = 0 TO TS - 1
                IF k >= OWATER THEN
                    c = LandPix%(1, (k - OWATER) MOD 16, (k - OWATER) \ 16, x, y)
                ELSE
                    c = LandPix%(0, k - OROCK, 0, x, y)
                END IF
                IF c THEN PSET (x, y), c
            NEXT
        NEXT
        EXIT SUB
    END IF
    SELECT CASE k
    CASE 0
        ' bare earth, its edge ragged and thinning into the grass
        FOR y = 0 TO TS - 1
            FOR x = 0 TO TS - 1
                d = SQR((x - 7.5) ^ 2 + (y - 7.5) ^ 2) + SIN(Ang!(x - 7.5, y - 7.5) * 3 + 1) * .9
                c = 0
                IF d < 5.5 THEN
                    c = 3: IF RND < .25 THEN c = 18
                ELSEIF d < 7.5 THEN
                    IF (x + y) MOD 2 = 0 OR RND < .3 THEN c = 3
                END IF
                IF c THEN PSET (x, y), c
            NEXT
        NEXT
        FOR i = 1 TO 3
            x = 4 + RND * 7: y = 4 + RND * 7
            PSET (x, y), 18: PSET (x, y + 1), 30
        NEXT
    CASE 1
        ' a flower or two, a pebble
        FOR i = 1 TO 2
            x = 2 + RND * 11: y = 2 + RND * 11
            PSET (x, y), 13 + (i - 1) * 14: PSET (x, y + 1), 1
        NEXT
        x = 2 + RND * 11: y = 2 + RND * 11
        PSET (x, y), 19: PSET (x + 1, y), 4
    CASE 2, 3, 4
        ' moss: crystals glowing out of the grass, more the more there is
        FOR i = 1 TO (k - 1) * 3 + 1
            cx = 2 + RND * 12: cy = 5 + RND * 10
            h = 2 + RND * 2
            PSET (cx, cy + 1), 1: PSET (cx + 1, cy + 1), 1
            FOR j = 0 TO h
                c = 9: IF j > h \ 2 THEN c = 22
                IF j = h THEN c = 8
                PSET (cx, cy - j), c
            NEXT
            PSET (cx + 1, cy - h + 1), 22: PSET (cx - 1, cy - h + 2), 9
            PSET (cx, cy - h - 1), 31
        NEXT
    CASE 5
        ' a fungus tree: its shadow, stem, a cap with glowing spots
        FOR y = 0 TO TS - 1
            FOR x = 0 TO TS - 1
                IF ((x - 9) / 6) ^ 2 + ((y - 14) / 2) ^ 2 <= 1 THEN PSET (x, y), 1
                IF x >= 7 AND x <= 9 AND y >= 7 AND y <= 14 THEN
                    c = 3: IF x = 9 THEN c = 30
                    PSET (x, y), c
                END IF
                d = ((x - 8) / 7.5) ^ 2 + ((y - 6) / 4.5) ^ 2
                IF d <= 1 AND y <= 8 THEN
                    c = 5: IF d < .55 THEN c = 27
                    IF d < .2 AND y < 5 THEN c = 19
                    IF d > .82 THEN c = 4
                    IF y = 8 THEN c = 4
                    PSET (x, y), c
                END IF
            NEXT
        NEXT
        PSET (5, 4), 8: PSET (11, 5), 8: PSET (8, 3), 31: PSET (3, 6), 22
    END SELECT
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
    IF t = TGROUND OR t = TGROUND2 OR t = TDIRT OR t = TMOSS THEN Passable% = -1
END FUNCTION

SUB SpireTick (g AS GameT, m() AS CellT, u() AS UnitT, bl() AS BldgT, fx() AS FxT, fk() AS FieldT, b AS INTEGER, dt AS SINGLE)
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
        ' the beam's colour is the side's: x2 below 0 marks the Clade's
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
    g.snd.song = 0
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
            CASE "m": m(x, y).terr = TMOSS: m(x, y).amt = 6 + INT(RND * 5)
            CASE "T": m(x, y).terr = TTREE
            CASE ELSE: m(x, y).terr = TGROUND
            END SELECT
        NEXT
    NEXT
    FOR i = 1 TO MAXU: u(i) = blank: NEXT
    FOR i = 1 TO MAXB: bl(i) = noB: NEXT
    FOR i = 1 TO MAXFX: fx(i) = noFx: NEXT
    side.sKind = -1: side.hallKind = -1: side.denKind = -1
    tm(0) = side: tm(1) = side
    tm(0).cash = 900: tm(1).cash = 700
    ClearFields fk()
    g.now = 0: g.placeKind = -1: g.ms.dragOn = 0: g.msgT = 0: g.alarmT = -99: g.checkT = 0
    g.ai.think = 3: g.ai.wave = 0: g.ai.size = 3: g.ai.nextWave = 150: g.ai.alarmT = 0: g.ai.coin = 0

    ' the Lodge, south-west; the Clade, north-east
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 0, -1, 3, 24
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 0, 0, 7, 23
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, -1, 34, 3
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, 0, 30, 2
    PlaceBuilding g, tm(), m(), u(), bl(), fk(), 1, 1, 34, 6
    FOR i = 1 TO 2
        FOR k = 0 TO 1
            s = FindSpawn%(m(), bl(), FindB%(bl(), k, BHALL))
            s = SpawnUnit%(tm(), m(), u(), k, UWARD, s MOD MAPW, s \ MAPW)
        NEXT
    NEXT
    tm(0).built = 0: tm(1).built = 0
    UpdateVis g, m(), u(), bl()
    g.vw.miniDirty = -1: g.vw.miniT = 0
    CenterOn g.vw, 7, 22
    g.state = GPLAY
    Say g, "Harvest moss, build, find the Ash Clade"
    Sfx g, 5
END SUB

SUB Sprite (art() AS INTEGER, idx AS INTEGER, x AS INTEGER, y AS INTEGER)
    PUT (x, y), art(0, AMSK + idx), AND
    PUT (x, y), art(0, ASPR + idx), OR
END SUB
