' CHARLIE BARKLEY: Charlie the Maltese tears through the Amber Downs.
' Arrows run. Space, Z or Up jumps, curled. Down while running rolls;
' Down while still crouches: jump to wind up, let Down go to launch.
' M mutes, N turns the tune off; Esc leaves. SCREEN 13 on two pages: one drawn, one shown.
' Held keys come from the keyboard's port (60h); where it reads FFh,
' from INKEY$'s repeats. Physics in pixels and seconds, 60 steps a
' second; the pictures are DATA, drawn by tools/charlieart.py. First the
' intro, how the chase began: any key skips it.
defint A-Z

' Charlie. x, y: the feet. gsp: speed along the ground, ang its rise in
' radians, uphill to the right > 0, right round on a loop. sg: the stretch
' of the ground's line it is on, or left last; slip: the seconds the keys
' are no help, slid off a slope too steep for its speed.
type Hero
    x as single
    y as single
    vx as single
    vy as single
    gsp as single
    ang as single
    slip as single
    lastx as single
    charge as single
    want as single
    coyote as single
    inv as single
    anim as single
    deadT as single
    idleT as single
    ground as integer
    roll as integer
    jumped as integer
    crouch as integer
    charging as integer
    stun as integer
    face as integer
    skid as integer
    sprung as integer
    dead as integer
    sg as integer
    win as integer
    barkT as single
    swim as integer
    wet as single
    sled as integer
    snow as integer
    goat as integer
    rideT as single
end type

' The game's time against the screen's frames: lag, the time not yet stepped; span, a frame's length as learnt; late,
' what the game is behind the clock; n, the frames seen.
type Clock
    lag as single
    span as single
    late as single
    n as single
end type

' Held keys, and jump's press since the last step.
type Pad
    l as integer
    r as integer
    d as integer
    jump as integer
    hit as integer
    was as integer
    bark as integer
    quit as integer
    start as integer
    mute as integer
end type

type Game
    mode as integer
    bones as integer
    lives as integer
    score as long
    bonus as long
    tallied as integer
    sig as long
    clock as single
    camx as single
    camy as single
    modeT as single
    shake as single
    homeX as integer
    homeY as integer
    page as integer
    quitting as integer
end type

' A bone knocked loose: t its time left.
type Loose
    x as single
    y as single
    vx as single
    vy as single
    t as single
end type

' A gull: where it is, its home, where its dive is aimed, the seconds till it may dive again, what it is doing,
' the bones it carries, and whether it heads left.
type Bird
    x as single
    y as single
    hx as integer
    hy as integer
    tx as single
    ty as single
    t as single
    st as integer
    loot as integer
    face as integer
end type

type Part
    x as single
    y as single
    vx as single
    vy as single
    t as single
    c as integer
end type

' A thrown snowball on its arc.
type Lob
    x as single
    y as single
    vx as single
    vy as single
    live as integer
end type

' A cue of the intro's script: what, from beat t0 to t1, on its way from x0, y0 to x1, y1 in the world, y from the
' ground and up under 0. ease: 1 gathering speed, 2 losing it; hop: an arc that high over its way; a: its own.
type Cue
    kind as integer
    t0 as single
    t1 as single
    x0 as single
    y0 as single
    x1 as single
    y1 as single
    ease as integer
    hop as integer
    a as integer
end type

' Who flees with a keepsake at an act's end: x, y the foot of its cart, or the floor under its crane, v its speed,
' t the seconds its last bump or its breaking still tells, hits its bumps. st: 0 waiting, 1 fleeing, 2 broken and
' the keepsake lying, 3 the keepsake taken back. A crane's crate is one too: x, y its foot, v its fall, t the
' seconds to its next step. st: 0 on the hook once t is out, 1 held over Charlie, 2 falling.
type Runner
    x as single
    y as single
    v as single
    t as single
    hits as integer
    st as integer
end type

' The world: MW by MH tiles of TS pixels; lv.w columns used. A tile's kind
' is its profile, lv.prof: each column's solid height from its bottom.
const SW = 320, SH = 200, TS = 16, MW = 800, MH = 30, NONE = 30000
const STEPT = 1! / 60!, PI = 3.141593
const GRAV = 750!, JUMPV = 330!, JUMPCUT = 160!, TOP = 360!
const RUNACC = 280!, DEC = 1800!, FRIC = 280!, AIRACC = 500!, SLOPE = 380!
const RFRIC = 60!, RDEC = 450!, RUPS = 300!, RDNS = 1100!, MAXV = 900!
' On bare ice: what is left of the ground's braking and its friction, and of a run's push.
const ICEGRIP = .12, ICEPUSH = .4
const SPRINGV = 560!, CHARGE0 = 480!, CHARGEMAX = 360!, BOUNCEV = 330!
const BOOSTV = 660!, LOOPR = 72, LD = 188, PIPER = 48
' A jump asked for this long before landing is made on it; one this long after running off an edge still is.
const JBUFFER = .1, COYOTE = .1
' The ground's line: its most points, a loop's and a quarter pipe's stretches, the curves there can be; under STALL
' Charlie cannot stay on a wall or a roof.
const NPTS = 1600, LOOPN = 48, PIPEN = 12, NZONE = 47, STALL = 150!
' The moving platforms there can be, and their width.
const NMOVE = 23, MOVEW = 48
' a boat's deck over the water it floats on, and over the floor it lies on
const FREEB = 7, HULL = 11
const HW = 7, HH = 24, STRIPH = 192, MIDH = 80
const MAXOBJ = 600, NLOOSE = 24, NPART = 48
const TITLE = 0, PLAYING = 1, CLEARED = 2, OVER = 3, INTRO = 4
const FIDLE = 0, FBLINK = 1, FTAP = 2, FRUN = 4, FDASH = 10, FBALL = 12
const FSKID = 16, FCROUCH = 17, FHURT = 18, FUP = 19, FWIN = 20, FSHIVER = 22, NF = 23

' What stands in the level, by x. From KLOOP on: scenery, drawn behind
' the ground.
const KBONE = 1, KCAT = 2, KSPRING = 3, KMARK = 4, KBOOST = 5, KGOAL = 6, KPLAT = 7
const KLOOP = 8, KTREE = 9, KBUSH = 10, KFLOWER = 11, KROCK = 12, KTHORN = 13, KLAMP = 14, KFAM = 15, KPIPE = 16
const KMOVE = 17, KPOST = 18, KTANK = 19, KVENT = 20, KJET = 21, KBUMP = 22, KSPIN = 23, KHOOP = 24
const CABLED = 64, CABLEH = 44, KBUTTON = 27, KCRAB = 28, KJELLY = 29, KBUOY = 30
' A span of the sands' old pier: the seconds it totters once trodden, and how far it falls.
const KSPAN = 31, SPANT = .25, SPANDROP = 400
' The works' own: a press and a leg of its gantry; a sweeper and a spark's carriage. A spark goes SPARKR either way
' along its rail, alight SPARKLIT steps of every SPARKP. A press hangs PRESSH over the floor, its head's foot
' PRESSUP over it when up; of its turn's PRESSP steps it is up till PRESSFALL, down by ten on, held till PRESSRISE,
' then rising; a press a step later for every PRESSLAG along, so a row's turns go along it with a run.
const KPRESS = 32, KLEG = 33, KSWEEP = 43, KSPARK = 44, SPARKR = 40, SPARKP = 150, SPARKLIT = 90
' The pass's own: deep snow, and a sled. Boarded, a sled goes off at SLEDV at the least; the snow's friction on
' it is SLEDGRIP of a roll's; Charlie stands SLEDH over the ground on it.
const KDRIFT = 34, KSLED = 45, SLEDV = 120!, SLEDGRIP = .3, SLEDH = 5
' Rolled through at SNOWV or faster, deep snow packs round him.
const SNOWV = 100!
' A snow-thrower, a cat behind a wall of snow, its state's sign the way it faces: its turn is THROWC of the
' things' steps, ducked till THROWUP are left, then up and open; with THROWAT left it throws, if Charlie is
' within THROWR, a snowball LOBT in the air, and ducks. NLOB of them fly at once.
const KTHROW = 46, THROWC = 75, THROWUP = 30, THROWAT = 10, THROWR = 180, LOBT = .9, NLOB = 4
' An icicle: its state 0 hung, the things' steps since it was loosed, or -1 broken. It trembles TREMBLE of
' them, then falls.
const KICICLE = 35, TREMBLE = 10
' A ski lift's chair, a platform that goes up its cable SEATV of the way a second while he stands on it; its
' frame stands SEATH over its seat.
const KSEAT = 36, SEATV = .35, SEATH = 30
' A goat, its state's sign the way it faces: 1 at graze, at home, till it sees him within GOATSEE on its ledge; then its
' head is down GOATWIND of the things' steps and it charges, GOATSTEP a step, till it is under or past him or
' GOATRUN from home, and stands dazed GOATDAZE steps; 0 while it is ridden or on its way home. Its lowered horns
' hurt. Come down on, it bolts on along the road, to the right, at GOATV, he astride GOATH up, till he is off the
' ground, stopped or GOATRIDE seconds on; then it runs home at STRAYV.
const KGOAT = 47, GOATSEE = 72, GOATWIND = 12, GCHARGE = 100, GOATDAZE = 30, GOATSTEP = 8, GOATRUN = 64
const GOATV = 240!, GOATH = 14, GOATRIDE = 2!, STRAYV = 300!
' A blizzard: of every GUSTP ticks the wind blows the last GUSTON, from the right, the snow streaming GUSTTELL
' before it does. On his feet it bears him back, WINDG a second each second on the ground and WINDA in the air,
' curled or not, till he goes with it at WINDV; curled or crouched on the ground, or mounted, it passes over him.
const GUSTP = 192, GUSTON = 72, GUSTTELL = 24, WINDG = 600!, WINDA = 900!, WINDV = 200!, CFLAKE = 118
' The avalanche: let go as he passes lv.avalX, its front comes on from AVALLEAD behind at AVALV, the snow AVALH deep
' behind it, and takes him where it reaches him.
const AVALV = 300!, AVALLEAD = 220, AVALH = 70, CSNOW = 117, CICE = 123
' An ice wall, WALLH tall, stops him WALLR from its middle: only a goat ridden breaks it.
const KWALL = 37, WALLH = 80, WALLR = 15
' A ledge's load of snow: its state 0 heaped, the things' steps since a bark set it off, or -1 gone. It trembles
' TREMBLE of them, slides LOADRUN along its ledge in LOADSLIDE more, gathering way, then falls at the speed it left
' with, to its heap's place LOADFAR on: there cats and snow-throwers are beaten. Its heap: its state 1 once it lies
' there, HEAPH tall and HEAPR either way at its foot; it stops him.
const KLOAD = 48, KHEAP = 49, LOADRUN = 40, LOADSLIDE = 20, LOADFAR = 104, HEAPH = 30, HEAPR = 24
const PRESSH = 112, PRESSUP = 68, PRESSP = 180, PRESSFALL = 90, PRESSRISE = 130, PRESSLAG = 4
' From KFRONT on: drawn over the ground. The sands' flume: a column of its fall, its level end, the tank at its
' head; the speed its water carries Charlie at.
const KFRONT = 40, KFLUME = 40, KSPOUT = 41, KCISTERN = 42, FLUMEV = 460!
const KWAVE = 25, KWAVE2 = 26, WAVEA = 96, WAVEH = WAVEA + 36, WAVEL = 192
const JETV = 500!, BUMPV = 400!, HOOPV = 640!, SPINR = 30, SPINH = 70, PITD = 5

' Images in art.img from art.at(k); a masked one's mask at k + 1. As charlieart.py's.
const IBG = 0, IMID = 1, ILOOP = 5, ITITLE = 7
const ITILE = 10, IHERO = 34, IBONE = IHERO + NF * 4
const ICAT = IBONE + 8, ISPRING = ICAT + 8, IBOOST = ISPRING + 4, IMARK = IBOOST + 4
const IGOAL = IMARK + 4, ITREE = IGOAL + 8, IBUSH = ITREE + 2, IFLOWER = IBUSH + 2
const IROCK = IFLOWER + 4, ITHORN = IROCK + 2, ILAMP = ITHORN + 2, ITILT = ILAMP + 2
const IDASH = ITILT + 324
const ICARD = IDASH + 2, IGLYPH = ICARD + 2, NGLYPH = 27, IHUD = IGLYPH + NGLYPH * 4, IPIPE = IHUD + 2
' The Chrome Works' own: built here, then charlieart.py's.
const IZ2 = IPIPE + 2, ITILE2 = IZ2 + 9, IMOVE = ITILE2 + 24, IPOST = IMOVE + 2, ITANK = IPOST + 2, IVENT = ITANK + 2
const ICARD2 = IVENT + 2, IBLD = ICARD2 + 2, IPLANT = IBLD + 12
const IDK = IPLANT + 8, IWT = IDK + 4, IWS = IWT + 32
const IJET = IWS + 4, IBUMP = IJET + 6, ISPIN = IBUMP + 4, IHOOP = ISPIN + 4, IWAVE = IHOOP + 4, IBTN = IWAVE + 8
' Amber Downs' ground, by the comb's column of 4 and row of 3: flats, slopes, the tile under each slope, the comb
' alone and in strips, quarter pipes; its moving platform.
const IAF = IBTN + 4, IAS = IAF + 12, IAU = IAS + 144, IAC = IAU + 72, IAL = IAC + 12, IAP = IAL + 4
const IAM = IAP + 24, IAE = IAM + 2
' The works' slopes, by the deck's four 16s: each kind's tile, then what of its deck runs on into the tile under.
const IZS = IAE + 8, IZU = IZS + 48
' Charlie round a ribbon's twist, 8 pictures of two frames, then facing left: stood on it, pressed flatter twice,
' seen from over its back, hung under it flatter twice and whole, seen from under.
const ITW = IZU + 48, ICATF = ITW + 96
' the intro's: the van, a wheel at two turns, the van's back door open, Charlie at its window and looking back
' from it; the old house, the boxes and one thrown, a hedge, the Mob's float, the basket
const IVAN = ICATF + 4, IWHEEL = IVAN + 2, IDOOR = IWHEEL + 4, IPEEK = IDOOR + 2, IHOUSE = IPEEK + 4
const IBOXES = IHOUSE + 2, ICRATE = IBOXES + 2, IHEDGE = ICRATE + 2, IFLOAT = IHEDGE + 2
const IBASKET = IFLOAT + 6
' Biscuit's honey cart less its wheels, as it goes and bumped: level, then leaning half a gentle slope's fall and
' all of one's, CARTLEAN higher over and under each time; then its wheel at two turns. Its wheels are WHEELS
' either side of its middle.
const ICART = IBASKET + 2, ICARTW = ICART + 12, CARTW = 56, CARTH = 34, CARTLEAN = 7, WHEELS = 13
' Sprocket's crane as it goes and bumped, a length of its rail, its crate, the toolbox.
const ICRANE = ICARTW + 4, IRAIL = ICRANE + 4, IWBOX = IRAIL + 2, ITOOLS = IWBOX + 2
' The sands' ground again as it is under the tide, picture for picture, and the water's edge.
const IWET = ITOOLS + 2, WETO = IWET - IAF, ISURF = IWET + 278, ICRAB = ISURF + 2
const IJELLY = ICRAB + 6, IBUOY = IJELLY + 4, ICHAIR = IBUOY + 4, IGULL = ICHAIR + 4
const IPEDAL = IGULL + 8, ITEDDY = IPEDAL + 4
' The flume: its fall and its end, four frames each of running water, the water falling from it, its tank.
const IFLUME = ITEDDY + 2, ISPOUT = IFLUME + 8, IRUNNEL = ISPOUT + 8, ICISTERN = IRUNNEL + 8
' The works' sweeper, two frames going left and their mirrors; a spark's carriage dark and two frames alight, a
' tile of its rail; a press's housing, a length of its rod, its head.
const ISWEEP = ICISTERN + 2, ISPARK = ISWEEP + 8, ISRAIL = ISPARK + 6, IPRESS = ISRAIL + 2
' The pass's keepsake, the fair one's camera.
' A sled as the ground lies, from a steep fall to the right to a steep rise; deep snow.
const ICAMERA = IPRESS + 6, ISLED = ICAMERA + 2, IDRIFT = ISLED + 10
' A geyser, where the works have a jet: its mouth, its column at two beats.
' The snowball he is in, at two turns.
const IGEYSER = IDRIFT + 2, ISNOWBALL = IGEYSER + 6
' A snow-thrower ducked and up, facing left, then their mirrors; its snowball.
const IFORT = ISNOWBALL + 4, ILOB = IFORT + 8, IICICLE = ILOB + 2, ISEAT = IICICLE + 2
' A goat's: at graze, then at a charge as the ground lies, five ways, two strides each; each with its mirror.
const IGOAT = ISEAT + 2, IGOATR = IGOAT + 4, IWALL = IGOATR + 40
' Frost on her toboggan as the ground lies, five ways; and knocked into deep snow.
const IFROST = IWALL + 4, IDUNKED = IFROST + 10
' Who meets Charlie, NFAM of them FAMSTEP apart, each in FAMF pictures: three of a wait, six of a cheer. The Baron's:
' four sat, his tail swaying and a blink, then two pointing.
const IFAM = IDUNKED + 2, NFAM = 5, FAMF = 9, FAMSTEP = 36, IBARON = IFAM + NFAM * FAMF * 2
' The pass's load of snow on its ledge, and down in a heap.
const ILOAD = IBARON + 12, IHEAP = ILOAD + 2, NIMG = IHEAP + 2
const HUDW = 108, HUDH = 40
' The levels: a zone's two acts, the second at another hour in other colours, zone by zone.
' the stages played, and after them the intro's road
const ZONES = 4, NSTAGE = ZONES * 2, SINTRO = NSTAGE
' the zones' names, 16 letters each; what marks CHARLIE.SAV as the game's: its zone is added to it
const ZNAMES = "AMBER DOWNS     CHROME WORKS    SALTWICK SANDS  WHITECAP PASS", SAVEMARK = 1128398848
' each zone's tune, its number in CHARLIE.DAT: a digit a zone
' The intro: BEATS a second, ILEN of them, the ground HORIZON down the screen between two bars BARH high. Its
' cues' kinds, in QNAMES' order, six letters a name.
const BEATS = 2!, ILEN = 112, HORIZON = 150, BARH = 14, NCUE = 112
' Between zones the bump again: its cues' first beat and how many, its song's number in CHARLIE.DAT.
const BUMP0 = 120, BUMPLEN = 16, BETWEEN = 12
const QCAM = 1, QHOUSE = 2, QBOXES = 3, QCRATE = 4, QHEDGE = 5, QFAM = 6, QVAN = 7, QFLOAT = 8
const QBARON = 9, QCAT = 10, QBASKET = 11, QEYES = 12, QHERO = 13, QWOOF = 14, QSND = 15
const QNAMES = "cam   house boxes crate hedge fam   van   float baron cat   basketeyes  hero  woof  snd   "
' the picture store's LONGs: for what every zone shows and the largest zone's own
const IMGSZ = 490000

const SCLEFT = 75, SCRIGHT = 77, SCUP = 72, SCDOWN = 80, SCSPACE = 57, SCZ = 44
const SJUMP = 0, SBONE = 1, SHURT = 2, SSPRING = 3, SPOP = 4, SROLL = 5
const SCHARGE = 6, SLAUNCH = 7, SCLEAR = 8, SDIE = 9, SMARK = 10, SBOOST = 11, SHISS = 12, SBARK = 13, SBUMP = 14
const SSQUAWK = 15, SCRACK = 16, SCLANK = 17
' A bark: how far it carries, the seconds to the next, and the steps a cat stands startled by it.
const BARKR = 72, BARKT = .4, FLINCH = 24
' A crab: the steps it keeps to its shell once barked at, and how fast Charlie goes up off its back.
const SHELLT = 150, STEPV = 430!
' How fast Charlie goes up off a jellyfish's bell, and how far under the sea's top its arms' ends float.
const JELLYV = 480!, JELLYD = 10
' A gull: how near Charlie passes under it for a dive, its dive's speed and its climb's, its rest between dives,
' the bones it takes, how far a bark reaches it; and what it is doing: gone is 0.
const NGULL = 6, GULLSEE = 100, DIVEV = 210!, CLIMBV = 90!, GULLREST = 2.5, GULLTAKE = 5, GULLHEAR = 130
const GHOVER = 1, GDIVE = 2, GCLIMB = 3, GSCARED = 4
' Kipper's pedal boat: its speed, what each bump adds to it, its length, and its deck over the sea.
const PEDALV = 130!, PEDALMORE = 40!, PEDALW = 48, PEDALUP = 8
' A chase: the cart's speed, when Charlie is far behind it, and away from a bump, a toboggan's less, for a sled to
' catch it again; how far ahead of the race's start it waits; how near touches it; the seconds a bump tells, the
' last of them bolting; the bumps that break it; its tune's number in CHARLIE.DAT.
const CRUISE = 330!, DAWDLE = 200!, BOLT = 560!, TOBOLT = 400!, CARTLEAD = 260, CARTR = 26
const DAZED = 1.4, BOLTS = 1!, NBUMP = 3, CHASETUNE = 5
' The level's end's songs, and game over's, by their numbers in CHARLIE.DAT; how long game over stays, its tune's
' length.
const CAUGHTUP = 2, PACKING = 10, LEFTBEHIND = 11, OVERT = 13
' The bonus is counted over the level's end's bars its lead plays, the last six but two: a step a beat.
const TALLYN = 16, TALLYBARS = 6
' The works' crane: its speed after Charlie, what each bump adds, and away from one; how near touches it; its
' gondola's foot over the floor, out of a jump's reach, and its rail's top; the seconds to its next crate, what
' each bump takes off them, and the seconds one is held over Charlie; the view's height over him, by habit and
' under a crane; the seconds its keepsake falls.
const CRANEV = 90!, CRANEUP = 25!, CRANEBOLT = 260!, CRANER = 22, CRANEH = 120, RAILUP = 164
const RELOAD = 1.6, HASTE = .3, POISE = .6, HEAD = 124, HEADUP = 172, TOOLFALL = .4
' The tide: the steps of its turn; how deep Charlie floats, and how deep he wades before he does; his paddling
' speed and its gain; his hop out of the water; the seconds of rolling that shake him dry, and his top speed
' till then; the first of the sea's colours.
const TIDEP = 1440, AFLOAT = 6, WADE = 10, PADV = 80!, PADACC = 260!, HOPV = 250!, DRYT = .35, WETTOP = 220!
const CSEA = 141

' The palette's ramps, dark to light, as charlieart.py's.
const CHUD = 0, CWHITE = 1, CINK = 2, CYELLOW = 3, CRED = 4
const CSCARF = 21, CGRASS = 25, CEARTH = 30, CSTONE = 38, CGOLD = 42
const CWATER = 144, CHEDGE = 245
const CCHASE = 213

declare sub Main (wd as World)
declare sub Allot (wd as World)
declare function Rooted% (lv as Level, i as integer, lo as integer)
declare sub Advance (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part, cu() as Cue)
declare sub StartIntro (wd as World, g as Game, cu() as Cue)
declare sub LoadCues (cu() as Cue)
declare function IntroOver% (wd as World, g as Game, k as Pad, cu() as Cue)
declare sub CueAt (lv as Level, cu() as Cue, i as integer, t as single, x as integer, y as integer)
declare sub IntroDraw (wd as World, g as Game, p as Hero, cu() as Cue)
declare sub DrawCue (wd as World, cu() as Cue, i as integer, now as single, p as Hero, cx as integer, cy as integer)
declare sub DrawVan (art as Store, x as integer, y as integer, wx as integer, a as integer)
declare sub DrawFloat (art as Store, se as Session, x as integer, y as integer, wx as integer, a as integer)
declare sub DrawFamily (art as Store, se as Session, x as integer, y as integer, cheer as integer)
declare function FamPic% (se as Session, n as integer, cheer as integer)
declare function BaronPic% (se as Session, aim as integer)
declare sub DrawCat (zn as Realm, art as Store, se as Session, x as integer, y as integer, a as integer, west as integer)
declare sub DrawEyes (se as Session, x as integer, y as integer, n as integer)
declare sub Pose (lv as Level, gr as Ground, se as Session, p as Hero, x as integer, y as integer, a as integer, west as integer)
declare sub MuteKey (mu as Deck, k as Pad)
declare sub Shimmer (zn as Realm, tn as Tones, se as Session)
declare sub Spent (k as Pad)
declare sub ZoneTune (mu as Deck, id as integer)
declare sub SceneTune (mu as Deck, id as integer)
declare sub EndTune (lv as Level, mu as Deck)
declare sub StartBump (wd as World, g as Game, nxt as integer)
declare sub DrawKeep (zn as Realm, art as Store, x as integer, y as integer)
declare function Sudden% (se as Session, g as Game)
declare function EndBeat! (mu as Deck, se as Session, g as Game)
declare sub Tally (mu as Deck, se as Session, g as Game, k as Pad)
declare sub HushSong (mu as Deck)
declare sub Mend (lv as Level)
declare sub Slide (mu as Deck, se as Session, p as Hero)
declare sub DrawSpout (lv as Level, art as Store, se as Session, x as integer, y as integer, c as integer, cy as integer)
declare sub ResetChase (lv as Level, mu as Deck)
declare sub Flee (lv as Level, zn as Realm, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub StartChase (lv as Level, mu as Deck)
declare function MakesOff! (zn as Realm)
declare sub MoveCart (lv as Level, p as Hero, away as single)
declare sub MeetCart (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub BreakCart (lv as Level, mu as Deck, se as Session, pt() as Part)
declare sub TakeKeep (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part)
declare sub DrawCart (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
declare sub DrawFled (lv as Level, art as Store, se as Session, x as integer, y as integer, v as integer)
declare sub DrawWheels (lv as Level, art as Store, x as integer, cy as integer)
declare sub Bumped (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, y as single)
declare sub MoveCrane (lv as Level, p as Hero)
declare sub MeetCrane (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, pt() as Part)
declare sub DropBox (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub BreakBox (lv as Level, mu as Deck, se as Session, pt() as Part)
declare sub DrawChase (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
declare sub DrawCrane (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
declare function Headroom% (lv as Level, zn as Realm, p as Hero)
declare sub Tint (tn as Tones, k as integer, c as long)
declare sub Shade (tn as Tones)
declare function Met% (wd as World, g as Game, p as Hero, k as Pad, pt() as Part)
declare sub OpenScreen ()
declare sub Flip (page as integer)
declare sub LoadTables (lv as Level)
declare sub LoadLevel (wd as World, z as integer)
declare sub AddMover (lv as Level, x as integer, y as integer, a as integer, k as integer, e as integer)
declare sub Movers (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero)
declare function Swing! (lv as Level, se as Session, m as integer, quarter as integer)
declare sub GoStage (wd as World, n as integer, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare function ZoneOf% (se as Session, st as integer)
declare function Plated% (k as integer)
declare sub ReadPics (art as Store)
declare sub LoadZoneArt (art as Store, z as integer)
declare sub SeekLevel (st as integer)
declare sub Column (lv as Level, c as integer, g as integer, kind as integer)
declare sub AddObj (lv as Level, k as integer, x as integer, y as integer)
declare sub Shelf (lv as Level, c as integer, g as integer, s as integer, n as integer, w as integer)
declare sub SortObjs (lv as Level)
declare sub BuildArt (art as Store, tn as Tones)
declare sub LoadArt (art as Store, tn as Tones)
declare sub Unpack (art as Store)
declare function Word& ()
declare function Chars$ ()
declare sub OpenData ()
declare sub Mirror (art as Store, k as integer, k2 as integer, w as integer, h as integer)
declare sub Flop (art as Store, k as integer, k2 as integer, w as integer, h as integer)
declare sub Track (lv as Level, gr as Ground)
declare sub TrackPipe (lv as Level, gr as Ground, c as integer)
declare sub TrackTile (lv as Level, gr as Ground, c as integer)
declare sub TrackLoop (gr as Ground, x as integer, y as integer)
declare sub TrackPlats (lv as Level, gr as Ground)
declare sub Plank (gr as Ground, x as integer, x1 as integer, y as integer)
declare sub TrackMovers (lv as Level, gr as Ground)
declare function ButtonAt% (lv as Level, x as integer)
declare sub TrackRibbons (lv as Level, gr as Ground)
declare sub RibbonEnd (gr as Ground, x1 as integer, y as integer)
declare sub Faces (lv as Level)
declare function Side% (lv as Level, c as integer, n as integer)
declare sub Bare (art as Store, x as integer, y0 as integer, n as integer, k as integer)
declare sub DrawCables (lv as Level, cx as integer, cy as integer)
declare sub Cycle (tn as Tones, se as Session)
declare function Ribbon% (gr as Ground, x as integer)
declare function Twist! (gr as Ground, p as Hero)
declare sub DrawOver (lv as Level, zn as Realm, art as Store, cx as integer, cy as integer)
declare sub Bark (lv as Level, mu as Deck, se as Session, p as Hero)
declare sub MeetCrab (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
declare sub MeetSweep (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
declare sub MeetPress (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
declare sub Beaten (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, i as integer, dy as integer)
declare function Lit% (lv as Level, se as Session, i as integer)
declare function Slick% (lv as Level, gr as Ground, p as Hero)
declare sub Weed (lv as Level, x as integer)
declare sub Clearing (lv as Level, c as integer)
declare sub Board (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
declare sub Alight (mu as Deck, se as Session, p as Hero, pt() as Part, c as integer)
declare function PressT% (lv as Level, se as Session, i as integer)
declare function Stroke% (lv as Level, se as Session, i as integer)
declare function PressAhead% (lv as Level, x as integer)
declare sub DrawSpark (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer, cx as integer)
declare sub DrawPress (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer)
declare sub DrawLeg (art as Store, x as integer, y as integer)
declare function CrabPic% (se as Session, s as integer)
declare sub MeetJelly (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
declare function Bob% (lv as Level, x as integer, h as integer, up as integer)
declare function Sunk% (lv as Level, y as integer)
declare sub MovePedalo (lv as Level)
declare sub MeetPedalo (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, pt() as Part)
declare sub DrawPedalo (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
declare function Glide% (b as Bird, x as single, y as single, v as single)
declare sub Spill (se as Session, b as Bird, lo() as Loose)
declare sub Gull (mu as Deck, se as Session, b as Bird, n as integer, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub Flock (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub DrawGulls (lv as Level, art as Store, se as Session, cx as integer, cy as integer)
declare function Adrift% (lv as Level, m as integer)
declare function Dip% (lv as Level, m as integer)
declare sub Flinch (lv as Level, i as integer, fromX as integer)
declare sub Prowl (lv as Level, i as integer)
declare sub DrawBark (p as Hero, cx as integer, cy as integer)
declare function CatPic% (se as Session, s as integer)
declare sub Pit (lv as Level, c as integer, g as integer, n as integer)
declare sub Knot (gr as Ground, x as single, y as single, k as integer)
declare function Bearing! (u as single, v as single)
declare function SegAt% (lv as Level, gr as Ground, x as integer, y as single)
declare function TileSeg% (gr as Ground, x as integer, y as single)
declare function PlatSeg% (gr as Ground, x as integer, y as single)
declare sub Along (gr as Ground, p as Hero, d as single)
declare sub Crease (p as Hero, t as single, dt as single, l as single, ahead as integer)
declare sub Drop (gr as Ground, p as Hero)
declare sub Settle (gr as Ground, p as Hero, s as integer, d as integer)
declare function Curve% (gr as Ground, p as Hero, x0 as single, y0 as single, d as integer)
declare sub LoopHalf (gr as Ground, p as Hero, z as integer, x0 as single, a as integer, b as integer)
declare sub Grab (art as Store, x1 as integer, y1 as integer, x2 as integer, y2 as integer, k as integer)
declare sub MaskGrab (art as Store, x as integer, y as integer, w as integer, h as integer, mx as integer, my as integer, k as integer)
declare sub Blit (art as Store, x as integer, y as integer, k as integer, w as integer, h as integer)
declare sub Poll (kb as Keys)
declare sub ReadPad (kb as Keys, mu as Deck, k as Pad)
declare function Held2% (kb as Keys, s as integer, t as single)
declare sub Pace (d as single, c as Clock)
declare sub ByClock (d as single, c as Clock)
declare function Steps% (c as Clock)
declare function Elapsed! (a as single, b as single)
declare sub Sfx (mu as Deck, se as Session, id as integer)
declare sub LoadSongs (mu as Deck)
declare sub LoadSong (mu as Deck, s as integer, id as integer)
declare sub ReadKit (mu as Deck, s as integer)
declare sub ReadList (a() as string)
declare sub ReadBar (mu as Deck, s as integer, n as integer, bas() as string, hits() as string, pat() as string, lead as integer)
declare sub FeedSong (mu as Deck, se as Session)
declare sub NextHalf (mu as Deck, se as Session)
declare sub StartSong (mu as Deck, se as Session)
declare sub PlayHalf (mu as Deck, sg as Song, b as integer, loud as integer)
declare sub Patch (sg as Song)
declare sub PickSong (mu as Deck, n as integer, late as integer)
declare function SongFor% (g as Game)
declare function Vol$ (level as integer, loud as integer, up as integer)
declare function Beat$ (mu as Deck, s as integer, x as string, loud as integer)
declare function Riff$ (p as string, h as integer, low as integer)
declare function Surf% (lv as Level, sx as integer, y as integer)
declare function Solid% (lv as Level, x as integer, y as integer)
declare function Wall% (lv as Level, p as Hero, dir as integer)
declare function Land% (lv as Level, gr as Ground, p as Hero, d as integer)
declare sub Tick (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
declare sub TitleStep (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
declare sub PlayStep (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
declare sub StageClear (mu as Deck, se as Session, g as Game)
declare sub ClearedStep (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
declare sub Pilot (lv as Level, gr as Ground, zn as Realm, se as Session, p as Hero, k as Pad)
declare sub PressOn (k as Pad)
declare sub HoldBack (p as Hero, k as Pad, v as integer)
declare function PilotSwim% (lv as Level, zn as Realm, p as Hero, k as Pad)
declare function PilotRide% (lv as Level, gr as Ground, se as Session, p as Hero, k as Pad)
declare function PilotRun% (lv as Level, gr as Ground, zn as Realm, se as Session, p as Hero, k as Pad)
declare sub PilotPress (lv as Level, se as Session, p as Hero, k as Pad, want as integer)
declare sub PilotGoat (lv as Level, p as Hero, k as Pad, want as integer)
declare sub PilotGaps (lv as Level, se as Session, p as Hero, k as Pad, want as integer)
declare function Ready% (lv as Level, se as Session, m as integer)
declare sub PilotFall (lv as Level, zn as Realm, p as Hero, k as Pad)
declare sub PilotGale (lv as Level, se as Session, p as Hero, k as Pad, want as integer)
declare sub ResetWorld (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub StartGame (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub LoadSave (se as Session)
declare sub Reach (se as Session, z as integer)
declare sub PickZone (se as Session, k as Pad)
declare function ZoneName$ (z as integer)
declare sub Flow (lv as Level, se as Session)
declare sub Soak (lv as Level, se as Session, p as Hero, pt() as Part)
declare sub Dunk (lv as Level, se as Session, p as Hero, pt() as Part)
declare sub Paddle (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero, l as integer, r as integer, hit as integer, pt() as Part)
declare sub DrawSea (lv as Level, cx as integer, cy as integer)
declare sub SeaBand (x0 as integer, x1 as integer, a as integer, b as integer, last as integer, c as integer)
declare sub DrawSurf (lv as Level, art as Store, cx as integer, cy as integer)
declare sub Spawn (lv as Level, gr as Ground, zn as Realm, mu as Deck, g as Game, p as Hero)
declare sub MoveHero (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero, k as Pad, pt() as Part)
declare sub KeepJump (mu as Deck, se as Session, p as Hero, hit as integer)
declare sub Walk (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero, l as integer, r as integer, d as integer, hit as integer, pt() as Part)
declare function Winding% (mu as Deck, se as Session, p as Hero, d as integer, hit as integer, pt() as Part)
declare sub Leap (mu as Deck, se as Session, p as Hero)
declare sub RollOn (p as Hero, l as integer, r as integer, grip as single)
declare sub RunOn (mu as Deck, se as Session, p as Hero, l as integer, r as integer, d as integer, grip as single, push as single, pt() as Part)
declare sub Shove (p as Hero, way as integer, grip as single, push as single)
declare sub Falter (gr as Ground, p as Hero)
declare sub Airborne (lv as Level, gr as Ground, se as Session, p as Hero, l as integer, r as integer, d as integer, hold as integer, pt() as Part)
declare sub Steer (p as Hero, way as integer)
declare sub Bonk (lv as Level, p as Hero)
declare sub Slow (v as single, f as single)
declare sub Touch (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub MeetBone (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
declare sub MeetCat (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
declare sub Tackle (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, dy as integer, lo() as Loose, pt() as Part)
declare sub MeetIcicle (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
declare sub MeetSled (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
declare sub MeetDrift (lv as Level, mu as Deck, se as Session, p as Hero, i as integer, pt() as Part)
declare sub MeetSpark (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
declare sub MeetSpring (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
declare sub MeetBoost (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
declare sub MeetFlume (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
declare sub MeetMark (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
declare sub MeetButton (lv as Level, mu as Deck, se as Session, p as Hero, i as integer, pt() as Part)
declare sub MeetJet (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
declare sub MeetBump (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
declare sub MeetSpin (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
declare sub MeetHoop (lv as Level, mu as Deck, se as Session, p as Hero, i as integer, pt() as Part)
declare sub Hurt (mu as Deck, se as Session, g as Game, p as Hero, fromX as integer, lo() as Loose, snd as integer)
declare sub Lose (mu as Deck, se as Session, p as Hero)
declare sub Dying (lv as Level, gr as Ground, zn as Realm, mu as Deck, g as Game, p as Hero)
declare sub Things (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub Totter (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, i as integer)
declare sub TakeAim (lv as Level, i as integer, p as Hero)
declare sub Unload (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part, i as integer)
declare sub Bury (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, h as integer)
declare sub LooseBones (lv as Level, se as Session, lo() as Loose, dt as single)
declare sub Particles (se as Session, pt() as Part, dt as single)
declare function Risen% (lv as Level, i as integer)
declare function Fallen% (lv as Level, i as integer)
declare function Plunged% (n as integer)
declare function Slid% (lv as Level, i as integer)
declare function HeapOf% (lv as Level, i as integer)
declare function Lull% (lv as Level, se as Session)
declare sub Avalanche (lv as Level, mu as Deck, se as Session, g as Game, p as Hero)
declare sub DrawAvalanche (lv as Level, art as Store, se as Session, cx as integer, cy as integer)
declare function SnowTop% (lv as Level, x as integer)
declare sub Buffet (lv as Level, se as Session, p as Hero)
declare sub DrawGale (lv as Level, se as Session)
declare sub Graze (lv as Level, se as Session, i as integer, p as Hero)
declare sub MeetGoat (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
declare sub Mount (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
declare sub Ride (lv as Level, p as Hero, l as integer, r as integer, d as integer)
declare sub Homeward (lv as Level, g as Game)
declare sub MeetWall (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
declare sub Balk (p as Hero, x as integer, r as integer)
declare sub MeetHeap (lv as Level, p as Hero, i as integer)
declare function GoatNear% (lv as Level, x as integer)
declare function Slant% (p as Hero)
declare sub DrawGoat (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer)
declare sub DrawLoad (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer)
declare sub DrawStray (lv as Level, art as Store, se as Session, cx as integer, cy as integer)
declare function Lie% (lv as Level, x as integer, y as integer)
declare sub DrawFrost (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
declare sub Plunge (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, i as integer)
declare sub Shatter (lv as Level, mu as Deck, se as Session, pt() as Part, i as integer)
declare sub Hurl (lv as Level, i as integer, p as Hero)
declare sub Volley (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub Splat (lv as Level, mu as Deck, se as Session, n as integer, pt() as Part)
declare sub MeetThrow (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
declare sub DrawLobs (lv as Level, art as Store, cx as integer, cy as integer)
declare sub Emit (se as Session, pt() as Part, x as single, y as single, vx as single, vy as single, t as single, c as integer)
declare sub Burst (se as Session, pt() as Part, x as single, y as single, n as integer, v as single, c as integer)
declare sub Camera (lv as Level, zn as Realm, g as Game, p as Hero, snap as integer)
declare sub Render (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub DrawBack (zn as Realm, art as Store, cx as integer, cy as integer)
declare sub DrawTiles (lv as Level, zn as Realm, art as Store, cx as integer, cy as integer)
declare sub DeckColumn (lv as Level, art as Store, tx as integer, cx as integer, cy as integer)
declare sub CombColumn (lv as Level, art as Store, tx as integer, cx as integer, cy as integer)
declare sub DrawThings (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer, back as integer)
declare sub DrawLoose (art as Store, se as Session, cx as integer, cy as integer, lo() as Loose)
declare sub DrawHero (gr as Ground, art as Store, se as Session, p as Hero, cx as integer, cy as integer)
declare sub Footing (gr as Ground, p as Hero, hu as single, hv as single, a as single, tw as integer)
declare sub SpeedLines (se as Session, x as integer, y as integer, d as integer)
declare sub DrawTwisted (art as Store, p as Hero, x as integer, y as integer, f as integer, a as single)
declare function Turned% (p as Hero)
declare sub DrawUpright (art as Store, se as Session, p as Hero, x as integer, y as integer, f as integer, hu as single, hv as single)
declare sub DrawTilted (art as Store, p as Hero, x as integer, y as integer, f as integer, t as integer, hu as single, hv as single)
declare function HeroFrame% (se as Session, p as Hero)
declare function Standing% (se as Session, p as Hero)
declare sub DrawParts (pt() as Part, cx as integer, cy as integer)
declare sub Hud (art as Store, g as Game)
declare sub HudLine (art as Store, row as integer, s as string, n as integer)
declare sub BuildGlyphs (art as Store)
declare sub Scatter (lv as Level, zn as Realm, c as integer, s as integer)
declare sub Say (p as Hero, cx as integer, cy as integer, s as string)
declare sub Panel (x1 as integer, y1 as integer, x2 as integer, y2 as integer)
declare sub Txt (row as integer, col as integer, s as string, c as integer)
declare sub Banner (row as integer, s as string, c as integer)
declare function Num$ (v as long, w as integer)

' A thing of the level: where it is, its kind and state, and its home x, which the level's order goes by.
type Thing
    x as integer
    y as integer
    k as integer
    s as integer
    a as integer
end type

' A moving platform: its thing o, its stretch s and curve z; its middle x, y and reach a; k 0 to and fro, 1 up and
' down, 2 a lift, t how far up it is; e the edge before it; b a bridge's button.
type Mover
    o as integer
    s as integer
    z as integer
    x as integer
    y as integer
    a as integer
    k as integer
    e as integer
    b as integer
    t as single
end type

' The goat out of its place, i: st 1 ridden, 2 on its way home from x, y, 0 none.
type Rover
    x as single
    y as single
    i as integer
    st as integer
end type

' The avalanche's front, and whether it is let go.
type Front
    x as single
    st as integer
end type

' The level as laid: w columns, wpx pixels, of the MW by MH map; a column's top, what shows of it, its faces, its
' glaze (the pass's bare ice; icing: those laid now are), the first thing at it. prof: a tile kind's solid height
' by column. The race from chaseX to chaseEnd and the avalanche's head avalX, NONE where there is none. The
' sea's surface tide, NONE without one, from tideLo at step tideAt to tideHi half a tide later. Who flees in its
' chase, a crane's crate, the goat out of its place, the avalanche.
type Level
    w as integer
    wpx as integer
    goalX as integer
    famX as integer
    startX as integer
    startY as integer
    chaseX as integer
    chaseEnd as integer
    avalX as integer
    tide as integer
    tideLo as integer
    tideHi as integer
    tideAt as long
    icing as integer
    fallow as integer
    gale as integer
    prof() as integer
    top() as integer
    vis() as integer
    faceL() as integer
    faceR() as integer
    glaze() as integer
    first() as integer
    nObj as integer
    ob() as Thing
    nMv as integer
    mv() as Mover
    nGulls as integer
    gulls() as Bird
    lobN as integer
    lobs() as Lob
    map() as integer
    cart as Runner
    box as Runner
    stray as Rover
    aval as Front
end type

' A stretch of the ground's line, from its point x, y to the next: its kind k (0 none, 1 the tiles' top, 2 a
' curve), l long, u, v a pixel along it, its rise a.
type Stretch
    x as single
    y as single
    u as single
    v as single
    l as single
    a as single
    k as integer
end type

' A curve: stretches a to b, from m on a loop's far half, between x l and r.
type Bend
    a as integer
    b as integer
    m as integer
    l as integer
    r as integer
end type

' The ground's line, what Charlie runs on: n points, the platforms' from platA; nz curves; a column's stretch.
type Ground
    n as integer
    platA as integer
    nz as integer
    pt() as Stretch
    cv() as Bend
    col() as integer
end type

' The zone played, 0 the Amber Downs, and its stage; the one ResetWorld is to load; its pictures: tiles, backdrop,
' loop, pipe, card, ribbon. decked: its ground is a deck, the works' kind, else earth, the Downs'.
type Realm
    id as integer
    stage as integer
    nextUp as integer
    tile as integer
    back as integer
    ring as integer
    pipe as integer
    card as integer
    wave as integer
    decked as integer
end type

' The pictures, end to end in img: where each starts, and the next free place. A zone's own go after all the others, at
' zoneAt: datAt, where each zone's start in CHARLIE.DAT; zone, whose are there, one more than its number.
type Store
    img() as long
    at() as long
    nextAt as long
    zoneAt as long
    zone as integer
    datAt() as long
end type

' The colours: the shared ones as built, each stage's own over them (how many, which, what), and those shown,
' dusk sixteenths of them.
type Tones
    day(255) as long
    now(255) as long
    n() as integer
    which() as integer
    own() as long
    glint(2) as long
    dusk as integer
end type

' The keys held and since when; the last scan code, and whether the keyboard's port answers.
type Keys
    held(127) as integer
    t(127) as single
    last as integer
    port as integer
end type

' The songs held at once, each in its slot: the zone's tune, Charlie's theme, the level's end. A song's most bars,
' voicings and drums.
const NSLOT = 3, ZTUNE = 0, THEME = 1, CAUGHT = 2
const NBAR = 30, NVOICE = 4, NDRUM = 8

' Half a bar as PLAY strings: its drums and their letters, the bass, the two chord voices, the lead, the first
' voice an octave down, and the third voice.
type Half
    drums as string
    kit as string
    bass as string
    one as string
    lead as string
    two as string
    under as string
    third as string
end type

' A song: its half bars, its bars' order and voicings, and its kit: the sounds of its lead, bass, low double and
' third voice; by voicing, its letter's place in voLet, the two chord voices' sounds and whether the double plays;
' by drum, its letter's place in drLet, its sound, how loud, what a fade rounds it up by, and its note.
type Song
    half() as Half
    order as string
    voicing as string
    tempo as integer
    bassIn as integer
    lvl(5) as integer
    fade as integer
    entry as integer
    ends as integer
    leadP as string
    bassP as string
    underP as string
    midP as string
    voLet as string
    drLet as string
    voOne() as string
    voTwo() as string
    voUnder() as integer
    drPatch() as string
    drNote() as string
    drVol() as integer
    drUp() as integer
end type

' The music: the slot playing, its bar and half; ended: it has ended and nothing plays. zHeld: the song in the
' zone's slot, one more than its number in CHARLIE.DAT: 0 before any.
type Deck
    song() as Song
    slot as integer
    bar as integer
    half as integer
    ended as integer
    coming as integer
    comingBar as integer
    fadeOut as integer
    fadeIn as integer
    stageTune as integer
    tune as integer
    zHeld as integer
    cHeld as integer
    songAt as long
    muted as integer
    barkAt as long
end type

' The game's steps; metT: the seconds Charlie has been with its family. The scene playing: the beat it ends at, the
' stage it leads to (0 the title), the zone its road is in. The furthest zone a game has reached, kept in
' CHARLIE.SAV, and the zone picked on the title to start at.
type Session
    ticks as long
    cheer as integer
    metT as single
    pastT as single
    looseN as integer
    partN as integer
    sceneEnd as integer
    sceneNext as integer
    road as integer
    reached as integer
    pick as integer
end type

' All of it. What runs the game takes it whole; the rest take the parts they use.
type World
    lv as Level
    gr as Ground
    zn as Realm
    art as Store
    tn as Tones
    kb as Keys
    mu as Deck
    se as Session
end type

dim wd as World
Allot wd

Main wd
end

' The records' arrays, sized: a TYPE's own bounds must be numbers, and these are constants.
sub Allot (wd as World)
    dim i as integer
    redim wd.lv.map(MW - 1, MH - 1)
    redim wd.lv.prof(7, TS - 1)
    redim wd.lv.top(MW)
    redim wd.lv.vis(MW)
    redim wd.lv.faceL(MW)
    redim wd.lv.faceR(MW)
    redim wd.lv.glaze(MW)
    redim wd.lv.first(MW)
    redim wd.lv.ob(MAXOBJ - 1)
    redim wd.lv.mv(NMOVE)
    redim wd.lv.gulls(NGULL - 1)
    redim wd.lv.lobs(NLOB - 1)
    redim wd.gr.pt(NPTS)
    redim wd.gr.cv(NZONE)
    redim wd.gr.col(MW)
    redim wd.art.img(IMGSZ)
    redim wd.art.at(NIMG)
    redim wd.art.datAt(ZONES - 1)
    redim wd.tn.n(NSTAGE - 1)
    redim wd.tn.which(NSTAGE - 1, 159)
    redim wd.tn.own(NSTAGE - 1, 159)
    redim wd.mu.song(NSLOT - 1)
    for i = 0 to NSLOT - 1
        redim wd.mu.song(i).half(NBAR * 2 - 1)
        redim wd.mu.song(i).voOne(NVOICE - 1)
        redim wd.mu.song(i).voTwo(NVOICE - 1)
        redim wd.mu.song(i).voUnder(NVOICE - 1)
        redim wd.mu.song(i).drPatch(NDRUM - 1)
        redim wd.mu.song(i).drNote(NDRUM - 1)
        redim wd.mu.song(i).drVol(NDRUM - 1)
        redim wd.mu.song(i).drUp(NDRUM - 1)
    next
end sub

' The intro's script, back to front: a cue's kind, the beats it is on from and to, where it starts and where it
' ends, its ease, its hop and its own number. A van's: 1 its door open, 2 Charlie at its window, 4 looking back
' from it. A float's: 1 the Baron aboard. A cat's: 1 the basket on it, 2 stood still. Charlie's: Pose's. A sound's:
' Sfx's. A scene every 16 beats, as the tune has them. What goes into the van, or is in it, comes before it.
Cues:
' the last morning: the family at the door, the boxes thrown aboard, Charlie round them all and in
data "cam", 0, 16, 0, 0, 60, 0, 0, 0, 0
data "house", 0, 32, 40, 0, 40, 0, 0, 0, 0
data "fam", 0, 16, 120, 0, 120, 0, 0, 0, 1
data "boxes", 0, 10, 232, 0, 232, 0, 0, 0, 0
data "crate", 4, 5.5, 232, -36, 276, -22, 0, 22, 0
data "crate", 8, 9.5, 232, -36, 276, -22, 0, 22, 0
data "hero", 14.5, 15.5, 240, 0, 280, -13, 0, 18, 3
data "van", 0, 16, 330, 0, 330, 0, 0, 0, 1
data "hero", 0, 3, 70, 0, 250, 0, 0, 0, 1
data "hero", 3, 6, 250, 0, 90, 0, 0, 0, 1
data "hero", 6, 9, 90, 0, 250, 0, 0, 0, 1
data "hero", 9, 11.5, 250, 0, 200, 0, 0, 0, 1
data "hero", 11.5, 13, 200, 0, 200, 0, 0, 0, 6
data "woof", 12, 12.8, 200, 0, 200, 0, 0, 0, 0
data "snd", 12, 12.5, 0, 0, 0, 0, 0, 0, 13
data "hero", 13, 14.5, 200, 0, 240, 0, 0, 0, 1
' the van pulls out, Charlie's head in the wind
data "cam", 16, 17, 80, 0, 80, 0, 0, 0, 0
data "cam", 17, 32, 80, 0, 1120, 0, 1, 0, 0
data "van", 16, 17, 330, 0, 330, 0, 0, 0, 2
data "van", 17, 32, 330, 0, 1280, 0, 1, 0, 2
' a hedge it passes: eyes, then the Mob, the Baron last; he points, their float comes, and they are after it
data "cam", 32, 48, 1400, 0, 1400, 0, 0, 0, 0
data "cat", 37, 40, 1570, 0, 1660, 0, 0, 0, 0
data "cat", 40, 46, 1660, 0, 1660, 0, 0, 0, 2
data "cat", 38, 41, 1560, 0, 1690, 0, 0, 0, 0
data "cat", 41, 46, 1690, 0, 1690, 0, 0, 0, 2
data "baron", 39, 39.75, 1600, 0, 1622, 0, 0, 10, 0
data "hedge", 32, 48, 1540, 0, 1540, 0, 0, 0, 0
data "eyes", 33.5, 37, 1496, -22, 1496, -22, 0, 0, 3
data "eyes", 37, 38.9, 1548, -22, 1548, -22, 0, 0, 1
data "van", 32, 36, 1280, 0, 1800, 0, 0, 0, 2
data "baron", 39.75, 40.5, 1622, 0, 1644, 0, 0, 10, 0
data "baron", 40.5, 41.5, 1644, 0, 1644, 0, 0, 0, 0
data "baron", 41.5, 45, 1644, 0, 1644, 0, 0, 0, 1
data "float", 42.5, 45, 1340, 0, 1590, 0, 2, 0, 0
data "float", 45, 45.75, 1590, 0, 1590, 0, 0, 0, 0
data "baron", 45, 45.75, 1644, 0, 1568, -31, 0, 16, 0
data "float", 45.75, 48, 1590, 0, 1890, 0, 1, 0, 1
data "cat", 46, 48, 1660, 0, 1840, 0, 0, 0, 0
data "cat", 46, 48, 1690, 0, 1860, 0, 0, 0, 0
' a bump: the latch pops and the basket is out; eyes in a hedge, a leap, and Biscuit is away with it
data "cam", 48, 64, 2140, 0, 2140, 0, 0, 0, 0
data "hedge", 48, 64, 2180, 0, 2180, 0, 0, 0, 0
data "eyes", 54, 58, 2146, -22, 2146, -22, 0, 0, 1
data "van", 48, 50, 1998, 0, 2258, 0, 0, 0, 2
data "van", 50, 51, 2258, 0, 2388, 0, 0, 9, 3
data "van", 51, 55, 2388, 0, 2908, 0, 0, 0, 3
data "snd", 50, 50.5, 0, 0, 0, 0, 0, 0, 3
data "basket", 50.2, 51.5, 2216, -30, 2196, 0, 0, 22, 0
data "basket", 51.5, 52.3, 2196, 0, 2186, 0, 0, 8, 0
data "basket", 52.3, 59.25, 2186, 0, 2186, 0, 0, 0, 0
data "cat", 58, 58.75, 2150, -20, 2172, 0, 0, 12, 0
data "cat", 58.75, 59.25, 2172, 0, 2172, 0, 0, 0, 2
data "snd", 58.75, 59, 0, 0, 0, 0, 0, 0, 12
data "cat", 59.25, 63.5, 2186, 0, 2500, 0, 0, 0, 1
' Charlie has seen: he barks from the window and no one hears; out of the door, then, and down
data "cam", 64, 72, 2700, 0, 3740, 0, 0, 0, 0
data "cam", 72, 75, 3740, 0, 3900, 0, 2, 0, 0
data "cam", 75, 100, 3900, 0, 3900, 0, 0, 0, 0
data "hero", 70.5, 72, 3689, -13, 3864, -13, 0, 0, 20
data "van", 64, 70.5, 2900, 0, 3745, 0, 0, 0, 5
data "van", 70.5, 80, 3745, 0, 4980, 0, 0, 0, 1
data "woof", 66, 66.8, 3152, -20, 3256, -20, 0, 0, 1
data "snd", 66, 66.5, 0, 0, 0, 0, 0, 0, 13
data "woof", 67.5, 68.3, 3347, -20, 3451, -20, 0, 0, 1
data "snd", 67.5, 68, 0, 0, 0, 0, 0, 0, 13
data "woof", 69, 69.8, 3542, -20, 3646, -20, 0, 0, 1
data "snd", 69, 69.5, 0, 0, 0, 0, 0, 0, 13
data "hero", 72, 73.5, 3864, -13, 3960, 0, 0, 28, 3
data "hero", 73.5, 75.5, 3960, 0, 4060, 0, 2, 0, 3
data "hero", 75.5, 80, 4060, 0, 4060, 0, 0, 0, 5
' the cat goes by him with the basket, then the Mob on its float: he looks after them, and barks
data "float", 85.5, 89.5, 3830, 0, 4330, 0, 0, 0, 1
data "hero", 80, 81, 4060, 0, 4060, 0, 0, 0, 0
data "hero", 81, 82.6, 4060, 0, 4060, 0, 0, 0, 16
data "cat", 81, 85, 3860, 0, 4300, 0, 0, 0, 1
data "hero", 82.6, 83.4, 4060, 0, 4060, 0, 0, 10, 5
data "hero", 83.4, 85.2, 4060, 0, 4060, 0, 0, 0, 0
data "hero", 85.2, 87.1, 4060, 0, 4060, 0, 0, 0, 16
data "hero", 87.1, 87.9, 4060, 0, 4060, 0, 0, 10, 5
data "hero", 87.9, 96, 4060, 0, 4060, 0, 0, 0, 0
data "cat", 86.5, 90.5, 3860, 0, 4300, 0, 0, 0, 0
data "cat", 87, 91, 3860, 0, 4300, 0, 0, 0, 0
data "woof", 92, 92.8, 4060, 0, 4060, 0, 0, 0, 0
data "snd", 92, 92.5, 0, 0, 0, 0, 0, 0, 13
data "woof", 93.5, 94.3, 4060, 0, 4060, 0, 0, 0, 0
data "snd", 93.5, 94, 0, 0, 0, 0, 0, 0, 13
' he winds up, as the player will, and is after them
data "hero", 96, 97, 4060, 0, 4060, 0, 0, 0, 4
data "hero", 97, 100, 4060, 0, 4060, 0, 0, 0, 7
data "snd", 97, 97.5, 0, 0, 0, 0, 0, 0, 6
data "snd", 98, 98.5, 0, 0, 0, 0, 0, 0, 6
data "snd", 99, 99.5, 0, 0, 0, 0, 0, 0, 6
data "snd", 100, 100.5, 0, 0, 0, 0, 0, 0, 7
data "hero", 100, 102, 4060, 0, 4260, 0, 1, 0, 2
data "hero", 102, 112, 4260, 0, 6160, 0, 0, 0, 2
data "cam", 100, 102, 3900, 0, 4020, 0, 1, 0, 0
data "cam", 102, 112, 4020, 0, 5940, 0, 0, 0, 0
' between zones, the bump again on the next zone's road: its keepsake out, and the cat that was following away
' with it
data "cam", 120, 136, 2140, 0, 2140, 0, 0, 0, 0
data "van", 120, 122, 1998, 0, 2258, 0, 0, 0, 2
data "van", 122, 123, 2258, 0, 2388, 0, 0, 9, 3
data "van", 123, 127, 2388, 0, 2908, 0, 0, 0, 3
data "snd", 122, 122.5, 0, 0, 0, 0, 0, 0, 3
data "basket", 122.2, 123.5, 2216, -30, 2196, 0, 0, 22, 0
data "basket", 123.5, 124.3, 2196, 0, 2186, 0, 0, 8, 0
data "basket", 124.3, 130.25, 2186, 0, 2186, 0, 0, 0, 0
data "cat", 126.5, 129.75, 2120, 0, 2172, 0, 0, 0, 0
data "cat", 129.75, 130.25, 2172, 0, 2172, 0, 0, 0, 2
data "snd", 129.75, 130, 0, 0, 0, 0, 0, 0, 12
data "cat", 130.25, 134.5, 2186, 0, 2500, 0, 0, 0, 1
data "end"

' The tunes are in CHARLIE.DAT with the pictures, from tools/charliesong.txt.

' Set up, then read input, step in fixed ticks and draw, till Esc on the
' title.
sub Main (wd as World)
    dim g as Game, p as Hero, k as Pad
    dim lo(NLOOSE - 1) as Loose, pt(NPART - 1) as Part, cu(NCUE) as Cue
    dim now as single, last as single, c as Clock, n as integer

    OpenScreen
    LoadTables wd.lv
    BuildArt wd.art, wd.tn
    g.page = 1
    StartIntro wd, g, cu()
    LoadSongs wd.mu
    LoadSave wd.se

    last = timer
    c.span = STEPT
    do
        ' the next frame on the screen (port 3DAh's retrace; with no display, the next look), the keys read meanwhile
        do
            Poll wd.kb
        loop until inp(&H3DA) and 8
        now = timer
        Pace Elapsed!(last, now), c
        last = now
        ReadPad wd.kb, wd.mu, k
        ' a picture for each step, none for a frame between two
        n = Steps%(c)
        if n = 0 then continue do
        do while n
            Advance wd, g, p, k, lo(), pt(), cu()
            n = n - 1
        loop
        PickSong wd.mu, SongFor%(g), Sudden%(wd.se, g)
        FeedSong wd.mu, wd.se
        if g.mode = INTRO then
            IntroDraw wd, g, p, cu()
        else
            Render wd, g, p, lo(), pt()
        end if
        Flip g.page
    loop until g.quitting

    screen 0
    width 80
end sub

' A step: of the intro, and on to the title once it is over; else of the game.
sub Advance (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part, cu() as Cue)
    if g.mode <> INTRO then
        Tick wd, g, p, k, lo(), pt()
        exit sub
    end if
    if IntroOver%(wd, g, k, cu()) = 0 then exit sub
    if wd.se.sceneNext = 0 then
        ResetWorld wd, g, p, lo(), pt()
        exit sub
    end if
    GoStage wd, wd.se.sceneNext, g, p, lo(), pt()
    wd.se.sceneNext = 0
end sub

sub OpenScreen
    screen 13, , 1, 0
end sub

' Shows the page just drawn, draws on the other.
sub Flip (page as integer)
    screen , , 1 - page, page
    page = 1 - page
end sub

' Each tile kind's column heights: 1 solid; 2, 3 rise and fall at 45
' degrees; 4 and 5, 6 and 7 rise and fall half as steep over two tiles.
sub LoadTables (lv as Level)
    dim i as integer
    for i = 0 to TS - 1
        lv.prof(1, i) = TS
        lv.prof(2, i) = i + 1
        lv.prof(3, i) = TS - i
        lv.prof(4, i) = (i + 2) \ 2
        lv.prof(5, i) = 8 + (i + 2) \ 2
        lv.prof(6, i) = TS - i \ 2
        lv.prof(7, i) = 8 - i \ 2
    next
end sub

' Image k upside down as k2.
sub Flop (art as Store, k as integer, k2 as integer, w as integer, h as integer)
    dim y as integer, strip(40) as long
    put (0, 0), art.img(art.at(k)), pset
    for y = 0 to h - 1
        get (0, y)-(w - 1, y), strip
        put (100, h - 1 - y), strip, pset
    next
    Grab art, 100, 0, 100 + w - 1, h - 1, k2
end sub

' A stage's zone: two acts a zone; the intro's road is the Downs.
function ZoneOf% (se as Session, st as integer)
    ZoneOf% = st \ 2
    if st = SINTRO then ZoneOf% = se.road
end function

' CHARLIE.DAT open at a stage's level: a zone's two acts, zone by zone, then the intro's road. They are written
' in tools/charlielevels.txt.
sub SeekLevel (st as integer)
    dim levels as long
    OpenData
    seek #1, 5
    levels = Word&
    seek #1, levels + 4 + st * 4
    seek #1, levels + Word&
end sub

' A prop as the works have it, where the van's road is theirs: a tank for a tree, a vent for a bush; nothing grows
' there, and no rock lies.
function Plated% (k as integer)
    select case k
    case KTREE: Plated% = KTANK
    case KBUSH: Plated% = KVENT
    case KFLOWER, KROCK: Plated% = 0
    case else: Plated% = k
    end select
end function

' The ground's columns, then what stands on them, from the stage's level: a command and its count, to its end.
sub LoadLevel (wd as World, st as integer)
    dim cmd as string, n as integer, c as integer, g as integer, bx as integer
    dim i as integer, s as integer, k as integer, z as integer, light as integer
    wd.zn.stage = st
    z = ZoneOf%(wd.se, st)
    wd.zn.id = z
    LoadZoneArt wd.art, z
    ' the intro's road is the Downs at dawn, lit as at evening; a later zone's is lit as its first act
    light = st
    if st = SINTRO then light = wd.se.road * 2
    if light = 0 and st = SINTRO then light = 1
    for i = 0 to 255
        wd.tn.now(i) = wd.tn.day(i)
    next
    for i = 0 to wd.tn.n(light) - 1
        wd.tn.now(wd.tn.which(light, i)) = wd.tn.own(light, i)
    next
    wd.lv.goalX = NONE
    wd.lv.famX = NONE
    wd.lv.chaseX = NONE
    wd.lv.tideLo = NONE
    wd.lv.tide = NONE
    wd.lv.cart.st = 0
    for i = 0 to 2
        wd.tn.glint(i) = wd.tn.now(CWATER + 1 + i)
    next
    Shade wd.tn
    redim wd.lv.map(MW - 1, MH - 1)
    redim wd.lv.glaze(MW)
    wd.lv.icing = 0
    wd.lv.gale = 0
    wd.lv.avalX = NONE
    wd.lv.fallow = 0
    wd.lv.nObj = 0
    wd.lv.nMv = 0
    wd.lv.nGulls = 0
    for i = 0 to MW
        wd.lv.top(i) = MH
    next
    g = 10
    wd.zn.decked = z = 1
    if wd.zn.decked then
        wd.zn.tile = ITILE2
        wd.zn.back = IZ2
        wd.zn.ring = IZ2 + 5
        wd.zn.pipe = IZ2 + 7
        wd.zn.card = ICARD2
        wd.zn.wave = IWAVE + 4
    else
        wd.zn.tile = ITILE
        wd.zn.back = IBG
        wd.zn.ring = ILOOP
        wd.zn.card = ICARD
        wd.zn.wave = IWAVE
    end if
    SeekLevel st
    do
        cmd = Chars$
        n = Word&
        s = (MH - g) * TS
        k = 0
        select case cmd
        case "flat"
            for i = 1 to n
                Column wd.lv, c, g, 1
                Scatter wd.lv, wd.zn, c, s
                c = c + 1
            next
        case "up45"
            for i = 1 to n
                Column wd.lv, c, g, 2
                g = g + 1
                c = c + 1
            next
        case "dn45"
            for i = 1 to n
                g = g - 1
                Column wd.lv, c, g, 3
                c = c + 1
            next
        case "flume"
            ' a 45 degree fall of n and a flume down it, from the tank at its head to its level end a column on;
            ' nothing grows by the tank
            Weed wd.lv, c * TS - 44
            AddObj wd.lv, KCISTERN, c * TS, s
            for i = 1 to n
                g = g - 1
                Column wd.lv, c, g, 3
                AddObj wd.lv, KFLUME, c * TS, (MH - g - 1) * TS
                c = c + 1
            next
            Column wd.lv, c, g, 1
            AddObj wd.lv, KSPOUT, c * TS, (MH - g) * TS
            c = c + 1
        case "upg"
            for i = 1 to n
                Column wd.lv, c, g, 4
                Column wd.lv, c + 1, g, 5
                g = g + 1
                c = c + 2
            next
        case "dng"
            for i = 1 to n
                g = g - 1
                Column wd.lv, c, g, 6
                Column wd.lv, c + 1, g, 7
                c = c + 2
            next
        case "gap": c = c + n
        case "ice"
            ' the ground from here bare ice, or with 0 under snow again
            wd.lv.icing = n <> 0
        case "wall": g = g + n
        case "pit"
            ' not a fall to nothing: a floor PITD down, a spring at its far wall
            Pit wd.lv, c, g, n
            c = c + n
        case "wave"
            ' a ribbon of n twists over a pit, each two halves of WAVEL, bones up its far side
            for i = 0 to n * 2 - 1
                AddObj wd.lv, KWAVE + (i and 1), (c + i * 12) * TS, s
                if (i and 1) <> 0 then continue for
                for k = 3 to 9 step 3
                    AddObj wd.lv, KBONE, (c + i * 12 + k) * TS, s - cint(WAVEA * (1 - cos(PI * k / 12)) / 2) - 6
                next
            next
            k = 0
            Pit wd.lv, c, g, n * 24
            c = c + n * 24
        case "bump": AddObj wd.lv, KBUMP, c * TS + 8, s - n * TS
        case "hoop": AddObj wd.lv, KHOOP, c * TS + 8, s - n * TS
        case "pipe"
            ' a wall with a quarter pipe up to it, over the three columns before: nothing grows there
            for i = 0 to wd.lv.nObj - 1
                if Rooted%(wd.lv, i, KTREE) and wd.lv.ob(i).x >= c * TS - PIPER then wd.lv.ob(i).k = 0
            next
            AddObj wd.lv, KPIPE, c * TS, s
            ' Amber Downs' pipe is drawn with the ground under it
            if z = 0 then
                wd.lv.vis(c - 1) = 0
                wd.lv.vis(c - 2) = 0
                wd.lv.vis(c - 3) = 0
            end if
            g = g + n
        case "drop": g = g - n
        case "mover"
            ' at each end of its way it lies 8 over the ground
            AddMover wd.lv, c * TS + (n * TS - MOVEW) \ 2, s, (n * TS - MOVEW) \ 2 + 8, 0, c * TS
            Pit wd.lv, c, g, n
            c = c + n
        case "boat"
            ' a pool n wide and a boat adrift in it, wall to wall: up and down with the tide, aground at low water
            AddMover wd.lv, c * TS + (n * TS - MOVEW) \ 2, s + PITD * TS - HULL, (n * TS - MOVEW) \ 2, 5, c * TS
            Pit wd.lv, c, g, n
            c = c + n
        case "pier"
            ' a pool n wide under an old pier, a span every three columns: each falls once it is trodden
            for i = 0 to n \ 3 - 1
                AddMover wd.lv, (c + i * 3) * TS, s, 0, 6, c * TS
            next
            Pit wd.lv, c, g, n
            c = c + n
        case "hover"
            AddMover wd.lv, c * TS + 8, s - n * TS \ 2, n * TS \ 2, 1, c * TS
            Pit wd.lv, c, g, 4
            c = c + 4
            g = g + n
        case "lift"
            AddMover wd.lv, c * TS, s, n * TS, 2, c * TS
            Pit wd.lv, c, g, 3
            c = c + 3
            g = g + n
        case "button"
            k = KBUTTON
            bx = c * TS + 8
        case "bridge"
            ' a pit n wide, platforms lying on its floor that rise to bridge it once the last button is trodden
            for i = 0 to n \ 3 - 1
                AddMover wd.lv, (c + i * 3) * TS, s + PITD * TS - 6, PITD * TS - 6, 4, c * TS
                wd.lv.mv(wd.lv.nMv - 1).b = bx
            next
            Pit wd.lv, c, g, n
            c = c + n
        case "cable"
            ' a basket down a cable over a pit n wide, the ground past it CABLED lower
            AddMover wd.lv, c * TS - 8, s, n * TS - MOVEW + 16, 3, c * TS
            Pit wd.lv, c, g - CABLED \ TS, n
            c = c + n
            g = g - CABLED \ TS
        case "chair"
            ' a ski lift's chair up a cable over a pit n wide, the ground past it CABLED higher
            AddMover wd.lv, c * TS - 8, s, n * TS - MOVEW + 16, 7, c * TS
            Pit wd.lv, c, g, n
            c = c + n
            g = g + CABLED \ TS
        case "plat"
            for i = 1 to n mod 100
                wd.lv.map(c + i, MH - g - n \ 100 - 1) = 1
                AddObj wd.lv, KPLAT, (c + i) * TS, s - (n \ 100 + 1) * TS
            next
        case "bones"
            for i = 0 to n - 1
                AddObj wd.lv, KBONE, (c + i) * TS + 8, s - 14
            next
        case "arc"
            for i = 0 to n - 1
                AddObj wd.lv, KBONE, (c + i) * TS + 8, s - 20 - cint(50 * sin(PI * (i + .5) / n))
            next
        case "hb"
            for i = 1 to n mod 100
                AddObj wd.lv, KBONE, (c + i) * TS + 8, s - (n \ 100) * TS - 14
            next
        case "cat": k = KCAT
        case "crab": k = KCRAB
        case "sweeper": k = KSWEEP
        case "sled": k = KSLED
        case "thrower": k = KTHROW
        case "gale": wd.lv.gale = -1
        case "avalanche": wd.lv.avalX = c * TS + 8
        case "goat"
            Clearing wd.lv, c
            k = KGOAT
        case "icewall"
            Clearing wd.lv, c
            k = KWALL
        case "icicle"
            ' a shelf of three, n tiles over the ground here, and an icicle under its middle
            Shelf wd.lv, c, g, s, n, 3
            AddObj wd.lv, KICICLE, c * TS + 24, s - n * TS - 6
        case "load"
            ' a shelf of four, n tiles over the ground here, a load of snow on it, and where its heap will lie
            Shelf wd.lv, c, g, s, n, 4
            AddObj wd.lv, KLOAD, c * TS + 32, s - (n + 1) * TS
            AddObj wd.lv, KHEAP, c * TS + 32 + LOADFAR, s
        case "drift": k = KDRIFT
        case "spark": k = KSPARK
        case "press"
            ' n presses on a gantry over level ground, four columns each, a leg at either end
            AddObj wd.lv, KLEG, c * TS, s
            for i = 0 to n * 4 - 1
                Column wd.lv, c + i, g, 1
                if (i and 3) = 1 then AddObj wd.lv, KPRESS, (c + i + 1) * TS, s
            next
            c = c + n * 4
            AddObj wd.lv, KLEG, c * TS - 8, s
        case "jelly"
            ' a jellyfish n columns on, a buoy: afloat where the sea is over the ground there, aground where not
            AddObj wd.lv, KJELLY, (c + n) * TS + 8, s
        case "buoy": AddObj wd.lv, KBUOY, (c + n) * TS + 8, s
        case "gull"
            ' a gull, its home n tiles over the ground here
            wd.lv.gulls(wd.lv.nGulls).hx = c * TS + 8
            wd.lv.gulls(wd.lv.nGulls).hy = s - n * TS
            wd.lv.nGulls = wd.lv.nGulls + 1
        case "spring": k = KSPRING
        case "mark": k = KMARK
        case "boost": k = KBOOST
        case "loop": k = KLOOP
        case "tree": k = KTREE
        case "bush": k = KBUSH
        case "flower": k = KFLOWER
        case "rock": k = KROCK
        case "thorn": k = KTHORN
        case "jet"
            Clearing wd.lv, c
            k = KJET
        case "spin": k = KSPIN
        case "goal"
            k = KGOAL
            wd.lv.goalX = c * TS + 8
        case "fam"
            k = KFAM
            wd.lv.famX = c * TS + 8
        case "chase"
            wd.lv.chaseX = c * TS
            wd.lv.chaseEnd = (c + n) * TS
        case "tide"
            ' the sea: its height at low water * 100 + at high, in the ground's tiles
            wd.lv.tideLo = (MH - n \ 100) * TS
            wd.lv.tideHi = (MH - n mod 100) * TS
            wd.lv.tide = wd.lv.tideLo
            wd.lv.tideAt = wd.se.ticks
        case "tune": wd.mu.stageTune = n
        case "dog"
            wd.lv.startX = c * TS + 8
            wd.lv.startY = s
        end select
        if wd.zn.decked then k = Plated%(k)
        if k then AddObj wd.lv, k, c * TS + 8, s
    loop until cmd = "end"
    close #1
    wd.lv.w = c
    wd.lv.wpx = c * TS
    ' nothing grows in front of the family
    for i = 0 to wd.lv.nObj - 1
        if Rooted%(wd.lv, i, KBUSH) and abs(wd.lv.ob(i).x - wd.lv.famX) < 112 then wd.lv.ob(i).k = 0
    next
    SortObjs wd.lv
    Track wd.lv, wd.gr
    Faces wd.lv
    ZoneTune wd.mu, wd.mu.stageTune
end sub

' The ground's line from the columns' tops, left to right, broken where they do not meet; a loop spliced in where
' it stands, a quarter pipe and its wall over the three columns before the wall; then the platforms' tops.
' Where a flat column stands over a lower flat one, or over none, its side shows: for how many tiles down.
sub Faces (lv as Level)
    dim c as integer
    for c = 0 to lv.w - 1
        lv.faceL(c) = 0
        lv.faceR(c) = 0
        if lv.vis(c) <> 1 and lv.vis(c) <> 2 then continue for
        if c > 0 then lv.faceL(c) = Side%(lv, c, c - 1)
        lv.faceR(c) = Side%(lv, c, c + 1)
    next
end sub

function Side% (lv as Level, c as integer, n as integer)
    Side% = 0
    if lv.top(n) = MH then
        Side% = MH - lv.top(c)
    elseif (lv.vis(n) = 1 or lv.vis(n) = 2) and lv.top(n) > lv.top(c) then
        Side% = lv.top(n) - lv.top(c)
    end if
end function

' A bare side of the ground from x, y0, n tiles down: picture k its turf's end, k + 2 the comb's.
sub Bare (art as Store, x as integer, y0 as integer, n as integer, k as integer)
    dim i as integer, y as integer, j as integer
    j = k
    y = y0
    for i = 1 to n
        if y >= SH then exit for
        if y > -TS then Blit art, x, y, j, TS, TS
        j = k + 2
        y = y + TS
    next
end sub

sub Track (lv as Level, gr as Ground)
    dim c as integer, i as integer
    gr.n = 0
    gr.nz = 0
    for c = 0 to MW
        gr.col(c) = -1
    next
    for i = 0 to lv.nObj - 1
        if lv.ob(i).k = KPIPE then gr.col((lv.ob(i).x - PIPER) \ TS) = -2 - i
    next
    for c = 0 to lv.w - 1
        if gr.col(c) < -1 then
            TrackPipe lv, gr, c
            c = c + PIPER \ TS - 1
        elseif lv.top(c) < MH then
            TrackTile lv, gr, c
        end if
    next
    gr.platA = gr.n
    TrackPlats lv, gr
    TrackMovers lv, gr
    TrackRibbons lv, gr
end sub

' A quarter pipe from column c, marked there with the thing that is it: up its curve, then its wall.
sub TrackPipe (lv as Level, gr as Ground, c as integer)
    dim x as integer, y as integer, j as integer, a as single
    x = c * TS
    y = lv.ob(-2 - gr.col(c)).y
    gr.col(c) = -1
    if gr.pt(gr.n - 1).x <> x or gr.pt(gr.n - 1).y <> y then Knot gr, csng(x), csng(y), 0
    gr.cv(gr.nz).a = gr.n - 1
    gr.cv(gr.nz).m = -1
    gr.cv(gr.nz).l = x
    gr.cv(gr.nz).r = x + PIPER
    for j = 1 to PIPEN - 1
        a = j * PI / 2 / PIPEN
        Knot gr, x + PIPER * sin(a), y - PIPER + PIPER * cos(a), 2
    next
    Knot gr, csng(x + PIPER), csng(y - PIPER), 2
    j = lv.top(c + PIPER \ TS) * TS
    if j < y - PIPER then Knot gr, csng(x + PIPER), csng(j), 2
    gr.cv(gr.nz).b = gr.n - 2
    gr.nz = gr.nz + 1
end sub

' Column c's top tile, a stretch from its left edge to its right, and the loops stood on it.
sub TrackTile (lv as Level, gr as Ground, c as integer)
    dim i as integer, j as integer, k as integer, x as integer, y as integer, yl as integer, yr as integer
    x = c * TS
    k = lv.map(c, lv.top(c))
    y = (lv.top(c) + 1) * TS
    ' a tile's top at its edges: its profile's, to the even pixel
    yl = y - (lv.prof(k, 0) and 30)
    yr = y - (lv.prof(k, TS - 1) and 30)
    j = gr.n = 0
    if not j then j = gr.pt(gr.n - 1).x <> x or gr.pt(gr.n - 1).y <> yl
    if j then Knot gr, csng(x), csng(yl), 0
    gr.col(c) = gr.n - 1
    for i = lv.first(c) to lv.nObj - 1
        if lv.ob(i).x >= x + TS then exit for
        if lv.ob(i).k = KLOOP then TrackLoop gr, lv.ob(i).x, yl
    next
    Knot gr, csng(x + TS), csng(yr), 1
end sub

' A loop stood at x, its foot at y: a ring of LOOPN stretches, a curve.
sub TrackLoop (gr as Ground, x as integer, y as integer)
    dim j as integer, a as single
    Knot gr, csng(x), csng(y), 1
    gr.cv(gr.nz).a = gr.n - 1
    gr.cv(gr.nz).m = gr.n - 1 + LOOPN \ 2
    gr.cv(gr.nz).l = x - LOOPR
    gr.cv(gr.nz).r = x + LOOPR
    for j = 1 to LOOPN - 1
        a = j * 2 * PI / LOOPN
        Knot gr, x + LOOPR * sin(a), y - LOOPR + LOOPR * cos(a), 2
    next
    Knot gr, csng(x), csng(y), 2
    gr.cv(gr.nz).b = gr.n - 2
    gr.nz = gr.nz + 1
end sub

' The platforms: those side by side at one height are one stretch.
sub TrackPlats (lv as Level, gr as Ground)
    dim i as integer, x as integer, x1 as integer, y as integer
    x = NONE
    for i = 0 to lv.nObj - 1
        if lv.ob(i).k <> KPLAT then continue for
        if x <> NONE and lv.ob(i).x = x1 and lv.ob(i).y = y then
            x1 = x1 + TS
            continue for
        end if
        Plank gr, x, x1, y
        x = lv.ob(i).x
        x1 = x + TS
        y = lv.ob(i).y
    next
    Plank gr, x, x1, y
end sub

' A platform's stretch from x to x1 at height y, where there is one.
sub Plank (gr as Ground, x as integer, x1 as integer, y as integer)
    if x = NONE then exit sub
    Knot gr, csng(x), csng(y), 0
    Knot gr, csng(x1), csng(y), 1
end sub

' The moving platforms: a stretch each, a curve of one, in the order they were made.
sub TrackMovers (lv as Level, gr as Ground)
    dim i as integer, k as integer
    for i = 0 to lv.nObj - 1
        if lv.ob(i).k <> KMOVE and lv.ob(i).k <> KSPAN and lv.ob(i).k <> KSEAT then continue for
        Knot gr, csng(lv.ob(i).x), csng(lv.ob(i).y), 0
        Knot gr, csng(lv.ob(i).x + MOVEW), csng(lv.ob(i).y), 3
        lv.mv(k).o = i
        lv.mv(k).s = gr.n - 2
        lv.mv(k).z = gr.nz
        lv.mv(k).t = 0
        if lv.mv(k).k = 4 then lv.mv(k).b = ButtonAt%(lv, lv.mv(k).b)
        gr.cv(gr.nz).a = gr.n - 2
        gr.cv(gr.nz).b = gr.n - 2
        gr.cv(gr.nz).m = -1
        gr.cv(gr.nz).l = lv.ob(i).x
        gr.cv(gr.nz).r = lv.ob(i).x + MOVEW
        gr.nz = gr.nz + 1
        k = k + 1
    next
end sub

' A bridge's button, the one at x: its number among the things, or x where there is none.
function ButtonAt% (lv as Level, x as integer)
    dim j as integer
    ButtonAt% = x
    for j = 0 to lv.nObj - 1
        if lv.ob(j).k <> KBUTTON or lv.ob(j).x <> x then continue for
        ButtonAt% = j
        exit function
    next
end function

' The ribbons: halves that meet are one stretch, 8 of flat at each end to run on and off by.
sub TrackRibbons (lv as Level, gr as Ground)
    dim i as integer, j as integer, x1 as integer, y as integer, a as single
    x1 = NONE
    for i = 0 to lv.nObj - 1
        if lv.ob(i).k <> KWAVE and lv.ob(i).k <> KWAVE2 then continue for
        if lv.ob(i).x <> x1 then
            RibbonEnd gr, x1, y
            y = lv.ob(i).y
            Knot gr, csng(lv.ob(i).x - 8), csng(y), 0
            gr.cv(gr.nz).a = gr.n - 1
            gr.cv(gr.nz).m = -2
            gr.cv(gr.nz).l = lv.ob(i).x - 8
            Knot gr, csng(lv.ob(i).x), csng(y), 2
        end if
        for j = 1 to 12
            a = PI * j / 12 - PI * (lv.ob(i).k = KWAVE2)
            Knot gr, csng(lv.ob(i).x + j * 16), y - WAVEA * (1 - cos(a)) / 2, 2
        next
        x1 = lv.ob(i).x + WAVEL
        gr.cv(gr.nz).r = x1 + 8
    next
    RibbonEnd gr, x1, y
end sub

' A ribbon's far end at x1, y, where there is one: its flat, and its curve closed.
sub RibbonEnd (gr as Ground, x1 as integer, y as integer)
    if x1 = NONE then exit sub
    Knot gr, csng(x1 + 8), csng(y), 2
    gr.cv(gr.nz).b = gr.n - 2
    gr.nz = gr.nz + 1
end sub

' A moving platform, its middle's left end at x, y; reach a, kind k; e the edge before it.
sub AddMover (lv as Level, x as integer, y as integer, a as integer, k as integer, e as integer)
    select case k
    case 6: AddObj lv, KSPAN, x, y
    case 7: AddObj lv, KSEAT, x, y
    case else: AddObj lv, KMOVE, x, y
    end select
    lv.mv(lv.nMv).x = x
    lv.mv(lv.nMv).y = y
    lv.mv(lv.nMv).a = a
    lv.mv(lv.nMv).k = k
    lv.mv(lv.nMv).e = e
    lv.nMv = lv.nMv + 1
end sub

' Down a flume: swept off his feet, curled, and no slower than its water runs.
sub Slide (mu as Deck, se as Session, p as Hero)
    if not p.roll then Sfx mu, se, SBOOST
    p.roll = -1
    p.face = 0
    if p.gsp < FLUMEV then p.gsp = FLUMEV
end sub

' The pier's fallen spans back in their places: Charlie starts again before them.
sub Mend (lv as Level)
    dim m as integer
    for m = 0 to lv.nMv - 1
        if lv.mv(m).k = 6 then lv.mv(m).t = 0
    next
    for m = 0 to lv.nObj - 1
        if lv.ob(m).k = KSLED then lv.ob(m).s = 0
        if lv.ob(m).k <> KGOAT then continue for
        lv.ob(m).s = 1
        lv.ob(m).x = lv.ob(m).a
    next
    lv.stray.st = 0
    lv.aval.st = 0
end sub

' How far cable m's far end hangs under its near one: a basket's cable falls, a ski lift's climbs.
function Dip% (lv as Level, m as integer)
    Dip% = CABLED
    if lv.mv(m).k = 7 then Dip% = -CABLED
end function

' Boat m is off the floor of its pool.
function Adrift% (lv as Level, m as integer)
    Adrift% = lv.tide - FREEB < lv.mv(m).y
end function

' Platform m's swing now, -1 to 1, held a while at each end when it goes up and down; quarter: a quarter of its
' way on, which tells where it is going.
function Swing! (lv as Level, se as Session, m as integer, quarter as integer)
    dim q as single, v as single
    if lv.mv(m).k = 5 then
        ' a boat by its own clock, which stops while it is aground
        q = lv.mv(m).t + m + quarter * PI / 2
    else
        q = se.ticks * STEPT * (1.1 - .2 * lv.mv(m).k) + m + quarter * PI / 2
    end if
    v = sin(q)
    if lv.mv(m).k = 1 then
        v = v * 1.3
        if v > 1 then v = 1
        if v < -1 then v = -1
    end if
    Swing! = v
end function

' The platforms a step on: to and fro, up and down, or a lift, up while Charlie stands on it and down again
' when not. Charlie goes with the one it stands on.
sub Movers (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero)
    dim m as integer, s as integer, o as integer, x as single, y as single, dx as single, dy as single
    for m = 0 to lv.nMv - 1
        s = lv.mv(m).s
        x = lv.mv(m).x
        y = lv.mv(m).y
        select case lv.mv(m).k
        case 0: x = x + lv.mv(m).a * Swing!(lv, se, m, 0)
        case 5
            if Adrift%(lv, m) then
                lv.mv(m).t = lv.mv(m).t + STEPT
                y = lv.tide - FREEB
            end if
            x = x + lv.mv(m).a * Swing!(lv, se, m, 0)
        case 1: y = y + lv.mv(m).a * Swing!(lv, se, m, 0)
        case 6
            ' a pier's span: trodden, it totters a moment, then falls from under who is on it, and stays fallen
            if lv.mv(m).t > 0 or (p.ground and p.sg = s) then lv.mv(m).t = lv.mv(m).t + STEPT
            if lv.mv(m).t > SPANT + 2 then lv.mv(m).t = SPANT + 2
            lv.ob(lv.mv(m).o).s = lv.mv(m).t > 0 and lv.mv(m).t < SPANT
            if lv.mv(m).t >= SPANT then
                if lv.mv(m).t - STEPT < SPANT then Sfx mu, se, SCRACK
                if p.ground and p.sg = s then Drop gr, p
                dy = lv.mv(m).t - SPANT
                y = y + 80 * dy + GRAV * dy * dy
                if y > lv.mv(m).y + SPANDROP then y = lv.mv(m).y + SPANDROP
            end if
        case 2
            if p.ground and p.sg = s then
                lv.mv(m).t = lv.mv(m).t + 90 * STEPT
            else
                lv.mv(m).t = lv.mv(m).t - 60 * STEPT
            end if
            if lv.mv(m).t < 0 then lv.mv(m).t = 0
            if lv.mv(m).t > lv.mv(m).a then lv.mv(m).t = lv.mv(m).a
            y = y - lv.mv(m).t
        case 4
            if lv.ob(lv.mv(m).b).s then lv.mv(m).t = lv.mv(m).t + 200 * STEPT
            if lv.mv(m).t > lv.mv(m).a then lv.mv(m).t = lv.mv(m).a
            y = y - lv.mv(m).t
        case 3, 7
            ' while stood on: a basket down its cable, faster as it goes, a chair up its own steadily; back when left
            if not p.ground or p.sg <> s then
                lv.mv(m).t = lv.mv(m).t - .4 * STEPT
            elseif lv.mv(m).k = 3 then
                lv.mv(m).t = lv.mv(m).t + (.25 + lv.mv(m).t) * .6 * STEPT
            else
                lv.mv(m).t = lv.mv(m).t + SEATV * STEPT
            end if
            if lv.mv(m).t < 0 then lv.mv(m).t = 0
            if lv.mv(m).t > 1 then lv.mv(m).t = 1
            x = x + lv.mv(m).a * lv.mv(m).t
            y = y + Dip%(lv, m) * lv.mv(m).t
        end select
        dx = x - gr.pt(s).x
        dy = y - gr.pt(s).y
        if dx = 0 and dy = 0 then continue for
        gr.pt(s).x = x
        gr.pt(s + 1).x = x + MOVEW
        gr.pt(s).y = y
        gr.pt(s + 1).y = y
        gr.cv(lv.mv(m).z).l = int(x)
        gr.cv(lv.mv(m).z).r = int(x) + MOVEW + 1
        o = lv.mv(m).o
        lv.ob(o).x = cint(x)
        lv.ob(o).y = cint(y)
        if not p.ground or p.sg <> s then continue for
        p.x = p.x + dx
        p.y = p.y + dy
    next
end sub

' The line's next point, x, y, a stretch of kind k to it from the last: 0, none.
sub Knot (gr as Ground, x as single, y as single, k as integer)
    dim i as integer, u as single, v as single, d as single
    gr.pt(gr.n).x = x
    gr.pt(gr.n).y = y
    gr.pt(gr.n).k = 0
    if k <> 0 then
        i = gr.n - 1
        u = x - gr.pt(i).x
        v = y - gr.pt(i).y
        d = sqr(u * u + v * v)
        u = u / d
        v = v / d
        gr.pt(i).u = u
        gr.pt(i).v = v
        gr.pt(i).l = d
        gr.pt(i).k = k
        gr.pt(i).a = Bearing!(u, v)
    end if
    gr.n = gr.n + 1
end sub

' The angle of ground that runs the way u, v.
function Bearing! (u as single, v as single)
    if u > 0 then
        Bearing! = atn(-v / u)
    elseif u < 0 then
        Bearing! = atn(-v / u) + PI
    else
        Bearing! = -sgn(v) * PI / 2
    end if
end function

' The tiles' stretch of the line under x, its top within 4 of y; -1 when there is none.
function SegAt% (lv as Level, gr as Ground, x as integer, y as single)
    dim s as integer
    SegAt% = -1
    if x < 0 or x >= lv.wpx then exit function
    s = TileSeg%(gr, x, y)
    if s < 0 then s = PlatSeg%(gr, x, y)
    SegAt% = s
end function

' The tiles' stretch under x, its top within 4 of y; -1 when there is none.
function TileSeg% (gr as Ground, x as integer, y as single)
    dim s as integer
    TileSeg% = -1
    s = gr.col(x \ TS)
    if s < 0 then exit function
    ' past a loop that stands in the column: the stretch after it
    if x > gr.pt(s + 1).x then s = s + LOOPN + 1
    if abs(gr.pt(s).y + (x - gr.pt(s).x) * gr.pt(s).v / gr.pt(s).u - y) < 4 then TileSeg% = s
end function

' The platform's stretch under x, at y within 4; -1 when there is none.
function PlatSeg% (gr as Ground, x as integer, y as single)
    dim s as integer
    PlatSeg% = -1
    for s = gr.platA to gr.n - 2
        if gr.pt(s).k <> 1 or x < gr.pt(s).x or x > gr.pt(s + 1).x or abs(gr.pt(s).y - y) >= 4 then continue for
        PlatSeg% = s
        exit function
    next
end function

' What grows past x is pulled up.
sub Weed (lv as Level, x as integer)
    dim i as integer
    for i = 0 to lv.nObj - 1
        if lv.ob(i).k >= KTREE and lv.ob(i).k <= KLAMP and lv.ob(i).x > x then lv.ob(i).k = 0
    next
end sub

' Nothing grows about column c, to hide what stands there.
sub Clearing (lv as Level, c as integer)
    Weed lv, c * TS - 28
    lv.fallow = c + 3
end sub

' Flowers, bushes and lantern plants along the flats, by the column; none before column lv.fallow.
sub Scatter (lv as Level, zn as Realm, c as integer, s as integer)
    if c < 8 or c < lv.fallow then exit sub
    if zn.decked then
        ' lamp posts, tanks and valve stacks along the decks
        select case (c * 37 + 11) mod 23
        case 0, 12: AddObj lv, KPOST, c * TS + 8, s
        case 5: AddObj lv, KTANK, c * TS + 8, s
        case 17: AddObj lv, KVENT, c * TS + 8, s
        end select
        exit sub
    end if
    select case (c * 37 + 11) mod 19
    case 0, 7: AddObj lv, KFLOWER, c * TS + 8, s
    case 3: AddObj lv, KBUSH, c * TS + 8, s
    case 11: AddObj lv, KLAMP, c * TS + 8, s
    end select
end sub

' Column c solid g tiles up from the bottom; kind over 1 a slope on top.
sub Column (lv as Level, c as integer, g as integer, kind as integer)
    dim r as integer
    for r = MH - g to MH - 1
        lv.map(c, r) = 1
    next
    lv.glaze(c) = lv.icing
    if kind > 1 then
        lv.map(c, MH - g - 1) = kind
        lv.top(c) = MH - g - 1
        lv.vis(c) = kind + 2
    else
        lv.top(c) = MH - g
        lv.vis(c) = 1 + (c and 1)
    end if
end sub

' Under a jump or a platform, not a fall to nothing: n columns from c of floor PITD under the ground g, bones on
' it, a spring at its far wall.
sub Pit (lv as Level, c as integer, g as integer, n as integer)
    dim i as integer, y as integer
    y = (MH - g + PITD) * TS
    for i = 0 to n - 1
        Column lv, c + i, g - PITD, 1
        if i > 1 and i < n - 3 and (i and 1) = 0 and not Sunk%(lv, y) then AddObj lv, KBONE, (c + i) * TS + 8, y - 14
    next
    if not Sunk%(lv, y) then AddObj lv, KSPRING, (c + n - 2) * TS + 8, y
end sub

' Ground at y is under the sea for good: deeper than Charlie wades, at low water too.
function Sunk% (lv as Level, y as integer)
    dim ebb as integer
    Sunk% = 0
    if lv.tideLo = NONE then exit function
    ebb = lv.tideLo
    if lv.tideHi > ebb then ebb = lv.tideHi
    Sunk% = y > ebb + WADE
end function

' A shelf w wide, n tiles over the ground, s, at column c; g the ground's height in tiles.
sub Shelf (lv as Level, c as integer, g as integer, s as integer, n as integer, w as integer)
    dim i as integer
    for i = 0 to w - 1
        lv.map(c + i, MH - g - n - 1) = 1
        AddObj lv, KPLAT, (c + i) * TS, s - (n + 1) * TS
    next
end sub

sub AddObj (lv as Level, k as integer, x as integer, y as integer)
    lv.ob(lv.nObj).k = k
    lv.ob(lv.nObj).x = x
    lv.ob(lv.nObj).y = y
    lv.ob(lv.nObj).a = x
    lv.nObj = lv.nObj + 1
end sub

' Whether thing i stands rooted in the ground: of a kind from lo to the lamp, or a post.
function Rooted% (lv as Level, i as integer, lo as integer)
    Rooted% = (lv.ob(i).k >= lo and lv.ob(i).k <= KLAMP) or lv.ob(i).k >= KPOST
end function

' The things by x, and first(c) the first at or past column c.
sub SortObjs (lv as Level)
    dim i as integer, j as integer, c as integer
    for i = 1 to lv.nObj - 1
        j = i
        do while j > 0
            if lv.ob(j - 1).x <= lv.ob(j).x then exit do
            swap lv.ob(j - 1), lv.ob(j)
            j = j - 1
        loop
    next
    i = 0
    for c = 0 to MW
        do while i < lv.nObj
            if lv.ob(i).x >= c * TS then exit do
            i = i + 1
        loop
        lv.first(c) = i
    next
    for i = 0 to lv.nObj - 1
        lv.ob(i).a = lv.ob(i).x
    next
end sub

' The pictures every zone shows: CHARLIE.DAT's, then those built from them or drawn here on the hidden page. A
' zone's own come after them with its level.
sub BuildArt (art as Store, tn as Tones)
    dim i as integer
    cls
    LoadArt art, tn
    ' the turned run, as drawn and mirrored: each upside down too, for the far side of a loop
    for i = 0 to 159
        Flop art, ITILT + i, ITILT + 160 + i, 32, 32
    next
    BuildGlyphs art
    art.zoneAt = art.nextAt
    for i = 0 to 255
        tn.day(i) = tn.now(i)
    next
end sub

' Colour k is c, shown dusk sixteenths as bright.
sub Tint (tn as Tones, k as integer, c as long)
    dim d as integer
    tn.now(k) = c
    d = tn.dusk
    palette k, (c and 63) * d \ 16 + 256& * ((c \ 256 and 63) * d \ 16) + 65536 * ((c \ 65536) * d \ 16)
end sub

' The tiles that move: a deck's lights run along it, a fan's blades turn, glints go down a pipe; by their colours.
sub Cycle (tn as Tones, se as Session)
    dim i as integer
    for i = 0 to 3
        if i = (se.ticks \ 8 and 3) then
            Tint tn, CCHASE + i, 4144952
        else
            Tint tn, CCHASE + i, 3550218
        end if
        if (i + se.ticks \ 4) and 2 then
            Tint tn, CCHASE + 4 + i, 2235666
        else
            Tint tn, CCHASE + 4 + i, 525317
        end if
        if i = (se.ticks \ 4 and 3) then
            Tint tn, CCHASE + 8 + i, 2242111
        else
            Tint tn, CCHASE + 8 + i, 991026
        end if
    next
end sub

sub Shade (tn as Tones)
    dim k as integer
    for k = 0 to 255
        Tint tn, k, tn.now(k)
    next
end sub

' The palette and the pictures every zone shows from CHARLIE.DAT.
sub LoadArt (art as Store, tn as Tones)
    dim n as integer, i as integer, k as integer, c as long
    tn.dusk = 16
    OpenData
    c = Word&
    c = Word&
    n = Word&
    for i = 1 to n
        k = Word&
        Tint tn, k, Word&
    next
    for k = 0 to NSTAGE - 1
        tn.n(k) = Word&
        for i = 0 to tn.n(k) - 1
            tn.which(k, i) = Word&
            tn.own(k, i) = Word&
        next
    next
    for k = 0 to ZONES - 1
        art.datAt(k) = Word&
    next
    ReadPics art
    close #1
end sub

' A group of CHARLIE.DAT's pictures, from where the file is, to the store: their count, then each, its mask and
' its mirror with it.
sub ReadPics (art as Store)
    dim n as integer, i as integer, k as integer, w as integer, h as integer, masked as integer, k2 as integer
    n = Word&
    for i = 1 to n
        k = Word&
        w = Word&
        h = Word&
        masked = Word&
        k2 = Word&
        art.at(k) = art.nextAt
        Unpack art
        if masked then
            art.at(k + 1) = art.nextAt
            Unpack art
        end if
        if k2 < 0 then continue for
        Mirror art, k, k2, w, h
        Mirror art, k + 1, k2 + 1, w, h
    next
end sub

' Zone z's own pictures in the store, where the last zone's were.
sub LoadZoneArt (art as Store, z as integer)
    if art.zone = z + 1 then exit sub
    art.zone = z + 1
    OpenData
    seek #1, art.datAt(z)
    art.nextAt = art.zoneAt
    ReadPics art
    close #1
end sub

' CHARLIE.DAT open as #1: where its tunes and its levels start, then its pictures.
sub OpenData
    open "CHARLIE.DAT" for binary as #1
    if lof(1) = 0 then
        close #1
        kill "CHARLIE.DAT"
        screen 0
        print "CHARLIE.DAT, the game's pictures and tunes, must be beside CHARLIE.BAS."
        end
    end if
end sub

' CHARLIE.DAT's next LONG.
function Word&
    dim v as long
    get #1, , v
    Word& = v
end function

' CHARLIE.DAT's next string: its length, then its characters.
function Chars$
    dim s as string
    s = space$(Word&)
    if len(s) then get #1, , s
    Chars$ = s
end function

' A run-length stream of the file's LONGs to art.img at art.nextAt: n then n values, -n then
' one value n times, 0 the end.
sub Unpack (art as Store)
    dim n as integer, e as long, v as long
    n = Word&
    do while n
        if n > 0 then
            for e = art.nextAt to art.nextAt + n - 1
                get #1, , art.img(e)
            next
        else
            n = -n
            v = Word&
            for e = art.nextAt to art.nextAt + n - 1
                art.img(e) = v
            next
        end if
        art.nextAt = art.nextAt + n
        n = Word&
    loop
end sub

' Image k, w by h, mirrored as k2: a column at a time.
sub Mirror (art as Store, k as integer, k2 as integer, w as integer, h as integer)
    dim x as integer, strip(40) as long
    put (0, 0), art.img(art.at(k)), pset
    for x = 0 to w - 1
        get (x, 0)-(x, h - 1), strip
        put (100 + w - 1 - x, 0), strip, pset
    next
    Grab art, 100, 0, 100 + w - 1, h - 1, k2
end sub

' The HUD's characters, each 9 by 9 over a shadow: yellow, then white.
' And room for the HUD's picture and mask.
sub BuildGlyphs (art as Store)
    dim i as integer, x as integer, y as integer, c as integer, ink as integer
    for c = 0 to 1
        ink = CYELLOW
        if c then ink = CWHITE
        for i = 0 to NGLYPH - 1
            line (0, 0)-(8, 8), 0, BF
            color CWHITE
            locate 24, 1
            print mid$("0123456789:xABCDEHILMNOPRST", i + 1, 1);
            for y = 0 to 7
                for x = 0 to 7
                    if point(x, 184 + y) then pset (x + 1, y + 1), CINK
                next
            next
            for y = 0 to 7
                for x = 0 to 7
                    if point(x, 184 + y) then pset (x, y), ink
                next
            next
            MaskGrab art, 0, 0, 9, 9, 20, 0, IGLYPH + (c * NGLYPH + i) * 2
        next
    next
    line (0, 184)-(8, 191), 0, BF
    Grab art, 0, 0, HUDW - 1, HUDH - 1, IHUD
    Grab art, 0, 0, HUDW - 1, HUDH - 1, IHUD + 1
end sub

sub Grab (art as Store, x1 as integer, y1 as integer, x2 as integer, y2 as integer, k as integer)
    art.at(k) = art.nextAt
    get (x1, y1)-(x2, y2), art.img(art.nextAt)
    art.nextAt = art.nextAt + (4 + clng(x2 - x1 + 1) * (y2 - y1 + 1) + 3) \ 4
end sub

' The w by h at x, y kept as k, and its mask, drawn at mx, my, as k + 1:
' 255 where it is clear, 0 where drawn.
sub MaskGrab (art as Store, x as integer, y as integer, w as integer, h as integer, mx as integer, my as integer, k as integer)
    dim i as integer, j as integer
    for j = 0 to h - 1
        for i = 0 to w - 1
            if point(x + i, y + j) then
                pset (mx + i, my + j), 0
            else
                pset (mx + i, my + j), 255
            end if
        next
    next
    Grab art, x, y, x + w - 1, y + h - 1, k
    Grab art, mx, my, mx + w - 1, my + h - 1, k + 1
end sub

' Masked image k, w by h, at x, y; nothing when wholly off the screen.
sub Blit (art as Store, x as integer, y as integer, k as integer, w as integer, h as integer)
    if x + w <= 0 or x >= SW or y + h <= 0 or y >= SH then exit sub
    put (x, y), art.img(art.at(k + 1)), and
    put (x, y), art.img(art.at(k)), or
end sub

' The keyboard port's last scan code: a press sets kb.held, a release clears.
sub Poll (kb as Keys)
    dim s as integer
    s = inp(&H60)
    if s = kb.last then exit sub
    kb.last = s
    if s = 255 or s = 224 then exit sub
    kb.port = -1
    if s and 128 then
        kb.held(s and 127) = 0
    else
        kb.held(s) = -1
    end if
end sub

sub ReadPad (kb as Keys, mu as Deck, k as Pad)
    dim c as string, t as single
    Poll kb
    t = timer
    do
        c = inkey$
        select case c
        case chr$(27): k.quit = -1
        case "m", "M": k.mute = -1
        case "n", "N"
            mu.tune = not mu.tune
            if not mu.tune then play "P64", "P64", "P64", , "P64", "P64", "P64", "P64"
        case chr$(13): k.start = -1
        case " "
            k.start = -1
            kb.t(SCSPACE) = t
        case "x", "X": k.bark = -1
        case "z", "Z": kb.t(SCZ) = t
        case chr$(0) + "K": kb.t(SCLEFT) = t
        case chr$(0) + "M": kb.t(SCRIGHT) = t
        case chr$(0) + "H": kb.t(SCUP) = t
        case chr$(0) + "P": kb.t(SCDOWN) = t
        end select
    loop while len(c)
    k.l = Held2%(kb, SCLEFT, t)
    k.r = Held2%(kb, SCRIGHT, t)
    k.d = Held2%(kb, SCDOWN, t)
    k.jump = Held2%(kb, SCSPACE, t) or Held2%(kb, SCZ, t) or Held2%(kb, SCUP, t)
    if k.jump and not k.was then k.hit = -1
    k.was = k.jump
end sub

' Key s held: by the port, or, without it, typed within a repeat's wait.
function Held2% (kb as Keys, s as integer, t as single)
    if kb.port then
        Held2% = kb.held(s)
    else
        Held2% = t - kb.t(s) < .55 and kb.t(s) > 0
    end if
end function

' A frame has been shown, d by the clock after the last: the time the game is behind by, c.lag, moved on. The game
' runs by the clock, the same at any frame rate; but the clock is too coarse to time one frame (TIMER moves 4 to 8 ms
' at a time, 55 on DOS), and stepped by it a frame showed two steps or none. So a frame is taken to last what frames
' have lasted (c.span, learnt from the clock): exactly a step, or half of one, where that is within a few percent, the
' lag then kept off the edge between a step and none; otherwise that long, with a little of what the game is behind
' the clock (c.late) made up each frame. A stall is made up at once.
sub Pace (d as single, c as Clock)
    dim a as single, r as single, ph as single
    c.n = c.n + 1
    if d < 3 * c.span or c.n < 3 then
        a = 1 / c.n
        if a < .005 then a = .005
        c.span = c.span + (d - c.span) * a
    end if
    r = c.span / STEPT
    a = 0
    if abs(r - 1) < .05 then a = STEPT
    if abs(r - .5) < .03 then a = STEPT / 2
    if a = 0 then
        ByClock d, c
        exit sub
    end if
    c.late = 0
    ph = c.lag - a * int(c.lag / a)
    c.lag = c.lag + (a / 2 - ph) * .05
    if d > 2.5 * a then a = a * cint(d / a)
    c.lag = c.lag + a
end sub

' A frame of no step's length: that long, with a little of what the game is behind the clock made up; a stall is
' made up at once.
sub ByClock (d as single, c as Clock)
    dim a as single
    a = c.span + c.late * .02
    c.late = c.late + d - a
    if abs(c.late) > .1 then
        a = a + c.late
        c.late = 0
    end if
    if a < 0 then a = 0
    c.lag = c.lag + a
end sub

' The steps the game is behind by, taken off its lag.
function Steps% (c as Clock)
    dim n as integer
    if c.lag > .1 then c.lag = .1
    do while c.lag >= STEPT
        c.lag = c.lag - STEPT
        n = n + 1
    loop
    Steps% = n
end function

function Elapsed! (a as single, b as single)
    dim d as single
    d = b - a
    if d < 0 or d > .1 then d = .1
    Elapsed! = d
end function

' A sound on the fourth voice, the tune's five left playing.
sub Sfx (mu as Deck, se as Session, id as integer)
    dim s as string
    if mu.muted then exit sub
    select case id
    case SJUMP: s = "L64O4CEGO5C"
    case SBONE
        ' a bark: a low woof slid up into, two voices beating, through a mouth; one at a time
        if abs(se.ticks - mu.barkAt) < 8 then exit sub
        mu.barkAt = se.ticks
        s = "W2I15Y2,1000,6H0,6,2,3K4,0,6L8O3D"
    case SHURT: s = "L32O3GECO2G"
    case SSPRING: s = "L64O3CGO4EO5CG"
    case SPOP: s = "L64O5GO4CO5E"
    case SROLL: s = "L64O3CFA"
    case SCHARGE: s = "L64O4CDEFG"
    case SLAUNCH: s = "L64O3GO4CGO5CG"
    case SCLEAR: s = "L16O5CEGO6CL8E"
    case SDIE: s = "L16O4CO3BAGL8F"
    case SMARK: s = "L32O5CEGO6C"
    case SBOOST: s = "L64O4CEGO5CEGO6C"
    case SHISS: s = "W3Y3,3500,0H3,8,0,3L4O6B"
    case SBARK
        ' two woofs, the second lower, through a mouth
        s = "W2I15Y2,1300,6H0,5,3,3K3,0,7L16O3FP32L8O3D"
    case SBUMP
        ' a thump, then his theme's first four notes
        s = "L32O2GP32O4GA+O5CL16D"
    case SSQUAWK
        ' a gull's cry, twice, falling
        s = "W2L32O6ECP32EC"
    case SCRACK
        ' a plank gives: a crack, then its thud
        s = "W3L32O5CO3GL16O2C"
    case SCLANK
        ' a press lands
        s = "W3L32O2CL16O1C"
    end select
    play , , , "T255W1V13Y0K0I0H0,0,15,0" + s
end sub

' The songs the game holds from its start; a stage's own comes with its level. The level's end is led by the
' zone's lead.
sub LoadSongs (mu as Deck)
    LoadSong mu, THEME, 1
    SceneTune mu, CAUGHTUP
    mu.slot = -1
    mu.coming = THEME
    mu.comingBar = 0
    mu.ended = -1
    mu.tune = -1
end sub

' Song id into the zone's slot, unless it is there: what was playing from the slot is dropped.
sub ZoneTune (mu as Deck, id as integer)
    if mu.zHeld = id + 1 then exit sub
    mu.zHeld = id + 1
    LoadSong mu, ZTUNE, id
    mu.song(CAUGHT).leadP = mu.song(ZTUNE).leadP
    if mu.slot = ZTUNE then HushSong mu
end sub

' Song id into the level's end's slot, unless it is there, the zone's lead its own: what was playing from the
' slot is dropped.
sub SceneTune (mu as Deck, id as integer)
    if mu.cHeld = id + 1 then exit sub
    mu.cHeld = id + 1
    LoadSong mu, CAUGHT, id
    mu.song(CAUGHT).leadP = mu.song(ZTUNE).leadP
    if mu.slot = CAUGHT then HushSong mu
end sub

' The level's end for the level there is: after a chase the longer one, the cat sent packing first.
sub EndTune (lv as Level, mu as Deck)
    if lv.chaseX = NONE then
        SceneTune mu, CAUGHTUP
    else
        SceneTune mu, PACKING
    end if
end sub

' The song playing dropped where it is: the next picked starts at once.
sub HushSong (mu as Deck)
    mu.slot = -1
    mu.ended = -1
    mu.fadeOut = 0
end sub

' Song id of CHARLIE.DAT into slot s: its order and levels, its kit, and its bars as PLAY strings.
sub LoadSong (mu as Deck, s as integer, id as integer)
    dim i as integer, n as integer, bars as integer, octs as integer, songs as long
    dim bas(7) as string, hits(7) as string, pat(7) as string
    OpenData
    songs = Word&
    seek #1, songs + 4 + id * 4
    seek #1, songs + Word&
    mu.song(s).order = Chars$
    mu.song(s).voicing = Chars$
    bars = Word&
    mu.song(s).tempo = Word&
    mu.song(s).bassIn = Word&
    for i = 0 to 5
        mu.song(s).lvl(i) = Word&
    next
    mu.song(s).fade = Word&
    mu.song(s).entry = Word&
    mu.song(s).ends = Word&
    octs = Word&
    ReadKit mu, s
    ReadList bas()
    ReadList hits()
    ReadList pat()
    for n = 0 to bars - 1
        ReadBar mu, s, n, bas(), hits(), pat(), 27 + 12 * octs
    next
    close #1
end sub

' Slot s's kit, CHARLIE.DAT's next: four sounds, its voicings and its drums.
sub ReadKit (mu as Deck, s as integer)
    dim i as integer, n as integer
    mu.song(s).leadP = Chars$
    mu.song(s).bassP = Chars$
    mu.song(s).underP = Chars$
    mu.song(s).midP = Chars$
    mu.song(s).voLet = ""
    n = Word&
    for i = 0 to n - 1
        mu.song(s).voLet = mu.song(s).voLet + Chars$
        mu.song(s).voOne(i) = Chars$
        mu.song(s).voTwo(i) = Chars$
        mu.song(s).voUnder(i) = Word&
    next
    mu.song(s).drLet = ""
    n = Word&
    for i = 0 to n - 1
        mu.song(s).drLet = mu.song(s).drLet + Chars$
        mu.song(s).drPatch(i) = Chars$
        mu.song(s).drVol(i) = Word&
        mu.song(s).drUp(i) = Word&
        mu.song(s).drNote(i) = Chars$
    next
end sub

' CHARLIE.DAT's next list of strings, after its count.
sub ReadList (a() as string)
    dim i as integer, n as integer
    n = Word&
    for i = 0 to n - 1
        a(i) = Chars$
    next
end sub

' Slot s's bar n, CHARLIE.DAT's next: its root, bass riff and drum pattern, and four riffs, made its two half bars'
' PLAY strings, the lead's lowest note `lead`. A second voice of two characters is a chord pattern, its x the note,
' an octave down. A third voice of no loudness is left out.
sub ReadBar (mu as Deck, s as integer, n as integer, bas() as string, hits() as string, pat() as string, lead as integer)
    dim root as integer, nb as integer, nd as integer, low as integer, j as integer, h as integer
    dim st as string, br as string, sol as string, mid as string
    root = Word&
    nb = Word&
    nd = Word&
    st = Chars$
    br = Chars$
    sol = Chars$
    mid = Chars$
    if mu.song(s).lvl(5) = 0 then mid = ""
    low = 27
    if len(st) = 2 then
        low = 15
        st = hits(val(left$(st, 1))) + right$(st, 1)
        do while instr(st, "x") and instr(st, "x") < 17
            mid$(st, instr(st, "x"), 1) = mid$(st, 17, 1)
        loop
    end if
    for j = 0 to 1
        h = n * 2 + j
        mu.song(s).half(h).kit = mid$(pat(nd), j * 8 + 1, 8)
        mu.song(s).half(h).drums = Beat$(mu, s, mu.song(s).half(h).kit, 8)
        mu.song(s).half(h).bass = Riff$(bas(nb - 1), j, 15 + root)
        mu.song(s).half(h).one = Riff$(br, j, 27)
        mu.song(s).half(h).lead = Riff$(sol, j, lead)
        mu.song(s).half(h).two = Riff$(st, j, low)
        mu.song(s).half(h).under = Riff$(br, j, 15)
        mu.song(s).half(h).third = Riff$(mid, j, 27)
    next
end sub

' Half a bar of slot s's drums, `loud` eighths as loud: each of x's letters its drum, any other a rest.
function Beat$ (mu as Deck, s as integer, x as string, loud as integer)
    dim q as integer, n as integer, d as string
    for q = 1 to 8
        n = instr(mu.song(s).drLet, mid$(x, q, 1)) - 1
        if n < 0 then
            d = d + "P16"
        else
            d = d + mu.song(s).drPatch(n) + Vol$(mu.song(s).drVol(n), loud, mu.song(s).drUp(n)) + mu.song(s).drNote(n)
        end if
    next
    Beat$ = d
end function

' PLAY's volume for a level `loud` eighths as loud, rounded up by `up` eighths.
function Vol$ (level as integer, loud as integer, up as integer)
    Vol$ = "V" + ltrim$(str$((level * loud + up) \ 8))
end function

' Half h of a bar's riff as PLAY's notes, its lowest note `low`: at most eight, as the drums' always are.
function Riff$ (p as string, h as integer, low as integer)
    dim i as integer, j as integer, r as integer, last as integer, note as integer
    dim c as string, s as string, ln as string
    if len(p) < 16 then
        Riff$ = "P2"
        exit function
    end if
    i = h * 8 + 1
    last = h * 8 + 8
    do while i <= last
        c = mid$(p, i, 1)
        j = i
        ' a note held over the half bar is struck again
        do while c = "-" and j > 1
            j = j - 1
            c = mid$(p, j, 1)
        loop
        r = 1
        if c = "." or c = "-" then
            note = 0
            do while i + r <= last and mid$(p, i + r, 1) = "."
                r = r + 1
            loop
        else
            note = low + instr("0123456789abcdefghijklmnopqrstuvwxyz", lcase$(c)) - 1
            do while i + r <= last and mid$(p, i + r, 1) = "-"
                r = r + 1
            loop
        end if
        i = i + r
        ' five and seven sixteenths: four and six, then a rest
        j = 0
        if r = 5 or r = 7 then
            r = r - 1
            j = 1
        end if
        select case r
        case 1: ln = "16"
        case 2, 3: ln = "8"
        case 4, 6: ln = "4"
        case else: ln = "2"
        end select
        if note then
            ln = "L" + ln + "N" + ltrim$(str$(note))
        else
            ln = "P" + ln
        end if
        if r = 3 or r = 6 then ln = ln + "."
        ' a capital slides from the note before
        if note > 0 and c <> lcase$(c) then ln = "K6" + ln + "K0"
        s = s + ln
        if j then s = s + "P16"
    loop
    Riff$ = s
end function

' The next half bar, once the last is nearly played: the drums are the first voice, eight sixteenths every time,
' so PLAY(0) counts the tune's own clock. The last four bars fade, an eighth a half bar.
' The music kept fed: when little is left to play, its next half bar.
sub FeedSong (mu as Deck, se as Session)
    if mu.muted or not mu.tune then exit sub
    if play(0) > 2 then exit sub
    NextHalf mu, se
end sub

' The playing song's next half bar to PLAY, its voices each their own string; or the song that was coming, begun.
' Nothing once a song has ended.
sub NextHalf (mu as Deck, se as Session)
    dim b as integer, loud as integer, n as integer
    ' the song picked comes in once this one has faded, or at once when it has ended
    if mu.coming >= 0 and (mu.fadeOut = 0 or mu.ended) then StartSong mu, se
    if mu.ended then exit sub
    n = len(mu.song(mu.slot).order)
    b = asc(mid$(mu.song(mu.slot).order, mu.bar + 1, 1)) - 97
    loud = (n - mu.bar) * 2 - mu.half
    if loud > 8 or mu.bar < n - mu.song(mu.slot).fade then loud = 8
    if mu.fadeOut > 0 then
        loud = loud * mu.fadeOut \ 3
        mu.fadeOut = mu.fadeOut - 1
    elseif mu.fadeIn > 0 then
        loud = loud * (3 - mu.fadeIn) \ 3
        mu.fadeIn = mu.fadeIn - 1
    end if
    PlayHalf mu, mu.song(mu.slot), b, loud
    mu.half = 1 - mu.half
    if mu.half then exit sub
    mu.bar = mu.bar + 1
    if mu.bar < n then exit sub
    mu.bar = 0
    mu.ended = mu.song(mu.slot).ends
end sub

' Song sg's bar b, the half the deck is at, to PLAY, `loud` eighths as loud: its voices each their own string.
sub PlayHalf (mu as Deck, sg as Song, b as integer, loud as integer)
    dim v as integer, h as integer
    dim t as string, d as string, bass as string, under as string
    dim one as string, two as string, solo as string, midv as string
    t = "T" + ltrim$(str$(sg.tempo))
    h = b * 2 + mu.half
    d = sg.half(h).drums
    if loud < 8 then d = Beat$(mu, mu.slot, sg.half(h).kit, loud)
    ' the bass waits its bars; the low double plays under the voicings that have it
    bass = Vol$(sg.lvl(4), loud, 0) + sg.half(h).bass
    if mu.bar < sg.bassIn then bass = "P2"
    v = instr(sg.voLet, mid$(sg.voicing, b + 1, 1)) - 1
    under = "P2"
    if sg.voUnder(v) then under = Vol$(sg.lvl(3), loud, 0) + sg.half(h).under
    one = t + sg.voOne(v) + Vol$(sg.lvl(0), loud, 0) + sg.half(h).one
    solo = t + sg.leadP + Vol$(sg.lvl(1), loud, 7) + sg.half(h).lead
    two = t + sg.voTwo(v) + Vol$(sg.lvl(2), loud, 0) + sg.half(h).two
    midv = t + Vol$(sg.lvl(5), loud, 0) + sg.half(h).third
    play t + d, t + bass, one, , solo, two, t + under, midv
end sub

' The song that was coming begins: its kit to the voices that keep theirs. In stereo: the first chord voice to the
' left, the second and the low double to the right; the room answers little.
sub StartSong (mu as Deck, se as Session)
    ' its first note sounds once what is queued has: the sixteenths left of the song before
    mu.songAt = se.ticks
    if mu.slot >= 0 then mu.songAt = se.ticks + play(0) * 900& \ mu.song(mu.slot).tempo
    mu.slot = mu.coming
    mu.bar = mu.comingBar
    mu.half = 0
    mu.coming = -1
    mu.ended = 0
    mu.fadeOut = 0
    Patch mu.song(mu.slot)
end sub

' Song sg's kit to the voices that keep theirs.
sub Patch (sg as Song)
    dim t as string
    t = "T" + ltrim$(str$(sg.tempo))
    play "MB" + t + "Z1,0L16", t + sg.bassP, t + "R2S4", , t + "R2" + sg.leadP, t + "R2S12", t + sg.underP, t + sg.midP
end sub

' Beats of the level's end sounded: by the tune's own clock, by the scene's where no music plays; under none before
' it has begun.
function EndBeat! (mu as Deck, se as Session, g as Game)
    EndBeat! = g.modeT * mu.song(CAUGHT).tempo / 60
    if mu.muted or not mu.tune then exit function
    EndBeat! = -1
    if mu.slot = CAUGHT then EndBeat! = (se.ticks - mu.songAt) * STEPT * mu.song(CAUGHT).tempo / 60
end function

' The bonus counted into the score in time with the level's end: a step and a woof on each beat of the bars its
' lead plays, the bonus shared between the steps left, in tens; all of it at a key.
sub Tally (mu as Deck, se as Session, g as Game, k as Pad)
    dim due as integer, take as long
    if g.bonus = 0 then exit sub
    due = int(EndBeat!(mu, se, g)) - (len(mu.song(CAUGHT).order) - TALLYBARS) * 4 + 1
    if due > TALLYN or k.hit or k.start then due = TALLYN
    if due <= g.tallied then exit sub
    do while g.tallied < due and g.bonus > 0
        take = ((g.bonus \ (TALLYN - g.tallied) + 9) \ 10) * 10
        g.tallied = g.tallied + 1
        if take > g.bonus then take = g.bonus
        g.bonus = g.bonus - take
        g.score = g.score + take
    loop
    Sfx mu, se, SBONE
end sub

' The slot to play for the game's state: the theme for the title, the level's end or game over, else the zone's
' tune.
function SongFor% (g as Game)
    select case g.mode
    case TITLE: SongFor% = THEME
    case CLEARED, OVER: SongFor% = CAUGHT
    case else: SongFor% = ZTUNE
    end select
end function

' Whether the state's song comes in at once and in full: the level's end, game over, the scene between zones.
function Sudden% (se as Session, g as Game)
    Sudden% = g.mode = CLEARED or g.mode = OVER or (g.mode = INTRO and se.sceneNext > 0)
end function

' Slot n's song, when it is not the one playing, from its start or, late, from the bar it comes in at. The one
' playing fades over a bar (half of one, late) and the next fades in over one; late, it comes in full.
sub PickSong (mu as Deck, n as integer, late as integer)
    if n = mu.slot then
        mu.coming = -1
        mu.fadeOut = 0
        exit sub
    end if
    if n = mu.coming then exit sub
    mu.coming = n
    mu.comingBar = 0
    mu.fadeOut = 2
    mu.fadeIn = 2
    if late = 0 then exit sub
    mu.comingBar = mu.song(n).entry
    mu.fadeOut = 1
    mu.fadeIn = 0
end sub

' The ground's top at column sx near y: in y's tile, the one above when
' that is full, the one below when it is empty; NONE when both are, or
' off the level.
function Surf% (lv as Level, sx as integer, y as integer)
    dim tx as integer, ty as integer, i as integer, h as integer
    Surf% = NONE
    if sx < 0 or sx >= lv.wpx or y < TS then exit function
    tx = sx \ TS
    i = sx and 15
    ty = y \ TS
    if ty >= MH - 1 then exit function
    h = lv.prof(lv.map(tx, ty), i)
    if h = TS then
        Surf% = ty * TS - lv.prof(lv.map(tx, ty - 1), i)
    elseif h = 0 then
        h = lv.prof(lv.map(tx, ty + 1), i)
        if h then Surf% = (ty + 2) * TS - h
    else
        Surf% = (ty + 1) * TS - h
    end if
end function

' Past the level's ends a wall; above and below it air.
function Solid% (lv as Level, x as integer, y as integer)
    if x < 0 or x >= lv.wpx then
        Solid% = -1
        exit function
    end if
    if y < 0 or y >= MH * TS then
        Solid% = 0
        exit function
    end if
    Solid% = (y and 15) >= TS - lv.prof(lv.map(x \ TS, y \ TS), x and 15)
end function

' Charlie's side toward dir in a wall: pushed out, and true.
function Wall% (lv as Level, p as Hero, dir as integer)
    dim n as integer, y as integer
    y = cint(p.y)
    do while Solid%(lv, cint(p.x) + dir * HW, y - 11) or Solid%(lv, cint(p.x) + dir * HW, y - HH + 4)
        p.x = p.x - dir
        n = n + 1
        if n > TS then exit do
    loop
    Wall% = n > 0
end function

' Falling Charlie's feet in the tiles' ground: on the line's stretch there, its speed along it kept.
function Land% (lv as Level, gr as Ground, p as Hero, d as integer)
    dim x as integer, y as integer, l as integer, c as integer, r as integer, s as integer
    x = cint(p.x)
    y = cint(p.y)
    l = Surf%(lv, x - 6, y)
    c = Surf%(lv, x, y)
    r = Surf%(lv, x + 6, y)
    if l > y or l < y - 14 then l = NONE
    if c > y or c < y - 14 then c = NONE
    if r > y or r < y - 14 then r = NONE
    if l = NONE and c = NONE and r = NONE then
        Land% = 0
        exit function
    end if
    ' by a foot on an edge: onto the edge
    if c <> NONE then
        s = SegAt%(lv, gr, x, csng(c))
    elseif l < r then
        s = SegAt%(lv, gr, x - 6, csng(l))
        ' going off that edge, not onto it: no ground
        if p.vx > 0 then s = -1
    else
        s = SegAt%(lv, gr, x + 6, csng(r))
        if p.vx < 0 then s = -1
    end if
    if s < 0 then
        Land% = 0
        exit function
    end if
    if p.x < gr.pt(s).x then p.x = gr.pt(s).x
    if p.x > gr.pt(s + 1).x then p.x = gr.pt(s + 1).x
    p.y = gr.pt(s).y + (p.x - gr.pt(s).x) * gr.pt(s).v / gr.pt(s).u
    Settle gr, p, s, d
    Land% = -1
end function

' Down on stretch s: the speed along it kept.
sub Settle (gr as Ground, p as Hero, s as integer, d as integer)
    p.sg = s
    p.ang = gr.pt(s).a
    p.gsp = p.vx * gr.pt(s).u + p.vy * gr.pt(s).v
    if p.jumped and not d and not p.sled then p.roll = 0
    ' hit, it lands where it is and stands afraid a while
    if p.stun then p.gsp = 0
    p.ground = -1
    p.jumped = 0
    p.sprung = 0
    p.coyote = 0
end sub

' Charlie in the air, come from x0, y0, through a curve's stretch from its open side: down on it where it can be
' stood on, or is met fast enough to run along; else off it, sliding. A loop's far half is not there for one that
' has not run its near half.
function Curve% (gr as Ground, p as Hero, x0 as single, y0 as single, d as integer)
    dim z as integer, s as integer, a as integer, b as integer, near as integer
    dim d0 as single, d1 as single, t as single, f as single
    Curve% = 0
    for z = 0 to gr.nz - 1
        if p.x < gr.cv(z).l - 8 or p.x > gr.cv(z).r + 8 then continue for
        a = gr.cv(z).a
        b = gr.cv(z).b
        LoopHalf gr, p, z, x0, a, b
        for s = a to b
            d1 = (p.x - gr.pt(s).x) * gr.pt(s).v - (p.y - gr.pt(s).y) * gr.pt(s).u
            if d1 >= 0 or p.vx * gr.pt(s).v - p.vy * gr.pt(s).u >= -1 then continue for
            d0 = (x0 - gr.pt(s).x) * gr.pt(s).v - (y0 - gr.pt(s).y) * gr.pt(s).u
            t = (p.x - gr.pt(s).x) * gr.pt(s).u + (p.y - gr.pt(s).y) * gr.pt(s).v
            ' from just behind it too, at a join; not behind a wall
            ' and from further behind a platform, which moves
            near = d0 > -1 + 3 * (gr.pt(s).k = 3) and (d0 >= 0 or gr.pt(s).u >= .35)
            if near = 0 or t < -1 or t > gr.pt(s).l + 1 then continue for
            if t < 0 then t = 0
            if t > gr.pt(s).l then t = gr.pt(s).l
            p.x = gr.pt(s).x + gr.pt(s).u * t
            p.y = gr.pt(s).y + gr.pt(s).v * t
            f = p.vx * gr.pt(s).u + p.vy * gr.pt(s).v
            if gr.pt(s).u >= .35 or abs(f) >= STALL then
                Settle gr, p, s, d
                Curve% = -1
            else
                p.x = p.x + gr.pt(s).v
                p.y = p.y - gr.pt(s).u
                p.vx = f * gr.pt(s).u
                p.vy = f * gr.pt(s).v
            end if
            exit function
        next
    next
end function

' Of curve z, a loop, the half that is there for him, from stretch a to b: its far one once he has run its near.
sub LoopHalf (gr as Ground, p as Hero, z as integer, x0 as single, a as integer, b as integer)
    dim far as integer
    if gr.cv(z).m < 0 then exit sub
    far = p.sg >= gr.cv(z).m
    if p.sg >= gr.platA then far = x0 > gr.cv(z).l + LOOPR
    if far then
        a = gr.cv(z).m
    else
        b = gr.cv(z).m - 1
    end if
end sub

' Charlie d pixels along the line, over its joins: slowed in a sharp hollow, into the air over a sharp crest or
' off the line's end.
sub Along (gr as Ground, p as Hero, d as single)
    dim i as integer, j as integer, a as integer, t as single, cr as single, dt as single
    i = p.sg
    t = (p.x - gr.pt(i).x) * gr.pt(i).u + (p.y - gr.pt(i).y) * gr.pt(i).v + d
    do
        if t > gr.pt(i).l then
            j = i + 1
            a = i
        elseif t < 0 then
            j = i - 1
            a = j
        else
            exit do
        end if
        if j >= 0 then if gr.pt(j).k = 0 then j = -1
        if j >= 0 then
            cr = gr.pt(a).u * gr.pt(a + 1).v - gr.pt(a).v * gr.pt(a + 1).u
            dt = gr.pt(a).u * gr.pt(a + 1).u + gr.pt(a).v * gr.pt(a + 1).v
            if cr > 0 and dt < .64 then j = -1
        end if
        if j < 0 then
            if t >= 0 then t = gr.pt(i).l
            if t < 0 then t = 0
            p.x = gr.pt(i).x + gr.pt(i).u * t
            p.y = gr.pt(i).y + gr.pt(i).v * t
            Drop gr, p
            exit sub
        end if
        if t < 0 then
            t = t + gr.pt(j).l
        else
            t = t - gr.pt(i).l
        end if
        if cr < 0 and dt < .7 then Crease p, t, dt, gr.pt(j).l, j > i
        i = j
    loop
    p.sg = i
    p.ang = gr.pt(i).a
    p.x = gr.pt(i).x + gr.pt(i).u * t
    p.y = gr.pt(i).y + gr.pt(i).v * t
end sub

' Over a crease that turns the ground up: speed is lost by the turn's cosine dt, and so is how far he is past the
' crease on the new stretch, l long; ahead, the crease is at its start.
sub Crease (p as Hero, t as single, dt as single, l as single, ahead as integer)
    dim k as single
    k = dt
    if k < 0 then k = 0
    p.gsp = p.gsp * k
    if ahead then
        t = t * k
    else
        t = l - (l - t) * k
    end if
end sub

' Off the ground, its speed kept: from a wall or a roof, clear of it.
sub Drop (gr as Ground, p as Hero)
    dim i as integer
    i = p.sg
    p.ground = 0
    p.vx = p.gsp * gr.pt(i).u
    p.vy = p.gsp * gr.pt(i).v
    if gr.pt(i).u > .7 then
        p.coyote = COYOTE
    elseif gr.pt(i).u < .35 then
        p.x = p.x + 2 * gr.pt(i).v
        p.y = p.y - 2 * gr.pt(i).u
    end if
end sub

' A step. The title plays itself: Charlie run by the pilot.
sub Tick (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
    wd.se.ticks = wd.se.ticks + 1
    if wd.zn.id = 1 and (wd.se.ticks and 3) = 0 then Cycle wd.tn, wd.se
    if wd.lv.tideLo <> NONE then Flow wd.lv, wd.se
    if wd.lv.nMv then Movers wd.lv, wd.gr, wd.mu, wd.se, p
    g.modeT = g.modeT + STEPT
    if g.shake > 0 then g.shake = g.shake - STEPT
    MuteKey wd.mu, k
    select case g.mode
    case TITLE: TitleStep wd, g, p, k, lo(), pt()
    case PLAYING: PlayStep wd, g, p, k, lo(), pt()
    case CLEARED: ClearedStep wd, g, p, k, lo(), pt()
    case OVER
        if g.modeT > OVERT or (g.modeT > 1.5 and (k.hit or k.start)) or k.quit then ResetWorld wd, g, p, lo(), pt()
    end select
    if (wd.se.ticks and 1) = 0 then Things wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    Shimmer wd.zn, wd.tn, wd.se
    Spent k
end sub

' The title's step: behind it the game plays itself, from the start again when that ends; a key starts a game.
sub TitleStep (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
    dim auto as Pad
    if k.quit then g.quitting = -1
    if p.dead then
        ResetWorld wd, g, p, lo(), pt()
    elseif p.x >= wd.lv.goalX then
        if Met%(wd, g, p, k, pt()) then ResetWorld wd, g, p, lo(), pt()
    else
        Pilot wd.lv, wd.gr, wd.zn, wd.se, p, auto
        MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, auto, pt()
        Touch wd.lv, wd.mu, wd.se, g, p, lo(), pt()
        if p.y > MH * TS + 24 then Lose wd.mu, wd.se, p
        Camera wd.lv, wd.zn, g, p, 0
    end if
    PickZone wd.se, k
    if k.hit or k.start then StartGame wd, g, p, lo(), pt()
end sub

' A game's step: Charlie moved, against the things and what chases, flies and is thrown; past the goal, the stage
' is cleared.
sub PlayStep (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
    if k.quit then ResetWorld wd, g, p, lo(), pt()
    if p.dead then
        Dying wd.lv, wd.gr, wd.zn, wd.mu, g, p
        exit sub
    end if
    g.clock = g.clock + STEPT
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, k, pt()
    Touch wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    if wd.lv.stray.st = 2 then Homeward wd.lv, g
    if wd.lv.avalX <> NONE then Avalanche wd.lv, wd.mu, wd.se, g, p
    Flee wd.lv, wd.zn, wd.mu, wd.se, g, p, lo(), pt()
    if wd.lv.nGulls then Flock wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    if wd.lv.lobN then Volley wd.lv, wd.mu, wd.se, g, p, lo(), pt()
    if p.y > MH * TS + 24 then Lose wd.mu, wd.se, p
    if p.x >= wd.lv.goalX then StageClear wd.mu, wd.se, g
    Camera wd.lv, wd.zn, g, p, 0
end sub

' The goal passed: the stage is cleared, its bonus by its bones and its time.
sub StageClear (mu as Deck, se as Session, g as Game)
    g.mode = CLEARED
    g.modeT = 0
    g.tallied = 0
    g.bonus = g.bones * 50& + 10& * (240 - int(g.clock))
    if g.bonus < g.bones * 50& then g.bonus = g.bones * 50&
    Sfx mu, se, SCLEAR
end sub

' A cleared stage's step: its tally; then, met, a while and the fade over, on to the next stage, or after the
' last, the title.
sub ClearedStep (wd as World, g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
    dim nxt as integer
    Tally wd.mu, wd.se, g, k
    if k.quit then
        ResetWorld wd, g, p, lo(), pt()
        exit sub
    end if
    if Met%(wd, g, p, k, pt()) = 0 then exit sub
    nxt = wd.zn.stage + 1
    if nxt > NSTAGE - 1 then
        ResetWorld wd, g, p, lo(), pt()
        exit sub
    end if
    Reach wd.se, ZoneOf%(wd.se, nxt)
    if ZoneOf%(wd.se, nxt) = wd.zn.id then
        GoStage wd, nxt, g, p, lo(), pt()
    else
        StartBump wd, g, nxt
    end if
end sub

' M: the sound off, or on again.
sub MuteKey (mu as Deck, k as Pad)
    if k.mute = 0 then exit sub
    mu.muted = not mu.muted
    k.mute = 0
    if mu.muted then play "P64", "P64", "P64", , "P64", "P64", "P64", "P64"
end sub

' The Downs' lake glints: its three tones change places, every 16 steps.
sub Shimmer (zn as Realm, tn as Tones, se as Session)
    dim i as integer
    if zn.id <> 0 or (se.ticks and 15) <> 0 then exit sub
    i = se.ticks \ 16
    Tint tn, CWATER + 1, tn.glint(i mod 3)
    Tint tn, CWATER + 2, tn.glint((i + 1) mod 3)
    Tint tn, CWATER + 3, tn.glint((i + 2) mod 3)
end sub

' The presses a step has seen are spent.
sub Spent (k as Pad)
    k.hit = 0
    k.start = 0
    k.quit = 0
    k.bark = 0
end sub

' The pad of a Charlie that holds right and jumps at pits, walls and cats.
' Whether a ribbon starts within 60 ahead of x: its edge is run off, not jumped.
function Ribbon% (gr as Ground, x as integer)
    dim z as integer
    Ribbon% = 0
    for z = 0 to gr.nz - 1
        if gr.cv(z).m < 0 and gr.pt(gr.cv(z).a).k = 2 and gr.cv(z).l > x - 16 and gr.cv(z).l < x + 60 then Ribbon% = -1
    next
end function

' A ribbon is a strip wound once round a line along its way, each twist 2 * WAVEL long: its near edge rises to a
' crest and falls, and what runs it goes round its inside, on its far side going up and behind it coming down.
' How far round p on one is: 0 upright at its foot, PI upside down under its crest; -1 on none.
function Twist! (gr as Ground, p as Hero)
    dim z as integer, t as single
    Twist! = -1
    if not p.ground then exit function
    for z = 0 to gr.nz - 1
        if gr.cv(z).m <> -2 or p.sg < gr.cv(z).a or p.sg > gr.cv(z).b then continue for
        t = (p.x - gr.cv(z).l - 8) / (2 * WAVEL)
        if t < 0 or p.x > gr.cv(z).r - 8 then t = 0
        Twist! = (t - int(t)) * 2 * PI
        exit function
    next
end function

' The game's own player: Right held, a jump where one is wanted, still where it must wait.
sub Pilot (lv as Level, gr as Ground, zn as Realm, se as Session, p as Hero, k as Pad)
    static hold as integer
    dim want as integer, hopNow as integer
    k.r = -1
    k.l = 0
    k.d = 0
    if p.swim then
        hopNow = PilotSwim%(lv, zn, p, k)
    elseif p.ground and gr.pt(p.sg).k = 3 then
        want = PilotRide%(lv, gr, se, p, k)
    elseif p.ground and gr.pt(p.sg).k = 1 then
        want = PilotRun%(lv, gr, zn, se, p, k)
    elseif not p.ground then
        PilotFall lv, zn, p, k
    end if
    if lv.gale and p.ground then PilotGale lv, se, p, k, want
    ' afloat a hop is made though the last is still held
    if hopNow or (want and hold = 0) then
        k.hit = -1
        hold = 24
    end if
    k.jump = hold > 0
    if hold > 0 then hold = hold - 1
end sub

' The pilot's keys: on to the right.
sub PressOn (k as Pad)
    k.r = -1
    k.l = 0
end sub

' The pilot's keys: no further, and back against a speed over v.
sub HoldBack (p as Hero, k as Pad, v as integer)
    k.r = 0
    if p.gsp > v then k.l = -1
end sub

' Afloat: on to the right, a hop where a bank is in the way, or a jellyfish. Whether to hop.
function PilotSwim% (lv as Level, zn as Realm, p as Hero, k as Pad)
    dim i as integer, d as integer, want as integer
    want = Solid%(lv, cint(p.x) + HW + 2, cint(p.y) - 11)
    for i = lv.first(cint(p.x) \ TS) to lv.nObj - 1
        if lv.ob(i).x > p.x + 34 then exit for
        if lv.ob(i).k = KJELLY and lv.ob(i).x > p.x then want = -1
    next
    PilotSwim% = want
    ' Kipper's boat about: still, and a hop as it comes, to come down on its deck
    if zn.id <> 2 or lv.cart.st <> 1 then exit function
    k.r = 0
    d = cint(lv.cart.x - p.x)
    want = lv.cart.t <= 0 and abs(d) < abs(lv.cart.v) * .55 + 12 and abs(d) > abs(lv.cart.v) * .45
    PilotSwim% = want and sgn(d) <> sgn(lv.cart.v)
end function

' On a platform: still, till there is ground ahead to jump to. Whether to jump.
function PilotRide% (lv as Level, gr as Ground, se as Session, p as Hero, k as Pad)
    dim y as integer, i as integer, m as integer, d as integer, want as integer
    y = cint(p.y)
    HoldBack p, k, 20
    i = cint(gr.pt(p.sg).x) + MOVEW + 10
    if Surf%(lv, i, y) <> NONE and not Solid%(lv, i, y - 12) then
        PressOn k
        want = -1
    end if
    for m = 0 to lv.nMv - 1
        if lv.mv(m).s <> p.sg then continue for
        select case lv.mv(m).k
        case 4, 6
            ' a bridge is run over, and a pier
            PressOn k
            want = 0
        case 5
            ' a boat low in its pool: off it at the wall, once the bank is a jump up
            d = lv.top((i + 6) \ TS) * TS
            if Swing!(lv, se, m, 0) > .85 and d < y and d > y - 44 then
                PressOn k
                want = -1
            end if
        end select
    next
    PilotRide% = want
end function

' On the ground: a jump at a gap, a wall, or what would hurt; a wait at a press, a goat, a platform's gap. Whether
' to jump.
function PilotRun% (lv as Level, gr as Ground, zn as Realm, se as Session, p as Hero, k as Pad)
    dim x as integer, y as integer, i as integer, d as integer, want as integer
    x = cint(p.x)
    y = cint(p.y)
    i = x + 22 + cint(abs(p.gsp)) \ 40
    if Surf%(lv, i, y) = NONE and Surf%(lv, i, y + 2 * TS) = NONE and not Ribbon%(gr, x) then want = -1
    ' a wall, not a ramp's foot
    if Solid%(lv, x + 13, y - 20) then want = -1
    for i = lv.first(x \ TS) to lv.nObj - 1
        if lv.ob(i).x > x + 130 then exit for
        if lv.ob(i).x <= x then continue for
        d = (lv.ob(i).k = KCAT and lv.ob(i).s <> 0) or lv.ob(i).k = KTHORN
        if d and lv.ob(i).x <= x + 50 then want = -1
        ' what is not rolled through, a crab, a jellyfish, a sweeper, a spark: jumped from further back the faster
        ' he goes
        d = lv.ob(i).k = KJELLY or lv.ob(i).k = KSPARK or (lv.ob(i).k = KCRAB and abs(lv.ob(i).s) = 1)
        d = d or ((lv.ob(i).k = KSWEEP or lv.ob(i).k = KTHROW) and lv.ob(i).s <> 0)
        if d and lv.ob(i).x <= x + 34 + abs(cint(p.gsp)) \ 8 then want = -1
    next
    PilotPress lv, se, p, k, want
    PilotGoat lv, p, k, want
    ' who flees is rolled into, once there is the speed to roll
    k.d = (zn.id = 0 or zn.id = 3) and lv.cart.st = 1 and lv.cart.x > x and lv.cart.x - x < 120 and p.gsp > 150
    PilotGaps lv, se, p, k, want
    PilotRun% = want
end function

' A row of presses ahead: to a stop before it, on once its first is well risen, and no jump under one.
sub PilotPress (lv as Level, se as Session, p as Hero, k as Pad, want as integer)
    dim i as integer, d as integer
    i = PressAhead%(lv, cint(p.x))
    if i = NONE then exit sub
    d = lv.ob(i).x - 26 - cint(p.x)
    if d < 60 then want = 0
    if d <= 0 or d >= 40 + cint(p.gsp) \ 5 then exit sub
    if (PressT%(lv, se, i) + 22) mod PRESSP > 52 then HoldBack p, k, 20
end sub

' A goat near: to a stand short of it, up as it charges, and down on its back.
sub PilotGoat (lv as Level, p as Hero, k as Pad, want as integer)
    dim i as integer, d as integer
    i = GoatNear%(lv, cint(p.x))
    if i = NONE or p.goat <> 0 then exit sub
    d = lv.ob(i).x - cint(p.x)
    if abs(lv.ob(i).s) = GCHARGE then
        k.r = 0
        want = abs(d) < 70
        exit sub
    end if
    if d <= 0 or d >= 56 + cint(p.gsp) \ 4 then exit sub
    want = 0
    HoldBack p, k, 20
end sub

' A platform's gap ahead: slowly to its edge, there till the platform is at it and staying, then on.
sub PilotGaps (lv as Level, se as Session, p as Hero, k as Pad, want as integer)
    dim m as integer, d as integer, x as integer, y as integer
    x = cint(p.x)
    y = cint(p.y)
    for m = 0 to lv.nMv - 1
        d = lv.mv(m).e - x
        if lv.mv(m).k = 4 or lv.mv(m).k = 6 then d = NONE
        if d <= -4 or d >= 300 or abs(lv.mv(m).y + lv.mv(m).a * (lv.mv(m).k = 1) - y) >= 200 then continue for
        if d < 50 then want = 0
        if p.gsp > 30 + d then k.r = 0
        if p.gsp > 70 + d then k.l = -1
        if d >= 10 then continue for
        if Ready%(lv, se, m) then
            PressOn k
        else
            HoldBack p, k, 5
        end if
    next
end sub

' Whether moving platform m is at its gap's edge and staying, to be stepped onto.
function Ready% (lv as Level, se as Session, m as integer)
    select case lv.mv(m).k
    case 0: Ready% = Swing!(lv, se, m, 0) < -.85 and Swing!(lv, se, m, 1) < 0
    case 5
        ' a boat aground is no way over: down into its pool
        Ready% = not Adrift%(lv, m) or (Swing!(lv, se, m, 0) < -.85 and Swing!(lv, se, m, 1) < 0)
    case 1: Ready% = Swing!(lv, se, m, 0) >= 1 and Swing!(lv, se, m, 1) > .2
    case 2, 3, 7: Ready% = lv.mv(m).t = 0
    end select
end function

' In the air: straight down onto Kipper's deck or a goat's back; slowed toward a platform's gap, to come down
' before it.
sub PilotFall (lv as Level, zn as Realm, p as Hero, k as Pad)
    dim m as integer, d as integer
    if zn.id = 2 and lv.cart.st = 1 then k.r = 0
    if GoatNear%(lv, cint(p.x)) <> NONE then k.r = 0
    for m = 0 to lv.nMv - 1
        d = lv.mv(m).e - cint(p.x)
        if lv.mv(m).k = 4 or lv.mv(m).k = 6 then d = NONE
        if d <= -40 or d >= 260 or p.coyote > 0 or p.vx <= d \ 3 or p.sg = lv.mv(m).s then continue for
        k.r = 0
        k.l = -1
    next
end sub

' A blizzard: no jump is begun from a walk that a gust would catch, and while it blows he keeps low.
sub PilotGale (lv as Level, se as Session, p as Hero, k as Pad, want as integer)
    if Lull%(lv, se) < 56 and want and abs(p.gsp) < 150 then
        want = 0
        HoldBack p, k, 20
    end if
    if Lull%(lv, se) = 0 then k.d = -1
    ' and from a crouch no jump: that would wind up a dash
    if p.crouch then want = 0
    ' curled and come to a stand on a slope, held there by the key: out of it by a jump
    if p.roll and abs(p.gsp) < 30 then want = -1
end sub

' The level as it starts, the title up, Charlie at the start for the pilot.
sub ResetWorld (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer
    if wd.zn.stage <> wd.zn.nextUp then LoadLevel wd, wd.zn.nextUp
    EndTune wd.lv, wd.mu
    g.mode = TITLE
    g.modeT = 0
    wd.se.cheer = 0
    wd.se.metT = 0
    wd.se.pastT = 0
    if wd.tn.dusk < 16 then
        wd.tn.dusk = 16
        Shade wd.tn
    end if
    g.lives = 3
    g.score = 0
    g.clock = 0
    g.homeX = wd.lv.startX
    g.homeY = wd.lv.startY
    for i = 0 to wd.lv.nObj - 1
        wd.lv.ob(i).x = wd.lv.ob(i).a
        wd.lv.ob(i).s = 0
        if wd.lv.ob(i).k = KBONE then wd.lv.ob(i).s = 1
        if wd.lv.ob(i).k = KCAT or wd.lv.ob(i).k = KCRAB or wd.lv.ob(i).k = KSWEEP then wd.lv.ob(i).s = -1
        if wd.lv.ob(i).k = KJELLY then wd.lv.ob(i).s = 1
        if wd.lv.ob(i).k = KTHROW then wd.lv.ob(i).s = THROWC
        if wd.lv.ob(i).k = KGOAT or wd.lv.ob(i).k = KWALL then wd.lv.ob(i).s = 1
    next
    for i = 0 to NLOB - 1
        wd.lv.lobs(i).live = 0
    next
    wd.lv.lobN = 0
    for i = 0 to NMOVE
        wd.lv.mv(i).t = 0
    next
    for i = 0 to wd.lv.nGulls - 1
        wd.lv.gulls(i).x = wd.lv.gulls(i).hx
        wd.lv.gulls(i).y = wd.lv.gulls(i).hy
        wd.lv.gulls(i).t = 0
        wd.lv.gulls(i).st = GHOVER
        wd.lv.gulls(i).loot = 0
    next
    for i = 0 to NLOOSE - 1
        lo(i).t = 0
    next
    for i = 0 to NPART - 1
        pt(i).t = 0
    next
    wd.se.looseN = 0
    wd.se.partN = 0
    Spawn wd.lv, wd.gr, wd.zn, wd.mu, g, p
end sub

' A level cleared: on to level n, the score and lives kept.
sub GoStage (wd as World, n as integer, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim sc as long, left0 as integer
    sc = g.score
    left0 = g.lives
    wd.zn.nextUp = n
    ResetWorld wd, g, p, lo(), pt()
    wd.zn.nextUp = 0
    g.mode = PLAYING
    g.score = sc
    g.lives = left0
end sub

' Past the goal, a game's or the title's: on to the family and no further, the view sliding on to hold them all
' with Charlie at its left; a while with them, then the picture fades. Whether it has.
function Met% (wd as World, g as Game, p as Hero, k as Pad, pt() as Part)
    dim idle as Pad, parted as integer
    idle.r = p.x < wd.lv.famX - 108 and not p.win
    MoveHero wd.lv, wd.gr, wd.mu, wd.se, p, idle, pt()
    if p.x > wd.lv.famX - 106 then
        p.x = wd.lv.famX - 106
        p.gsp = 0
        p.vx = 0
        p.roll = 0
    end if
    if p.ground and abs(p.gsp) < 5 and p.x >= wd.lv.famX - 108 then
        p.win = -1
        p.face = 0
    end if
    wd.se.cheer = p.win
    g.camx = g.camx + (wd.lv.famX - 166 - g.camx) * 4 * STEPT
    wd.se.pastT = wd.se.pastT + STEPT
    if p.win and g.bonus = 0 then wd.se.metT = wd.se.metT + STEPT
    parted = wd.se.metT > 6 or wd.se.pastT > 40 or (wd.se.metT > 2 and (k.hit or k.start))
    if wd.tn.dusk = 16 and parted then wd.tn.dusk = 15
    if wd.tn.dusk < 16 and (wd.se.ticks and 3) = 0 then
        wd.tn.dusk = wd.tn.dusk - 1
        Shade wd.tn
    end if
    Met% = wd.tn.dusk <= 0
end function

' A game from its start, at the first act of the zone picked on the title.
sub StartGame (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
    wd.zn.nextUp = wd.se.pick * 2
    ResetWorld wd, g, p, lo(), pt()
    wd.zn.nextUp = 0
    g.mode = PLAYING
end sub

' ---- progress ----

function ZoneName$ (z as integer)
    ZoneName$ = rtrim$(mid$(ZNAMES, z * 16 + 1, 16))
end function

' The furthest zone reached, from CHARLIE.SAV: the first without the file, or with one that is not the game's.
sub LoadSave (se as Session)
    dim v as long
    se.reached = 0
    open "CHARLIE.SAV" for binary as #2
    if lof(2) >= 4 then get #2, , v
    if lof(2) = 0 then
        close #2
        kill "CHARLIE.SAV"
    else
        close #2
    end if
    v = v - SAVEMARK
    if v > 0 and v < ZONES then se.reached = v
end sub

' Zone z is reached: kept in CHARLIE.SAV when it is further than any before.
sub Reach (se as Session, z as integer)
    dim v as long
    if z <= se.reached then exit sub
    se.reached = z
    v = SAVEMARK + z
    open "CHARLIE.SAV" for binary as #2
    put #2, , v
    close #2
end sub

' On the title Left and Right pick the zone a game starts at, of those reached: a step a press.
sub PickZone (se as Session, k as Pad)
    static leaning as integer
    if (k.l or k.r) and not leaning then
        se.pick = se.pick + k.l - k.r
        if se.pick < 0 then se.pick = 0
        if se.pick > se.reached then se.pick = se.reached
    end if
    leaning = k.l or k.r
end sub

sub Spawn (lv as Level, gr as Ground, zn as Realm, mu as Deck, g as Game, p as Hero)
    dim fresh as Hero
    p = fresh
    p.x = g.homeX
    p.y = g.homeY
    p.ground = -1
    p.sg = SegAt%(lv, gr, g.homeX, csng(g.homeY))
    g.bones = 0
    ResetChase lv, mu
    Mend lv
    Camera lv, zn, g, p, -1
end sub

sub MoveHero (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero, k as Pad, pt() as Part)
    dim l as integer, r as integer, d as integer, hit as integer, hold as integer
    dim v as single
    ' a snowball lasts as long as its roll; broken, by a hit or his uncurling, it flies apart
    if p.snow < 0 and not p.roll then p.snow = 1
    if p.snow > 0 then
        p.snow = 0
        Burst se, pt(), p.x, p.y - 12, 12, 140, CWHITE
        Sfx mu, se, SPOP
    end if
    ' a ride lasts as long as its roll: come to a stand, hurt or thrown, the sled is left in pieces
    if p.sled and not p.roll then Alight mu, se, p, pt(), CEARTH + 4
    ' a jump held through the hit is a jump as it ends: it was forgotten
    if p.stun and p.ground and p.inv < 1 then
        p.stun = 0
        if k.jump then k.hit = -1
    end if
    if p.stun = 0 then
        l = k.l
        r = k.r
        d = k.d
        hit = k.hit
        hold = k.jump
    end if
    if p.barkT > 0 then p.barkT = p.barkT - STEPT
    if k.bark and p.stun = 0 and p.barkT <= 0 then Bark lv, mu, se, p
    KeepJump mu, se, p, hit
    if p.coyote > 0 then p.coyote = p.coyote - STEPT
    if p.inv > 0 then p.inv = p.inv - STEPT
    if p.goat then Ride lv, p, l, r, d
    p.lastx = p.x
    ' slid off a slope too steep for its speed, the keys are no help a while
    if p.slip > 0 and p.ground then
        l = 0
        r = 0
    end if
    if p.slip > 0 then p.slip = p.slip - STEPT
    if lv.gale then Buffet lv, se, p
    if p.swim then
        Paddle lv, gr, mu, se, p, l, r, hit, pt()
    elseif p.ground then
        Walk lv, gr, mu, se, p, l, r, d, hit, pt()
    else
        Airborne lv, gr, se, p, l, r, d, hold, pt()
    end if
    if lv.tide <> NONE then Soak lv, se, p, pt()
    if p.x < 8 then
        p.x = 8
        p.gsp = 0
        p.vx = 0
    end if
    if p.x > lv.wpx - 8 then
        p.x = lv.wpx - 8
        p.gsp = 0
        p.vx = 0
    end if
    if p.ground then
        v = abs(p.gsp)
    else
        v = abs(p.vx)
    end if
    if v < 5 and p.ground then
        p.idleT = p.idleT + STEPT
    else
        p.idleT = 0
    end if
    if p.roll or p.charging then
        if p.charging then v = 300 + p.charge
        p.anim = p.anim + (v + 120) * STEPT / 9
    elseif v >= TOP - 5 then
        p.anim = p.anim + 20 * STEPT
    else
        p.anim = p.anim + v * STEPT / 7
    end if
end sub

' A jump pressed just before landing, or just after the ground ran out, was lost: it is kept a moment.
sub KeepJump (mu as Deck, se as Session, p as Hero, hit as integer)
    if hit then p.want = JBUFFER
    if p.want <= 0 then exit sub
    p.want = p.want - STEPT
    if p.ground then
        hit = -1
        p.want = 0
        exit sub
    end if
    if p.coyote <= 0 then exit sub
    p.vy = -JUMPV
    p.jumped = -1
    p.roll = -1
    p.coyote = 0
    p.want = 0
    Sfx mu, se, SJUMP
end sub

' Onto sled i: off on it at once, to the right, curled in nothing but the game's reckoning.
sub Board (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    lv.ob(i).s = 1
    p.sled = -1
    p.roll = -1
    p.jumped = 0
    p.face = 0
    if p.ground then
        if p.gsp < SLEDV then p.gsp = SLEDV
    elseif p.vx < SLEDV then
        p.vx = SLEDV
    end if
    Sfx mu, se, SROLL
end sub

' Off the sled, which is no more: a puff of colour c where it was.
sub Alight (mu as Deck, se as Session, p as Hero, pt() as Part, c as integer)
    p.sled = 0
    Burst se, pt(), p.x, p.y - 4, 10, 120, c
    Sfx mu, se, SPOP
end sub

' Whether Charlie is on bare ice: it keeps his speed and will not give it back.
function Slick% (lv as Level, gr as Ground, p as Hero)
    Slick% = 0
    if gr.pt(p.sg).k = 1 and p.x >= 0 then Slick% = lv.glaze(cint(p.x) \ TS)
end function

' On the ground: wind up, jump, roll, run; then along it, a few pixels a
' move, into walls and onto the ground's curve.
sub Walk (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero, l as integer, r as integer, d as integer, hit as integer, pt() as Part)
    dim n as integer, s as integer, i as integer, grip as single, push as single
    grip = 1
    push = 1
    if Slick%(lv, gr, p) then
        grip = ICEGRIP
        push = ICEPUSH
    end if
    if p.sled then grip = grip * SLEDGRIP
    if p.charging then
        if Winding%(mu, se, p, d, hit, pt()) then exit sub
    elseif hit then
        Leap mu, se, p
        exit sub
    end if
    p.crouch = d and abs(p.gsp) < 20 and not p.roll
    p.skid = 0
    if p.roll then
        RollOn p, l, r, grip
    else
        RunOn mu, se, p, l, r, d, grip, push, pt()
    end if
    if p.gsp > MAXV then p.gsp = MAXV
    if p.gsp < -MAXV then p.gsp = -MAXV
    n = 1 + int(abs(p.gsp) * STEPT / 8)
    for s = 1 to n
        Along gr, p, p.gsp * STEPT / n
        if not p.ground then exit sub
        ' a wall stops it on the tiles' ground; a curve is run up
        i = p.sg
        if gr.pt(i).k = 2 or p.gsp = 0 then continue for
        if Wall%(lv, p, sgn(p.gsp)) = 0 then continue for
        p.gsp = 0
        p.y = gr.pt(i).y + (p.x - gr.pt(i).x) * gr.pt(i).v / gr.pt(i).u
        Along gr, p, 0
        if not p.ground then exit sub
    next
    Falter gr, p
end sub

' A dash wound up: each press winds it further; Down let go, he is away. Whether it is still held.
function Winding% (mu as Deck, se as Session, p as Hero, d as integer, hit as integer, pt() as Part)
    dim dir as integer
    dir = 1 - 2 * p.face
    p.charge = p.charge * (1 - 2 * STEPT)
    if hit then
        p.charge = p.charge + 130
        if p.charge > CHARGEMAX then p.charge = CHARGEMAX
        Sfx mu, se, SCHARGE
        Burst se, pt(), p.x - dir * 10, p.y - 2, 3, 70, CSTONE + 3
    end if
    Winding% = d
    if d then exit function
    p.charging = 0
    p.roll = -1
    p.gsp = (CHARGE0 + p.charge) * dir
    Sfx mu, se, SLAUNCH
end function

' The jump key on the ground: from a crouch a dash is begun; else he jumps, off the ground as it lies.
sub Leap (mu as Deck, se as Session, p as Hero)
    dim sn as single, cs as single
    if p.crouch then
        p.charging = -1
        p.charge = 0
        Sfx mu, se, SCHARGE
        exit sub
    end if
    sn = sin(p.ang)
    cs = cos(p.ang)
    p.vx = p.gsp * cs - JUMPV * sn
    p.vy = -p.gsp * sn - JUMPV * cs
    p.ground = 0
    p.roll = -1
    p.jumped = -1
    p.crouch = 0
    Sfx mu, se, SJUMP
end sub

' Rolling: the slope pulls harder down than it holds back up, and the keys only brake.
sub RollOn (p as Hero, l as integer, r as integer, grip as single)
    dim sn as single, f as single
    sn = sin(p.ang)
    f = RDNS
    if (p.gsp > 0) = (sn > 0) then f = RUPS
    p.gsp = p.gsp - f * sn * STEPT
    ' a sled has no brake
    if l and p.gsp > 0 and not p.sled then p.gsp = p.gsp - RDEC * grip * STEPT
    if r and p.gsp < 0 and not p.sled then p.gsp = p.gsp + RDEC * grip * STEPT
    Slow p.gsp, RFRIC * grip
    if abs(p.gsp) < 30 and abs(sn) < .2 then p.roll = 0
end sub

' Running: the slope, then the keys, or with none the ground's drag. Down at speed, he curls up.
sub RunOn (mu as Deck, se as Session, p as Hero, l as integer, r as integer, d as integer, grip as single, push as single, pt() as Part)
    dim sn as single, cs as single
    sn = sin(p.ang)
    cs = cos(p.ang)
    if p.gsp <> 0 or abs(sn) > .75 or p.slip > 0 then p.gsp = p.gsp - SLOPE * sn * STEPT
    if p.crouch then
        Slow p.gsp, FRIC * 4 * grip
    elseif r then
        Shove p, 1, grip, push
    elseif l then
        Shove p, -1, grip, push
    elseif p.slip <= 0 and cs >= .7 then
        ' on steep ground nothing but the slope slows it
        Slow p.gsp, FRIC * grip
    end if
    if d and abs(p.gsp) >= 60 then
        p.roll = -1
        p.crouch = 0
        Sfx mu, se, SROLL
    end if
    if p.skid and (se.ticks and 3) = 0 then Emit se, pt(), p.x, p.y - 1, -p.gsp * .1, -30, .4, CSTONE + 3
end sub

' A key held, way 1 right or -1 left: against his speed a brake, and a skid; with it a push, up to his most.
sub Shove (p as Hero, way as integer, grip as single, push as single)
    dim v as single, most as single
    most = TOP
    if p.wet > 0 then most = WETTOP
    v = p.gsp * way
    if v < 0 then
        v = v + DEC * grip * STEPT
        p.skid = v < -80
    elseif v < most then
        v = v + RUNACC * push * STEPT
        if v > most then v = most
    end if
    p.gsp = v * way
    if v >= 0 then p.face = (1 - way) \ 2
end sub

' Too slow for a wall or a roof, off it; for a steep slope, down it; and off the roof of a ribbon's twist, down
' past the ribbon.
sub Falter (gr as Ground, p as Hero)
    if abs(p.gsp) >= STALL then exit sub
    if gr.pt(p.sg).u < .35 then
        Drop gr, p
    elseif gr.pt(p.sg).u < .7 then
        p.slip = .5
    end if
    if Twist!(gr, p) < 0 then exit sub
    if cos(Twist!(gr, p)) >= -.5 then exit sub
    Drop gr, p
    p.y = p.y + 26
end sub

' In the air: gravity, steering, a jump cut short when let go; then a few
' pixels a move, into walls, ceilings and onto the ground.
sub Airborne (lv as Level, gr as Ground, se as Session, p as Hero, l as integer, r as integer, d as integer, hold as integer, pt() as Part)
    dim n as integer, s as integer, m as single, landed as integer, fell as single, x0 as single, y0 as single
    p.crouch = 0
    p.charging = 0
    p.skid = 0
    p.vy = p.vy + GRAV * STEPT
    if p.vy > 900 then p.vy = 900
    if r then
        Steer p, 1
    elseif l then
        Steer p, -1
    end if
    if p.jumped and not hold and p.vy < -JUMPCUT then p.vy = -JUMPCUT
    m = abs(p.vx)
    if abs(p.vy) > m then m = abs(p.vy)
    n = 1 + int(m * STEPT / 8)
    for s = 1 to n
        x0 = p.x
        y0 = p.y
        p.x = p.x + p.vx * STEPT / n
        if p.vx <> 0 then if Wall%(lv, p, sgn(p.vx)) then p.vx = 0
        p.y = p.y + p.vy * STEPT / n
        fell = p.vy
        landed = Curve%(gr, p, x0, y0, d)
        if landed then exit for
        if p.vy < 0 then
            Bonk lv, p
            continue for
        end if
        fell = p.vy
        landed = Land%(lv, gr, p, d)
        if landed then exit for
    next
    if landed and fell > 420 then Burst se, pt(), p.x, p.y - 2, 5, 60, CSTONE + 3
end sub

' In the air a key held, way 1 right or -1 left: he turns that way and gathers speed, up to his most.
sub Steer (p as Hero, way as integer)
    dim v as single
    p.face = (1 - way) \ 2
    v = p.vx * way
    if v >= TOP then exit sub
    v = v + AIRACC * STEPT
    if v > TOP then v = TOP
    p.vx = v * way
end sub

' Rising, his head against a ceiling: stopped under it.
sub Bonk (lv as Level, p as Hero)
    dim x as integer, y as integer
    x = cint(p.x)
    y = cint(p.y) - HH
    if Solid%(lv, x, y) = 0 and Solid%(lv, x - 4, y) = 0 and Solid%(lv, x + 4, y) = 0 then exit sub
    p.y = (y \ TS + 1) * TS + HH
    p.vy = 0
end sub

' v toward 0 by f a second, not past it.
sub Slow (v as single, f as single)
    if v > 0 then
        v = v - f * STEPT
        if v < 0 then v = 0
    elseif v < 0 then
        v = v + f * STEPT
        if v > 0 then v = 0
    end if
end sub

' Charlie against the things near it, and the loose bones.
sub Touch (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer, c as integer, px as integer, py as integer, x as integer, dx as integer
    px = cint(p.x)
    py = cint(p.y)
    c = (px - 80) \ TS
    if c < 0 then c = 0
    i = lv.first(c)
    do while i < lv.nObj
        x = lv.ob(i).x
        if x > px + 80 then exit do
        dx = abs(x - px)
        select case lv.ob(i).k
        case KBONE: MeetBone lv, mu, se, g, p, i, pt()
        case KCAT: MeetCat lv, mu, se, g, p, i, lo(), pt()
        case KCRAB: MeetCrab lv, mu, se, g, p, i, lo(), pt()
        case KSWEEP: MeetSweep lv, mu, se, g, p, i, lo(), pt()
        case KTHROW: MeetThrow lv, mu, se, g, p, i, lo(), pt()
        case KGOAT: MeetGoat lv, mu, se, g, p, i, lo()
        case KWALL: MeetWall lv, mu, se, g, p, i, pt()
        case KHEAP: MeetHeap lv, p, i
        case KICICLE: MeetIcicle lv, mu, se, g, p, i, lo(), pt()
        case KSLED: MeetSled lv, mu, se, p, i
        case KDRIFT: MeetDrift lv, mu, se, p, i, pt()
        case KSPARK: MeetSpark lv, mu, se, g, p, i, lo()
        case KPRESS: MeetPress lv, mu, se, g, p, i, lo()
        case KJELLY: MeetJelly lv, mu, se, g, p, i, lo(), pt()
        case KSPRING: MeetSpring lv, mu, se, p, i
        case KBOOST: MeetBoost lv, mu, se, g, p, i, pt()
        case KFLUME, KSPOUT: MeetFlume lv, mu, se, p, i
        case KMARK: MeetMark lv, mu, se, g, p, i, pt()
        case KGOAL: if px >= x then lv.ob(i).s = 1
        case KTHORN: if dx < 12 and abs(py - lv.ob(i).y) < 8 then Hurt mu, se, g, p, x, lo(), SHURT
        case KBUTTON: MeetButton lv, mu, se, p, i, pt()
        case KJET: MeetJet lv, mu, se, p, i
        case KBUMP, KBUOY: MeetBump lv, mu, se, g, p, i, pt()
        case KSPIN: MeetSpin lv, mu, se, g, p, i, lo()
        case KHOOP: MeetHoop lv, mu, se, p, i, pt()
        end select
        i = i + 1
    loop
    if se.looseN = 0 then exit sub
    for i = 0 to NLOOSE - 1
        if lo(i).t <= 0 or lo(i).t >= 3.3 then continue for
        if abs(lo(i).x - px) >= 12 or abs(lo(i).y - py + 13) >= 16 then continue for
        lo(i).t = 0
        se.looseN = se.looseN - 1
        g.bones = g.bones + 1
        Sfx mu, se, SBONE
    next
end sub

' A bone where the level put it: taken.
sub MeetBone (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    if lv.ob(i).s = 0 or abs(lv.ob(i).x - cint(p.x)) >= 12 then exit sub
    if abs(lv.ob(i).y - cint(p.y) + 13) >= 16 then exit sub
    lv.ob(i).s = 0
    g.bones = g.bones + 1
    g.score = g.score + 10
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y), 4, 50, CGOLD + 4
    Sfx mu, se, SBONE
end sub

' A cat of the Mob: rolled or charged into it is beaten, and he bounces off it; else it scratches.
sub MeetCat (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    if lv.ob(i).s = 0 or abs(lv.ob(i).x - cint(p.x)) >= 15 then exit sub
    if abs(lv.ob(i).y - 6 - cint(p.y) + 13) >= 17 then exit sub
    Tackle lv, mu, se, g, p, i, 6, lo(), pt()
end sub

' One of the Mob run into: rolled or charged into it is beaten, dy up from its foot, and he bounces off it; else
' it hurts.
sub Tackle (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, dy as integer, lo() as Loose, pt() as Part)
    dim x as integer
    if p.roll = 0 and p.charging = 0 then
        x = lv.ob(i).x
        Hurt mu, se, g, p, x, lo(), SHISS
        exit sub
    end if
    Beaten lv, mu, se, g, pt(), i, dy
    if not p.ground and p.vy > 0 then p.vy = -BOUNCEV
end sub

' An icicle: walked under, it is loosed; falling, it hurts whom it lands on, and breaks.
sub MeetIcicle (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim x as integer, dx as integer, py as integer, y as integer
    x = lv.ob(i).x
    dx = abs(x - cint(p.x))
    py = cint(p.y)
    if lv.ob(i).s = 0 and dx < 20 and py > lv.ob(i).y and py < lv.ob(i).y + 140 then lv.ob(i).s = 1
    y = lv.ob(i).y + Fallen%(lv, i)
    if lv.ob(i).s <= TREMBLE or dx >= 9 or py <= y or py - 26 >= y + 16 then exit sub
    Hurt mu, se, g, p, x, lo(), SHURT
    Shatter lv, mu, se, pt(), i
end sub

' A sled left on the snow: landed on or run onto, he boards it.
sub MeetSled (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    if lv.ob(i).s <> 0 or p.sled or p.stun then exit sub
    if not p.ground and p.vy <= 0 then exit sub
    if abs(lv.ob(i).x - cint(p.x)) >= 14 or abs(cint(p.y) - lv.ob(i).y + 5) >= 10 then exit sub
    Board lv, mu, se, p, i
end sub

' Deep snow: it stops a sled dead and he tumbles out of it; rolled through, it packs round him.
sub MeetDrift (lv as Level, mu as Deck, se as Session, p as Hero, i as integer, pt() as Part)
    if not p.ground or abs(lv.ob(i).x - cint(p.x)) >= 22 or abs(cint(p.y) - lv.ob(i).y) >= 8 then exit sub
    if p.sled then
        Alight mu, se, p, pt(), CWHITE
        p.roll = 0
        p.gsp = p.gsp * .25
        exit sub
    end if
    if p.roll = 0 or p.snow <> 0 or abs(p.gsp) < SNOWV then exit sub
    p.snow = -1
    Burst se, pt(), p.x, p.y - 8, 8, 100, CWHITE
    Sfx mu, se, SROLL
end sub

' A spark on its cable: lit, it stings.
sub MeetSpark (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
    dim x as integer, py as integer
    x = lv.ob(i).x
    py = cint(p.y)
    if abs(x - cint(p.x)) >= 11 or py <= lv.ob(i).y - 24 or py - 26 >= lv.ob(i).y then exit sub
    if Lit%(lv, se, i) then Hurt mu, se, g, p, x, lo(), SHISS
end sub

' A spring: come down on or stood on, it throws him up.
sub MeetSpring (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    dim py as integer
    py = cint(p.y)
    if abs(lv.ob(i).x - cint(p.x)) >= 11 or py < lv.ob(i).y - 12 or py > lv.ob(i).y + 2 then exit sub
    if not p.ground and p.vy < 0 then exit sub
    if p.ground then p.vx = p.gsp
    p.ground = 0
    p.vy = -SPRINGV
    p.y = lv.ob(i).y - 12
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
    lv.ob(i).s = 8
    Sfx mu, se, SSPRING
end sub

' A boost pad: run over slower than it sends, he is sent the way he faces.
sub MeetBoost (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    if not p.ground or abs(lv.ob(i).x - cint(p.x)) >= 12 then exit sub
    if abs(cint(p.y) - lv.ob(i).y) >= 4 or abs(p.gsp) >= BOOSTV then exit sub
    p.gsp = BOOSTV * (1 - 2 * p.face)
    g.shake = .2
    Burst se, pt(), p.x, p.y - 4, 8, 150, CGOLD + 4
    Sfx mu, se, SBOOST
end sub

' A flume's tile or a spout's: stood in its water, he slides.
sub MeetFlume (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    dim px as integer, py as integer
    px = cint(p.x)
    py = cint(p.y)
    if not p.ground or px < lv.ob(i).x or px >= lv.ob(i).x + TS then exit sub
    if py > lv.ob(i).y - 6 and py < lv.ob(i).y + TS + 6 then Slide mu, se, p
end sub

' A marker post: passed, it is where he comes back to.
sub MeetMark (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    if lv.ob(i).s <> 0 or cint(p.x) < lv.ob(i).x then exit sub
    lv.ob(i).s = 1
    g.homeX = lv.ob(i).x
    g.homeY = lv.ob(i).y
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y - 28), 8, 80, CGOLD + 4
    Sfx mu, se, SMARK
end sub

' A button in the floor: stood on, it is pressed.
sub MeetButton (lv as Level, mu as Deck, se as Session, p as Hero, i as integer, pt() as Part)
    if lv.ob(i).s <> 0 or not p.ground then exit sub
    if abs(lv.ob(i).x - cint(p.x)) >= 9 or abs(cint(p.y) - lv.ob(i).y) >= 4 then exit sub
    lv.ob(i).s = 1
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y - 6), 8, 80, CGOLD + 4
    Sfx mu, se, SMARK
end sub

' A jet, blowing 80 ticks of 180: who is over it goes up.
sub MeetJet (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    dim dy as integer
    dy = cint(p.y) - lv.ob(i).y
    if (se.ticks + lv.ob(i).x) mod 180 >= 80 or abs(lv.ob(i).x - cint(p.x)) >= 10 then exit sub
    if dy > 2 or dy <= -64 or p.vy <= -JETV + 60 then exit sub
    if p.ground then
        p.vx = p.gsp
        Sfx mu, se, SHISS
    end if
    p.ground = 0
    p.vy = -JETV
    p.roll = 0
    p.jumped = 0
    p.sprung = -1
end sub

' A bumper or a buoy: touched, it throws him off the way he came at it.
sub MeetBump (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    dim dx as integer, dy as integer, d as single
    dx = cint(p.x) - lv.ob(i).x
    dy = cint(p.y) - 12 - lv.ob(i).y
    if lv.ob(i).s <> 0 or abs(dx) >= 20 or abs(dy) >= 20 then exit sub
    d = sqr(csng(dx) * dx + csng(dy) * dy)
    if d >= 20 then exit sub
    if d < 1 then
        d = 1
        dy = -1
    end if
    p.ground = 0
    p.vx = dx / d * BUMPV
    p.vy = dy / d * BUMPV - 60
    p.jumped = 0
    p.sprung = -1
    lv.ob(i).s = 10
    g.score = g.score + 10
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y), 6, 90, CGOLD + 4
    Sfx mu, se, SSPRING
end sub

' A ball going round its post: it hurts.
sub MeetSpin (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
    dim a as single, x as integer, bx as integer, by as integer
    x = lv.ob(i).x
    a = se.ticks * .06 + x
    bx = x + cint(SPINR * cos(a))
    by = lv.ob(i).y - SPINH + cint(SPINR * sin(a))
    if abs(bx - cint(p.x)) < 11 and abs(by - cint(p.y) + 12) < 16 then Hurt mu, se, g, p, x, lo(), SHURT
end sub

' A hoop: through it in the air he goes on, faster, the way he was going.
sub MeetHoop (lv as Level, mu as Deck, se as Session, p as Hero, i as integer, pt() as Part)
    if lv.ob(i).s <> 0 or p.ground or abs(lv.ob(i).x - cint(p.x)) >= 9 then exit sub
    if abs(lv.ob(i).y - cint(p.y) + 12) >= 18 then exit sub
    p.vx = HOOPV * (1 + 2 * (p.vx < 0))
    p.vy = -180
    p.sprung = -1
    lv.ob(i).s = 20
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y), 8, 120, CGOLD + 4
    Sfx mu, se, SBOOST
end sub

' Hit: the bones fly loose and Charlie is knocked back; with none, it is out.
sub Hurt (mu as Deck, se as Session, g as Game, p as Hero, fromX as integer, lo() as Loose, snd as integer)
    dim i as integer, n as integer, a as single, v as single
    if p.inv > 0 then exit sub
    ' a snowball takes the hit for him
    if p.snow < 0 then
        p.snow = 1
        p.inv = 1
        exit sub
    end if
    if snd = SHISS then Sfx mu, se, snd
    if g.bones = 0 then
        Lose mu, se, p
        exit sub
    end if
    n = g.bones
    if n > NLOOSE then n = NLOOSE
    g.bones = 0
    for i = 0 to n - 1
        a = PI * (.15 + .7 * i / n)
        v = 240 - 70 * (i and 1)
        lo(i).x = p.x
        lo(i).y = p.y - 13
        lo(i).vx = cos(a) * v * (1 - 2 * (i and 1)) * .8
        lo(i).vy = -sin(a) * v
        lo(i).t = 4
    next
    se.looseN = n
    p.ground = 0
    p.roll = 0
    p.charging = 0
    p.crouch = 0
    p.jumped = 0
    p.stun = -1
    p.inv = 2
    p.vy = -260
    if p.x < fromX then
        p.vx = -140
    else
        p.vx = 140
    end if
    g.shake = .25
    if snd = SHURT then Sfx mu, se, snd
end sub

sub Lose (mu as Deck, se as Session, p as Hero)
    if p.dead then exit sub
    p.dead = -1
    p.deadT = 0
    p.vy = -340
    p.ground = 0
    Sfx mu, se, SDIE
end sub

' Out: Charlie rises and falls through everything; then the next life.
sub Dying (lv as Level, gr as Ground, zn as Realm, mu as Deck, g as Game, p as Hero)
    p.deadT = p.deadT + STEPT
    p.vy = p.vy + GRAV * STEPT
    p.y = p.y + p.vy * STEPT
    if p.deadT < 2 then exit sub
    g.lives = g.lives - 1
    if g.lives <= 0 then
        g.mode = OVER
        g.modeT = 0
        SceneTune mu, LEFTBEHIND
    else
        Spawn lv, gr, zn, mu, g, p
    end if
end sub

' Thirty times a second: cats near the view pace about their homes,
' turning at edges and walls; springs settle; loose bones bounce and
' fade; sparks fall.
' A cat's step: startled, it stands and counts its state down to a prowl's; prowling, a pixel along its beat, turned
' at the beat's end, a drop or a wall.
sub Prowl (lv as Level, i as integer)
    dim x as integer, y as integer, s as integer
    if abs(lv.ob(i).s) > 1 then
        lv.ob(i).s = lv.ob(i).s - sgn(lv.ob(i).s)
        exit sub
    end if
    x = lv.ob(i).x + lv.ob(i).s
    y = lv.ob(i).y
    s = Surf%(lv, x + lv.ob(i).s * 8, y)
    if abs(x - lv.ob(i).a) > 40 or abs(s - y) > 6 or Solid%(lv, x + lv.ob(i).s * 8, y - 6) then
        lv.ob(i).s = -lv.ob(i).s
        exit sub
    end if
    lv.ob(i).x = x
    s = Surf%(lv, x, y)
    if abs(s - y) <= 6 then lv.ob(i).y = s
end sub

' Charlie at crab i. Down on its back he goes up off it, higher off its shell, and it ducks in a while; at its
' side its claws pinch, even a rolling Charlie. In its shell it pinches nobody, and a roll knocks it off the sand.
sub MeetCrab (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim y as integer
    y = lv.ob(i).y
    if lv.ob(i).s = 0 or abs(lv.ob(i).x - p.x) >= 15 or p.y < y - 22 or p.y > y + 10 then exit sub
    if not p.ground and p.vy > 0 and p.y < y - 4 then
        p.vy = -BOUNCEV
        if abs(lv.ob(i).s) > 1 then p.vy = -STEPV
        p.jumped = 0
        if abs(lv.ob(i).s) < SHELLT \ 3 then lv.ob(i).s = sgn(lv.ob(i).s) * (SHELLT \ 3)
        Sfx mu, se, SSPRING
    elseif abs(lv.ob(i).s) = 1 then
        Hurt mu, se, g, p, lv.ob(i).x, lo(), SHURT
    elseif p.roll or p.charging then
        Beaten lv, mu, se, g, pt(), i, 6
    end if
end sub

' Thing i beaten: gone, its points, a shake and a puff dy over its foot.
sub Beaten (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, i as integer, dy as integer)
    lv.ob(i).s = 0
    g.score = g.score + 100
    g.shake = .15
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y - dy), 10, 130, CWHITE
    Sfx mu, se, SPOP
end sub

' A sweeper met: armoured before, its hood hurts whoever meets it, curled or not; landed on, or rolled into from
' behind, it is beaten.
sub MeetSweep (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim y as integer, behind as integer
    y = lv.ob(i).y
    if lv.ob(i).s = 0 or abs(lv.ob(i).x - p.x) >= 18 or p.y < y - 20 or p.y > y + 10 then exit sub
    behind = sgn(p.x - lv.ob(i).x) <> sgn(lv.ob(i).s)
    if not p.ground and p.vy > 0 and p.y < y - 8 then
        Beaten lv, mu, se, g, pt(), i, 8
        p.vy = -BOUNCEV
    elseif behind and (p.roll or p.charging) then
        Beaten lv, mu, se, g, pt(), i, 8
    else
        Hurt mu, se, g, p, lv.ob(i).x, lo(), SHURT
    end if
end sub

' Whether spark i is alight: its carriage hurts then, and is safe dark.
function Lit% (lv as Level, se as Session, i as integer)
    Lit% = (se.ticks + lv.ob(i).a) mod SPARKP < SPARKLIT
end function

' Where press i is in its turn, 0 to PRESSP - 1: a press further along a row is later.
function PressT% (lv as Level, se as Session, i as integer)
    PressT% = (se.ticks + PRESSP * 100 - lv.ob(i).a \ PRESSLAG) mod PRESSP
end function

' How far down press i's head is, 0 to PRESSUP.
function Stroke% (lv as Level, se as Session, i as integer)
    dim t as integer
    t = PressT%(lv, se, i)
    if t < PRESSFALL then
        Stroke% = 0
    elseif t < PRESSFALL + 10 then
        Stroke% = (t - PRESSFALL) * PRESSUP \ 10
    elseif t < PRESSRISE then
        Stroke% = PRESSUP
    else
        Stroke% = PRESSUP - (t - PRESSRISE) * PRESSUP \ (PRESSP - PRESSRISE)
    end if
end function

' A press met: its head, wherever in its stroke, hurts who is under it or against it. It lands with a clank.
sub MeetPress (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
    dim y as integer
    if PressT%(lv, se, i) = PRESSFALL + 10 then Sfx mu, se, SCLANK
    y = lv.ob(i).y - PRESSUP + Stroke%(lv, se, i)
    if abs(lv.ob(i).x - p.x) < 26 and p.y - 26 < y and p.y > y - 14 then Hurt mu, se, g, p, lv.ob(i).x, lo(), SHURT
end sub

' For the pilot at x: the nearest press he is under or coming to, NONE with none near.
function PressAhead% (lv as Level, x as integer)
    dim i as integer, c as integer
    PressAhead% = NONE
    c = (x - 64) \ TS
    if c < 0 then c = 0
    i = lv.first(c)
    do while i < lv.nObj
        if lv.ob(i).x > x + 200 then exit do
        if lv.ob(i).k = KPRESS and lv.ob(i).x + 50 >= x then
            PressAhead% = i
            exit do
        end if
        i = i + 1
    loop
end function

' ---- the sands' gulls ----

' Gull b toward x, y at v: there, or nearer. Whether it is there.
function Glide% (b as Bird, x as single, y as single, v as single)
    dim dx as single, dy as single, d as single
    dx = x - b.x
    dy = y - b.y
    d = sqr(dx * dx + dy * dy)
    if dx <> 0 then b.face = dx < 0
    if d <= v * STEPT then
        b.x = x
        b.y = y
        Glide% = -1
    else
        b.x = b.x + dx / d * v * STEPT
        b.y = b.y + dy / d * v * STEPT
        Glide% = 0
    end if
end function

' Gull b lets go of the bones it carries: they fall loose, to be picked up.
sub Spill (se as Session, b as Bird, lo() as Loose)
    dim i as integer
    for i = 0 to NLOOSE - 1
        if b.loot = 0 then exit for
        if lo(i).t > 0 then continue for
        lo(i).x = b.x
        lo(i).y = b.y
        lo(i).vx = (b.loot * 37 mod 90) - 45
        lo(i).vy = -40
        lo(i).t = 4
        se.looseN = se.looseN + 1
        b.loot = b.loot - 1
    next
    b.loot = 0
end sub

' Gull b, the n'th, for a step. It hovers about its home till Charlie, with bones, passes under: then dives at
' where he will be, takes bones if it reaches him, and climbs home with them, to dive no more. A bark scares it
' home and makes it drop what it took; a rolling Charlie knocks it out of the air.
sub Gull (mu as Deck, se as Session, b as Bird, n as integer, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim x as single, y as single, lead as single
    x = b.hx + 24 * sin(se.ticks * .02 + n * 2)
    y = b.hy + 4 * sin(se.ticks * .07 + n)
    select case b.st
    case GHOVER
        b.face = cos(se.ticks * .02 + n * 2) < 0
        b.x = x
        b.y = y
        if b.t > 0 then b.t = b.t - STEPT
        if b.t <= 0 and b.loot = 0 and g.bones > 0 and abs(p.x - x) < GULLSEE and p.y > y + 30 then
            lead = (p.y - 13 - y) / DIVEV * 60
            b.tx = p.x + (p.x - p.lastx) * lead
            b.ty = p.y - 13
            b.st = GDIVE
            Sfx mu, se, SSQUAWK
        end if
    case GDIVE: if Glide%(b, b.tx, b.ty, DIVEV) then b.st = GCLIMB
    case GCLIMB
        if Glide%(b, x, y, CLIMBV) then
            b.st = GHOVER
            b.t = GULLREST
        end if
    case GSCARED
        Spill se, b, lo()
        b.st = GCLIMB
    end select
    if abs(b.x - p.x) >= 13 or abs(b.y - p.y + 13) >= 15 then exit sub
    if p.roll or p.charging then
        Spill se, b, lo()
        b.st = 0
        g.score = g.score + 100
        Burst se, pt(), b.x, b.y, 10, 130, CWHITE
        Sfx mu, se, SPOP
    elseif b.st = GDIVE then
        b.loot = g.bones
        if b.loot > GULLTAKE then b.loot = GULLTAKE
        g.bones = g.bones - b.loot
        b.st = GCLIMB
        Sfx mu, se, SSQUAWK
    end if
end sub

sub Flock (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer
    for i = 0 to lv.nGulls - 1
        if lv.gulls(i).st > 0 then Gull mu, se, lv.gulls(i), i, g, p, lo(), pt()
    next
end sub

' The gulls, a bone under one that carries any; wings still in a dive.
sub DrawGulls (lv as Level, art as Store, se as Session, cx as integer, cy as integer)
    dim i as integer, x as integer, y as integer, k as integer
    for i = 0 to lv.nGulls - 1
        x = cint(lv.gulls(i).x) - cx
        y = cint(lv.gulls(i).y) - cy
        if lv.gulls(i).st <= 0 or x <= -20 or x >= SW + 20 then continue for
        k = IGULL - 4 * lv.gulls(i).face
        if lv.gulls(i).st <> GDIVE then k = k + ((se.ticks \ 8) and 1) * 2
        Blit art, x - 13, y - 10, k, 26, 20
        if lv.gulls(i).loot then Blit art, x - 5, y + 3, IBONE, 10, 10
    next
end sub

' Where a thing afloat at x lies: h under the sea's top, or up over the ground there when the sea is lower.
function Bob% (lv as Level, x as integer, h as integer, up as integer)
    dim y as integer
    y = lv.top(x \ TS) * TS - up
    if lv.tide <> NONE and lv.tide + h < y then y = lv.tide + h
    Bob% = y
end function

' Charlie at jellyfish i. Down on its bell he is thrown high, and it stings nobody while it is flat; his feet in
' it, afloat or not, it stings: not a Charlie on a boat's deck over it.
sub MeetJelly (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim y as integer
    y = lv.ob(i).y
    if p.inv > 0 or lv.ob(i).s > 1 or abs(lv.ob(i).x - p.x) >= 12 or p.y < y - 26 or p.y > y + 8 then exit sub
    if not p.ground and not p.swim and p.vy > 0 and p.y < y - 10 then
        p.vy = -JELLYV
        p.jumped = 0
        p.sprung = -1
        lv.ob(i).s = 10
        Sfx mu, se, SSPRING
    elseif p.y > y - 14 then
        p.swim = 0
        Hurt mu, se, g, p, lv.ob(i).x, lo(), SHURT
    end if
end sub

' A cat barked at from fromX stands startled FLINCH steps, then prowls away from there: its state's size is the
' steps left, its sign the way it will go.
sub Flinch (lv as Level, i as integer, fromX as integer)
    lv.ob(i).s = FLINCH
    if lv.ob(i).x < fromX then lv.ob(i).s = -FLINCH
end sub

' How far icicle i has fallen: nothing till it has trembled.
function Fallen% (lv as Level, i as integer)
    Fallen% = Plunged%(lv.ob(i).s - TREMBLE)
end function

' How far a thing has fallen, n of the things' steps into its fall.
function Plunged% (n as integer)
    dim t as single
    Plunged% = 0
    if n <= 0 then exit function
    t = n * STEPT * 2
    Plunged% = cint(GRAV * t * t / 2)
end function

' How far along load i has come: nothing till it has trembled, LOADRUN at its ledge's end, LOADFAR at the most.
function Slid% (lv as Level, i as integer)
    dim n as integer, x as integer
    Slid% = 0
    n = lv.ob(i).s - TREMBLE
    if n <= 0 then exit function
    x = LOADRUN * n * n \ (LOADSLIDE * LOADSLIDE)
    if n > LOADSLIDE then x = LOADRUN + 2 * LOADRUN * (n - LOADSLIDE) \ LOADSLIDE
    if x > LOADFAR then x = LOADFAR
    Slid% = x
end function

' Load i's heap, the next along; NONE without one.
function HeapOf% (lv as Level, i as integer)
    dim j as integer
    HeapOf% = NONE
    for j = i + 1 to lv.nObj - 1
        if lv.ob(j).a > lv.ob(i).a + LOADFAR then exit for
        if lv.ob(j).k <> KHEAP then continue for
        HeapOf% = j
        exit for
    next
end function

' The avalanche's step: let go as he passes its head, its front comes on, the ground shaking while it is in
' view, and takes him where it reaches him.
sub Avalanche (lv as Level, mu as Deck, se as Session, g as Game, p as Hero)
    if lv.aval.st = 0 then
        if p.x < lv.avalX then exit sub
        lv.aval.st = 1
        lv.aval.x = lv.avalX - AVALLEAD
        g.shake = .5
        Sfx mu, se, SCRACK
    end if
    lv.aval.x = lv.aval.x + AVALV * STEPT
    if (se.ticks and 15) = 0 and lv.aval.x > g.camx - 40 then g.shake = .12
    if lv.aval.x > p.x - 6 then Lose mu, se, p
end sub

' The avalanche over the view: its snow from the view's left to its front, AVALH deep on the ground it has
' covered, paler at its top; its face rounded, its top heaving, balls of snow rolling on both; blocks of ice
' thrown on ahead of it.
sub DrawAvalanche (lv as Level, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, wx as integer, d as integer, h as integer, y as integer, j as integer
    if lv.aval.st = 0 or lv.aval.x < cx then exit sub
    for x = 0 to SW - 1 step 4
        wx = cx + x
        d = cint(lv.aval.x) - wx
        if d <= 0 then exit for
        h = AVALH
        if d < 64 then h = cint(AVALH * sqr(d / 64))
        h = h + cint(4 * sin((wx + se.ticks * 3) / 11))
        y = SnowTop%(lv, wx) - cy - h
        line (x, y + 22)-(x + 3, SH - 1), CFLAKE, BF
        line (x, y)-(x + 3, y + 21), CSNOW, BF
        line (x, y)-(x + 3, y + 1), CWHITE, BF
        ' a ball on its top every 36 along, turning
        j = wx \ 36
        if wx mod 36 >= 4 or d <= 24 then continue for
        Blit art, x - 12, y - 14 + j mod 3 * 4, ISNOWBALL + ((se.ticks \ 8 + j) and 1) * 2, 28, 28
    next
    ' and down its face, tumbling
    for j = 0 to 3
        wx = cint(lv.aval.x) - 20 + j * 5 - (se.ticks * 2 + j * 9) mod 12
        y = SnowTop%(lv, wx) - cy - 26 - j * 13 + (se.ticks + j * 7) mod 10
        Blit art, wx - cx - 14, y, ISNOWBALL + ((se.ticks \ 6 + j) and 1) * 2, 28, 28
    next
    for j = 0 to 4
        wx = cint(lv.aval.x) + 8 + j * 11 + (se.ticks * (2 + j)) mod 16
        y = SnowTop%(lv, wx) - cy - 6 - abs((se.ticks * (4 + j) + j * 13) mod 36 - 18)
        line (wx - cx, y)-(wx - cx + 3, y + 3), CICE, BF
        line (wx - cx, y)-(wx - cx + 3, y), CWHITE
    next
end sub

' The ground's top at x, for the snow to lie on.
function SnowTop% (lv as Level, x as integer)
    dim y as integer
    y = Surf%(lv, x, lv.top(x \ TS) * TS + 8)
    if y = NONE then y = lv.top(x \ TS) * TS
    SnowTop% = y
end function

' The ticks of calm left before the blizzard's next gust: 0 while it blows. The wind drops while the avalanche
' comes: nothing may stop him then, and a gust would.
function Lull% (lv as Level, se as Session)
    dim t as integer
    t = GUSTP - GUSTON - se.ticks mod GUSTP
    if t < 0 then t = 0
    if lv.avalX <> NONE and lv.aval.st = 1 then t = GUSTP
    Lull% = t
end function

' A gust on him: borne back on his feet and in the air, spared curled or crouched on the ground, or mounted.
sub Buffet (lv as Level, se as Session, p as Hero)
    if Lull%(lv, se) > 0 or p.swim or p.sled or p.goat then exit sub
    if not p.ground then
        if p.vx > -WINDV then p.vx = p.vx - WINDA * STEPT
    elseif not (p.roll or p.crouch or p.charging) then
        if p.gsp > -WINDV then p.gsp = p.gsp - WINDG * STEPT
    end if
end sub

' The blizzard over the view: flakes aslant always; a gust's streaks of snow from the right, fewer as it rises.
sub DrawGale (lv as Level, se as Session)
    dim j as integer, x as integer, y as integer, n as integer
    for j = 0 to 23
        x = SW - (se.ticks * (2 + (j and 3)) + j * 61) mod (SW + 16)
        y = (se.ticks * (1 + (j and 1)) + j * 37) mod SH
        pset (x, y), CWHITE
        pset (x + 1, y - 1), CFLAKE
    next
    if Lull%(lv, se) > GUSTTELL then exit sub
    n = 17
    if Lull%(lv, se) > 0 then n = 7
    for j = 0 to n
        x = SW + 40 - (se.ticks * (9 + j mod 5) + j * 71) mod (SW + 80)
        y = (j * 47 + (se.ticks \ 2) * 3) mod SH
        line (x, y)-(x + 8 + (j mod 3) * 6, y - 2 - j mod 3), CWHITE
        line (x + 4, y + 1)-(x + 10 + (j mod 3) * 6, y - 1 - j mod 3), CFLAKE
    next
end sub

' A goat's step: off its place it trots home; at graze there it faces him and, seeing him on its ledge, lowers its
' head, in a blizzard only with the calm left to charge in; then it charges till it is under or past him or its
' ledge ends, and stands dazed.
sub Graze (lv as Level, se as Session, i as integer, p as Hero)
    dim s as integer, f as integer, x as integer
    s = abs(lv.ob(i).s)
    f = sgn(lv.ob(i).s)
    select case s
    case 1
        if lv.ob(i).x <> lv.ob(i).a then
            f = sgn(lv.ob(i).a - lv.ob(i).x)
            lv.ob(i).x = lv.ob(i).x + f * GOATSTEP \ 2
        else
            f = 1 + 2 * (p.x < lv.ob(i).x)
            x = abs(p.x - lv.ob(i).x) < GOATSEE and abs(p.y - lv.ob(i).y) < 30
            if x and p.goat = 0 and (not lv.gale or Lull%(lv, se) > GUSTP \ 2) then s = GOATWIND + 1
        end if
    case 2: s = GCHARGE
    case GCHARGE
        x = lv.ob(i).x + f * GOATSTEP
        if (p.x - x) * f < 0 or abs(x - lv.ob(i).a) > GOATRUN or Surf%(lv, x + f * 12, lv.ob(i).y) <> lv.ob(i).y then
            s = GCHARGE + GOATDAZE
        else
            lv.ob(i).x = x
        end if
    case else
        s = s - 1
        if s = GCHARGE then s = 1
    end select
    lv.ob(i).s = s * f
end sub

' A goat met: come down on, it is ridden; its horns, lowered or at a charge, hurt.
sub MeetGoat (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose)
    dim s as integer
    s = abs(lv.ob(i).s)
    if s = 0 or p.goat or abs(lv.ob(i).x - p.x) >= 16 or p.y < lv.ob(i).y - 30 or p.y > lv.ob(i).y + 8 then exit sub
    if not p.ground and p.vy > 0 and p.y < lv.ob(i).y - 6 and not p.sled then
        Mount lv, mu, se, p, i
    elseif s > 1 and s <= GCHARGE and p.y > lv.ob(i).y - 18 then
        Hurt mu, se, g, p, lv.ob(i).x, lo(), SHURT
    end if
end sub

' Onto goat i's back: it bolts on along the road. A goat still on its way home is there.
sub Mount (lv as Level, mu as Deck, se as Session, p as Hero, i as integer)
    if lv.stray.st then lv.ob(lv.stray.i).s = 1
    lv.stray.i = i
    lv.stray.st = 1
    lv.stray.x = lv.ob(i).x
    lv.stray.y = lv.ob(i).y
    lv.ob(i).s = 0
    lv.ob(i).x = lv.ob(i).a
    p.goat = -1
    p.rideT = 0
    p.x = lv.stray.x
    p.y = lv.stray.y - 1
    p.face = 0
    p.vx = GOATV
    p.vy = 60
    p.roll = 0
    p.jumped = 0
    Sfx mu, se, SBUMP
end sub

' Astride a goat: it runs on along the road and takes no rein. Off the ground, stopped or GOATRIDE on, he is off
' it, and it turns for home from where it last stood.
sub Ride (lv as Level, p as Hero, l as integer, r as integer, d as integer)
    p.rideT = p.rideT + STEPT
    l = 0
    r = 0
    d = 0
    if p.rideT > .1 and (not p.ground or p.rideT > GOATRIDE or abs(p.x - p.lastx) < 1) then
        p.goat = 0
        lv.stray.st = 2
    elseif p.ground then
        p.gsp = GOATV
        p.face = 0
        p.roll = 0
        lv.stray.x = p.x
        lv.stray.y = p.y
    end if
end sub

' The goat let go runs home along the ground; there, or out of sight, it is at graze again.
sub Homeward (lv as Level, g as Game)
    dim y as integer
    lv.stray.x = lv.stray.x + sgn(lv.ob(lv.stray.i).a - lv.stray.x) * STRAYV * STEPT
    y = Surf%(lv, cint(lv.stray.x), cint(lv.stray.y))
    if y <> NONE then lv.stray.y = y
    if abs(lv.stray.x - lv.ob(lv.stray.i).a) >= 6 and abs(lv.stray.x - g.camx - SW \ 2) <= SW then exit sub
    lv.ob(lv.stray.i).s = 1
    lv.stray.st = 0
end sub

' An ice wall across the road: he is stopped at it; a goat ridden goes through, and leaves its stump.
sub MeetWall (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, pt() as Part)
    if lv.ob(i).s = 0 or abs(lv.ob(i).x - p.x) >= WALLR then exit sub
    if p.y <= lv.ob(i).y - WALLH or p.y > lv.ob(i).y + 20 then exit sub
    if p.goat then
        lv.ob(i).s = 0
        g.score = g.score + 100
        g.shake = .2
        Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y - WALLH \ 2), 16, 180, CWHITE
        Sfx mu, se, SCRACK
        exit sub
    end if
    Balk p, lv.ob(i).x, WALLR
end sub

' Charlie stopped r from x, on the side of it he is.
sub Balk (p as Hero, x as integer, r as integer)
    dim s as integer
    s = 1 + 2 * (p.x < x)
    p.x = x + s * r
    if p.ground then
        if p.gsp * s < 0 then p.gsp = 0
    elseif p.vx * s < 0 then
        p.vx = 0
    end if
end sub

' A slid load's heap across the road: he is stopped at its flank, nearer its middle the higher his feet, and
' over it goes free.
sub MeetHeap (lv as Level, p as Hero, i as integer)
    dim h as single, r as integer
    if lv.ob(i).s = 0 then exit sub
    h = lv.ob(i).y - p.y
    if h >= HEAPH or h < -20 then exit sub
    if h < 0 then h = 0
    r = cint(HEAPR * (1 - h / HEAPH))
    if abs(p.x - lv.ob(i).x) < r then Balk p, lv.ob(i).x, r
end sub

' The goat not ridden within 150 of x, NONE when there is none.
function GoatNear% (lv as Level, x as integer)
    dim i as integer, c as integer
    GoatNear% = NONE
    c = (x - 220) \ TS
    if c < 0 then c = 0
    i = lv.first(c)
    do while i < lv.nObj
        if lv.ob(i).a > x + 220 then exit do
        if lv.ob(i).k = KGOAT and lv.ob(i).s <> 0 and abs(lv.ob(i).x - x) < 150 then GoatNear% = i
        i = i + 1
    loop
end function

' How the ground lies under him, of the five ways what rides it is drawn: -2 to 2, level 0.
function Slant% (p as Hero)
    dim t as integer
    if p.ground then t = cint(p.ang * 8 / PI)
    if t > 2 then t = 2
    if t < -2 then t = -2
    Slant% = t
end function

' A ledge's load at x, y on the screen: shaking once set off, sliding off, falling; nothing once it is down.
sub DrawLoad (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer)
    dim s as integer, jig as integer
    s = lv.ob(i).s
    if s < 0 then exit sub
    jig = s > 0 and s <= TREMBLE and (se.ticks and 2) <> 0
    Blit art, x - 34 + jig + Slid%(lv, i), y - 16 + Plunged%(s - TREMBLE - LOADSLIDE), ILOAD, 76, 22
end sub

' Goat i at x, y on the screen: its head up at graze and dazed, down while it gathers itself and charges.
sub DrawGoat (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer)
    dim s as integer, f as integer
    s = abs(lv.ob(i).s)
    if s = 0 then exit sub
    f = -2 * (lv.ob(i).s < 0)
    if s > 1 and s <= GCHARGE then
        Blit art, x - 17, y - 40, IGOATR + (5 + (s = GCHARGE) * ((se.ticks \ 6) and 1)) * 4 + f, 34, 58
    else
        Blit art, x - 17, y - 23, IGOAT + f, 34, 24
    end if
end sub

' How the ground lies at x near y, of the five ways what rides it is drawn: -2 to 2, level 0.
function Lie% (lv as Level, x as integer, y as integer)
    dim a as integer, b as integer, t as integer
    a = Surf%(lv, x - 8, y)
    b = Surf%(lv, x + 8, y)
    if a <> NONE and b <> NONE then t = (a - b) \ 8
    if t > 2 then t = 2
    if t < -2 then t = -2
    Lie% = t
end function

' The goat on its way home, as the ground lies under it.
sub DrawStray (lv as Level, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, f as integer
    x = cint(lv.stray.x)
    y = cint(lv.stray.y)
    f = ((Lie%(lv, x, y) + 2) * 2 + ((se.ticks \ 6) and 1)) * 4 - 2 * (lv.ob(lv.stray.i).a < x)
    Blit art, x - cx - 17, y - cy - 40, IGOATR + f, 34, 58
end sub

' Icicle i breaks.
sub Shatter (lv as Level, mu as Deck, se as Session, pt() as Part, i as integer)
    Burst se, pt(), csng(lv.ob(i).x), csng(lv.ob(i).y + Fallen%(lv, i) + 12), 8, 110, CWHITE
    lv.ob(i).s = -1
    Sfx mu, se, SCRACK
end sub

' Icicle i falling: it breaks on the cat or the snow-thrower under it, which is beaten, or on the ground.
sub Plunge (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, i as integer)
    dim j as integer, y as integer, d as integer
    y = lv.ob(i).y + Fallen%(lv, i) + 16
    for j = i - 4 to i + 4
        if j < 0 or j >= lv.nObj or lv.ob(i).s <= 0 then continue for
        d = (lv.ob(j).k = KCAT or lv.ob(j).k = KTHROW) and lv.ob(j).s <> 0
        if d = 0 or abs(lv.ob(j).x - lv.ob(i).x) >= 14 or y <= lv.ob(j).y - 16 then continue for
        Beaten lv, mu, se, g, pt(), j, 8
        Shatter lv, mu, se, pt(), i
    next
    if lv.ob(i).s > 0 and Solid%(lv, lv.ob(i).x, y) then Shatter lv, mu, se, pt(), i
end sub

' Whether snow-thrower i is up from behind its wall: open to a roll, and about to throw.
function Risen% (lv as Level, i as integer)
    Risen% = abs(lv.ob(i).s) <= THROWUP and abs(lv.ob(i).s) > THROWAT
end function

' Thrower i's snowball, from its paw, lobbed to land where Charlie is now; none if he is far.
sub Hurl (lv as Level, i as integer, p as Hero)
    dim n as integer
    if abs(p.x - lv.ob(i).x) > THROWR or abs(p.y - lv.ob(i).y) > 60 then exit sub
    for n = 0 to NLOB - 1
        if not lv.lobs(n).live then exit for
    next
    if n = NLOB then exit sub
    lv.lobs(n).x = lv.ob(i).x + 6 * sgn(lv.ob(i).s)
    lv.lobs(n).y = lv.ob(i).y - 20
    lv.lobs(n).vx = (p.x - lv.lobs(n).x) / LOBT
    lv.lobs(n).vy = (p.y - 12 - lv.lobs(n).y) / LOBT - GRAV * LOBT / 2
    lv.lobs(n).live = -1
    lv.lobN = lv.lobN + 1
end sub

' Snowball n bursts.
sub Splat (lv as Level, mu as Deck, se as Session, n as integer, pt() as Part)
    lv.lobs(n).live = 0
    lv.lobN = lv.lobN - 1
    Burst se, pt(), lv.lobs(n).x, lv.lobs(n).y, 6, 90, CWHITE
    Sfx mu, se, SPOP
end sub

' The snowballs in the air, each on its arc till it meets Charlie, whom it hurts, or the ground.
sub Volley (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim n as integer
    for n = 0 to NLOB - 1
        if lv.lobs(n).live = 0 then continue for
        lv.lobs(n).vy = lv.lobs(n).vy + GRAV * STEPT
        lv.lobs(n).x = lv.lobs(n).x + lv.lobs(n).vx * STEPT
        lv.lobs(n).y = lv.lobs(n).y + lv.lobs(n).vy * STEPT
        if abs(lv.lobs(n).x - p.x) < 10 and abs(lv.lobs(n).y - p.y + 13) < 14 then
            Hurt mu, se, g, p, cint(lv.lobs(n).x - sgn(lv.lobs(n).vx) * 10), lo(), SHURT
            Splat lv, mu, se, n, pt()
        elseif Solid%(lv, cint(lv.lobs(n).x), cint(lv.lobs(n).y)) or lv.lobs(n).y > MH * TS then
            Splat lv, mu, se, n, pt()
        end if
    next
end sub

' A snow-thrower met: ducked, its wall throws him back, neither hurt; up, it is a cat, beaten by a roll.
sub MeetThrow (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, i as integer, lo() as Loose, pt() as Part)
    dim s as integer
    if lv.ob(i).s = 0 or abs(lv.ob(i).x - p.x) >= 16 or p.y < lv.ob(i).y - 22 or p.y > lv.ob(i).y + 8 then exit sub
    if Risen%(lv, i) then
        Tackle lv, mu, se, g, p, i, 8, lo(), pt()
        exit sub
    end if
    s = 1 + 2 * (p.x < lv.ob(i).x)
    p.x = lv.ob(i).x + s * 16
    if p.ground then
        p.gsp = s * 150
    else
        p.vx = s * 150
    end if
    Sfx mu, se, SBUMP
end sub

' The snowballs in the air.
sub DrawLobs (lv as Level, art as Store, cx as integer, cy as integer)
    dim n as integer
    for n = 0 to NLOB - 1
        if lv.lobs(n).live then Blit art, cint(lv.lobs(n).x) - cx - 4, cint(lv.lobs(n).y) - cy - 4, ILOB, 8, 8
    next
end sub

' Charlie barks: every cat the bark reaches flinches, every crab goes into its shell, every gull is scared, every
' snow-thrower ducks.
sub Bark (lv as Level, mu as Deck, se as Session, p as Hero)
    dim i as integer, c as integer, dx as single, dy as single, hung as integer
    p.barkT = BARKT
    Sfx mu, se, SBARK
    c = (cint(p.x) - BARKR - 40) \ TS
    if c < 0 then c = 0
    for i = lv.first(c) to lv.nObj - 1
        if lv.ob(i).a > p.x + BARKR + 40 then exit for
        dx = abs(lv.ob(i).x - p.x)
        dy = abs(lv.ob(i).y - p.y)
        ' an icicle or a ledge's load is loosed from further under it than he can jump
        hung = lv.ob(i).k = KICICLE or lv.ob(i).k = KLOAD
        if hung and lv.ob(i).s = 0 and dx <= BARKR and dy < 140 then lv.ob(i).s = 1
        if lv.ob(i).s = 0 or dx > BARKR or dy >= 40 then continue for
        select case lv.ob(i).k
        case KCAT: Flinch lv, i, cint(p.x)
        case KCRAB: lv.ob(i).s = sgn(lv.ob(i).s) * SHELLT
        case KTHROW: lv.ob(i).s = sgn(lv.ob(i).s) * THROWC
        end select
    next
    for i = 0 to lv.nGulls - 1
        if lv.gulls(i).st <= 0 then continue for
        if abs(lv.gulls(i).x - p.x) >= GULLHEAR or abs(lv.gulls(i).y - p.y) >= GULLHEAR then continue for
        lv.gulls(i).st = GSCARED
    next
end sub

sub Things (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer, c as integer
    ' typed: a const made from a const and left typeless is rounded to a whole number, 0 here
    const DT! = STEPT * 2
    c = (cint(g.camx) - 100) \ TS
    if c < 0 then c = 0
    for i = lv.first(c) to lv.nObj - 1
        if lv.ob(i).a > g.camx + SW + 100 then exit for
        select case lv.ob(i).k
        case KCAT, KCRAB, KSWEEP: if lv.ob(i).s <> 0 then Prowl lv, i
        case KSPARK: lv.ob(i).x = lv.ob(i).a + cint(SPARKR * sin((se.ticks + lv.ob(i).a) * .03))
        case KICICLE: Totter lv, mu, se, g, pt(), i
        case KLOAD: Unload lv, mu, se, g, p, lo(), pt(), i
        case KGOAT: if lv.ob(i).s <> 0 then Graze lv, se, i, p
        case KTHROW: TakeAim lv, i, p
        case KSPRING, KBUMP, KHOOP: if lv.ob(i).s > 0 then lv.ob(i).s = lv.ob(i).s - 1
        case KBUOY
            lv.ob(i).y = Bob%(lv, lv.ob(i).x, -4, 9)
            if lv.ob(i).s > 0 then lv.ob(i).s = lv.ob(i).s - 1
        case KJELLY
            lv.ob(i).y = Bob%(lv, lv.ob(i).x, JELLYD, 0)
            if lv.ob(i).s > 1 then lv.ob(i).s = lv.ob(i).s - 1
        end select
    next
    if se.looseN then LooseBones lv, se, lo(), DT
    if se.partN then Particles se, pt(), DT
end sub

' Icicle i, loosed: it trembles a while, then falls.
sub Totter (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, i as integer)
    if lv.ob(i).s <= 0 then exit sub
    lv.ob(i).s = lv.ob(i).s + 1
    if lv.ob(i).s > TREMBLE then Plunge lv, mu, se, g, pt(), i
end sub

' Load i of snow, set off: a step of its trembling, its slide off the ledge and its fall. Down on the road it is
' gone and its heap lies there; Charlie under it is hurt.
sub Unload (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part, i as integer)
    dim h as integer
    if lv.ob(i).s <= 0 then exit sub
    lv.ob(i).s = lv.ob(i).s + 1
    h = HeapOf%(lv, i)
    if h = NONE then exit sub
    if lv.ob(i).y + Plunged%(lv.ob(i).s - TREMBLE - LOADSLIDE) < lv.ob(h).y then exit sub
    lv.ob(i).s = -1
    lv.ob(h).s = 1
    Bury lv, mu, se, g, pt(), h
    if abs(p.x - lv.ob(h).x) >= HEAPR or abs(p.y - lv.ob(h).y) >= HEAPH then exit sub
    Hurt mu, se, g, p, lv.ob(h).x, lo(), SHURT
end sub

' Heap h, come down: the cats and snow-throwers under it are beaten.
sub Bury (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, h as integer)
    dim j as integer, foe as integer
    for j = h - 6 to h + 6
        if j < 0 or j >= lv.nObj then continue for
        foe = (lv.ob(j).k = KCAT or lv.ob(j).k = KTHROW) and lv.ob(j).s <> 0
        if foe = 0 or abs(lv.ob(j).x - lv.ob(h).x) >= HEAPR + 10 then continue for
        if abs(lv.ob(j).y - lv.ob(h).y) < 24 then Beaten lv, mu, se, g, pt(), j, 8
    next
    g.shake = .3
    Burst se, pt(), csng(lv.ob(h).x), csng(lv.ob(h).y - 12), 16, 160, CWHITE
    Sfx mu, se, SCRACK
end sub

' Snow-thrower i: a step of its turn, facing Charlie; and its throw.
sub TakeAim (lv as Level, i as integer, p as Hero)
    dim s as integer
    if lv.ob(i).s = 0 then exit sub
    s = abs(lv.ob(i).s) - 1
    if s = 0 then s = THROWC
    lv.ob(i).s = s * (1 + 2 * (p.x > lv.ob(i).x))
    if s = THROWAT then Hurl lv, i, p
end sub

' The bones knocked loose, dt on: each falls and bounces till its time is up or it is off the world's foot.
sub LooseBones (lv as Level, se as Session, lo() as Loose, dt as single)
    dim i as integer, x as integer, y as integer, s as integer
    for i = 0 to NLOOSE - 1
        if lo(i).t <= 0 then continue for
        lo(i).t = lo(i).t - dt
        lo(i).vy = lo(i).vy + GRAV * .6 * dt
        lo(i).x = lo(i).x + lo(i).vx * dt
        if Solid%(lv, cint(lo(i).x), cint(lo(i).y) - 3) then lo(i).vx = -lo(i).vx
        lo(i).y = lo(i).y + lo(i).vy * dt
        x = cint(lo(i).x)
        y = cint(lo(i).y) + 4
        s = Surf%(lv, x, y)
        if s <= y and y - s < 14 and lo(i).vy > 0 then
            lo(i).y = s - 4
            lo(i).vy = -lo(i).vy * .7
        end if
        if lo(i).y > MH * TS + 16 then lo(i).t = 0
        if lo(i).t <= 0 then se.looseN = se.looseN - 1
    next
end sub

' The particles, dt on: each on its arc till its time is up.
sub Particles (se as Session, pt() as Part, dt as single)
    dim i as integer
    for i = 0 to NPART - 1
        if pt(i).t <= 0 then continue for
        pt(i).t = pt(i).t - dt
        pt(i).x = pt(i).x + pt(i).vx * dt
        pt(i).y = pt(i).y + pt(i).vy * dt
        pt(i).vy = pt(i).vy + 300 * dt
        if pt(i).t <= 0 then se.partN = se.partN - 1
    next
end sub

sub Emit (se as Session, pt() as Part, x as single, y as single, vx as single, vy as single, t as single, c as integer)
    dim i as integer
    if se.partN >= NPART then exit sub
    for i = 0 to NPART - 1
        if pt(i).t > 0 then continue for
        pt(i).x = x
        pt(i).y = y
        pt(i).vx = vx
        pt(i).vy = vy
        pt(i).t = t
        pt(i).c = c
        se.partN = se.partN + 1
        exit sub
    next
end sub

' n sparks out from x, y at speed v.
sub Burst (se as Session, pt() as Part, x as single, y as single, n as integer, v as single, c as integer)
    dim i as integer, a as single
    for i = 0 to n - 1
        a = 2 * PI * (i + rnd * .5) / n
        Emit se, pt(), x, y, cos(a) * v, sin(a) * v - v * .5, .5, c
    next
end sub

' The view ahead of Charlie's run, kept in the level; snap puts it there.
sub Camera (lv as Level, zn as Realm, g as Game, p as Hero, snap as integer)
    dim tx as single, ty as single, v as single
    if p.ground then
        v = p.gsp
    else
        v = p.vx
    end if
    v = v * .2
    if v > 70 then v = 70
    if v < -70 then v = -70
    tx = p.x - SW \ 2 + v
    ty = p.y - Headroom%(lv, zn, p)
    if snap then
        g.camx = tx
        g.camy = ty
    else
        g.camx = g.camx + (tx - g.camx) * 7 * STEPT
        g.camy = g.camy + (ty - g.camy) * 6 * STEPT
        if p.x - g.camx > 250 then g.camx = p.x - 250
        if p.x - g.camx < 70 then g.camx = p.x - 70
        if p.y - g.camy > 172 then g.camy = p.y - 172
        if p.y - g.camy < 48 then g.camy = p.y - 48
    end if
    if g.camx < 0 then g.camx = 0
    if g.camx > lv.wpx - SW then g.camx = lv.wpx - SW
    if g.camy < 0 then g.camy = 0
    if g.camy > MH * TS - SH then g.camy = MH * TS - SH
end sub

' How far under the view's top Charlie is kept: further in a crane's hall while it hangs there, to see it.
function Headroom% (lv as Level, zn as Realm, p as Hero)
    Headroom% = HEAD
    if zn.id <> 1 or lv.chaseX = NONE or p.x < lv.chaseX or p.x > lv.chaseEnd then exit function
    if lv.cart.st = 1 or lv.cart.st = 2 then Headroom% = HEADUP
end function

' ---- the chase ----

' The chase as before Charlie came to it: who flees waiting a little way into the race, the zone's tune back. A
' keepsake taken stays taken.
sub ResetChase (lv as Level, mu as Deck)
    if lv.chaseX = NONE or lv.cart.st = 3 then exit sub
    lv.cart.st = 0
    lv.cart.hits = 0
    lv.cart.v = 0
    lv.cart.t = 0
    lv.cart.x = lv.chaseX + CARTLEAD
    lv.cart.y = lv.top(cint(lv.cart.x) \ TS) * TS
    lv.box.st = 0
    lv.box.t = RELOAD
    ZoneTune mu, mu.stageTune
end sub

' A step of the chase, by its state: begun as Charlie comes to the race; then who flees does, by its zone, and he
' may meet it; broken, its keepsake is his when he reaches it.
sub Flee (lv as Level, zn as Realm, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    if lv.chaseX = NONE then exit sub
    if lv.cart.t > 0 then lv.cart.t = lv.cart.t - STEPT
    select case lv.cart.st
    case 0
        if zn.id = 2 then lv.cart.y = lv.tide - PEDALUP
        if p.x >= lv.chaseX then StartChase lv, mu
    case 1
        select case zn.id
        case 0, 3
            ' a cart and a toboggan go alike
            MoveCart lv, p, MakesOff!(zn)
            MeetCart lv, mu, se, g, p, lo(), pt()
        case 1
            MoveCrane lv, p
            MeetCrane lv, mu, se, g, p, pt()
            DropBox lv, mu, se, g, p, lo(), pt()
        case 2
            MovePedalo lv
            MeetPedalo lv, mu, se, g, p, pt()
        end select
    case 2
        if zn.id = 1 and lv.cart.t > 2 - TOOLFALL then exit sub
        ' the sands' keepsake floats
        if zn.id = 2 then lv.cart.y = lv.tide
        ' over it will do: he comes down off the last bump past it
        if abs(p.x - lv.cart.x) < 16 and p.y > lv.cart.y - 90 and p.y < lv.cart.y + 8 then TakeKeep lv, mu, se, g, pt()
    end select
end sub

' The cart is off, to the chase's tune in the zone's slot: the zone's own lead plays it, and the level's end still.
sub StartChase (lv as Level, mu as Deck)
    dim zlead as string
    lv.cart.st = 1
    zlead = mu.song(CAUGHT).leadP
    ZoneTune mu, CHASETUNE
    mu.song(ZTUNE).leadP = zlead
    mu.song(CAUGHT).leadP = zlead
end sub

' How fast who flees makes off from a bump.
function MakesOff! (zn as Realm)
    MakesOff! = BOLT
    if zn.id = 3 then MakesOff! = TOBOLT
end function

' The cart along the ground: dawdling when Charlie is far behind it, slow while a bump has it dazed, then making
' off at `away`; no further than the race's end.
sub MoveCart (lv as Level, p as Hero, away as single)
    dim want as single, s as integer
    want = CRUISE
    if lv.cart.x - p.x > 240 then want = DAWDLE
    if lv.cart.t > 0 then want = away
    if lv.cart.t > BOLTS then want = 60
    lv.cart.v = lv.cart.v + (want - lv.cart.v) * 5 * STEPT
    lv.cart.x = lv.cart.x + lv.cart.v * STEPT
    if lv.cart.x >= lv.chaseEnd then
        lv.cart.x = lv.chaseEnd
        lv.cart.v = 0
    end if
    s = Surf%(lv, cint(lv.cart.x), cint(lv.cart.y))
    if s <> NONE then lv.cart.y = s
end sub

' Charlie at the cart, once it is over its last bump: curled, he bumps it and is thrown back off it, or on a sled
' checked; else it hurts him. Its last bump breaks it.
sub MeetCart (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    if lv.cart.t > BOLTS or p.dead then exit sub
    if abs(p.x - lv.cart.x) > CARTR or p.y < lv.cart.y - 46 or p.y > lv.cart.y + 6 then exit sub
    if not (p.roll or p.charging) then
        Hurt mu, se, g, p, cint(lv.cart.x), lo(), SHISS
        exit sub
    end if
    Bumped lv, mu, se, g, pt(), lv.cart.y - 20
    ' sled against sled he is checked, not thrown: a sled thrown back has no brake to stop it
    if p.sled and p.ground then p.gsp = SLEDV
    if p.sled and not p.ground then p.vx = SLEDV
    if p.sled then exit sub
    p.ground = 0
    p.jumped = 0
    p.vx = -120
    p.vy = -BOUNCEV
end sub

' A bump told, its sparks at height y: who flees is dazed, and broken by its last.
sub Bumped (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part, y as single)
    lv.cart.hits = lv.cart.hits + 1
    lv.cart.t = DAZED
    g.score = g.score + 500
    g.shake = .2
    Burst se, pt(), lv.cart.x + 0, y, 12, 150, CYELLOW
    Sfx mu, se, SBUMP
    if lv.cart.hits >= NBUMP then BreakCart lv, mu, se, pt()
end sub

' Who flees is broken and the keepsake left lying; the zone's tune again.
sub BreakCart (lv as Level, mu as Deck, se as Session, pt() as Part)
    lv.cart.st = 2
    lv.cart.t = 2
    Burst se, pt(), lv.cart.x + 0, lv.cart.y - 12, 16, 200, CEARTH + 4
    ZoneTune mu, mu.stageTune
end sub

sub TakeKeep (lv as Level, mu as Deck, se as Session, g as Game, pt() as Part)
    lv.cart.st = 3
    g.score = g.score + 1000
    Burst se, pt(), lv.cart.x + 0, lv.cart.y - 8, 12, 120, CYELLOW
    Sfx mu, se, SMARK
end sub

' Who flees, by its zone; nothing once its keepsake is taken back.
sub DrawChase (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    if lv.chaseX = NONE or lv.cart.st = 3 then exit sub
    select case zn.id
    case 0: DrawCart lv, zn, art, se, cx, cy
    case 1: DrawCrane lv, zn, art, se, cx, cy
    case 2: DrawPedalo lv, zn, art, se, cx, cy
    case 3: DrawFrost lv, zn, art, se, cx, cy
    end select
end sub

' Frost on her toboggan as the ground lies, shaken while a bump has her dazed, the bumps she can yet take over
' her; knocked off it, she sits in deep snow, the camera lying by her.
sub DrawFrost (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, i as integer
    x = cint(lv.cart.x) - cx
    y = cint(lv.cart.y) - cy
    if lv.cart.st = 2 then
        Blit art, x - 62, y - 21, IDUNKED, 56, 22
        DrawKeep zn, art, x, y - ((se.ticks \ 16) and 1)
        exit sub
    end if
    if lv.cart.t > BOLTS then x = x + ((se.ticks \ 2) and 1) * 2 - 1
    Blit art, x - 17, y - 46, IFROST + (Lie%(lv, cint(lv.cart.x), cint(lv.cart.y)) + 2) * 2, 34, 65
    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 40)-(x - 6 + i * 8, y - 36), CYELLOW, BF
    next
end sub

' Kipper's pedal boat on the sea, from one end of its channel to the other and back: faster for each bump it has
' taken, and still a moment after one.
sub MovePedalo (lv as Level)
    lv.cart.y = lv.tide - PEDALUP
    if lv.cart.t > BOLTS then exit sub
    if lv.cart.v = 0 then lv.cart.v = 1
    lv.cart.v = sgn(lv.cart.v) * (PEDALV + lv.cart.hits * PEDALMORE)
    lv.cart.x = lv.cart.x + lv.cart.v * STEPT
    if lv.cart.x > lv.chaseEnd - PEDALW \ 2 then lv.cart.v = -1
    if lv.cart.x < lv.chaseX + PEDALW \ 2 then lv.cart.v = 1
end sub

' Charlie down onto the pedal boat's deck, out of the air: a bump, it turns round and he goes up off it.
sub MeetPedalo (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, pt() as Part)
    if lv.cart.t > BOLTS or p.dead or p.ground or p.swim or p.vy <= 0 then exit sub
    if abs(p.x - lv.cart.x) > PEDALW \ 2 or p.y < lv.cart.y - 6 or p.y > lv.cart.y + 14 then exit sub
    Bumped lv, mu, se, g, pt(), lv.cart.y - 8
    lv.cart.v = -lv.cart.v
    p.vy = -BOUNCEV
    p.jumped = 0
end sub

' The pedal boat, a pip over it for each bump it can still take; broken, the teddy afloat where it sank, and the
' cat paddling off a while.
sub DrawPedalo (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, i as integer
    x = cint(lv.cart.x) - cx
    y = cint(lv.cart.y) - cy
    if lv.cart.st = 2 then
        DrawKeep zn, art, x, y + 3 - ((se.ticks \ 16) and 1)
        DrawFled lv, art, se, x, y - 9, 120
        exit sub
    end if
    Blit art, x - 26, y - 22, IPEDAL - 2 * (lv.cart.t > BOLTS), 52, 34
    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 34)-(x - 6 + i * 8, y - 30), CYELLOW, BF
    next
end sub

' The cat making off from (x, y), v a second, a while after its ride broke.
sub DrawFled (lv as Level, art as Store, se as Session, x as integer, y as integer, v as integer)
    if lv.cart.t <= 0 then exit sub
    Blit art, x + cint((2 - lv.cart.t) * v), y, ICAT + ((se.ticks \ 4) and 1) * 2, 20, 14
end sub

' The cart, a pip over it for each bump it can still take; broken, the keepsake where it fell, and the cat making
' off a while.
sub DrawCart (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, i as integer
    x = cint(lv.cart.x) - cx
    y = cint(lv.cart.y) - cy
    if lv.cart.st = 2 then
        DrawKeep zn, art, x, y - ((se.ticks \ 16) and 1)
        DrawFled lv, art, se, x, y - 13, 260
        exit sub
    end if
    DrawWheels lv, art, x, cy
    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 50)-(x - 6 + i * 8, y - 46), CYELLOW, BF
    next
end sub

' The cart on its ground, its middle at x on the screen: each wheel on the ground under it, turning as it goes,
' and the planks between them, leaning by what the wheels' grounds differ: not at all, half a gentle slope's fall,
' or all of one's.
sub DrawWheels (lv as Level, art as Store, x as integer, cy as integer)
    dim ya as integer, yb as integer, lean as integer, k as integer
    ya = Surf%(lv, cint(lv.cart.x) - WHEELS, cint(lv.cart.y))
    yb = Surf%(lv, cint(lv.cart.x) + WHEELS, cint(lv.cart.y))
    if ya = NONE then ya = cint(lv.cart.y)
    if yb = NONE then yb = cint(lv.cart.y)
    lean = (yb - ya + 3) \ 7
    if lean < 0 then lean = 0
    if lean > 2 then lean = 2
    k = ICART + lean * 4
    if lv.cart.t > BOLTS then k = k + 2
    Blit art, x - CARTW \ 2, (ya + yb) \ 2 - cy - 41 - lean * CARTLEAN, k, CARTW, CARTH + lean * CARTLEAN * 2
    k = ICARTW + ((cint(lv.cart.x) \ 8) and 1) * 2
    Blit art, x - WHEELS - 6, ya - cy - 12, k, 13, 13
    Blit art, x + WHEELS - 6, yb - cy - 12, k, 13, 13
end sub

' ---- the tide ----

' The sea's level by the clock: from where the level began it to its other end and back, every TIDEP steps.
sub Flow (lv as Level, se as Session)
    lv.tide = lv.tideLo - cint((lv.tideLo - lv.tideHi) * (1 - cos((se.ticks - lv.tideAt) * 2 * PI / TIDEP)) / 2)
end sub

' Charlie and the sea. He is afloat once his feet are under it, falling or deeper than he can wade. Out of it he
' is wet, slow and dripping, till a roll shakes him dry.
sub Soak (lv as Level, se as Session, p as Hero, pt() as Part)
    if p.swim then exit sub
    if (p.ground and p.y > lv.tide + WADE) or (not p.ground and p.vy >= 0 and p.y > lv.tide + AFLOAT) then
        Dunk lv, se, p, pt()
        exit sub
    end if
    if p.wet <= 0 then exit sub
    if p.roll and p.ground then
        p.wet = p.wet - STEPT
        if p.wet <= 0 then Burst se, pt(), p.x, p.y - 12, 10, 130, CSEA + 1
    elseif (se.ticks and 7) = 0 then
        Emit se, pt(), p.x, p.y - 14, 0, 30, .3, CSEA + 2
    end if
end sub

' Into the sea: what he ran at he swims at, and a fall splashes. A hurt ends on landing: the sea is a landing too.
sub Dunk (lv as Level, se as Session, p as Hero, pt() as Part)
    if p.ground then
        p.vx = p.gsp * cos(p.ang)
    elseif p.vy > 200 then
        Burst se, pt(), p.x, lv.tide + 0!, 8, 110, CSEA
    end if
    p.swim = -1
    p.ground = 0
    p.stun = 0
end sub

' Afloat: at the water's surface as it rises and falls, paddled left and right, slowly; a jump is a hop out of
' it, and ground come to under the surface is waded onto.
sub Paddle (lv as Level, gr as Ground, mu as Deck, se as Session, p as Hero, l as integer, r as integer, hit as integer, pt() as Part)
    p.roll = 0
    p.jumped = 0
    p.sprung = 0
    p.crouch = 0
    p.charging = 0
    p.skid = 0
    p.wet = DRYT
    if hit then
        p.swim = 0
        p.vy = -HOPV
        p.jumped = -1
        Burst se, pt(), p.x, lv.tide + 0!, 5, 80, CSEA
        Sfx mu, se, SJUMP
        exit sub
    end if
    if r then
        if p.vx < PADV then p.vx = p.vx + PADACC * STEPT
        p.face = 0
    elseif l then
        if p.vx > -PADV then p.vx = p.vx - PADACC * STEPT
        p.face = 1
    else
        Slow p.vx, PADACC
    end if
    ' the speed he came in with is soon lost
    if abs(p.vx) > PADV then Slow p.vx, PADACC * 4
    p.x = p.x + p.vx * STEPT
    if p.vx <> 0 then
        if Wall%(lv, p, sgn(p.vx)) then p.vx = 0
    end if
    p.y = lv.tide + AFLOAT
    p.vy = 0
    if Land%(lv, gr, p, 0) then p.swim = 0
end sub

' The sea in each column whose ground is under it: bands of water from its surface down behind the column's top
' tile, a run of columns alike at a time.
sub DrawSea (lv as Level, cx as integer, cy as integer)
    dim tx as integer, tx0 as integer, tx1 as integer, y0 as integer, y1 as integer, x0 as integer, x1 as integer
    y0 = lv.tide - cy
    if y0 >= SH then exit sub
    tx1 = cx \ TS + SW \ TS
    if tx1 > lv.w - 1 then tx1 = lv.w - 1
    tx = cx \ TS
    do while tx <= tx1
        if lv.top(tx) * TS > lv.tide then
            tx0 = tx
            do while tx < tx1 and lv.top(tx + 1) = lv.top(tx0)
                tx = tx + 1
            loop
            y1 = (lv.top(tx0) + 1) * TS - 1 - cy
            x0 = tx0 * TS - cx
            x1 = tx * TS + TS - 1 - cx
            SeaBand x0, x1, y0, y0 + 1, y1, CSEA + 1
            SeaBand x0, x1, y0 + 2, y0 + 7, y1, CSEA + 2
            SeaBand x0, x1, y0 + 8, y0 + 25, y1, CSEA + 3
            SeaBand x0, x1, y0 + 26, SH - 1, y1, CSEA + 4
        end if
        tx = tx + 1
    loop
end sub

' Rows a to b of a band of the sea, of colour c, no lower than row last or the screen's foot.
sub SeaBand (x0 as integer, x1 as integer, a as integer, b as integer, last as integer, c as integer)
    dim y0 as integer, y1 as integer
    y0 = a
    y1 = b
    if y0 < 0 then y0 = 0
    if y1 > last then y1 = last
    if y1 > SH - 1 then y1 = SH - 1
    if y0 <= y1 then line (x0, y0)-(x1, y1), c, BF
end sub

' The water's edge over what is in it: foam and the first of the water, in each column of the sea deep enough
' for them; in a shallower one the foam's line alone, and on a slope nothing.
sub DrawSurf (lv as Level, art as Store, cx as integer, cy as integer)
    dim tx as integer, tx1 as integer, y as integer, deep as integer
    y = lv.tide - cy - 1
    if y >= SH or y < -8 then exit sub
    tx1 = cx \ TS + SW \ TS
    if tx1 > lv.w - 1 then tx1 = lv.w - 1
    for tx = cx \ TS to tx1
        deep = lv.top(tx) * TS - lv.tide
        if deep <= 0 or lv.vis(tx) > 2 then continue for
        if deep >= 7 then
            Blit art, tx * TS - cx, y, ISURF, TS, 8
        else
            line (tx * TS - cx, y + 1)-(tx * TS - cx + TS - 1, y + 1), CSEA, BF
        end if
    next
end sub

' ---- the works' chase ----

' The crane along its rail, kept to the hall: after Charlie, to be over him, faster for each bump, and still while
' it holds a crate over him; bumped, still a while, then off from him at speed.
sub MoveCrane (lv as Level, p as Hero)
    dim want as single, dir as integer
    dir = sgn(p.x - lv.cart.x)
    want = dir * (CRANEV + CRANEUP * lv.cart.hits)
    if lv.box.st = 1 then want = 0
    if lv.cart.t > 0 then want = CRANEBOLT * (2 * (dir > 0) + 1)
    if lv.cart.t > BOLTS then want = 0
    lv.cart.v = lv.cart.v + (want - lv.cart.v) * 5 * STEPT
    lv.cart.x = lv.cart.x + lv.cart.v * STEPT
    if lv.cart.x < lv.chaseX + 40 then lv.cart.x = lv.chaseX + 40
    if lv.cart.x > lv.chaseEnd - 40 then lv.cart.x = lv.chaseEnd - 40
end sub

' Charlie thrown up to the crane: in the air at its gondola he bumps it and is knocked aside, out of a jet's way,
' and down, or up off it if he came down on it; its crate is lost.
sub MeetCrane (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, pt() as Part)
    dim foot as single
    if lv.cart.t > BOLTS or p.dead or p.ground then exit sub
    foot = lv.cart.y - CRANEH
    if abs(p.x - lv.cart.x) > CRANER or p.y - 26 > foot or p.y < foot - 24 then exit sub
    if lv.box.st > 0 or lv.box.t <= 0 then BreakBox lv, mu, se, pt()
    Bumped lv, mu, se, g, pt(), foot - 12
    p.vx = 140 * (2 * (p.x <= lv.cart.x) + 1)
    p.vy = 160
    if p.y < foot - 14 then p.vy = -200
end sub

' The crane's crate, by its state: on the hook, and held still once it is over Charlie; let go a moment later; then
' falling, to hurt him if it meets him and to break on the floor.
sub DropBox (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    if lv.box.t > 0 then lv.box.t = lv.box.t - STEPT
    select case lv.box.st
    case 0
        lv.box.x = lv.cart.x
        lv.box.y = lv.cart.y - CRANEH + 16
        if lv.box.t <= 0 and lv.cart.t <= 0 and abs(p.x - lv.cart.x) < 10 then
            lv.box.st = 1
            lv.box.t = POISE
        end if
    case 1
        if lv.box.t <= 0 then
            lv.box.st = 2
            lv.box.v = 0
        end if
    case 2
        lv.box.v = lv.box.v + GRAV * STEPT
        lv.box.y = lv.box.y + lv.box.v * STEPT
        if abs(p.x - lv.box.x) < 14 and p.y > lv.box.y - 16 and p.y - 26 < lv.box.y and not p.dead then
            Hurt mu, se, g, p, cint(lv.box.x), lo(), SHISS
            BreakBox lv, mu, se, pt()
        elseif lv.box.y >= lv.cart.y then
            BreakBox lv, mu, se, pt()
        end if
    end select
end sub

' The crate in bits where it is, the next a while off: sooner for each bump the crane has had.
sub BreakBox (lv as Level, mu as Deck, se as Session, pt() as Part)
    Burst se, pt(), lv.box.x + 0, lv.box.y - 8, 10, 140, CEARTH + 4
    Sfx mu, se, SPOP
    lv.box.st = 0
    lv.box.t = RELOAD - HASTE * lv.cart.hits
end sub

' The rail over the hall and the crane on it, a pip over it for each bump it can still take, its crate on the hook
' or falling; broken, its lamp out and the toolbox fallen from it.
sub DrawCrane (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer)
    dim x as integer, y as integer, i as integer, fy as integer, a as integer, b as integer, jolt as integer
    x = cint(lv.cart.x) - cx
    fy = cint(lv.cart.y) - cy
    y = fy - RAILUP
    a = (cx - lv.chaseX) \ 32
    if a < 0 then a = 0
    b = (cx + SW - lv.chaseX) \ 32
    if b > (lv.chaseEnd - lv.chaseX) \ 32 - 1 then b = (lv.chaseEnd - lv.chaseX) \ 32 - 1
    for i = a to b
        Blit art, lv.chaseX + i * 32 - cx, y, IRAIL, 32, 8
    next
    Blit art, x - 22, y - 2, ICRANE - 2 * (lv.cart.st = 2 or lv.cart.t > BOLTS), 44, 46
    if lv.cart.st = 2 then
        i = cint((lv.cart.t - 2 + TOOLFALL) * CRANEH / TOOLFALL)
        if i < 0 then i = 0
        DrawKeep zn, art, x, fy - i - ((se.ticks \ 16) and 1)
        exit sub
    end if
    for i = lv.cart.hits to NBUMP - 1
        line (x - 10 + i * 8, y - 7)-(x - 6 + i * 8, y - 3), CYELLOW, BF
    next
    if lv.box.st = 0 and lv.box.t > 0 then exit sub
    jolt = (lv.box.st = 1) * ((se.ticks \ 3) and 1)
    Blit art, cint(lv.box.x) - cx - 8 + jolt, cint(lv.box.y) - cy - 16, IWBOX, 16, 16
end sub

' ---- the intro ----

' The intro from its first beat: its road, its script, the view on the road's start.
sub StartIntro (wd as World, g as Game, cu() as Cue)
    wd.se.road = 0
    wd.se.sceneNext = 0
    wd.se.sceneEnd = ILEN
    LoadLevel wd, SINTRO
    LoadCues cu()
    g.mode = INTRO
    g.modeT = 0
    g.camx = 0
    g.camy = wd.lv.startY - HORIZON
end sub

' Between zones the van's bump again, on the road of stage nxt's zone: its look, its keepsake and a cat, to four
' bars of their own; then that stage.
sub StartBump (wd as World, g as Game, nxt as integer)
    wd.se.road = ZoneOf%(wd.se, nxt)
    wd.se.sceneNext = nxt
    wd.se.sceneEnd = BUMP0 + BUMPLEN
    LoadLevel wd, SINTRO
    ZoneTune wd.mu, BETWEEN
    wd.tn.dusk = 16
    Shade wd.tn
    g.mode = INTRO
    g.modeT = BUMP0 / BEATS
    g.camy = wd.lv.startY - HORIZON
end sub

' The script from its DATA: a row a cue, "end" after the last, its kind 0.
sub LoadCues (cu() as Cue)
    dim n as integer, kind as string
    restore Cues
    do
        read kind
        cu(n).kind = (instr(QNAMES, left$(kind + "     ", 6)) + 5) \ 6
        if cu(n).kind = 0 then exit do
        read cu(n).t0, cu(n).t1, cu(n).x0, cu(n).y0, cu(n).x1, cu(n).y1, cu(n).ease, cu(n).hop, cu(n).a
        n = n + 1
    loop
end sub

' A step of the intro: its view and its sounds by the script, its last beat fading. Whether it is over: played
' out, or a key pressed.
function IntroOver% (wd as World, g as Game, k as Pad, cu() as Cue)
    dim i as integer, x as integer, y as integer, was as single, now as single
    wd.se.ticks = wd.se.ticks + 1
    MuteKey wd.mu, k
    was = g.modeT * BEATS
    g.modeT = g.modeT + STEPT
    now = g.modeT * BEATS
    do while cu(i).kind
        if now >= cu(i).t0 and now < cu(i).t1 then
            select case cu(i).kind
            case QCAM
                CueAt wd.lv, cu(), i, now, x, y
                g.camx = x
            case QSND: if was < cu(i).t0 then Sfx wd.mu, wd.se, cu(i).a
            end select
        end if
        i = i + 1
    loop
    Shimmer wd.zn, wd.tn, wd.se
    if now > wd.se.sceneEnd - 1 and wd.tn.dusk > 0 and (wd.se.ticks and 1) = 0 then
        wd.tn.dusk = wd.tn.dusk - 1
        Shade wd.tn
    end if
    IntroOver% = now >= wd.se.sceneEnd or k.hit or k.start or k.quit
    Spent k
end function

' Where cue i is at beat t, in the world: along its way, eased, a hop's arc over it.
sub CueAt (lv as Level, cu() as Cue, i as integer, t as single, x as integer, y as integer)
    dim u as single, v as single
    if cu(i).t1 > cu(i).t0 then u = (t - cu(i).t0) / (cu(i).t1 - cu(i).t0)
    v = u
    if cu(i).ease = 1 then v = u * u
    if cu(i).ease = 2 then v = u * (2 - u)
    x = cint(cu(i).x0 + (cu(i).x1 - cu(i).x0) * v)
    y = lv.startY + cint(cu(i).y0 + (cu(i).y1 - cu(i).y0) * v - 4 * cu(i).hop * u * (1 - u))
end sub

' The intro's picture: the Downs, then each cue that is on, in the script's order, between two black bars.
sub IntroDraw (wd as World, g as Game, p as Hero, cu() as Cue)
    dim i as integer, cx as integer, cy as integer, now as single
    cx = int(g.camx)
    cy = int(g.camy)
    now = g.modeT * BEATS
    DrawBack wd.zn, wd.art, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.se, cx, cy, -1
    DrawTiles wd.lv, wd.zn, wd.art, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.se, cx, cy, 0
    do while cu(i).kind
        if now >= cu(i).t0 and now < cu(i).t1 then DrawCue wd, cu(), i, now, p, cx, cy
        i = i + 1
    loop
    line (0, 0)-(SW - 1, BARH - 1), 0, BF
    line (0, SH - BARH)-(SW - 1, SH - 1), 0, BF
end sub

' Cue i's picture at beat `now`, the view at cx, cy: its foot's middle where the cue is.
sub DrawCue (wd as World, cu() as Cue, i as integer, now as single, p as Hero, cx as integer, cy as integer)
    dim wx as integer, wy as integer, x as integer, y as integer, a as integer
    CueAt wd.lv, cu(), i, now, wx, wy
    x = wx - cx
    y = wy - cy
    a = cu(i).a
    select case cu(i).kind
    case QHOUSE: Blit wd.art, x - 66, y - 123, IHOUSE, 132, 124
    case QBOXES: Blit wd.art, x - 22, y - 39, IBOXES, 44, 40
    case QCRATE: Blit wd.art, x - 10, y - 15, ICRATE, 20, 16
    case QHEDGE: Blit wd.art, x - 80, y - 51, IHEDGE, 160, 52
    case QFAM: DrawFamily wd.art, wd.se, x, y, a
    case QVAN: DrawVan wd.art, x, y, wx, a
    case QFLOAT: DrawFloat wd.art, wd.se, x, y, wx, a
    case QBARON: Blit wd.art, x - 20, y - 43, BaronPic%(wd.se, a), 40, 44
    case QCAT: DrawCat wd.zn, wd.art, wd.se, x, y, a, cu(i).x1 < cu(i).x0
    case QBASKET: DrawKeep wd.zn, wd.art, x, y
    case QEYES: DrawEyes wd.se, x, y, a
    case QHERO
        Pose wd.lv, wd.gr, wd.se, p, wx, wy, a, cu(i).x1 < cu(i).x0
        DrawHero wd.gr, wd.art, wd.se, p, cx, cy
    case QWOOF
        p.x = wx
        p.y = wy
        p.face = a
        p.barkT = BARKT - (now - cu(i).t0) / BEATS
        DrawBark p, cx, cy
    end select
end sub

' The zone's keepsake, its foot's middle at x, y on the screen: the picnic basket, the toolbox, the teddy, the
' camera.
sub DrawKeep (zn as Realm, art as Store, x as integer, y as integer)
    select case zn.id
    case 0: Blit art, x - 9, y - 14, IBASKET, 18, 15
    case 1: Blit art, x - 10, y - 13, ITOOLS, 20, 14
    case 2: Blit art, x - 7, y - 15, ITEDDY, 14, 16
    case 3: Blit art, x - 9, y - 13, ICAMERA, 18, 14
    end select
end sub

' The van, its foot's middle at x, y on the screen and wx along the road: it rides a pixel up and down as it goes,
' its wheels turning. a: 1 its back door open, 2 Charlie leaning from its window, open, 4 looking back from its rear one.
sub DrawVan (art as Store, x as integer, y as integer, wx as integer, a as integer)
    dim ry as integer, k as integer
    ry = y - 79 - ((wx \ 40) and 1)
    Blit art, x - 70, ry, IVAN, 140, 80
    k = IWHEEL + ((wx \ 6) and 1) * 2
    Blit art, x - 47, y - 18, k, 18, 18
    Blit art, x + 29, y - 18, k, 18, 18
    if a and 1 then Blit art, x - 73, ry + 31, IDOOR, 12, 36
    if a and 2 then Blit art, x + 13, ry + 32, IPEEK, 30, 20
    if a and 4 then Blit art, x - 17, ry + 32, IPEEK + 2, 30, 20
end sub

' The Mob's float likewise, rattling. a: 1 the Baron sat on its crates.
sub DrawFloat (art as Store, se as Session, x as integer, y as integer, wx as integer, a as integer)
    dim ry as integer, k as integer
    ry = y - 39 - ((wx \ 24) and 1)
    Blit art, x - 44, ry, IFLOAT, 88, 40
    k = IWHEEL + ((wx \ 6) and 1) * 2
    Blit art, x - 33, y - 18, k, 18, 18
    Blit art, x + 15, y - 18, k, 18, 18
    if a and 1 then Blit art, x - 42, ry - 35, BaronPic%(se, 0), 40, 44
end sub

' The Baron's picture: sat, his tail swaying and now and then a blink; or pointing.
function BaronPic% (se as Session, aim as integer)
    dim s as integer, f as integer
    s = (se.ticks \ 12) and 7
    f = s and 3
    if f = 3 then f = 1
    if s = 7 then f = 3
    if aim then f = 4 + ((s \ 2) and 1)
    BaronPic% = IBARON + f * 2
end function

' Who meets Charlie, the middle one's feet at x, y. A picture's first LONG holds its size, as GET left it: each
' stands with its middle column on its place in the line and its last row on the ground.
sub DrawFamily (art as Store, se as Session, x as integer, y as integer, cheer as integer)
    dim n as integer, k as integer, w as integer, h as integer
    for n = 0 to NFAM - 1
        k = FamPic%(se, n, cheer)
        w = (art.img(art.at(k)) and 65535) \ 8
        h = art.img(art.at(k)) \ 65536
        Blit art, x + (n - NFAM \ 2) * FAMSTEP - w \ 2, y - h + 1, k, w, h
    next
end sub

' The picture of the family's n-th: waiting it sways and now and then blinks; cheering it runs its six, each out
' of step with the next.
function FamPic% (se as Session, n as integer, cheer as integer)
    dim s as integer, f as integer
    s = (se.ticks \ 16 + n * 3) mod 12
    f = (s \ 3) and 1
    if s = 10 then f = 2
    if cheer then f = 3 + (se.ticks \ 6 + n * 2) mod 6
    FamPic% = IFAM + (n * FAMF + f) * 2
end function

' A cat of the Mob, a stride of its prowl, west or east. a: 1 the basket on its back, 2 stood still.
sub DrawCat (zn as Realm, art as Store, se as Session, x as integer, y as integer, a as integer, west as integer)
    dim k as integer
    k = ICAT
    if (a and 2) = 0 then k = k + ((se.ticks \ 6) and 1) * 2
    if west then k = k + 4
    Blit art, x - 10, y - 13, k, 20, 14
    if a and 1 then DrawKeep zn, art, x - 2, y - 12
end sub

' Pairs of eyes in a hedge, n of them from x, y, each in a gap of its leaves and blinking in its turn.
sub DrawEyes (se as Session, x as integer, y as integer, n as integer)
    dim i as integer, ex as integer, ey as integer
    for i = 0 to n - 1
        ex = x + i * 26
        ey = y - (i and 1) * 9
        line (ex - 1, ey - 2)-(ex + 7, ey + 3), CHEDGE, BF
        line (ex - 2, ey - 1)-(ex + 8, ey + 2), CHEDGE, BF
        if (se.ticks + i * 37) mod 90 < 6 then continue for
        line (ex, ey)-(ex + 1, ey + 1), CYELLOW, BF
        line (ex + 5, ey)-(ex + 6, ey + 1), CYELLOW, BF
    next
end sub

' Charlie as a cue has it, at x, y in the world. a: 0 stood, 1 running, 2 at full tilt, 3 curled, 4 crouched,
' 5 shaken, 6 glad, 7 wound up; 16 more, or west, facing left.
sub Pose (lv as Level, gr as Ground, se as Session, p as Hero, x as integer, y as integer, a as integer, west as integer)
    dim fresh as Hero
    p = fresh
    p.x = x
    p.y = y
    p.ground = -1
    p.sg = SegAt%(lv, gr, x, csng(lv.startY))
    p.anim = se.ticks / 4!
    if west or (a and 16) then p.face = 1
    select case a and 15
    case 1: p.gsp = 200 * (1 - 2 * p.face)
    case 2: p.gsp = 450 * (1 - 2 * p.face)
    case 3: p.roll = -1
    case 4: p.crouch = -1
    case 5: p.stun = -1
    case 6: p.win = -1
    case 7: p.charging = -1
    end select
end sub

' Back to front, then the screen's words.
sub Render (wd as World, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim cx as integer, cy as integer, x as integer, s as string
    cx = int(g.camx)
    cy = int(g.camy)
    if g.shake > 0 then cy = cy + ((wd.se.ticks and 2) - 1) * 2
    if g.mode <> TITLE then Hud wd.art, g
    DrawBack wd.zn, wd.art, cx, cy
    if wd.lv.tide <> NONE then DrawSea wd.lv, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.se, cx, cy, -1
    DrawTiles wd.lv, wd.zn, wd.art, cx, cy
    DrawThings wd.lv, wd.zn, wd.art, wd.se, cx, cy, 0
    if wd.lv.stray.st = 2 then DrawStray wd.lv, wd.art, wd.se, cx, cy
    if wd.lv.nMv then DrawCables wd.lv, cx, cy
    if wd.se.looseN then DrawLoose wd.art, wd.se, cx, cy, lo()
    DrawChase wd.lv, wd.zn, wd.art, wd.se, cx, cy
    DrawHero wd.gr, wd.art, wd.se, p, cx, cy
    if wd.lv.avalX <> NONE then DrawAvalanche wd.lv, wd.art, wd.se, cx, cy
    if wd.lv.gale then DrawGale wd.lv, wd.se
    if wd.lv.nGulls then DrawGulls wd.lv, wd.art, wd.se, cx, cy
    if wd.lv.lobN then DrawLobs wd.lv, wd.art, cx, cy
    if wd.lv.tide <> NONE then DrawSurf wd.lv, wd.art, cx, cy
    DrawOver wd.lv, wd.zn, wd.art, cx, cy
    if p.barkT > BARKT - .2 then DrawBark p, cx, cy
    if wd.se.partN then DrawParts pt(), cx, cy
    select case g.mode
    case TITLE
        Blit wd.art, (SW - 170) \ 2, 22, ITITLE, 170, 26
        Blit wd.art, (SW - 170) \ 2, 52, IDASH, 170, 26
        if wd.se.reached then
            ' the zone a game starts at, an arrow each way there is another to pick
            Panel 0, 165, SW - 1, SH - 1
            s = ZoneName$(wd.se.pick)
            if wd.se.pick > 0 then s = "< " + s else s = "  " + s
            if wd.se.pick < wd.se.reached then s = s + " >" else s = s + "  "
            Banner 22, s, CWHITE
        else
            Panel 0, 181, SW - 1, SH - 1
        end if
        if (wd.se.ticks \ 30) and 1 then
            Banner 24, "PRESS SPACE", CYELLOW
        else
            Banner 24, "SPACE JUMPS  DOWN ROLLS  X BARKS", CWHITE
        end if
    case PLAYING
        Blit wd.art, 6, 5, IHUD, HUDW, HUDH
        if p.stun then
            Say p, cx, cy, "HEY!"
        elseif p.idleT > 5 then
            Say p, cx, cy, "C'MON!"
        elseif p.idleT > 3 then
            Say p, cx, cy, "..."
        end if
        if g.modeT < 2.6 then
            x = cint(g.modeT * 900) - SW
            if x > 0 then x = 0
            if g.modeT > 2.2 then x = cint((g.modeT - 2.2) * 900)
            line (x, 84)-(x + SW - 1, 110), CSCARF, BF
            line (x, 84)-(x + SW - 1, 85), CGOLD + 3, BF
            line (x, 109)-(x + SW - 1, 110), CGOLD + 3, BF
            Blit wd.art, x + (SW - 178) \ 2, 89, wd.zn.card, 178, 18
            if (wd.zn.stage and 1) and x = 0 then Txt 14, 17, "ACT 2", CYELLOW
        end if
    case CLEARED
        Blit wd.art, 6, 5, IHUD, HUDW, HUDH
        ' up and to the right: under it Charlie meets its family
        Panel 116, 4, 315, 36
        Txt 2, 27 - (len(ZoneName$(wd.zn.id)) + 8) \ 2, ZoneName$(wd.zn.id) + " CLEARED", CYELLOW
        Txt 3, 22, "BONUS " + Num$(g.bonus, 6), CWHITE
        Txt 4, 22, "SCORE " + Num$(g.score, 6), CWHITE
        if p.win then Say p, cx, cy, "WOOF!"
    case OVER
        Blit wd.art, 6, 5, IHUD, HUDW, HUDH
        Panel 108, 68, 211, 92
        Banner 10, "GAME OVER", CYELLOW
    end select
end sub

' The far backdrop an eighth of the view's scroll, the near hills half,
' both wrapping; the hills sink as the view rises.
sub DrawBack (zn as Realm, art as Store, cx as integer, cy as integer)
    dim o as integer, y as integer
    o = (cx \ 8) mod SW
    put (-o, 0), art.img(art.at(zn.back)), pset
    if o then put (SW - o, 0), art.img(art.at(zn.back)), pset
    o = (cx \ 2) mod SW
    y = 132 - cy \ 6
    if y >= SH then exit sub
    put (-o, y), art.img(art.at(zn.back + 2)), and
    put (-o, y), art.img(art.at(zn.back + 1)), or
    if o = 0 then exit sub
    put (SW - o, y), art.img(art.at(zn.back + 2)), and
    put (SW - o, y), art.img(art.at(zn.back + 1)), or
end sub

' Each column in view: its top tile, then its earth in strips below.
sub DrawTiles (lv as Level, zn as Realm, art as Store, cx as integer, cy as integer)
    dim tx as integer, tx1 as integer
    tx1 = cx \ TS + SW \ TS
    if tx1 > lv.w - 1 then tx1 = lv.w - 1
    for tx = cx \ TS to tx1
        if lv.top(tx) * TS - cy >= SH then continue for
        if zn.decked then
            DeckColumn lv, art, tx, cx, cy
        else
            CombColumn lv, art, tx, cx, cy
        end if
    next
end sub

' A column of the works: its deck's top, then its wall, which hangs from the world, not the deck: single tiles down
' to its next 128, then strips. A slope's deck runs on over the wall under it.
sub DeckColumn (lv as Level, art as Store, tx as integer, cx as integer, cy as integer)
    dim x as integer, y as integer, v as integer, k as integer, r as integer
    x = tx * TS - cx
    y = lv.top(tx) * TS - cy
    v = lv.vis(tx)
    if y > -TS and v <= 2 then put (x, y), art.img(art.at(IDK + (tx and 3))), pset
    if y > -TS and v > 2 then Blit art, x, y, IZS + ((v - 4) * 4 + (tx and 3)) * 2, TS, TS
    y = y + TS
    k = (tx and 3) * 8
    r = lv.top(tx) + 1
    do while (r and 7) <> 0 and y < SH
        if y > -TS then put (x, y), art.img(art.at(IWT + k + (r and 7))), pset
        r = r + 1
        y = y + TS
    loop
    do while y < SH
        if y > -128 then put (x, y), art.img(art.at(IWS + (tx and 3))), pset
        y = y + 128
    loop
    y = (lv.top(tx) + 1) * TS - cy
    if v > 2 and y < SH and y > -TS then Blit art, x, y, IZU + ((v - 4) * 4 + (tx and 3)) * 2, TS, TS
end sub

' A column of the downs: its turf, then its comb, which hangs from the world too, of period 64 by 48: a slope's turf
' runs on into the tile under it; under a pipe, 0, the pipe's own picture is the ground. A column whose top the tide
' is over is of the wet pictures, w on from the dry; so is the pass's bare ice.
sub CombColumn (lv as Level, art as Store, tx as integer, cx as integer, cy as integer)
    dim x as integer, y as integer, v as integer, k as integer, r as integer, w as integer
    x = tx * TS - cx
    y = lv.top(tx) * TS - cy
    v = lv.vis(tx)
    r = lv.top(tx)
    k = (tx and 3) * 3
    if r * TS > lv.tide or lv.glaze(tx) then w = WETO
    if y > -TS and v > 2 then Blit art, x, y, IAS + w + ((v - 4) * 12 + k + r mod 3) * 2, TS, TS
    if y > -TS and v > 0 and v <= 2 then put (x, y), art.img(art.at(IAF + w + k + r mod 3)), pset
    y = y + TS
    if v > 2 and y < SH then
        if y > -TS then put (x, y), art.img(art.at(IAU + w + (v - 4) * 12 + k + r mod 3)), pset
        y = y + TS
        r = r + 1
    end if
    r = r + 1
    do while r mod 3 <> 0 and y < SH
        if y > -TS then put (x, y), art.img(art.at(IAC + w + k + r mod 3)), pset
        r = r + 1
        y = y + TS
    loop
    do while y < SH
        if y > -STRIPH then put (x, y), art.img(art.at(IAL + w + (tx and 3))), pset
        y = y + STRIPH
    loop
    if lv.faceL(tx) then Bare art, x, lv.top(tx) * TS - cy, lv.faceL(tx), IAE
    if lv.faceR(tx) then Bare art, x, lv.top(tx) * TS - cy, lv.faceR(tx), IAE + 4
end sub

' The things in view: back, the scenery behind the ground; else the rest.
sub DrawThings (lv as Level, zn as Realm, art as Store, se as Session, cx as integer, cy as integer, back as integer)
    dim i as integer, c as integer, x as integer, y as integer, k as integer, f as integer, a as single
    c = (cx - WAVEL) \ TS
    if c < 0 then c = 0
    for i = lv.first(c) to lv.nObj - 1
        x = lv.ob(i).x - cx
        if x > SW + 96 then exit for
        k = lv.ob(i).k
        if (k >= KLOOP and k < KFRONT) <> back then continue for
        y = lv.ob(i).y - cy
        select case k
        case KBONE: if lv.ob(i).s then Blit art, x - 5, y - 5, IBONE + ((se.ticks \ 6) and 3) * 2, 10, 10
        case KCAT: if lv.ob(i).s then Blit art, x - 10, y - 13, CatPic%(se, lv.ob(i).s), 20, 14
        case KCRAB: if lv.ob(i).s then Blit art, x - 12, y - 15, CrabPic%(se, lv.ob(i).s), 24, 15
        case KSPRING
            if zn.id = 2 then
                Blit art, x - 15, y - 21, ICHAIR - 2 * (lv.ob(i).s > 0), 30, 22
            else
                Blit art, x - 8, y - 15, ISPRING - 2 * (lv.ob(i).s > 0), 16, 16
            end if
        case KBOOST: Blit art, x - 12, y - 9, IBOOST + ((se.ticks \ 4) and 1) * 2, 24, 10
        case KMARK: Blit art, x - 6, y - 31, IMARK + 2 * lv.ob(i).s, 12, 32
        case KGOAL
            f = 0
            if lv.ob(i).s then f = (se.ticks \ 4) and 3
            Blit art, x - 12, y - 47, IGOAL + f * 2, 24, 48
        case KPLAT: Blit art, x, y, zn.tile + 18, TS, TS
        case KLOOP: Blit art, x - LD \ 2, y - LOOPR - LD \ 2, zn.ring, LD, LD
        case KPIPE
            if zn.decked then
                Blit art, x - PIPER, y - PIPER, zn.pipe, PIPER, PIPER
            else
                k = IAP + (((lv.ob(i).x \ TS + 1) and 3) * 3 + (lv.ob(i).y \ TS) mod 3) * 2
                Blit art, x - PIPER, y - PIPER, k, PIPER, PIPER + TS
            end if
        case KSLED: if lv.ob(i).s = 0 then Blit art, x - 15, y - 25, ISLED + 4, 30, 42
        case KTHROW
            if lv.ob(i).s then Blit art, x - 14, y - 25, IFORT - 2 * Risen%(lv, i) - 4 * (lv.ob(i).s < 0), 28, 26
        case KDRIFT: Blit art, x - 28, y - 11, IDRIFT, 56, 12
        case KICICLE
            ' loosed, it trembles
            k = lv.ob(i).s > 0 and lv.ob(i).s <= TREMBLE and (se.ticks and 2) <> 0
            if lv.ob(i).s >= 0 then Blit art, x - 3 - k, y + Fallen%(lv, i), IICICLE, 7, 16
        case KSWEEP
            k = ISWEEP + ((se.ticks \ 6) and 1) * 2 - 4 * (lv.ob(i).s > 0)
            if lv.ob(i).s then Blit art, x - 15, y - 19, k, 30, 20
        case KSPARK: DrawSpark lv, art, se, i, x, y, cx
        case KPRESS: DrawPress lv, art, se, i, x, y
        case KLEG: DrawLeg art, x, y
        case KFLUME: Blit art, x, y - 8, IFLUME + ((se.ticks \ 3) and 3) * 2, TS, 29
        case KSPOUT: DrawSpout lv, art, se, x, y, lv.ob(i).x \ TS, cy
        case KCISTERN: Blit art, x - 30, y - 25, ICISTERN, 30, 30
        case KSEAT: Blit art, x, y - SEATH, ISEAT, MOVEW, SEATH + 8
        case KWALL
            if lv.ob(i).s then
                Blit art, x - 9, y - 81, IWALL, 18, 82
            else
                Blit art, x - 9, y - 21, IWALL + 2, 18, 22
            end if
        case KGOAT: DrawGoat lv, art, se, i, x, y
        case KLOAD: DrawLoad lv, art, se, i, x, y
        case KHEAP: if lv.ob(i).s then Blit art, x - 30, y - 33, IHEAP, 60, 34
        case KSPAN
            ' three planks; tottering, it shakes
            if lv.ob(i).s then y = y + ((se.ticks \ 2) and 1)
            for f = 0 to 2
                Blit art, x + f * TS, y, zn.tile + 18, TS, TS
            next
        case KMOVE
            if zn.decked then
                Blit art, x, y, IMOVE, MOVEW, 12
            else
                Blit art, x, y, IAM, MOVEW, 12
            end if
        case KPOST: Blit art, x - 7, y - 47, IPOST, 16, 48
        case KTANK: Blit art, x - 16, y - 39, ITANK, 32, 40
        case KVENT: Blit art, x - 8, y - 27, IVENT, 16, 28
        case KTREE: Blit art, x - 32, y - 78, ITREE, 64, 80
        case KBUSH: Blit art, x - 20, y - 18, IBUSH, 40, 20
        case KFLOWER: Blit art, x - 6, y - 14, IFLOWER + ((se.ticks \ 30 + i) and 1) * 2, 12, 16
        case KROCK: Blit art, x - 10, y - 10, IROCK, 20, 12
        case KTHORN: Blit art, x - 8, y - 10, ITHORN, 16, 12
        case KFAM
            ' who meets Charlie: they cheer once it has come to them
            DrawFamily art, se, x, y, se.cheer
        case KLAMP: Blit art, x - 10, y - 42, ILAMP, 20, 44
        case KJET
            ' in the pass a geyser
            k = IJET
            if zn.id = 3 then k = IGEYSER
            Blit art, x - 8, y - 6, k, 16, 6
            f = k + 2 + ((se.ticks \ 3) and 1) * 2
            if (se.ticks + lv.ob(i).x) mod 180 < 80 then Blit art, x - 6, y - 70, f, 12, 64
        case KBUMP: Blit art, x - 12, y - 12, IBUMP - 2 * (lv.ob(i).s > 0), 24, 24
        case KBUOY: Blit art, x - 9, y - 9, IBUOY - 2 * (lv.ob(i).s > 0), 18, 18
        case KJELLY: Blit art, x - 8, y - 18 + ((se.ticks \ 20) and 1), IJELLY - 2 * (lv.ob(i).s > 1), 16, 18
        case KSPIN
            line (x - 1, y - SPINH)-(x, y - 1), CINK, BF
            a = se.ticks * .06 + lv.ob(i).x
            for f = 1 to 3
                pset (x + cint(SPINR * f / 4 * cos(a)), y - SPINH + cint(SPINR * f / 4 * sin(a))), CWHITE
            next
            Blit art, x - 4, y - SPINH - 4, ISPIN + 2, 8, 8
            Blit art, x + cint(SPINR * cos(a)) - 8, y - SPINH + cint(SPINR * sin(a)) - 8, ISPIN, 16, 16
        case KBUTTON: Blit art, x - 8, y - 8, IBTN + 2 * lv.ob(i).s, 16, 8
        case KWAVE
            ' a twist's far half; its near one is drawn over Charlie
            Blit art, x, y - WAVEA - 22, zn.wave, WAVEL, WAVEH
        case KHOOP: Blit art, x - 6, y - 16, IHOOP - 2 * (lv.ob(i).s > 0), 12, 32
        end select
    next
end sub

' Spark i, its carriage at x, y on the screen: its rail, a tile's length from its home either way and between;
' the carriage dark, or alight.
sub DrawSpark (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer, cx as integer)
    dim f as integer, k as integer
    for f = -3 to 2
        Blit art, lv.ob(i).a + f * TS - cx, y - 4, ISRAIL, TS, 4
    next
    k = ISPARK
    if Lit%(lv, se, i) then k = ISPARK + 2 + ((se.ticks \ 3) and 1) * 2
    Blit art, x - 11, y - 24, k, 22, 22
end sub

' Press i at x, the floor at y on the screen: its length of girder, its rod down to its head, its housing.
sub DrawPress (lv as Level, art as Store, se as Session, i as integer, x as integer, y as integer)
    dim yy as integer, hy as integer
    Blit art, x - 32, y - PRESSH - 8, IRAIL, 32, 8
    Blit art, x, y - PRESSH - 8, IRAIL, 32, 8
    hy = y - PRESSUP + Stroke%(lv, se, i) - 15
    for yy = hy - 8 to y - PRESSH + 12 step -8
        Blit art, x - 4, yy, IPRESS + 2, 8, 8
    next
    Blit art, x - 18, y - PRESSH, IPRESS, 36, 21
    Blit art, x - 19, hy, IPRESS + 4, 38, 16
end sub

' A leg of a gantry, its left at x, the floor at y on the screen.
sub DrawLeg (art as Store, x as integer, y as integer)
    dim yy as integer
    for yy = y - 8 to y - PRESSH step -8
        Blit art, x, yy, IPRESS + 2, 8, 8
    next
end sub

' A flume's end at x, y on the screen, over column c: the water falling from it to the sea, or to the floor of a
' dry pool, then its trough.
sub DrawSpout (lv as Level, art as Store, se as Session, x as integer, y as integer, c as integer, cy as integer)
    dim e as integer, yy as integer, k as integer
    k = ((se.ticks \ 3) and 3) * 2
    e = lv.top(c + 1) * TS
    if lv.tide <> NONE and lv.tide < e then e = lv.tide + 2
    e = e - cy
    if e > y then
        Blit art, x + TS - 1, y - 8, IRUNNEL + k, 5, 8
        for yy = e - 8 to y step -8
            Blit art, x + TS - 1, yy, IRUNNEL + k, 5, 8
        next
    end if
    Blit art, x, y - 8, ISPOUT + k, TS, 13
end sub

' The cables, their posts, and what hangs from them: a basket by its two lines, a chair by its hanger and grip.
sub DrawCables (lv as Level, cx as integer, cy as integer)
    dim m as integer, x as integer, y as integer, x1 as integer, bx as integer, by as integer, i as integer
    dim d as integer
    for m = 0 to lv.nMv - 1
        if lv.mv(m).k <> 3 and lv.mv(m).k <> 7 then continue for
        x = lv.mv(m).e - 8 - cx
        y = lv.mv(m).y - CABLEH - cy
        x1 = x + lv.mv(m).a + MOVEW
        d = Dip%(lv, m)
        if x >= SW or x1 < 0 then continue for
        line (x, y)-(x1, y + d), CINK
        line (x, y + 1)-(x1, y + d + 1), CYELLOW
        line (x - 1, y - 2)-(x, y + CABLEH), CINK, BF
        line (x1, y + d - 2)-(x1 + 1, y + d + CABLEH), CINK, BF
        bx = lv.ob(lv.mv(m).o).x - cx
        by = lv.ob(lv.mv(m).o).y - cy
        if lv.mv(m).k = 3 then
            for i = 4 to MOVEW - 4 step MOVEW - 8
                line (bx + i, by)-(bx + i, y + 1 + clng(bx + i - x) * d \ (x1 - x)), CINK
            next
        else
            bx = bx + MOVEW \ 2
            i = y + clng(bx - x) * d \ (x1 - x)
            line (bx - 1, i)-(bx, by - SEATH), CINK, BF
            line (bx - 3, i - 1)-(bx + 2, i + 2), CINK, BF
        end if
    next
end sub

sub DrawLoose (art as Store, se as Session, cx as integer, cy as integer, lo() as Loose)
    dim i as integer, k as integer
    k = IBONE + ((se.ticks \ 6) and 3) * 2
    for i = 0 to NLOOSE - 1
        if lo(i).t <= 1 and (lo(i).t <= 0 or (se.ticks and 4) = 0) then continue for
        Blit art, cint(lo(i).x) - 5 - cx, cint(lo(i).y) - 5 - cy, k, 10, 10
    next
end sub

' Charlie, and streaks of wind behind it at speed; running on a slope, the
' frame turned to it.
' What is nearer than Charlie: the near half of each ribbon's twist, which hides what runs behind it.
sub DrawOver (lv as Level, zn as Realm, art as Store, cx as integer, cy as integer)
    dim i as integer, c as integer, x as integer
    c = (cx - WAVEL) \ TS
    if c < 0 then c = 0
    i = lv.first(c)
    do while i < lv.nObj
        x = lv.ob(i).x - cx
        if x > SW then exit do
        if lv.ob(i).k = KWAVE2 then Blit art, x, lv.ob(i).y - cy - WAVEA - 22, zn.wave + 2, WAVEL, WAVEH
        i = i + 1
    loop
end sub

' A cat's picture by its state: startled, turned to what barked, or a stride of its prowl the way it goes.
function CatPic% (se as Session, s as integer)
    if abs(s) > 1 then
        CatPic% = ICATF - 2 * (s > 0)
    else
        CatPic% = ICAT + ((se.ticks \ 8) and 1) * 2 - 4 * (s < 0)
    end if
end function

' A crab's picture by its state: in its shell, or a frame of its scuttle.
function CrabPic% (se as Session, s as integer)
    if abs(s) > 1 then
        CrabPic% = ICRAB + 4
    else
        CrabPic% = ICRAB + ((se.ticks \ 8) and 1) * 2
    end if
end function

' The bark, seen: two arcs before his muzzle, wider as it goes.
sub DrawBark (p as Hero, cx as integer, cy as integer)
    dim x as integer, y as integer, r as integer, a as single
    x = cint(p.x) - cx + (1 - 2 * p.face) * 12
    y = cint(p.y) - cy - 18
    r = 5 + cint((BARKT - p.barkT) * 50)
    ' a quarter turn about the way he faces: to the right through 0, to the left about PI
    a = PI * (7 / 4 - p.face)
    circle (x, y), r, CWHITE, a, a + PI / 2 - 2 * PI * (1 - p.face)
    circle (x, y), r + 4, CWHITE, a, a + PI / 2 - 2 * PI * (1 - p.face)
end sub

sub DrawHero (gr as Ground, art as Store, se as Session, p as Hero, cx as integer, cy as integer)
    dim x as integer, y as integer, f as integer, t as integer, tw as integer
    dim hu as single, hv as single, a as single, v as single
    if p.inv > 0 and not p.stun and (se.ticks and 4) then exit sub
    x = cint(p.x) - cx
    y = cint(p.y) - cy
    Footing gr, p, hu, hv, a, tw
    v = p.vx
    if p.ground then v = p.gsp
    if abs(v) > 400 and not p.dead then SpeedLines se, x, y - cint(14 * hu), sgn(v)
    f = HeroFrame%(se, p)
    ' on a sled: it lies as the ground does, and he stands on it
    if p.sled then
        Blit art, x - 15, y - 25, ISLED + (Slant%(p) + 2) * 2, 30, 42
        x = x + cint(SLEDH * hv)
        y = y - cint(SLEDH * hu)
    end if
    ' astride a goat: it gallops as the ground lies, and he is GOATH up its back
    if p.goat then
        Blit art, x - 17, y - 40, IGOATR + ((Slant%(p) + 2) * 2 + ((se.ticks \ 6) and 1)) * 4, 34, 58
        x = x + cint(GOATH * hv)
        y = y - cint(GOATH * hu)
    end if
    if tw and f >= FRUN and f < FBALL then
        DrawTwisted art, p, x, y, f, a
        exit sub
    end if
    if p.ground and f >= FRUN and f < FBALL then t = Turned%(p)
    if t = 0 then
        DrawUpright art, se, p, x, y, f, hu, hv
        exit sub
    end if
    DrawTilted art, p, x, y, f, t, hu, hv
end sub

' The ground's lie under him, hu and hv; round a ribbon's twist he keeps to the inside, over the line then under
' it: a the turn he is at, tw set.
sub Footing (gr as Ground, p as Hero, hu as single, hv as single, a as single, tw as integer)
    hu = 1
    if not p.ground then exit sub
    hu = gr.pt(p.sg).u
    hv = gr.pt(p.sg).v
    if Twist!(gr, p) < 0 then exit sub
    a = Twist!(gr, p)
    hu = cos(a)
    hv = 0
    tw = -1
end sub

' The air he cuts at speed: four lines behind him, d the way he goes, about height y.
sub SpeedLines (se as Session, x as integer, y as integer, d as integer)
    dim j as integer, sy as integer, n as integer
    for j = 0 to 3
        sy = y - 12 + (se.ticks * 7 + j * 11) mod 22
        n = 12 + (se.ticks * 5 + j * 9) mod 18
        line (x - d * 14, sy)-(x - d * (14 + n), sy), CWHITE
    next
end sub

' Running round a twist: rolled with the ribbon, a twelfth of a turn a picture, its paws' line 12 off its middle.
sub DrawTwisted (art as Store, p as Hero, x as integer, y as integer, f as integer, a as single)
    dim t as integer, j as integer
    t = cint(a * 6 / PI) mod 12
    j = (f - FRUN) \ 3
    if f >= FDASH then j = f - FDASH
    Blit art, x - 16, y - 16 - cint(12 * cos(t * PI / 6)), ITW + (p.face * 24 + t * 2 + j) * 2, 32, 32
end sub

' How far he is turned to the ground he runs on: sixteenths of a turn, nose up.
function Turned% (p as Hero)
    dim v as single, t as integer
    v = p.ang
    if p.face then v = -v
    t = cint(v * 8 / PI)
    if t < 0 then t = t + 16
    Turned% = t and 15
end function

' Not turned: as drawn. Curled on the ground, his middle is 13 off it, whichever way that is; in a snowball none
' of him shows; afraid, he shakes.
sub DrawUpright (art as Store, se as Session, p as Hero, x as integer, y as integer, f as integer, hu as single, hv as single)
    dim curled as integer, bx as integer, by as integer
    bx = x
    by = y
    curled = f >= FBALL and f < FBALL + 4
    if p.ground and curled then
        bx = bx + cint(13 * hv)
        by = by + 13 - cint(13 * hu)
    end if
    if p.snow < 0 and curled then
        Blit art, bx - 14, by - 28, ISNOWBALL + (int(p.anim) and 1) * 2, 28, 28
        exit sub
    end if
    if f = FHURT or f = FSHIVER then bx = bx + ((se.ticks \ 2) and 1) * 2 - 1
    Blit art, bx - 16, by - 31, IHERO + (p.face * NF + f) * 2, 32, 32
end sub

' Turned about his middle, 12 off the ground, t sixteenths: the picture as drawn (0) or mirrored to face left (1);
' past half a turn the one half a turn back upside down, mirrored (3) or not (2).
sub DrawTilted (art as Store, p as Hero, x as integer, y as integer, f as integer, t as integer, hu as single, hv as single)
    dim j as integer, k as integer, q as integer, bx as integer, by as integer
    k = (f - FRUN) \ 2
    if f >= FDASH then k = 3 + f - FDASH
    j = p.face
    q = t
    if q >= 8 then
        q = q - 8
        j = 3 - j
    end if
    bx = x + cint(12 * hv) - 16 + (j and 1)
    by = y - cint(12 * hu) - 18 - 5 * (j > 1)
    Blit art, bx, by, ITILT + (j * 40 + q * 5 + k) * 2, 32, 32
end sub

function HeroFrame% (se as Session, p as Hero)
    HeroFrame% = FRUN + int(p.anim) mod 6
    if p.dead or p.stun then
        HeroFrame% = FHURT
        if (se.ticks \ 3) and 1 then HeroFrame% = FSHIVER
    elseif p.sled or p.goat then
        HeroFrame% = FDASH
    elseif p.roll or p.charging then
        HeroFrame% = FBALL + (int(p.anim) and 3)
    elseif p.swim then
        HeroFrame% = FRUN + (se.ticks \ 10) mod 6
    elseif not p.ground then
        if p.sprung then HeroFrame% = FUP
    elseif p.skid then
        HeroFrame% = FSKID
    elseif p.crouch then
        HeroFrame% = FCROUCH
    elseif p.win then
        HeroFrame% = FWIN + ((se.ticks \ 20) and 1)
    elseif abs(p.gsp) < 5 then
        HeroFrame% = Standing%(se, p)
    elseif abs(p.gsp) >= TOP - 5 then
        HeroFrame% = FDASH + (int(p.anim) and 1)
    end if
end function

' Stood still: a blink now and then, and after a while a tap of the paw.
function Standing% (se as Session, p as Hero)
    Standing% = FIDLE
    if se.ticks mod 180 < 8 then Standing% = FBLINK
    if p.idleT > 3 then Standing% = FTAP + ((se.ticks \ 12) and 1)
end function

sub DrawParts (pt() as Part, cx as integer, cy as integer)
    dim i as integer, x as integer, y as integer
    for i = 0 to NPART - 1
        if pt(i).t <= 0 then continue for
        x = cint(pt(i).x) - cx
        y = cint(pt(i).y) - cy
        if x < 0 or x >= SW - 1 or y < 0 or y >= SH - 1 then continue for
        pset (x, y), pt(i).c
        if pt(i).t > .25 then pset (x + 1, y), pt(i).c
    next
end sub

' The score, time, bones and lives, top left over the scene: a picture
' and its mask, put together from the characters when a number moves.
' Called before the frame is drawn: it draws on the page's corner.
sub Hud (art as Store, g as Game)
    dim s as integer, sig as long
    s = int(g.clock)
    sig = g.score * 7 + g.bones * 131& + g.lives * 100003 + s * 13& + 1
    if sig = g.sig then exit sub
    g.sig = sig
    line (0, 0)-(HUDW - 1, HUDH - 1), 0, BF
    line (0, 100)-(HUDW - 1, 100 + HUDH - 1), 255, BF
    HudLine art, 0, "SCORE " + Num$(g.score, 6), 6
    HudLine art, 1, "TIME  " + ltrim$(str$(s \ 60)) + ":" + Num$(clng(s mod 60), 2), 6
    HudLine art, 2, "BONES " + Num$(clng(g.bones), 3), 6
    HudLine art, 3, "CHARLIE x" + ltrim$(str$(g.lives)), 7
    get (0, 0)-(HUDW - 1, HUDH - 1), art.img(art.at(IHUD))
    get (0, 100)-(HUDW - 1, 100 + HUDH - 1), art.img(art.at(IHUD + 1))
end sub

' A row of the HUD's picture and mask: s, its first n characters yellow.
sub HudLine (art as Store, row as integer, s as string, n as integer)
    dim i as integer, k as integer
    for i = 1 to len(s)
        k = instr("0123456789:xABCDEHILMNOPRST", mid$(s, i, 1)) - 1
        if k < 0 then continue for
        k = IGLYPH + (k - NGLYPH * (i > n)) * 2
        put (i * 9 - 9, row * 10), art.img(art.at(k)), pset
        put (i * 9 - 9, 100 + row * 10), art.img(art.at(k + 1)), pset
    next
end sub

' What Charlie says, in a box over its head.
sub Say (p as Hero, cx as integer, cy as integer, s as string)
    dim row as integer, col as integer
    col = (cint(p.x) - cx) \ 8 - len(s) \ 2 + 1
    row = (cint(p.y) - cy - 42) \ 8
    if col < 2 then col = 2
    if col > 39 - len(s) then col = 39 - len(s)
    if row < 3 then row = 3
    if row > 23 then row = 23
    Panel col * 8 - 12, row * 8 - 12, (col + len(s)) * 8 - 5, row * 8 + 3
    Txt row, col, s, CWHITE
end sub

' A dark box with a gold edge, for words: every other pixel of it, the scene through the rest.
sub Panel (x1 as integer, y1 as integer, x2 as integer, y2 as integer)
    dim y as integer
    for y = y1 to y2
        if (x1 + y) and 1 then
            line (x1, y)-(x2, y), CHUD, , &H5555
        else
            line (x1, y)-(x2, y), CHUD, , &HAAAA
        end if
    next
    line (x1, y1)-(x2, y2), CGOLD + 2, B
end sub

sub Txt (row as integer, col as integer, s as string, c as integer)
    color c
    locate row, col
    print s;
end sub

' s centred on row.
sub Banner (row as integer, s as string, c as integer)
    Txt row, 21 - len(s) \ 2, s, c
end sub

' v in w digits, 0s first.
function Num$ (v as long, w as integer)
    Num$ = right$(string$(w, "0") + ltrim$(str$(v)), w)
end function
