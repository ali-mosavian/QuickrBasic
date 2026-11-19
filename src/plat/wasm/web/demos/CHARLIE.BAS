' CHARLIE BARKLEY: Charlie the Maltese tears through the Amber Downs.
' Arrows run. Space, Z or Up jumps, curled. Down while running rolls;
' Down while still crouches: jump to wind up, let Down go to launch.
' M mutes, N turns the tune off; Esc leaves. SCREEN 13 on two pages: one drawn, one shown.
' Held keys come from the keyboard's port (60h); where it reads FFh,
' from INKEY$'s repeats. Physics in pixels and seconds, 60 steps a
' second; the pictures are DATA, drawn by tools/charlieart.py.
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

type Part
    x as single
    y as single
    vx as single
    vy as single
    t as single
    c as integer
end type

' The world: MW by MH tiles of TS pixels; LW columns used. A tile's kind
' is its profile, Prof: each column's solid height from its bottom.
const SW = 320, SH = 200, TS = 16, MW = 800, MH = 30, NONE = 30000
const STEPT = 1! / 60!, PI = 3.141593
const GRAV = 750!, JUMPV = 330!, JUMPCUT = 160!, TOP = 360!
const RUNACC = 280!, DEC = 1800!, FRIC = 280!, AIRACC = 500!, SLOPE = 380!
const RFRIC = 60!, RDEC = 450!, RUPS = 300!, RDNS = 1100!, MAXV = 900!
const SPRINGV = 560!, CHARGE0 = 480!, CHARGEMAX = 360!, BOUNCEV = 330!
const BOOSTV = 660!, LOOPR = 72, LD = 188, PIPER = 48
' A jump asked for this long before landing is made on it; one this long after running off an edge still is.
const JBUFFER = .1, COYOTE = .1
' The ground's line: its most points, a loop's and a quarter pipe's stretches, the curves there can be; under STALL
' Charlie cannot stay on a wall or a roof.
const NPTS = 1600, LOOPN = 48, PIPEN = 12, NZONE = 47, STALL = 150!
' The moving platforms there can be, and their width.
const NMOVE = 23, MOVEW = 48
const HW = 7, HH = 24, STRIPH = 192, MIDH = 80
const MAXOBJ = 600, NLOOSE = 24, NPART = 48
const TITLE = 0, PLAYING = 1, CLEARED = 2, OVER = 3
const FIDLE = 0, FBLINK = 1, FTAP = 2, FRUN = 4, FDASH = 10, FBALL = 12
const FSKID = 16, FCROUCH = 17, FHURT = 18, FUP = 19, FWIN = 20, FSHIVER = 22, NF = 23

' What stands in the level, by x. From KLOOP on: scenery, drawn behind
' the ground.
const KBONE = 1, KCAT = 2, KSPRING = 3, KMARK = 4, KBOOST = 5, KGOAL = 6, KPLAT = 7
const KLOOP = 8, KTREE = 9, KBUSH = 10, KFLOWER = 11, KROCK = 12, KTHORN = 13, KLAMP = 14, KFAM = 15, KPIPE = 16
const KMOVE = 17, KPOST = 18, KTANK = 19, KVENT = 20, KJET = 21, KBUMP = 22, KSPIN = 23, KHOOP = 24
const CABLED = 64, CABLEH = 44, KBUTTON = 27
const KWAVE = 25, KWAVE2 = 26, WAVEA = 96, WAVEH = WAVEA + 36, WAVEL = 192
const JETV = 500!, BUMPV = 400!, HOOPV = 640!, SPINR = 30, SPINH = 70, PITD = 5

' Images in Img from At(k); a masked one's mask at k + 1. As charlieart.py's.
const IBG = 0, IMID = 1, ILOOP = 5, ITITLE = 7
const ITILE = 10, IHERO = 34, IBONE = IHERO + NF * 4
const ICAT = IBONE + 8, ISPRING = ICAT + 8, IBOOST = ISPRING + 4, IMARK = IBOOST + 4
const IGOAL = IMARK + 4, ITREE = IGOAL + 8, IBUSH = ITREE + 2, IFLOWER = IBUSH + 2
const IROCK = IFLOWER + 4, ITHORN = IROCK + 2, ILAMP = ITHORN + 2, ITILT = ILAMP + 2
const IFAM = ITILT + 320, IDASH = IFAM + 4
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
const ITW = IZU + 48, ICATF = ITW + 96, NIMG = ICATF + 4
const HUDW = 108, HUDH = 40
' The levels: the two zones, then each again at another hour, in other colours.
const NSTAGE = 4
const IMGSZ = 420000

const SCLEFT = 75, SCRIGHT = 77, SCUP = 72, SCDOWN = 80, SCSPACE = 57, SCZ = 44
const SJUMP = 0, SBONE = 1, SHURT = 2, SSPRING = 3, SPOP = 4, SROLL = 5
const SCHARGE = 6, SLAUNCH = 7, SCLEAR = 8, SDIE = 9, SMARK = 10, SBOOST = 11, SHISS = 12, SBARK = 13
' A bark: how far it carries, the seconds to the next, and the steps a cat stands startled by it.
const BARKR = 72, BARKT = .4, FLINCH = 24

' The palette's ramps, dark to light, as charlieart.py's.
const CHUD = 0, CWHITE = 1, CINK = 2, CYELLOW = 3, CRED = 4
const CSCARF = 21, CGRASS = 25, CEARTH = 30, CSTONE = 38, CGOLD = 42
const CWATER = 144
const CCHASE = 213

declare sub Main ()
declare sub Tint (k as integer, c as long)
declare sub Shade ()
declare function Met% (g as Game, p as Hero, k as Pad, pt() as Part)
declare sub OpenScreen ()
declare sub Flip (page as integer)
declare sub LoadTables ()
declare sub LoadLevel (z as integer)
declare sub AddMover (x as integer, y as integer, a as integer, k as integer, e as integer)
declare sub Movers (p as Hero)
declare function Swing! (m as integer, quarter as integer)
declare sub GoStage (n as integer, g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub Column (c as integer, g as integer, kind as integer)
declare sub AddObj (k as integer, x as integer, y as integer)
declare sub SortObjs ()
declare sub BuildArt ()
declare sub LoadArt ()
declare sub Unpack ()
declare function Word& ()
declare function Chars$ ()
declare sub OpenData ()
declare sub Mirror (k as integer, k2 as integer, w as integer, h as integer)
declare sub Flop (k as integer, k2 as integer, w as integer, h as integer)
declare sub Track ()
declare sub Faces ()
declare function Side% (c as integer, n as integer)
declare sub Bare (x as integer, y0 as integer, n as integer, k as integer)
declare sub DrawCables (cx as integer, cy as integer)
declare sub Cycle ()
declare function Ribbon% (x as integer)
declare function Twist! (p as Hero)
declare sub DrawOver (cx as integer, cy as integer)
declare sub Bark (p as Hero)
declare sub Flinch (i as integer, fromX as integer)
declare sub Prowl (i as integer)
declare sub DrawBark (p as Hero, cx as integer, cy as integer)
declare function CatPic% (s as integer)
declare sub Pit (c as integer, g as integer, n as integer)
declare sub Knot (x as single, y as single, k as integer)
declare function SegAt% (x as integer, y as single)
declare sub Along (p as Hero, d as single)
declare sub Drop (p as Hero)
declare sub Settle (p as Hero, s as integer, d as integer)
declare function Curve% (p as Hero, x0 as single, y0 as single, d as integer)
declare sub Grab (x1 as integer, y1 as integer, x2 as integer, y2 as integer, k as integer)
declare sub MaskGrab (x as integer, y as integer, w as integer, h as integer, mx as integer, my as integer, k as integer)
declare sub Blit (x as integer, y as integer, k as integer, w as integer, h as integer)
declare sub Poll ()
declare sub ReadPad (k as Pad)
declare function Held2% (s as integer, t as single)
declare sub Pace (d as single, c as Clock)
declare function Steps% (c as Clock)
declare function Elapsed! (a as single, b as single)
declare sub Sfx (id as integer)
declare sub LoadSongs ()
declare sub LoadSong (s as integer, id as integer)
declare sub ReadKit (s as integer)
declare sub ReadList (a() as string)
declare sub ReadBar (s as integer, n as integer, bas() as string, hits() as string, pat() as string)
declare sub FeedSong ()
declare sub NextHalf ()
declare sub StartSong ()
declare sub PickSong (n as integer, late as integer)
declare function SongFor% (g as Game)
declare function Vol$ (level as integer, loud as integer, up as integer)
declare function Beat$ (s as integer, x as string, loud as integer)
declare function Riff$ (p as string, h as integer, low as integer)
declare function Surf% (sx as integer, y as integer)
declare function Solid% (x as integer, y as integer)
declare function Wall% (p as Hero, dir as integer)
declare function Land% (p as Hero, d as integer)
declare sub Tick (g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
declare sub Pilot (p as Hero, k as Pad)
declare sub ResetWorld (g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub StartGame (g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub Spawn (g as Game, p as Hero)
declare sub MoveHero (p as Hero, k as Pad, pt() as Part)
declare sub Walk (p as Hero, l as integer, r as integer, d as integer, hit as integer, pt() as Part)
declare sub Airborne (p as Hero, l as integer, r as integer, d as integer, hold as integer, pt() as Part)
declare sub Slow (v as single, f as single)
declare sub Touch (g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub Hurt (g as Game, p as Hero, fromX as integer, lo() as Loose, snd as integer)
declare sub Lose (p as Hero)
declare sub Dying (g as Game, p as Hero)
declare sub Things (g as Game, lo() as Loose, pt() as Part)
declare sub Emit (pt() as Part, x as single, y as single, vx as single, vy as single, t as single, c as integer)
declare sub Burst (pt() as Part, x as single, y as single, n as integer, v as single, c as integer)
declare sub Camera (g as Game, p as Hero, snap as integer)
declare sub Render (g as Game, p as Hero, lo() as Loose, pt() as Part)
declare sub DrawBack (cx as integer, cy as integer)
declare sub DrawTiles (cx as integer, cy as integer)
declare sub DrawThings (cx as integer, cy as integer, back as integer)
declare sub DrawLoose (cx as integer, cy as integer, lo() as Loose)
declare sub DrawHero (p as Hero, cx as integer, cy as integer)
declare function HeroFrame% (p as Hero)
declare sub DrawParts (pt() as Part, cx as integer, cy as integer)
declare sub Hud (g as Game)
declare sub HudLine (row as integer, s as string, n as integer)
declare sub BuildGlyphs ()
declare sub Scatter (c as integer, s as integer)
declare sub Say (p as Hero, cx as integer, cy as integer, s as string)
declare sub Panel (x1 as integer, y1 as integer, x2 as integer, y2 as integer)
declare sub Txt (row as integer, col as integer, s as string, c as integer)
declare sub Banner (row as integer, s as string, c as integer)
declare function Num$ (v as long, w as integer)

dim shared Map(MW - 1, MH - 1) as integer, Prof(7, TS - 1) as integer
dim shared ColTop(MW) as integer, ColVis(MW) as integer, FaceL(MW) as integer, FaceR(MW) as integer
' The ground's line, what Charlie runs on: points GX, GY, GN of them; from each to the next a stretch of kind GK (0
' none, 1 the tiles' top, 2 a curve), GL long, GU, GV a pixel along it, its rise GA. ColSeg: a column's stretch;
' PlatA: the platforms' first. A curve z: stretches ZA to ZB, from ZM on a loop's far half, between x ZL and ZR.
dim shared GX(NPTS) as single, GY(NPTS) as single, GU(NPTS) as single, GV(NPTS) as single, GL(NPTS) as single
dim shared GA(NPTS) as single, GK(NPTS) as integer, GN as integer, ColSeg(MW) as integer, PlatA as integer
dim shared ZA(NZONE) as integer, ZB(NZONE) as integer, ZM(NZONE) as integer, ZL(NZONE) as integer
dim shared ZR(NZONE) as integer, NZ as integer
' The zone played, 0 the Amber Downs, 1 the Chrome Works, and the one ResetWorld is to load; its pictures: tiles,
' strips, backdrop, loop, pipe, card.
' The shared colours as built, and each zone's own over them: how many, which, what.
dim shared Pal0(255) as long, ZPN(NSTAGE - 1) as integer, ZPK(NSTAGE - 1, 159) as integer, ZPC(NSTAGE - 1, 159) as long
dim shared Stage as integer
dim shared ZWave as integer, MvB(NMOVE) as integer
dim shared Zone as integer, NextUp as integer, ZTile as integer, ZBack as integer
dim shared ZLoop as integer, ZPipe as integer, ZCard as integer
' A moving platform m: its thing MvO, its stretch MvS and curve MvZ; its middle MvX, MvY and reach MvA; MvK 0 to
' and fro, 1 up and down, 2 a lift, MvT how far up it is; MvE the edge before it.
dim shared MvO(NMOVE) as integer, MvS(NMOVE) as integer, MvZ(NMOVE) as integer, MvX(NMOVE) as integer
dim shared MvY(NMOVE) as integer, MvA(NMOVE) as integer, MvK(NMOVE) as integer, MvE(NMOVE) as integer
dim shared MvT(NMOVE) as single, NMv as integer
dim shared Img(IMGSZ) as long, At(NIMG) as long, NextAt as long
dim shared ObX(MAXOBJ - 1) as integer, ObY(MAXOBJ - 1) as integer, ObK(MAXOBJ - 1) as integer
dim shared ObS(MAXOBJ - 1) as integer, ObA(MAXOBJ - 1) as integer
dim shared First(MW) as integer, NObj as integer
dim shared Held(127) as integer, KeyT(127) as single, LastScan as integer, PortOK as integer
dim shared LW as integer, LWPX as integer, GoalX as integer, FamX as integer, Cheer as integer
dim shared StartX as integer, StartY as integer
dim shared Tune as integer, BarkAt as long
dim shared Ticks as long, Muted as integer, LooseN as integer, PartN as integer
dim shared Glint(2) as long
' The colours as loaded, Dusk sixteenths of them shown; MetT: the seconds Charlie has been with its family.
dim shared Pal(255) as long, Dusk as integer, MetT as single, PastT as single
' The songs held at once, each in its slot: the zone's tune, Charlie's theme, the level's end. A song's most bars,
' voicings and drums.
const NSLOT = 3, ZTUNE = 0, THEME = 1, CAUGHT = 2
const NBAR = 30, NVOICE = 4, NDRUM = 8
' A slot's half bars as PLAY strings: five tracks each (drums, bass, first chord voice, lead, second), the drums'
' letters, the first voice an octave down, and the third voice.
dim shared Trk(NSLOT * NBAR * 10) as string, Kit(NSLOT * NBAR * 2) as string
dim shared Org(NSLOT * NBAR * 2) as string, Third(NSLOT * NBAR * 2) as string
dim shared Order(NSLOT - 1) as string, Voicing(NSLOT - 1) as string, Tempo(NSLOT - 1) as integer
dim shared BassIn(NSLOT - 1) as integer, Lvl(NSLOT - 1, 4) as integer, Fade(NSLOT - 1) as integer
dim shared Entry(NSLOT - 1) as integer, Ends(NSLOT - 1) as integer
' A slot's kit: the sounds of its lead, bass, low double and third voice; by voicing, its letter's place in VoLet,
' the two chord voices' sounds and whether the double plays; by drum, its letter's place in DrLet, its sound, how
' loud, what a fade rounds it up by, and its note.
dim shared LeadP(NSLOT - 1) as string, BassP(NSLOT - 1) as string, UnderP(NSLOT - 1) as string
dim shared MidP(NSLOT - 1) as string, VoLet(NSLOT - 1) as string, DrLet(NSLOT - 1) as string
dim shared VoOne(NSLOT - 1, NVOICE - 1) as string, VoTwo(NSLOT - 1, NVOICE - 1) as string
dim shared VoUnder(NSLOT - 1, NVOICE - 1) as integer
dim shared DrPatch(NSLOT - 1, NDRUM - 1) as string, DrNote(NSLOT - 1, NDRUM - 1) as string
dim shared DrVol(NSLOT - 1, NDRUM - 1) as integer, DrUp(NSLOT - 1, NDRUM - 1) as integer
' The slot playing, its bar and half; Ended: it has ended and nothing plays.
dim shared Sng as integer, Bar as integer, HalfBar as integer, Ended as integer
dim shared Coming as integer, ComingBar as integer, FadeOut as integer, FadeIn as integer

Main
end

' The Amber Downs, left to right: a command and its count. g, the ground's
' height in tiles, starts at 10. plat and hb: height * 100 + length.
Level:
data flat,4, dog,0, flat,4, flower,0, flat,4, bones,6, flat,8, tree,0, flat,4
data upg,2, flat,3, cat,0, flat,6, bush,0, dng,3, boost,0, bones,8, flat,10
data loop,0, flat,8, bones,6, flat,6, up45,2, flat,2, arc,7, gap,7, flat,8
data mark,0, flat,4, dn45,4, dng,4, boost,0, bones,10, flat,8, loop,0, flat,6, boost,0, flat,6, loop,0, flat,8
data tree,0, flat,4, boost,0, flat,6, pipe,6, flat,6, bones,6, flat,22
data boost,0, flat,4, wave,1, flat,8
data dng,5, boost,0, bones,10, flat,10, up45,3, arc,11, pit,12, flat,10
data cat,0, flat,6, bush,0, flat,4, spring,0, flat,2, plat,608, hb,706, flat,16, spin,0, flat,8
data upg,4, flat,4, flower,0, flat,4
data mark,0, flat,4, cable,22, flat,8, dn45,2, dng,2, boost,0, flat,5, loop,0, flat,6, boost,0, bones,8, flat,6, loop,0
data flat,6, boost,0, flat,6, loop,0, flat,10, up45,4, hoop,1, arc,10, pit,12, flat,10
data boost,0, flat,6, pipe,7, flat,6, jet,0, flat,3, bump,5, flat,3, bump,6, flat,3, bump,5, flat,6
data dng,5, boost,0, bones,10, flat,12, loop,0, flat,8, up45,3, arc,10, pit,11, flat,8
data flower,0, upg,3, flat,4, cat,0, dng,3, flat,6, tree,0, flat,4, button,0, flat,8, bridge,12, flat,6
data mark,0, flat,4, dn45,3, dng,5, boost,0, bones,8, flat,8, loop,0, flat,6, boost,0, flat,6, loop,0, flat,8
data thorn,0, flat,6, rock,0, flat,4, up45,3, arc,8, pit,9, flat,8
data dng,3, boost,0, bones,10, flat,10, loop,0, flat,7, boost,0, flat,6, up45,4, hoop,1, arc,12, pit,14
data flat,22, spin,0, flat,8, flower,0, flat,4
data goal,0, flat,9, fam,0, flat,6, bush,0, flat,5, "end",0

' pit: a jump's gap with a floor under it and a spring out; the platforms' gaps have one too.
' button, then bridge: a pit that wide, bridged once the button before it is trodden.
' cable: a basket down a cable over a pit that wide, the ground past it four lower.
' The Chrome Works: mover: a gap that wide with a platform to and fro over it. hover: a gap of 4 with one up and
' down, the ground past it that much higher. lift: a gap of 3 with a lift, the ground past it that much higher.
Level2:
data flat,4, dog,0, flat,8, bones,6, flat,8, boost,0, bones,8, flat,10, loop,0, flat,10
data boost,0, flat,4, wave,1, flat,8
data dng,3, boost,0, flat,8, loop,0, flat,6, boost,0, flat,6, loop,0, flat,8, up45,3, arc,9, pit,10, flat,8
data cat,0, flat,12, mover,8, flat,8, jet,0, flat,3, bump,5, flat,3, bump,5, flat,6, button,0, flat,10, bridge,9, flat,6, mark,0, flat,6
data boost,0, flat,6, pipe,6, flat,8, bones,8, flat,6, dng,4, boost,0, bones,10, flat,10, loop,0, flat,10
data lift,6, flat,8, spin,0, flat,8, bones,6, flat,6, cable,20, flat,8, dng,3, boost,0, flat,8, loop,0, flat,6, boost,0, flat,6, loop,0
data flat,8, up45,4, hoop,1, arc,11, pit,12, flat,10
data mark,0, flat,6, cat,0, flat,12, hover,5, flat,8, bones,6, flat,6, dng,4, boost,0, bones,10, flat,10, loop,0
data flat,8, drop,4, flat,16, mover,7, flat,8, boost,0, flat,6, pipe,5, flat,8, dng,3, boost,0, flat,8, loop,0
data flat,10, goal,0, flat,9, fam,0, flat,6, flat,5, "end",0

' The Amber Downs at evening.
Level3:
data flat,4, dog,0, flat,6, bones,6, flat,6, boost,0, flat,6, loop,0, flat,6, boost,0, flat,6, loop,0, flat,8
data up45,3, arc,9, pit,10, flat,8, flower,0, flat,4
data dng,4, boost,0, bones,10, flat,10, loop,0, flat,8, boost,0, flat,4, wave,2, flat,8
data cat,0, flat,8, tree,0, flat,6, upg,3, flat,4, button,0, flat,8, bridge,10, flat,6, mark,0, flat,6
data dn45,3, dng,3, boost,0, bones,8, flat,8, loop,0, flat,6, up45,4, hoop,1, arc,11, pit,12, flat,10
data spring,0, flat,2, plat,608, hb,706, flat,16, bush,0, flat,6, cable,22, flat,8
data boost,0, flat,6, pipe,6, flat,6, bones,6, flat,22, boost,0, flat,4, wave,1, flat,8
data dng,5, boost,0, bones,10, flat,12, loop,0, flat,6, boost,0, flat,6, loop,0, flat,8, thorn,0, flat,6, rock,0, flat,4
data jet,0, flat,3, bump,5, flat,3, bump,6, flat,6, mark,0, flat,4, up45,3, arc,10, pit,11, flat,8
data dng,3, boost,0, bones,10, flat,10, loop,0, flat,10, spin,0, flat,8, flower,0, flat,4
data goal,0, flat,9, fam,0, flat,6, bush,0, flat,5, "end",0

' The Chrome Works by night.
Level4:
data flat,4, dog,0, flat,8, bones,6, flat,6, boost,0, flat,6, loop,0, flat,6, boost,0, flat,6, loop,0, flat,16
data mover,7, flat,8, bones,6, flat,6, dng,4, boost,0, bones,10, flat,10, loop,0, flat,8
data up45,3, arc,9, pit,10, flat,8, cat,0, flat,10, hover,5, flat,8, mark,0, flat,6
data boost,0, flat,4, wave,1, flat,8, dng,3, boost,0, flat,8, loop,0, flat,8, button,0, flat,10, bridge,10, flat,8
data jet,0, flat,3, bump,5, flat,3, bump,5, flat,8, lift,6, flat,8, bones,6, flat,6
data dng,4, boost,0, bones,10, flat,10, loop,0, flat,6, boost,0, flat,6, loop,0, flat,8
data up45,4, hoop,1, arc,11, pit,12, flat,10, mark,0, flat,6, cable,20, flat,8
data boost,0, flat,6, pipe,5, flat,8, bones,6, flat,20, drop,4, flat,16, mover,8, flat,8
data dng,3, boost,0, flat,8, loop,0, flat,10, spin,0, flat,8
data goal,0, flat,9, fam,0, flat,6, flat,5, "end",0

' The tunes are in CHARLIE.DAT with the pictures, from tools/charliesong.txt.

' Set up, then read input, step in fixed ticks and draw, till Esc on the
' title.
sub Main
    dim g as Game, p as Hero, k as Pad
    dim lo(NLOOSE - 1) as Loose, pt(NPART - 1) as Part
    dim now as single, last as single, c as Clock, n as integer

    OpenScreen
    LoadTables
    BuildArt
    LoadLevel 0
    LoadSongs
    g.page = 1
    ResetWorld g, p, lo(), pt()

    last = timer
    c.span = STEPT
    do
        ' the next frame on the screen (port 3DAh's retrace; with no display, the next look), the keys read meanwhile
        do
            Poll
        loop until inp(&H3DA) and 8
        now = timer
        Pace Elapsed!(last, now), c
        last = now
        ReadPad k
        ' a picture for each step, none for a frame between two
        n = Steps%(c)
        if n then
            do while n
                Tick g, p, k, lo(), pt()
                n = n - 1
            loop
            PickSong SongFor%(g), g.mode = CLEARED
            FeedSong
            Render g, p, lo(), pt()
            Flip g.page
        end if
    loop until g.quitting

    screen 0
    width 80
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
sub LoadTables
    dim i as integer
    for i = 0 to TS - 1
        Prof(1, i) = TS
        Prof(2, i) = i + 1
        Prof(3, i) = TS - i
        Prof(4, i) = (i + 2) \ 2
        Prof(5, i) = 8 + (i + 2) \ 2
        Prof(6, i) = TS - i \ 2
        Prof(7, i) = 8 - i \ 2
    next
end sub

' Image k upside down as k2.
sub Flop (k as integer, k2 as integer, w as integer, h as integer)
    dim y as integer, strip(40) as long
    put (0, 0), Img(At(k)), pset
    for y = 0 to h - 1
        get (0, y)-(w - 1, y), strip
        put (100, h - 1 - y), strip, pset
    next
    Grab 100, 0, 100 + w - 1, h - 1, k2
end sub

' The ground's columns, then what stands on them, from the Level DATA.
sub LoadLevel (st as integer)
    dim cmd as string, n as integer, c as integer, g as integer, bx as integer
    dim i as integer, s as integer, k as integer, z as integer
    Stage = st
    z = st and 1
    Zone = z
    for i = 0 to 255
        Pal(i) = Pal0(i)
    next
    for i = 0 to ZPN(st) - 1
        Pal(ZPK(st, i)) = ZPC(st, i)
    next
    for i = 0 to 2
        Glint(i) = Pal(CWATER + 1 + i)
    next
    Shade
    erase Map
    NObj = 0
    NMv = 0
    for i = 0 to MW
        ColTop(i) = MH
    next
    g = 10
    if z then
        ZTile = ITILE2
        ZBack = IZ2
        ZLoop = IZ2 + 5
        ZPipe = IZ2 + 7
        ZCard = ICARD2
        ZWave = IWAVE + 4
        if st = 1 then
            restore Level2
        else
            restore Level4
        end if
    else
        ZTile = ITILE
        ZBack = IBG
        ZLoop = ILOOP
        ZCard = ICARD
        ZWave = IWAVE
        if st = 0 then
            restore Level
        else
            restore Level3
        end if
    end if
    do
        read cmd, n
        s = (MH - g) * TS
        k = 0
        select case cmd
        case "flat"
            for i = 1 to n
                Column c, g, 1
                Scatter c, s
                c = c + 1
            next
        case "up45"
            for i = 1 to n
                Column c, g, 2
                g = g + 1
                c = c + 1
            next
        case "dn45"
            for i = 1 to n
                g = g - 1
                Column c, g, 3
                c = c + 1
            next
        case "upg"
            for i = 1 to n
                Column c, g, 4
                Column c + 1, g, 5
                g = g + 1
                c = c + 2
            next
        case "dng"
            for i = 1 to n
                g = g - 1
                Column c, g, 6
                Column c + 1, g, 7
                c = c + 2
            next
        case "gap"
            c = c + n
        case "wall"
            g = g + n
        case "pit"
            ' not a fall to nothing: a floor PITD down, a spring at its far wall
            Pit c, g, n
            c = c + n
        case "wave"
            ' a ribbon of n twists over a pit, each two halves of WAVEL, bones up its far side
            for i = 0 to n * 2 - 1
                AddObj KWAVE + (i and 1), (c + i * 12) * TS, s
                if (i and 1) = 0 then
                    for k = 3 to 9 step 3
                        AddObj KBONE, (c + i * 12 + k) * TS, s - cint(WAVEA * (1 - cos(PI * k / 12)) / 2) - 6
                    next
                end if
            next
            k = 0
            Pit c, g, n * 24
            c = c + n * 24
        case "bump"
            AddObj KBUMP, c * TS + 8, s - n * TS
        case "hoop"
            AddObj KHOOP, c * TS + 8, s - n * TS
        case "pipe"
            ' a wall with a quarter pipe up to it, over the three columns before: nothing grows there
            for i = 0 to NObj - 1
                if ((ObK(i) >= KTREE and ObK(i) <= KLAMP) or ObK(i) >= KPOST) and ObX(i) >= c * TS - PIPER then ObK(i) = 0
            next
            AddObj KPIPE, c * TS, s
            ' Amber Downs' pipe is drawn with the ground under it
            if z = 0 then
                ColVis(c - 1) = 0
                ColVis(c - 2) = 0
                ColVis(c - 3) = 0
            end if
            g = g + n
        case "drop"
            g = g - n
        case "mover"
            ' at each end of its way it lies 8 over the ground
            AddMover c * TS + (n * TS - MOVEW) \ 2, s, (n * TS - MOVEW) \ 2 + 8, 0, c * TS
            Pit c, g, n
            c = c + n
        case "hover"
            AddMover c * TS + 8, s - n * TS \ 2, n * TS \ 2, 1, c * TS
            Pit c, g, 4
            c = c + 4
            g = g + n
        case "lift"
            AddMover c * TS, s, n * TS, 2, c * TS
            Pit c, g, 3
            c = c + 3
            g = g + n
        case "button"
            k = KBUTTON
            bx = c * TS + 8
        case "bridge"
            ' a pit n wide, platforms lying on its floor that rise to bridge it once the last button is trodden
            for i = 0 to n \ 3 - 1
                AddMover (c + i * 3) * TS, s + PITD * TS - 6, PITD * TS - 6, 4, c * TS
                MvB(NMv - 1) = bx
            next
            Pit c, g, n
            c = c + n
        case "cable"
            ' a basket down a cable over a pit n wide, the ground past it CABLED lower
            AddMover c * TS - 8, s, n * TS - MOVEW + 16, 3, c * TS
            Pit c, g - CABLED \ TS, n
            c = c + n
            g = g - CABLED \ TS
        case "plat"
            for i = 1 to n mod 100
                Map(c + i, MH - g - n \ 100 - 1) = 1
                AddObj KPLAT, (c + i) * TS, s - (n \ 100 + 1) * TS
            next
        case "bones"
            for i = 0 to n - 1
                AddObj KBONE, (c + i) * TS + 8, s - 14
            next
        case "arc"
            for i = 0 to n - 1
                AddObj KBONE, (c + i) * TS + 8, s - 20 - cint(50 * sin(PI * (i + .5) / n))
            next
        case "hb"
            for i = 1 to n mod 100
                AddObj KBONE, (c + i) * TS + 8, s - (n \ 100) * TS - 14
            next
        case "cat"
            k = KCAT
        case "spring"
            k = KSPRING
        case "mark"
            k = KMARK
        case "boost"
            k = KBOOST
        case "loop"
            k = KLOOP
        case "tree"
            k = KTREE
        case "bush"
            k = KBUSH
        case "flower"
            k = KFLOWER
        case "rock"
            k = KROCK
        case "thorn"
            k = KTHORN
        case "jet"
            k = KJET
        case "spin"
            k = KSPIN
        case "goal"
            k = KGOAL
            GoalX = c * TS + 8
        case "fam"
            k = KFAM
            FamX = c * TS + 8
        case "dog"
            StartX = c * TS + 8
            StartY = s
        end select
        if k then AddObj k, c * TS + 8, s
    loop until cmd = "end"
    LW = c
    LWPX = c * TS
    ' nothing grows in front of the family
    for i = 0 to NObj - 1
        if ((ObK(i) >= KBUSH and ObK(i) <= KLAMP) or ObK(i) >= KPOST) and abs(ObX(i) - FamX) < 112 then ObK(i) = 0
    next
    SortObjs
    Track
    Faces
end sub

' The ground's line from the columns' tops, left to right, broken where they do not meet; a loop spliced in where
' it stands, a quarter pipe and its wall over the three columns before the wall; then the platforms' tops.
' Where a flat column stands over a lower flat one, or over none, its side shows: for how many tiles down.
sub Faces
    dim c as integer
    for c = 0 to LW - 1
        FaceL(c) = 0
        FaceR(c) = 0
        if ColVis(c) = 1 or ColVis(c) = 2 then
            if c > 0 then FaceL(c) = Side%(c, c - 1)
            FaceR(c) = Side%(c, c + 1)
        end if
    next
end sub

function Side% (c as integer, n as integer)
    Side% = 0
    if ColTop(n) = MH then
        Side% = MH - ColTop(c)
    elseif (ColVis(n) = 1 or ColVis(n) = 2) and ColTop(n) > ColTop(c) then
        Side% = ColTop(n) - ColTop(c)
    end if
end function

' A bare side of the ground from x, y0, n tiles down: picture k its turf's end, k + 2 the comb's.
sub Bare (x as integer, y0 as integer, n as integer, k as integer)
    dim i as integer, y as integer, j as integer
    j = k
    y = y0
    for i = 1 to n
        if y >= SH then exit for
        if y > -TS then Blit x, y, j, TS, TS
        j = k + 2
        y = y + TS
    next
end sub

sub Track
    dim c as integer, i as integer, j as integer, k as integer, x as integer, y as integer
    dim yl as integer, yr as integer, x1 as integer, a as single
    GN = 0
    NZ = 0
    for c = 0 to MW
        ColSeg(c) = -1
    next
    for i = 0 to NObj - 1
        if ObK(i) = KPIPE then ColSeg((ObX(i) - PIPER) \ TS) = -2 - i
    next
    for c = 0 to LW - 1
        x = c * TS
        if ColSeg(c) < -1 then
            y = ObY(-2 - ColSeg(c))
            ColSeg(c) = -1
            if GX(GN - 1) <> x or GY(GN - 1) <> y then Knot csng(x), csng(y), 0
            ZA(NZ) = GN - 1
            ZM(NZ) = -1
            ZL(NZ) = x
            ZR(NZ) = x + PIPER
            for j = 1 to PIPEN - 1
                a = j * PI / 2 / PIPEN
                Knot x + PIPER * sin(a), y - PIPER + PIPER * cos(a), 2
            next
            Knot csng(x + PIPER), csng(y - PIPER), 2
            j = ColTop(c + PIPER \ TS) * TS
            if j < y - PIPER then Knot csng(x + PIPER), csng(j), 2
            ZB(NZ) = GN - 2
            NZ = NZ + 1
            c = c + PIPER \ TS - 1
        elseif ColTop(c) < MH then
            k = Map(c, ColTop(c))
            y = (ColTop(c) + 1) * TS
            ' a tile's top at its edges: its profile's, to the even pixel
            yl = y - (Prof(k, 0) and 30)
            yr = y - (Prof(k, TS - 1) and 30)
            j = GN = 0
            if not j then j = GX(GN - 1) <> x or GY(GN - 1) <> yl
            if j then Knot csng(x), csng(yl), 0
            ColSeg(c) = GN - 1
            i = First(c)
            do while i < NObj
                if ObX(i) >= x + TS then exit do
                if ObK(i) = KLOOP then
                    Knot csng(ObX(i)), csng(yl), 1
                    ZA(NZ) = GN - 1
                    ZM(NZ) = GN - 1 + LOOPN \ 2
                    ZL(NZ) = ObX(i) - LOOPR
                    ZR(NZ) = ObX(i) + LOOPR
                    for j = 1 to LOOPN - 1
                        a = j * 2 * PI / LOOPN
                        Knot ObX(i) + LOOPR * sin(a), yl - LOOPR + LOOPR * cos(a), 2
                    next
                    Knot csng(ObX(i)), csng(yl), 2
                    ZB(NZ) = GN - 2
                    NZ = NZ + 1
                end if
                i = i + 1
            loop
            Knot csng(x + TS), csng(yr), 1
        end if
    next
    PlatA = GN
    x = NONE
    for i = 0 to NObj - 1
        if ObK(i) = KPLAT then
            if x <> NONE and ObX(i) = x1 and ObY(i) = y then
                x1 = x1 + TS
            else
                if x <> NONE then
                    Knot csng(x), csng(y), 0
                    Knot csng(x1), csng(y), 1
                end if
                x = ObX(i)
                x1 = x + TS
                y = ObY(i)
            end if
        end if
    next
    if x <> NONE then
        Knot csng(x), csng(y), 0
        Knot csng(x1), csng(y), 1
    end if
    ' the moving platforms: a stretch each, a curve of one, in the order they were made
    k = 0
    for i = 0 to NObj - 1
        if ObK(i) = KMOVE then
            Knot csng(ObX(i)), csng(ObY(i)), 0
            Knot csng(ObX(i) + MOVEW), csng(ObY(i)), 3
            MvO(k) = i
            MvS(k) = GN - 2
            MvZ(k) = NZ
            MvT(k) = 0
            ' a bridge's button, by where it is
            if MvK(k) = 4 then
                for j = 0 to NObj - 1
                    if ObK(j) = KBUTTON and ObX(j) = MvB(k) then
                        MvB(k) = j
                        exit for
                    end if
                next
            end if
            ZA(NZ) = GN - 2
            ZB(NZ) = GN - 2
            ZM(NZ) = -1
            ZL(NZ) = ObX(i)
            ZR(NZ) = ObX(i) + MOVEW
            NZ = NZ + 1
            k = k + 1
        end if
    next
    ' the ribbons: halves that meet are one stretch, 8 of flat at each end to run on and off by
    x1 = NONE
    for i = 0 to NObj - 1
        if ObK(i) = KWAVE or ObK(i) = KWAVE2 then
            if ObX(i) <> x1 then
                if x1 <> NONE then
                    Knot csng(x1 + 8), csng(y), 2
                    ZB(NZ) = GN - 2
                    NZ = NZ + 1
                end if
                y = ObY(i)
                Knot csng(ObX(i) - 8), csng(y), 0
                ZA(NZ) = GN - 1
                ZM(NZ) = -2
                ZL(NZ) = ObX(i) - 8
                Knot csng(ObX(i)), csng(y), 2
            end if
            for j = 1 to 12
                a = PI * j / 12 - PI * (ObK(i) = KWAVE2)
                Knot csng(ObX(i) + j * 16), y - WAVEA * (1 - cos(a)) / 2, 2
            next
            x1 = ObX(i) + WAVEL
            ZR(NZ) = x1 + 8
        end if
    next
    if x1 <> NONE then
        Knot csng(x1 + 8), csng(y), 2
        ZB(NZ) = GN - 2
        NZ = NZ + 1
    end if
end sub

' A moving platform, its middle's left end at x, y; reach a, kind k; e the edge before it.
sub AddMover (x as integer, y as integer, a as integer, k as integer, e as integer)
    AddObj KMOVE, x, y
    MvX(NMv) = x
    MvY(NMv) = y
    MvA(NMv) = a
    MvK(NMv) = k
    MvE(NMv) = e
    NMv = NMv + 1
end sub

' Platform m's swing now, -1 to 1, held a while at each end when it goes up and down; quarter: a quarter of its
' way on, which tells where it is going.
function Swing! (m as integer, quarter as integer)
    dim q as single, v as single
    q = Ticks * STEPT * (1.1 - .2 * MvK(m)) + m + quarter * PI / 2
    v = sin(q)
    if MvK(m) = 1 then
        v = v * 1.3
        if v > 1 then v = 1
        if v < -1 then v = -1
    end if
    Swing! = v
end function

' The platforms a step on: to and fro, up and down, or a lift, up while Charlie stands on it and down again
' when not. Charlie goes with the one it stands on.
sub Movers (p as Hero)
    dim m as integer, s as integer, o as integer, x as single, y as single, dx as single, dy as single
    for m = 0 to NMv - 1
        s = MvS(m)
        x = MvX(m)
        y = MvY(m)
        select case MvK(m)
        case 0
            x = x + MvA(m) * Swing!(m, 0)
        case 1
            y = y + MvA(m) * Swing!(m, 0)
        case 2
            if p.ground and p.sg = s then
                MvT(m) = MvT(m) + 90 * STEPT
            else
                MvT(m) = MvT(m) - 60 * STEPT
            end if
            if MvT(m) < 0 then MvT(m) = 0
            if MvT(m) > MvA(m) then MvT(m) = MvA(m)
            y = y - MvT(m)
        case 4
            if ObS(MvB(m)) then MvT(m) = MvT(m) + 200 * STEPT
            if MvT(m) > MvA(m) then MvT(m) = MvA(m)
            y = y - MvT(m)
        case 3
            ' down its cable while stood on, faster as it goes; back up when left
            if p.ground and p.sg = s then
                MvT(m) = MvT(m) + (.25 + MvT(m)) * .6 * STEPT
            else
                MvT(m) = MvT(m) - .4 * STEPT
            end if
            if MvT(m) < 0 then MvT(m) = 0
            if MvT(m) > 1 then MvT(m) = 1
            x = x + MvA(m) * MvT(m)
            y = y + CABLED * MvT(m)
        end select
        dx = x - GX(s)
        dy = y - GY(s)
        if dx <> 0 or dy <> 0 then
            GX(s) = x
            GX(s + 1) = x + MOVEW
            GY(s) = y
            GY(s + 1) = y
            ZL(MvZ(m)) = int(x)
            ZR(MvZ(m)) = int(x) + MOVEW + 1
            o = MvO(m)
            ObX(o) = cint(x)
            ObY(o) = cint(y)
            if p.ground and p.sg = s then
                p.x = p.x + dx
                p.y = p.y + dy
            end if
        end if
    next
end sub

' The line's next point, x, y, a stretch of kind k to it from the last: 0, none.
sub Knot (x as single, y as single, k as integer)
    dim i as integer, u as single, v as single, d as single
    GX(GN) = x
    GY(GN) = y
    GK(GN) = 0
    if k <> 0 then
        i = GN - 1
        u = x - GX(i)
        v = y - GY(i)
        d = sqr(u * u + v * v)
        u = u / d
        v = v / d
        GU(i) = u
        GV(i) = v
        GL(i) = d
        GK(i) = k
        if u > 0 then
            GA(i) = atn(-v / u)
        elseif u < 0 then
            GA(i) = atn(-v / u) + PI
        else
            GA(i) = -sgn(v) * PI / 2
        end if
    end if
    GN = GN + 1
end sub

' The tiles' stretch of the line under x, its top within 4 of y; -1 when there is none.
function SegAt% (x as integer, y as single)
    dim s as integer
    SegAt% = -1
    if x < 0 or x >= LWPX then exit function
    s = ColSeg(x \ TS)
    if s >= 0 then
        ' past a loop that stands in the column: the stretch after it
        if x > GX(s + 1) then s = s + LOOPN + 1
        if abs(GY(s) + (x - GX(s)) * GV(s) / GU(s) - y) < 4 then
            SegAt% = s
            exit function
        end if
    end if
    for s = PlatA to GN - 2
        if GK(s) = 1 and x >= GX(s) and x <= GX(s + 1) and abs(GY(s) - y) < 4 then
            SegAt% = s
            exit function
        end if
    next
end function

' Flowers, bushes and lantern plants along the flats, by the column.
sub Scatter (c as integer, s as integer)
    if c < 8 then exit sub
    if Zone then
        ' lamp posts, tanks and valve stacks along the decks
        select case (c * 37 + 11) mod 23
        case 0, 12
            AddObj KPOST, c * TS + 8, s
        case 5
            AddObj KTANK, c * TS + 8, s
        case 17
            AddObj KVENT, c * TS + 8, s
        end select
        exit sub
    end if
    select case (c * 37 + 11) mod 19
    case 0, 7
        AddObj KFLOWER, c * TS + 8, s
    case 3
        AddObj KBUSH, c * TS + 8, s
    case 11
        AddObj KLAMP, c * TS + 8, s
    end select
end sub

' Column c solid g tiles up from the bottom; kind over 1 a slope on top.
sub Column (c as integer, g as integer, kind as integer)
    dim r as integer
    for r = MH - g to MH - 1
        Map(c, r) = 1
    next
    if kind > 1 then
        Map(c, MH - g - 1) = kind
        ColTop(c) = MH - g - 1
        ColVis(c) = kind + 2
    else
        ColTop(c) = MH - g
        ColVis(c) = 1 + (c and 1)
    end if
end sub

' Under a jump or a platform, not a fall to nothing: n columns from c of floor PITD under the ground g, bones on
' it, a spring at its far wall.
sub Pit (c as integer, g as integer, n as integer)
    dim i as integer
    for i = 0 to n - 1
        Column c + i, g - PITD, 1
        if i > 1 and i < n - 3 and (i and 1) = 0 then AddObj KBONE, (c + i) * TS + 8, (MH - g + PITD) * TS - 14
    next
    AddObj KSPRING, (c + n - 2) * TS + 8, (MH - g + PITD) * TS
end sub

sub AddObj (k as integer, x as integer, y as integer)
    ObK(NObj) = k
    ObX(NObj) = x
    ObY(NObj) = y
    ObA(NObj) = x
    NObj = NObj + 1
end sub

' The things by x, and First(c) the first at or past column c.
sub SortObjs
    dim i as integer, j as integer, x as integer, y as integer, k as integer, c as integer
    for i = 1 to NObj - 1
        x = ObX(i)
        y = ObY(i)
        k = ObK(i)
        j = i - 1
        do while j >= 0
            if ObX(j) <= x then exit do
            ObX(j + 1) = ObX(j)
            ObY(j + 1) = ObY(j)
            ObK(j + 1) = ObK(j)
            j = j - 1
        loop
        ObX(j + 1) = x
        ObY(j + 1) = y
        ObK(j + 1) = k
    next
    i = 0
    for c = 0 to MW
        do while i < NObj
            if ObX(i) >= c * TS then exit do
            i = i + 1
        loop
        First(c) = i
    next
    for i = 0 to NObj - 1
        ObA(i) = ObX(i)
    next
end sub

' Every picture: the DATA's, then those built from them or drawn here on
' the hidden page; the backdrop last, as it fills the page.
sub BuildArt
    dim i as integer
    cls
    LoadArt
    ' the turned run, as drawn and mirrored: each upside down too, for the far side of a loop
    for i = 0 to 159
        Flop ITILT + i, ITILT + 160 + i, 32, 32
    next
    BuildGlyphs
    for i = 0 to 255
        Pal0(i) = Pal(i)
    next
end sub

' Colour k is c, shown Dusk sixteenths as bright.
sub Tint (k as integer, c as long)
    Pal(k) = c
    palette k, (c and 63) * Dusk \ 16 + 256& * ((c \ 256 and 63) * Dusk \ 16) + 65536 * ((c \ 65536) * Dusk \ 16)
end sub

' The tiles that move: a deck's lights run along it, a fan's blades turn, glints go down a pipe; by their colours.
sub Cycle
    dim i as integer
    for i = 0 to 3
        if i = (Ticks \ 8 and 3) then
            Tint CCHASE + i, 4144952
        else
            Tint CCHASE + i, 3550218
        end if
        if (i + Ticks \ 4) and 2 then
            Tint CCHASE + 4 + i, 2235666
        else
            Tint CCHASE + 4 + i, 525317
        end if
        if i = (Ticks \ 4 and 3) then
            Tint CCHASE + 8 + i, 2242111
        else
            Tint CCHASE + 8 + i, 991026
        end if
    next
end sub

sub Shade
    dim k as integer
    for k = 0 to 255
        Tint k, Pal(k)
    next
end sub

' The palette and the pictures from CHARLIE.DAT; a mirrored one's mirror made.
sub LoadArt
    dim n as integer, i as integer, k as integer, w as integer, h as integer
    dim masked as integer, k2 as integer, c as long
    Dusk = 16
    OpenData
    c = Word&
    n = Word&
    for i = 1 to n
        k = Word&
        Tint k, Word&
    next
    for k = 0 to NSTAGE - 1
        ZPN(k) = Word&
        for i = 0 to ZPN(k) - 1
            ZPK(k, i) = Word&
            ZPC(k, i) = Word&
        next
    next
    n = Word&
    for i = 1 to n
        k = Word&
        w = Word&
        h = Word&
        masked = Word&
        k2 = Word&
        At(k) = NextAt
        Unpack
        if masked then
            At(k + 1) = NextAt
            Unpack
        end if
        if k2 >= 0 then
            Mirror k, k2, w, h
            Mirror k + 1, k2 + 1, w, h
        end if
    next
    close #1
end sub

' CHARLIE.DAT open as #1: where its tunes start, then its pictures.
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

' A run-length stream of the file's LONGs to Img at NextAt: n then n values, -n then
' one value n times, 0 the end.
sub Unpack
    dim n as integer, e as long, v as long
    n = Word&
    do while n
        if n > 0 then
            for e = NextAt to NextAt + n - 1
                get #1, , Img(e)
            next
        else
            n = -n
            v = Word&
            for e = NextAt to NextAt + n - 1
                Img(e) = v
            next
        end if
        NextAt = NextAt + n
        n = Word&
    loop
end sub

' Image k, w by h, mirrored as k2: a column at a time.
sub Mirror (k as integer, k2 as integer, w as integer, h as integer)
    dim x as integer, strip(40) as long
    put (0, 0), Img(At(k)), pset
    for x = 0 to w - 1
        get (x, 0)-(x, h - 1), strip
        put (100 + w - 1 - x, 0), strip, pset
    next
    Grab 100, 0, 100 + w - 1, h - 1, k2
end sub

' The HUD's characters, each 9 by 9 over a shadow: yellow, then white.
' And room for the HUD's picture and mask.
sub BuildGlyphs
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
            MaskGrab 0, 0, 9, 9, 20, 0, IGLYPH + (c * NGLYPH + i) * 2
        next
    next
    line (0, 184)-(8, 191), 0, BF
    Grab 0, 0, HUDW - 1, HUDH - 1, IHUD
    Grab 0, 0, HUDW - 1, HUDH - 1, IHUD + 1
end sub

sub Grab (x1 as integer, y1 as integer, x2 as integer, y2 as integer, k as integer)
    At(k) = NextAt
    get (x1, y1)-(x2, y2), Img(NextAt)
    NextAt = NextAt + (4 + clng(x2 - x1 + 1) * (y2 - y1 + 1) + 3) \ 4
end sub

' The w by h at x, y kept as k, and its mask, drawn at mx, my, as k + 1:
' 255 where it is clear, 0 where drawn.
sub MaskGrab (x as integer, y as integer, w as integer, h as integer, mx as integer, my as integer, k as integer)
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
    Grab x, y, x + w - 1, y + h - 1, k
    Grab mx, my, mx + w - 1, my + h - 1, k + 1
end sub

' Masked image k, w by h, at x, y; nothing when wholly off the screen.
sub Blit (x as integer, y as integer, k as integer, w as integer, h as integer)
    if x + w <= 0 or x >= SW or y + h <= 0 or y >= SH then exit sub
    put (x, y), Img(At(k + 1)), and
    put (x, y), Img(At(k)), or
end sub

' The keyboard port's last scan code: a press sets Held, a release clears.
sub Poll
    dim s as integer
    s = inp(&H60)
    if s = LastScan then exit sub
    LastScan = s
    if s = 255 or s = 224 then exit sub
    PortOK = -1
    if s and 128 then
        Held(s and 127) = 0
    else
        Held(s) = -1
    end if
end sub

sub ReadPad (k as Pad)
    dim c as string, t as single
    Poll
    t = timer
    do
        c = inkey$
        select case c
        case chr$(27)
            k.quit = -1
        case "m", "M"
            k.mute = -1
        case "n", "N"
            Tune = not Tune
            if not Tune then play "P64", "P64", "P64", , "P64", "P64", "P64", "P64"
        case chr$(13)
            k.start = -1
        case " "
            k.start = -1
            KeyT(SCSPACE) = t
        case "x", "X"
            k.bark = -1
        case "z", "Z"
            KeyT(SCZ) = t
        case chr$(0) + "K"
            KeyT(SCLEFT) = t
        case chr$(0) + "M"
            KeyT(SCRIGHT) = t
        case chr$(0) + "H"
            KeyT(SCUP) = t
        case chr$(0) + "P"
            KeyT(SCDOWN) = t
        end select
    loop while len(c)
    k.l = Held2%(SCLEFT, t)
    k.r = Held2%(SCRIGHT, t)
    k.d = Held2%(SCDOWN, t)
    k.jump = Held2%(SCSPACE, t) or Held2%(SCZ, t) or Held2%(SCUP, t)
    if k.jump and not k.was then k.hit = -1
    k.was = k.jump
end sub

' Key s held: by the port, or, without it, typed within a repeat's wait.
function Held2% (s as integer, t as single)
    if PortOK then
        Held2% = Held(s)
    else
        Held2% = t - KeyT(s) < .55 and KeyT(s) > 0
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
    if abs(r - 1) < .05 then
        a = STEPT
    elseif abs(r - .5) < .03 then
        a = STEPT / 2
    else
        a = c.span + c.late * .02
        c.late = c.late + d - a
        if abs(c.late) > .1 then
            a = a + c.late
            c.late = 0
        end if
        if a < 0 then a = 0
        c.lag = c.lag + a
        exit sub
    end if
    c.late = 0
    ph = c.lag - a * int(c.lag / a)
    c.lag = c.lag + (a / 2 - ph) * .05
    if d > 2.5 * a then a = a * cint(d / a)
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
sub Sfx (id as integer)
    dim s as string
    if Muted then exit sub
    select case id
    case SJUMP
        s = "L64O4CEGO5C"
    case SBONE
        ' a bark: a low woof slid up into, two voices beating, through a mouth; one at a time
        if abs(Ticks - BarkAt) < 8 then exit sub
        BarkAt = Ticks
        s = "W2I15Y2,1000,6H0,6,2,3K4,0,6L8O3D"
    case SHURT
        s = "L32O3GECO2G"
    case SSPRING
        s = "L64O3CGO4EO5CG"
    case SPOP
        s = "L64O5GO4CO5E"
    case SROLL
        s = "L64O3CFA"
    case SCHARGE
        s = "L64O4CDEFG"
    case SLAUNCH
        s = "L64O3GO4CGO5CG"
    case SCLEAR
        s = "L16O5CEGO6CL8E"
    case SDIE
        s = "L16O4CO3BAGL8F"
    case SMARK
        s = "L32O5CEGO6C"
    case SBOOST
        s = "L64O4CEGO5CEGO6C"
    case SHISS
        s = "W3Y3,3500,0H3,8,0,3L4O6B"
    case SBARK
        ' two woofs, the second lower, through a mouth
        s = "W2I15Y2,1300,6H0,5,3,3K3,0,7L16O3FP32L8O3D"
    end select
    play , , , "T255W1V13Y0K0I0H0,0,15,0" + s
end sub

' The songs the game holds from its start, the theme the first to play; the level's end is led by the zone's lead.
sub LoadSongs
    LoadSong ZTUNE, 0
    LoadSong THEME, 1
    LoadSong CAUGHT, 2
    LeadP(CAUGHT) = LeadP(ZTUNE)
    Sng = -1
    Coming = THEME
    ComingBar = 0
    Ended = -1
    Tune = -1
end sub

' Song id of CHARLIE.DAT into slot s: its order and levels, its kit, and its bars as PLAY strings.
sub LoadSong (s as integer, id as integer)
    dim i as integer, n as integer, bars as integer, songs as long
    dim bas(7) as string, hits(7) as string, pat(7) as string
    OpenData
    songs = Word&
    seek #1, songs + 4 + id * 4
    seek #1, songs + Word&
    Order(s) = Chars$
    Voicing(s) = Chars$
    bars = Word&
    Tempo(s) = Word&
    BassIn(s) = Word&
    for i = 0 to 4
        Lvl(s, i) = Word&
    next
    Fade(s) = Word&
    Entry(s) = Word&
    Ends(s) = Word&
    ReadKit s
    ReadList bas()
    ReadList hits()
    ReadList pat()
    for n = 0 to bars - 1
        ReadBar s, n, bas(), hits(), pat()
    next
    close #1
end sub

' Slot s's kit, CHARLIE.DAT's next: four sounds, its voicings and its drums.
sub ReadKit (s as integer)
    dim i as integer, n as integer
    LeadP(s) = Chars$
    BassP(s) = Chars$
    UnderP(s) = Chars$
    MidP(s) = Chars$
    VoLet(s) = ""
    n = Word&
    for i = 0 to n - 1
        VoLet(s) = VoLet(s) + Chars$
        VoOne(s, i) = Chars$
        VoTwo(s, i) = Chars$
        VoUnder(s, i) = Word&
    next
    DrLet(s) = ""
    n = Word&
    for i = 0 to n - 1
        DrLet(s) = DrLet(s) + Chars$
        DrPatch(s, i) = Chars$
        DrVol(s, i) = Word&
        DrUp(s, i) = Word&
        DrNote(s, i) = Chars$
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
' PLAY strings. A second voice of two characters is a chord pattern, its x the note, an octave down.
sub ReadBar (s as integer, n as integer, bas() as string, hits() as string, pat() as string)
    dim root as integer, nb as integer, nd as integer, low as integer, j as integer, h as integer
    dim st as string, br as string, sol as string, mid as string
    root = Word&
    nb = Word&
    nd = Word&
    st = Chars$
    br = Chars$
    sol = Chars$
    mid = Chars$
    low = 27
    if len(st) = 2 then
        low = 15
        st = hits(val(left$(st, 1))) + right$(st, 1)
        do while instr(st, "x") and instr(st, "x") < 17
            mid$(st, instr(st, "x"), 1) = mid$(st, 17, 1)
        loop
    end if
    for j = 0 to 1
        h = (s * NBAR + n) * 2 + j
        Kit(h) = mid$(pat(nd), j * 8 + 1, 8)
        Trk(h * 5) = Beat$(s, Kit(h), 8)
        Trk(h * 5 + 1) = Riff$(bas(nb - 1), j, 15 + root)
        Trk(h * 5 + 2) = Riff$(br, j, 27)
        Trk(h * 5 + 3) = Riff$(sol, j, 27)
        Trk(h * 5 + 4) = Riff$(st, j, low)
        Org(h) = Riff$(br, j, 15)
        Third(h) = Riff$(mid, j, 27)
    next
end sub

' Half a bar of slot s's drums, `loud` eighths as loud: each of x's letters its drum, any other a rest.
function Beat$ (s as integer, x as string, loud as integer)
    dim q as integer, n as integer, d as string
    for q = 1 to 8
        n = instr(DrLet(s), mid$(x, q, 1)) - 1
        if n < 0 then
            d = d + "P16"
        else
            d = d + DrPatch(s, n) + Vol$(DrVol(s, n), loud, DrUp(s, n)) + DrNote(s, n)
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
        case 1
            ln = "16"
        case 2, 3
            ln = "8"
        case 4, 6
            ln = "4"
        case else
            ln = "2"
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
sub FeedSong
    if Muted or not Tune then exit sub
    if play(0) > 2 then exit sub
    NextHalf
end sub

' The playing song's next half bar to PLAY, its voices each their own string; or the song that was coming, begun.
' Nothing once a song has ended.
sub NextHalf
    dim h as integer, b as integer, v as integer, loud as integer
    dim t as string, d as string, bass as string, under as string
    dim one as string, two as string, solo as string, midv as string
    ' the song picked comes in once this one has faded, or at once when it has ended
    if Coming >= 0 and (FadeOut = 0 or Ended) then StartSong
    if Ended then exit sub
    t = "T" + ltrim$(str$(Tempo(Sng)))
    b = asc(mid$(Order(Sng), Bar + 1, 1)) - 97
    h = (Sng * NBAR + b) * 2 + HalfBar
    loud = (len(Order(Sng)) - Bar) * 2 - HalfBar
    if loud > 8 or Bar < len(Order(Sng)) - Fade(Sng) then loud = 8
    if FadeOut > 0 then
        loud = loud * FadeOut \ 3
        FadeOut = FadeOut - 1
    elseif FadeIn > 0 then
        loud = loud * (3 - FadeIn) \ 3
        FadeIn = FadeIn - 1
    end if
    d = Trk(h * 5)
    if loud < 8 then d = Beat$(Sng, Kit(h), loud)
    ' the bass waits its bars; the low double plays under the voicings that have it
    bass = Vol$(Lvl(Sng, 4), loud, 0) + Trk(h * 5 + 1)
    if Bar < BassIn(Sng) then bass = "P2"
    v = instr(VoLet(Sng), mid$(Voicing(Sng), b + 1, 1)) - 1
    under = "P2"
    if VoUnder(Sng, v) then under = Vol$(Lvl(Sng, 3), loud, 0) + Org(h)
    one = t + VoOne(Sng, v) + Vol$(Lvl(Sng, 0), loud, 0) + Trk(h * 5 + 2)
    solo = t + LeadP(Sng) + Vol$(Lvl(Sng, 1), loud, 7) + Trk(h * 5 + 3)
    two = t + VoTwo(Sng, v) + Vol$(Lvl(Sng, 2), loud, 0) + Trk(h * 5 + 4)
    midv = t + Vol$(Lvl(Sng, 2), loud, 0) + Third(h)
    play t + d, t + bass, one, , solo, two, t + under, midv
    HalfBar = 1 - HalfBar
    if HalfBar then exit sub
    Bar = Bar + 1
    if Bar < len(Order(Sng)) then exit sub
    Bar = 0
    Ended = Ends(Sng)
end sub

' The song that was coming begins: its kit to the voices that keep theirs. In stereo: the first chord voice to the
' left, the second and the low double to the right; the room answers little.
sub StartSong
    dim t as string
    Sng = Coming
    Bar = ComingBar
    HalfBar = 0
    Coming = -1
    Ended = 0
    FadeOut = 0
    t = "T" + ltrim$(str$(Tempo(Sng)))
    play "MB" + t + "Z1,0L16", t + BassP(Sng), t + "R2S4", , t + "R2" + LeadP(Sng), t + "R2S12", t + UnderP(Sng), t + MidP(Sng)
end sub

' The slot to play for the game's state: the theme for the title, the level's end, else the zone's tune.
function SongFor% (g as Game)
    select case g.mode
    case TITLE
        SongFor% = THEME
    case CLEARED
        SongFor% = CAUGHT
    case else
        SongFor% = ZTUNE
    end select
end function

' Slot n's song, when it is not the one playing, from its start or, late, from the bar it comes in at. The one
' playing fades over a bar (half of one, late) and the next fades in over one; late, it comes in full.
sub PickSong (n as integer, late as integer)
    if n = Sng then
        Coming = -1
        FadeOut = 0
        exit sub
    end if
    if n = Coming then exit sub
    Coming = n
    ComingBar = 0
    FadeOut = 2
    FadeIn = 2
    if late then
        ComingBar = Entry(n)
        FadeOut = 1
        FadeIn = 0
    end if
end sub

' The ground's top at column sx near y: in y's tile, the one above when
' that is full, the one below when it is empty; NONE when both are, or
' off the level.
function Surf% (sx as integer, y as integer)
    dim tx as integer, ty as integer, i as integer, h as integer
    Surf% = NONE
    if sx < 0 or sx >= LWPX or y < TS then exit function
    tx = sx \ TS
    i = sx and 15
    ty = y \ TS
    if ty >= MH - 1 then exit function
    h = Prof(Map(tx, ty), i)
    if h = TS then
        Surf% = ty * TS - Prof(Map(tx, ty - 1), i)
    elseif h = 0 then
        h = Prof(Map(tx, ty + 1), i)
        if h then Surf% = (ty + 2) * TS - h
    else
        Surf% = (ty + 1) * TS - h
    end if
end function

' Past the level's ends a wall; above and below it air.
function Solid% (x as integer, y as integer)
    if x < 0 or x >= LWPX then
        Solid% = -1
        exit function
    end if
    if y < 0 or y >= MH * TS then
        Solid% = 0
        exit function
    end if
    Solid% = (y and 15) >= TS - Prof(Map(x \ TS, y \ TS), x and 15)
end function

' Charlie's side toward dir in a wall: pushed out, and true.
function Wall% (p as Hero, dir as integer)
    dim n as integer, y as integer
    y = cint(p.y)
    do while Solid%(cint(p.x) + dir * HW, y - 11) or Solid%(cint(p.x) + dir * HW, y - HH + 4)
        p.x = p.x - dir
        n = n + 1
        if n > TS then exit do
    loop
    Wall% = n > 0
end function

' Falling Charlie's feet in the tiles' ground: on the line's stretch there, its speed along it kept.
function Land% (p as Hero, d as integer)
    dim x as integer, y as integer, l as integer, c as integer, r as integer, s as integer
    x = cint(p.x)
    y = cint(p.y)
    l = Surf%(x - 6, y)
    c = Surf%(x, y)
    r = Surf%(x + 6, y)
    if l > y or l < y - 14 then l = NONE
    if c > y or c < y - 14 then c = NONE
    if r > y or r < y - 14 then r = NONE
    if l = NONE and c = NONE and r = NONE then
        Land% = 0
        exit function
    end if
    ' by a foot on an edge: onto the edge
    if c <> NONE then
        s = SegAt%(x, csng(c))
    elseif l < r then
        s = SegAt%(x - 6, csng(l))
        ' going off that edge, not onto it: no ground
        if p.vx > 0 then s = -1
    else
        s = SegAt%(x + 6, csng(r))
        if p.vx < 0 then s = -1
    end if
    if s < 0 then
        Land% = 0
        exit function
    end if
    if p.x < GX(s) then p.x = GX(s)
    if p.x > GX(s + 1) then p.x = GX(s + 1)
    p.y = GY(s) + (p.x - GX(s)) * GV(s) / GU(s)
    Settle p, s, d
    Land% = -1
end function

' Down on stretch s: the speed along it kept.
sub Settle (p as Hero, s as integer, d as integer)
    p.sg = s
    p.ang = GA(s)
    p.gsp = p.vx * GU(s) + p.vy * GV(s)
    if p.jumped and not d then p.roll = 0
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
function Curve% (p as Hero, x0 as single, y0 as single, d as integer)
    dim z as integer, s as integer, a as integer, b as integer, far as integer
    dim d0 as single, d1 as single, t as single, f as single
    Curve% = 0
    for z = 0 to NZ - 1
        if p.x >= ZL(z) - 8 and p.x <= ZR(z) + 8 then
            a = ZA(z)
            b = ZB(z)
            if ZM(z) >= 0 then
                far = p.sg >= ZM(z)
                if p.sg >= PlatA then far = x0 > ZL(z) + LOOPR
                if far then
                    a = ZM(z)
                else
                    b = ZM(z) - 1
                end if
            end if
            for s = a to b
                d1 = (p.x - GX(s)) * GV(s) - (p.y - GY(s)) * GU(s)
                if d1 < 0 and p.vx * GV(s) - p.vy * GU(s) < -1 then
                    d0 = (x0 - GX(s)) * GV(s) - (y0 - GY(s)) * GU(s)
                    t = (p.x - GX(s)) * GU(s) + (p.y - GY(s)) * GV(s)
                    ' from just behind it too, at a join; not behind a wall
                    ' and from further behind a platform, which moves
                    if d0 > -1 + 3 * (GK(s) = 3) and (d0 >= 0 or GU(s) >= .35) and t >= -1 and t <= GL(s) + 1 then
                        if t < 0 then t = 0
                        if t > GL(s) then t = GL(s)
                        p.x = GX(s) + GU(s) * t
                        p.y = GY(s) + GV(s) * t
                        f = p.vx * GU(s) + p.vy * GV(s)
                        if GU(s) >= .35 or abs(f) >= STALL then
                            Settle p, s, d
                            Curve% = -1
                        else
                            p.x = p.x + GV(s)
                            p.y = p.y - GU(s)
                            p.vx = f * GU(s)
                            p.vy = f * GV(s)
                        end if
                        exit function
                    end if
                end if
            next
        end if
    next
end function

' Charlie d pixels along the line, over its joins: slowed in a sharp hollow, into the air over a sharp crest or
' off the line's end.
sub Along (p as Hero, d as single)
    dim i as integer, j as integer, a as integer, t as single, cr as single, dt as single
    i = p.sg
    t = (p.x - GX(i)) * GU(i) + (p.y - GY(i)) * GV(i) + d
    do
        if t > GL(i) then
            j = i + 1
            a = i
        elseif t < 0 then
            j = i - 1
            a = j
        else
            exit do
        end if
        if j >= 0 then if GK(j) = 0 then j = -1
        if j >= 0 then
            cr = GU(a) * GV(a + 1) - GV(a) * GU(a + 1)
            dt = GU(a) * GU(a + 1) + GV(a) * GV(a + 1)
            if cr > 0 and dt < .64 then j = -1
        end if
        if j < 0 then
            if t < 0 then
                t = 0
            else
                t = GL(i)
            end if
            p.x = GX(i) + GU(i) * t
            p.y = GY(i) + GV(i) * t
            Drop p
            exit sub
        end if
        if t < 0 then
            t = t + GL(j)
        else
            t = t - GL(i)
        end if
        if cr < 0 and dt < .7 then
            if dt < 0 then dt = 0
            p.gsp = p.gsp * dt
            if j > i then
                t = t * dt
            else
                t = GL(j) - (GL(j) - t) * dt
            end if
        end if
        i = j
    loop
    p.sg = i
    p.ang = GA(i)
    p.x = GX(i) + GU(i) * t
    p.y = GY(i) + GV(i) * t
end sub

' Off the ground, its speed kept: from a wall or a roof, clear of it.
sub Drop (p as Hero)
    dim i as integer
    i = p.sg
    p.ground = 0
    p.vx = p.gsp * GU(i)
    p.vy = p.gsp * GV(i)
    if GU(i) > .7 then
        p.coyote = COYOTE
    elseif GU(i) < .35 then
        p.x = p.x + 2 * GV(i)
        p.y = p.y - 2 * GU(i)
    end if
end sub

' A step. The title plays itself: Charlie run by the pilot.
sub Tick (g as Game, p as Hero, k as Pad, lo() as Loose, pt() as Part)
    dim idle as Pad, auto as Pad, i as integer
    Ticks = Ticks + 1
    if Zone = 1 and (Ticks and 3) = 0 then Cycle
    if NMv then Movers p
    g.modeT = g.modeT + STEPT
    if g.shake > 0 then g.shake = g.shake - STEPT
    if k.mute then
        Muted = not Muted
        k.mute = 0
        if Muted then play "P64", "P64", "P64", , "P64", "P64", "P64", "P64"
    end if
    select case g.mode
    case TITLE
        if k.quit then g.quitting = -1
        if p.dead then
            ResetWorld g, p, lo(), pt()
        elseif p.x >= GoalX then
            if Met%(g, p, k, pt()) then ResetWorld g, p, lo(), pt()
        else
            Pilot p, auto
            MoveHero p, auto, pt()
            Touch g, p, lo(), pt()
            if p.y > MH * TS + 24 then Lose p
            Camera g, p, 0
        end if
        if k.hit or k.start then StartGame g, p, lo(), pt()
    case PLAYING
        if k.quit then ResetWorld g, p, lo(), pt()
        if p.dead then
            Dying g, p
        else
            g.clock = g.clock + STEPT
            MoveHero p, k, pt()
            Touch g, p, lo(), pt()
            if p.y > MH * TS + 24 then Lose p
            if p.x >= GoalX then
                g.mode = CLEARED
                g.modeT = 0
                g.bonus = g.bones * 50& + 10& * (240 - int(g.clock))
                if g.bonus < g.bones * 50& then g.bonus = g.bones * 50&
                Sfx SCLEAR
            end if
            Camera g, p, 0
        end if
    case CLEARED
        if g.modeT > 1.5 and g.bonus > 0 then
            g.bonus = g.bonus - 50
            g.score = g.score + 50
            if g.bonus < 0 then
                g.score = g.score + g.bonus
                g.bonus = 0
            end if
            if (Ticks and 3) = 0 then Sfx SBONE
        end if
        ' met, a while and the fade over: on to the next zone, or after the last, the title
        if k.quit then
            ResetWorld g, p, lo(), pt()
        elseif Met%(g, p, k, pt()) then
            if Stage < NSTAGE - 1 then
                GoStage Stage + 1, g, p, lo(), pt()
            else
                ResetWorld g, p, lo(), pt()
            end if
        end if
    case OVER
        if g.modeT > 5 or (g.modeT > 1.5 and (k.hit or k.start)) or k.quit then ResetWorld g, p, lo(), pt()
    end select
    if (Ticks and 1) = 0 then Things g, lo(), pt()
    if Zone = 0 and (Ticks and 15) = 0 then
        i = Ticks \ 16
        Tint CWATER + 1, Glint(i mod 3)
        Tint CWATER + 2, Glint((i + 1) mod 3)
        Tint CWATER + 3, Glint((i + 2) mod 3)
    end if
    k.hit = 0
    k.start = 0
    k.quit = 0
    k.bark = 0
end sub

' The pad of a Charlie that holds right and jumps at pits, walls and cats.
' Whether a ribbon starts within 60 ahead of x: its edge is run off, not jumped.
function Ribbon% (x as integer)
    dim z as integer
    Ribbon% = 0
    for z = 0 to NZ - 1
        if ZM(z) < 0 and GK(ZA(z)) = 2 and ZL(z) > x - 16 and ZL(z) < x + 60 then Ribbon% = -1
    next
end function

' A ribbon is a strip wound once round a line along its way, each twist 2 * WAVEL long: its near edge rises to a
' crest and falls, and what runs it goes round its inside, on its far side going up and behind it coming down.
' How far round p on one is: 0 upright at its foot, PI upside down under its crest; -1 on none.
function Twist! (p as Hero)
    dim z as integer, t as single
    Twist! = -1
    if not p.ground then exit function
    for z = 0 to NZ - 1
        if ZM(z) = -2 and p.sg >= ZA(z) and p.sg <= ZB(z) then
            t = (p.x - ZL(z) - 8) / (2 * WAVEL)
            if t < 0 or p.x > ZR(z) - 8 then t = 0
            Twist! = (t - int(t)) * 2 * PI
            exit function
        end if
    next
end function

sub Pilot (p as Hero, k as Pad)
    static hold as integer
    dim x as integer, y as integer, i as integer, want as integer, m as integer, d as integer, ready as integer
    k.r = -1
    k.l = 0
    if p.ground and GK(p.sg) = 3 then
        ' on a platform: still, till there is ground ahead to jump to
        x = cint(p.x)
        y = cint(p.y)
        k.r = 0
        if p.gsp > 20 then k.l = -1
        i = cint(GX(p.sg)) + MOVEW + 10
        if Surf%(i, y) <> NONE and not Solid%(i, y - 12) then
            k.r = -1
            k.l = 0
            want = -1
        end if
        ' a bridge is run over
        for m = 0 to NMv - 1
            if MvS(m) = p.sg and MvK(m) = 4 then
                k.r = -1
                k.l = 0
                want = 0
            end if
        next
    elseif p.ground and GK(p.sg) = 1 then
        x = cint(p.x)
        y = cint(p.y)
        i = x + 22 + cint(abs(p.gsp)) \ 40
        if Surf%(i, y) = NONE and Surf%(i, y + 2 * TS) = NONE and not Ribbon%(x) then want = -1
        ' a wall, not a ramp's foot
        if Solid%(x + 13, y - 20) then want = -1
        i = First(x \ TS)
        do while i < NObj
            if ObX(i) > x + 50 then exit do
            if ((ObK(i) = KCAT and ObS(i) <> 0) or ObK(i) = KTHORN) and ObX(i) > x then want = -1
            i = i + 1
        loop
        ' a platform's gap ahead: slowly to its edge, there till the platform is at it and staying, then on
        for m = 0 to NMv - 1
            d = MvE(m) - x
            if MvK(m) = 4 then d = NONE
            if d > -4 and d < 300 and abs(MvY(m) + MvA(m) * (MvK(m) = 1) - y) < 200 then
                if d < 50 then want = 0
                if p.gsp > 70 + d then
                    k.r = 0
                    k.l = -1
                else
                    if p.gsp > 30 + d then k.r = 0
                end if
                if d < 10 then
                    select case MvK(m)
                    case 0
                        ready = Swing!(m, 0) < -.85 and Swing!(m, 1) < 0
                    case 1
                        ready = Swing!(m, 0) >= 1 and Swing!(m, 1) > .2
                    case 2, 3
                        ready = MvT(m) = 0
                    end select
                    if ready then
                        k.r = -1
                        k.l = 0
                    else
                        k.r = 0
                        if p.gsp > 5 then k.l = -1
                    end if
                end if
            end if
        next
    elseif not p.ground then
        ' in the air toward a platform's gap: slowed, to come down before it
        for m = 0 to NMv - 1
            d = MvE(m) - cint(p.x)
            if MvK(m) = 4 then d = NONE
            if d > -40 and d < 260 and p.coyote <= 0 and p.vx > d \ 3 and p.sg <> MvS(m) then
                k.r = 0
                k.l = -1
            end if
        next
    end if
    if want and hold = 0 then
        k.hit = -1
        hold = 24
    end if
    if hold > 0 then
        k.jump = -1
        hold = hold - 1
    else
        k.jump = 0
    end if
end sub

' The level as it starts, the title up, Charlie at the start for the pilot.
sub ResetWorld (g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer
    if Stage <> NextUp then LoadLevel NextUp
    g.mode = TITLE
    g.modeT = 0
    Cheer = 0
    MetT = 0
    PastT = 0
    if Dusk < 16 then
        Dusk = 16
        Shade
    end if
    g.lives = 3
    g.score = 0
    g.clock = 0
    g.homeX = StartX
    g.homeY = StartY
    for i = 0 to NObj - 1
        ObX(i) = ObA(i)
        ObS(i) = 0
        if ObK(i) = KBONE then ObS(i) = 1
        if ObK(i) = KCAT then ObS(i) = -1
    next
    for i = 0 to NMOVE
        MvT(i) = 0
    next
    for i = 0 to NLOOSE - 1
        lo(i).t = 0
    next
    for i = 0 to NPART - 1
        pt(i).t = 0
    next
    LooseN = 0
    PartN = 0
    Spawn g, p
end sub

' A level cleared: on to level n, the score and lives kept.
sub GoStage (n as integer, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim sc as long, lv as integer
    sc = g.score
    lv = g.lives
    NextUp = n
    ResetWorld g, p, lo(), pt()
    NextUp = 0
    g.mode = PLAYING
    g.score = sc
    g.lives = lv
end sub

' Past the goal, a game's or the title's: on to the family and no further, the view sliding on to hold them all
' with Charlie at its left; a while with them, then the picture fades. Whether it has.
function Met% (g as Game, p as Hero, k as Pad, pt() as Part)
    dim idle as Pad
    idle.r = p.x < FamX - 108 and not p.win
    MoveHero p, idle, pt()
    if p.x > FamX - 106 then
        p.x = FamX - 106
        p.gsp = 0
        p.vx = 0
        p.roll = 0
    end if
    if p.ground and abs(p.gsp) < 5 and p.x >= FamX - 108 then
        p.win = -1
        p.face = 0
    end if
    Cheer = p.win
    g.camx = g.camx + (FamX - 166 - g.camx) * 4 * STEPT
    PastT = PastT + STEPT
    if p.win and g.bonus = 0 then MetT = MetT + STEPT
    if Dusk = 16 and (MetT > 6 or PastT > 40 or (MetT > 2 and (k.hit or k.start))) then Dusk = 15
    if Dusk < 16 and (Ticks and 3) = 0 then
        Dusk = Dusk - 1
        Shade
    end if
    Met% = Dusk <= 0
end function

sub StartGame (g as Game, p as Hero, lo() as Loose, pt() as Part)
    ResetWorld g, p, lo(), pt()
    g.mode = PLAYING
end sub

sub Spawn (g as Game, p as Hero)
    dim fresh as Hero
    p = fresh
    p.x = g.homeX
    p.y = g.homeY
    p.ground = -1
    p.sg = SegAt%(g.homeX, csng(g.homeY))
    g.bones = 0
    Camera g, p, -1
end sub

sub MoveHero (p as Hero, k as Pad, pt() as Part)
    dim l as integer, r as integer, d as integer, hit as integer, hold as integer
    dim v as single
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
    if k.bark and p.stun = 0 and p.barkT <= 0 then Bark p
    ' a jump pressed just before landing, or just after the ground ran out, was lost: it is kept a moment
    if hit then p.want = JBUFFER
    if p.want > 0 then
        p.want = p.want - STEPT
        if p.ground then
            hit = -1
            p.want = 0
        elseif p.coyote > 0 then
            p.vy = -JUMPV
            p.jumped = -1
            p.roll = -1
            p.coyote = 0
            p.want = 0
            Sfx SJUMP
        end if
    end if
    if p.coyote > 0 then p.coyote = p.coyote - STEPT
    if p.inv > 0 then p.inv = p.inv - STEPT
    p.lastx = p.x
    ' slid off a slope too steep for its speed, the keys are no help a while
    if p.slip > 0 then
        p.slip = p.slip - STEPT
        if p.ground then
            l = 0
            r = 0
        end if
    end if
    if p.ground then
        Walk p, l, r, d, hit, pt()
    else
        Airborne p, l, r, d, hold, pt()
    end if
    if p.x < 8 then
        p.x = 8
        p.gsp = 0
        p.vx = 0
    end if
    if p.x > LWPX - 8 then
        p.x = LWPX - 8
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

' On the ground: wind up, jump, roll, run; then along it, a few pixels a
' move, into walls and onto the ground's curve.
sub Walk (p as Hero, l as integer, r as integer, d as integer, hit as integer, pt() as Part)
    dim sn as single, cs as single, f as single, n as integer, s as integer, dir as integer, i as integer
    sn = sin(p.ang)
    cs = cos(p.ang)
    dir = 1 - 2 * p.face
    if p.charging then
        p.charge = p.charge * (1 - 2 * STEPT)
        if hit then
            p.charge = p.charge + 130
            if p.charge > CHARGEMAX then p.charge = CHARGEMAX
            Sfx SCHARGE
            Burst pt(), p.x - dir * 10, p.y - 2, 3, 70, CSTONE + 3
        end if
        if d then exit sub
        p.charging = 0
        p.roll = -1
        p.gsp = (CHARGE0 + p.charge) * dir
        Sfx SLAUNCH
    elseif hit then
        if p.crouch then
            p.charging = -1
            p.charge = 0
            Sfx SCHARGE
            exit sub
        end if
        p.vx = p.gsp * cs - JUMPV * sn
        p.vy = -p.gsp * sn - JUMPV * cs
        p.ground = 0
        p.roll = -1
        p.jumped = -1
        p.crouch = 0
        Sfx SJUMP
        exit sub
    end if

    p.crouch = d and abs(p.gsp) < 20 and not p.roll
    p.skid = 0
    if p.roll then
        if (p.gsp > 0) = (sn > 0) then
            f = RUPS
        else
            f = RDNS
        end if
        p.gsp = p.gsp - f * sn * STEPT
        if l and p.gsp > 0 then p.gsp = p.gsp - RDEC * STEPT
        if r and p.gsp < 0 then p.gsp = p.gsp + RDEC * STEPT
        Slow p.gsp, RFRIC
        if abs(p.gsp) < 30 and abs(sn) < .2 then p.roll = 0
    else
        if p.gsp <> 0 or abs(sn) > .75 or p.slip > 0 then p.gsp = p.gsp - SLOPE * sn * STEPT
        if p.crouch then
            Slow p.gsp, FRIC * 4
        elseif r then
            if p.gsp < 0 then
                p.gsp = p.gsp + DEC * STEPT
                p.skid = p.gsp < -80
            elseif p.gsp < TOP then
                p.gsp = p.gsp + RUNACC * STEPT
                if p.gsp > TOP then p.gsp = TOP
            end if
            if p.gsp >= 0 then p.face = 0
        elseif l then
            if p.gsp > 0 then
                p.gsp = p.gsp - DEC * STEPT
                p.skid = p.gsp > 80
            elseif p.gsp > -TOP then
                p.gsp = p.gsp - RUNACC * STEPT
                if p.gsp < -TOP then p.gsp = -TOP
            end if
            if p.gsp <= 0 then p.face = 1
        elseif p.slip <= 0 and cs >= .7 then
            ' on steep ground nothing but the slope slows it
            Slow p.gsp, FRIC
        end if
        if d and abs(p.gsp) >= 60 then
            p.roll = -1
            p.crouch = 0
            Sfx SROLL
        end if
        if p.skid and (Ticks and 3) = 0 then Emit pt(), p.x, p.y - 1, -p.gsp * .1, -30, .4, CSTONE + 3
    end if
    if p.gsp > MAXV then p.gsp = MAXV
    if p.gsp < -MAXV then p.gsp = -MAXV

    n = 1 + int(abs(p.gsp) * STEPT / 8)
    for s = 1 to n
        Along p, p.gsp * STEPT / n
        if not p.ground then exit sub
        ' a wall stops it on the tiles' ground; a curve is run up
        i = p.sg
        if GK(i) <> 2 and p.gsp <> 0 then
            if Wall%(p, sgn(p.gsp)) then
                p.gsp = 0
                p.y = GY(i) + (p.x - GX(i)) * GV(i) / GU(i)
                Along p, 0
                if not p.ground then exit sub
            end if
        end if
    next
    ' too slow for a wall or a roof, off it; for a steep slope, down it
    i = p.sg
    if abs(p.gsp) < STALL then
        if GU(i) < .35 then
            Drop p
        elseif GU(i) < .7 then
            p.slip = .5
        end if
        ' and off the roof of a ribbon's twist, down past the ribbon
        if Twist!(p) >= 0 then
            if cos(Twist!(p)) < -.5 then
                Drop p
                p.y = p.y + 26
            end if
        end if
    end if
end sub

' In the air: gravity, steering, a jump cut short when let go; then a few
' pixels a move, into walls, ceilings and onto the ground.
sub Airborne (p as Hero, l as integer, r as integer, d as integer, hold as integer, pt() as Part)
    dim n as integer, s as integer, m as single, y as integer, fell as single, x0 as single, y0 as single
    p.crouch = 0
    p.charging = 0
    p.skid = 0
    p.vy = p.vy + GRAV * STEPT
    if p.vy > 900 then p.vy = 900
    if r then
        if p.vx < TOP then
            p.vx = p.vx + AIRACC * STEPT
            if p.vx > TOP then p.vx = TOP
        end if
        p.face = 0
    elseif l then
        if p.vx > -TOP then
            p.vx = p.vx - AIRACC * STEPT
            if p.vx < -TOP then p.vx = -TOP
        end if
        p.face = 1
    end if
    if p.jumped and not hold and p.vy < -JUMPCUT then p.vy = -JUMPCUT
    m = abs(p.vx)
    if abs(p.vy) > m then m = abs(p.vy)
    n = 1 + int(m * STEPT / 8)
    for s = 1 to n
        x0 = p.x
        y0 = p.y
        p.x = p.x + p.vx * STEPT / n
        if p.vx <> 0 then if Wall%(p, sgn(p.vx)) then p.vx = 0
        p.y = p.y + p.vy * STEPT / n
        fell = p.vy
        if Curve%(p, x0, y0, d) then
            if fell > 420 then Burst pt(), p.x, p.y - 2, 5, 60, CSTONE + 3
            exit for
        end if
        if p.vy < 0 then
            y = cint(p.y) - HH
            if Solid%(cint(p.x), y) or Solid%(cint(p.x) - 4, y) or Solid%(cint(p.x) + 4, y) then
                p.y = (y \ TS + 1) * TS + HH
                p.vy = 0
            end if
        else
            fell = p.vy
            if Land%(p, d) then
                if fell > 420 then Burst pt(), p.x, p.y - 2, 5, 60, CSTONE + 3
                exit for
            end if
        end if
    next
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
sub Touch (g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim i as integer, c as integer, px as integer, py as integer, x as integer, dx as integer
    dim dy as integer, d as single, a as single
    px = cint(p.x)
    py = cint(p.y)
    c = (px - 80) \ TS
    if c < 0 then c = 0
    i = First(c)
    do while i < NObj
        x = ObX(i)
        if x > px + 80 then exit do
        dx = abs(x - px)
        select case ObK(i)
        case KBONE
            if ObS(i) and dx < 12 then
                if abs(ObY(i) - py + 13) < 16 then
                    ObS(i) = 0
                    g.bones = g.bones + 1
                    g.score = g.score + 10
                    Burst pt(), csng(x), csng(ObY(i)), 4, 50, CGOLD + 4
                    Sfx SBONE
                end if
            end if
        case KCAT
            if ObS(i) <> 0 and dx < 15 then
                if abs(ObY(i) - 6 - py + 13) < 17 then
                    if p.roll or p.charging then
                        ObS(i) = 0
                        g.score = g.score + 100
                        g.shake = .15
                        Burst pt(), csng(x), csng(ObY(i) - 6), 10, 130, CWHITE
                        Sfx SPOP
                        if not p.ground and p.vy > 0 then p.vy = -BOUNCEV
                    else
                        Hurt g, p, x, lo(), SHISS
                    end if
                end if
            end if
        case KSPRING
            if dx < 11 and py >= ObY(i) - 12 and py <= ObY(i) + 2 then
                if p.ground or p.vy >= 0 then
                    if p.ground then p.vx = p.gsp
                    p.ground = 0
                    p.vy = -SPRINGV
                    p.y = ObY(i) - 12
                    p.roll = 0
                    p.jumped = 0
                    p.sprung = -1
                    ObS(i) = 8
                    Sfx SSPRING
                end if
            end if
        case KBOOST
            if dx < 12 and p.ground and abs(py - ObY(i)) < 4 and abs(p.gsp) < BOOSTV then
                p.gsp = BOOSTV * (1 - 2 * p.face)
                g.shake = .2
                Burst pt(), p.x, p.y - 4, 8, 150, CGOLD + 4
                Sfx SBOOST
            end if
        case KMARK
            if ObS(i) = 0 and px >= x then
                ObS(i) = 1
                g.homeX = x
                g.homeY = ObY(i)
                Burst pt(), csng(x), csng(ObY(i) - 28), 8, 80, CGOLD + 4
                Sfx SMARK
            end if
        case KGOAL
            if px >= x then ObS(i) = 1
        case KTHORN
            if dx < 12 and abs(py - ObY(i)) < 8 then Hurt g, p, x, lo(), SHURT
        case KBUTTON
            if ObS(i) = 0 and dx < 9 and p.ground and abs(py - ObY(i)) < 4 then
                ObS(i) = 1
                Burst pt(), csng(x), csng(ObY(i) - 6), 8, 80, CGOLD + 4
                Sfx SMARK
            end if
        case KJET
            ' blowing 80 ticks of 180: who is over it goes up
            if (Ticks + x) mod 180 < 80 and dx < 10 and py <= ObY(i) + 2 and py > ObY(i) - 64 and p.vy > -JETV + 60 then
                if p.ground then
                    p.vx = p.gsp
                    Sfx SHISS
                end if
                p.ground = 0
                p.vy = -JETV
                p.roll = 0
                p.jumped = 0
                p.sprung = -1
            end if
        case KBUMP
            dy = py - 12 - ObY(i)
            if ObS(i) = 0 and dx < 20 and abs(dy) < 20 then
                d = sqr(csng(px - x) * (px - x) + csng(dy) * dy)
                if d < 20 then
                    if d < 1 then
                        d = 1
                        dy = -1
                    end if
                    p.ground = 0
                    p.vx = (px - x) / d * BUMPV
                    p.vy = dy / d * BUMPV - 60
                    p.jumped = 0
                    p.sprung = -1
                    ObS(i) = 10
                    g.score = g.score + 10
                    Burst pt(), csng(x), csng(ObY(i)), 6, 90, CGOLD + 4
                    Sfx SSPRING
                end if
            end if
        case KSPIN
            a = Ticks * .06 + x
            dy = ObY(i) - SPINH + cint(SPINR * sin(a))
            if abs(x + cint(SPINR * cos(a)) - px) < 11 and abs(dy - py + 12) < 16 then Hurt g, p, x, lo(), SHURT
        case KHOOP
            ' through it in the air: on, faster, the way Charlie was going
            if ObS(i) = 0 and dx < 9 and abs(ObY(i) - py + 12) < 18 and not p.ground then
                p.vx = HOOPV * (1 + 2 * (p.vx < 0))
                p.vy = -180
                p.sprung = -1
                ObS(i) = 20
                Burst pt(), csng(x), csng(ObY(i)), 8, 120, CGOLD + 4
                Sfx SBOOST
            end if
        end select
        i = i + 1
    loop
    if LooseN = 0 then exit sub
    for i = 0 to NLOOSE - 1
        if lo(i).t > 0 and lo(i).t < 3.3 then
            if abs(lo(i).x - px) < 12 and abs(lo(i).y - py + 13) < 16 then
                lo(i).t = 0
                LooseN = LooseN - 1
                g.bones = g.bones + 1
                Sfx SBONE
            end if
        end if
    next
end sub

' Hit: the bones fly loose and Charlie is knocked back; with none, it is out.
sub Hurt (g as Game, p as Hero, fromX as integer, lo() as Loose, snd as integer)
    dim i as integer, n as integer, a as single, v as single
    if p.inv > 0 then exit sub
    if snd = SHISS then Sfx snd
    if g.bones = 0 then
        Lose p
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
    LooseN = n
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
    if snd = SHURT then Sfx snd
end sub

sub Lose (p as Hero)
    if p.dead then exit sub
    p.dead = -1
    p.deadT = 0
    p.vy = -340
    p.ground = 0
    Sfx SDIE
end sub

' Out: Charlie rises and falls through everything; then the next life.
sub Dying (g as Game, p as Hero)
    p.deadT = p.deadT + STEPT
    p.vy = p.vy + GRAV * STEPT
    p.y = p.y + p.vy * STEPT
    if p.deadT < 2 then exit sub
    g.lives = g.lives - 1
    if g.lives <= 0 then
        g.mode = OVER
        g.modeT = 0
    else
        Spawn g, p
    end if
end sub

' Thirty times a second: cats near the view pace about their homes,
' turning at edges and walls; springs settle; loose bones bounce and
' fade; sparks fall.
' A cat's step: startled, it stands and counts its state down to a prowl's; prowling, a pixel along its beat, turned
' at the beat's end, a drop or a wall.
sub Prowl (i as integer)
    dim x as integer, y as integer, s as integer
    if abs(ObS(i)) > 1 then
        ObS(i) = ObS(i) - sgn(ObS(i))
        exit sub
    end if
    x = ObX(i) + ObS(i)
    y = ObY(i)
    s = Surf%(x + ObS(i) * 8, y)
    if abs(x - ObA(i)) > 40 or abs(s - y) > 6 or Solid%(x + ObS(i) * 8, y - 6) then
        ObS(i) = -ObS(i)
        exit sub
    end if
    ObX(i) = x
    s = Surf%(x, y)
    if abs(s - y) <= 6 then ObY(i) = s
end sub

' A cat barked at from fromX stands startled FLINCH steps, then prowls away from there: its state's size is the
' steps left, its sign the way it will go.
sub Flinch (i as integer, fromX as integer)
    ObS(i) = FLINCH
    if ObX(i) < fromX then ObS(i) = -FLINCH
end sub

' Charlie barks: every cat the bark reaches flinches.
sub Bark (p as Hero)
    dim i as integer, c as integer
    p.barkT = BARKT
    Sfx SBARK
    c = (cint(p.x) - BARKR - 40) \ TS
    if c < 0 then c = 0
    i = First(c)
    do while i < NObj
        if ObA(i) > p.x + BARKR + 40 then exit do
        if ObK(i) = KCAT and ObS(i) <> 0 and abs(ObX(i) - p.x) <= BARKR and abs(ObY(i) - p.y) < 40 then Flinch i, cint(p.x)
        i = i + 1
    loop
end sub

sub Things (g as Game, lo() as Loose, pt() as Part)
    dim i as integer, x as integer, y as integer, s as integer, c as integer
    const DT = STEPT * 2
    c = (cint(g.camx) - 100) \ TS
    if c < 0 then c = 0
    i = First(c)
    do while i < NObj
        if ObA(i) > g.camx + SW + 100 then exit do
        if ObK(i) = KCAT and ObS(i) <> 0 then
            Prowl i
        elseif (ObK(i) = KSPRING or ObK(i) = KBUMP or ObK(i) = KHOOP) and ObS(i) > 0 then
            ObS(i) = ObS(i) - 1
        end if
        i = i + 1
    loop
    if LooseN then
        for i = 0 to NLOOSE - 1
            if lo(i).t > 0 then
                lo(i).t = lo(i).t - DT
                lo(i).vy = lo(i).vy + GRAV * .6 * DT
                lo(i).x = lo(i).x + lo(i).vx * DT
                if Solid%(cint(lo(i).x), cint(lo(i).y) - 3) then lo(i).vx = -lo(i).vx
                lo(i).y = lo(i).y + lo(i).vy * DT
                x = cint(lo(i).x)
                y = cint(lo(i).y) + 4
                s = Surf%(x, y)
                if s <= y and y - s < 14 and lo(i).vy > 0 then
                    lo(i).y = s - 4
                    lo(i).vy = -lo(i).vy * .7
                end if
                if lo(i).y > MH * TS + 16 then lo(i).t = 0
                if lo(i).t <= 0 then LooseN = LooseN - 1
            end if
        next
    end if
    if PartN then
        for i = 0 to NPART - 1
            if pt(i).t > 0 then
                pt(i).t = pt(i).t - DT
                pt(i).x = pt(i).x + pt(i).vx * DT
                pt(i).y = pt(i).y + pt(i).vy * DT
                pt(i).vy = pt(i).vy + 300 * DT
                if pt(i).t <= 0 then PartN = PartN - 1
            end if
        next
    end if
end sub

sub Emit (pt() as Part, x as single, y as single, vx as single, vy as single, t as single, c as integer)
    dim i as integer
    if PartN >= NPART then exit sub
    for i = 0 to NPART - 1
        if pt(i).t <= 0 then
            pt(i).x = x
            pt(i).y = y
            pt(i).vx = vx
            pt(i).vy = vy
            pt(i).t = t
            pt(i).c = c
            PartN = PartN + 1
            exit sub
        end if
    next
end sub

' n sparks out from x, y at speed v.
sub Burst (pt() as Part, x as single, y as single, n as integer, v as single, c as integer)
    dim i as integer, a as single
    for i = 0 to n - 1
        a = 2 * PI * (i + rnd * .5) / n
        Emit pt(), x, y, cos(a) * v, sin(a) * v - v * .5, .5, c
    next
end sub

' The view ahead of Charlie's run, kept in the level; snap puts it there.
sub Camera (g as Game, p as Hero, snap as integer)
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
    ty = p.y - 124
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
    if g.camx > LWPX - SW then g.camx = LWPX - SW
    if g.camy < 0 then g.camy = 0
    if g.camy > MH * TS - SH then g.camy = MH * TS - SH
end sub

' Back to front, then the screen's words.
sub Render (g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim cx as integer, cy as integer, x as integer
    cx = int(g.camx)
    cy = int(g.camy)
    if g.shake > 0 then cy = cy + ((Ticks and 2) - 1) * 2
    if g.mode <> TITLE then Hud g
    DrawBack cx, cy
    DrawThings cx, cy, -1
    DrawTiles cx, cy
    DrawThings cx, cy, 0
    if NMv then DrawCables cx, cy
    if LooseN then DrawLoose cx, cy, lo()
    DrawHero p, cx, cy
    DrawOver cx, cy
    if p.barkT > BARKT - .2 then DrawBark p, cx, cy
    if PartN then DrawParts pt(), cx, cy
    select case g.mode
    case TITLE
        Blit (SW - 170) \ 2, 22, ITITLE, 170, 26
        Blit (SW - 170) \ 2, 52, IDASH, 170, 26
        Panel 0, 181, SW - 1, SH - 1
        if (Ticks \ 30) and 1 then
            Banner 24, "PRESS SPACE", CYELLOW
        else
            Banner 24, "SPACE JUMPS  DOWN ROLLS  X BARKS", CWHITE
        end if
    case PLAYING
        Blit 6, 5, IHUD, HUDW, HUDH
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
            Blit x + (SW - 178) \ 2, 89, ZCard, 178, 18
            if Stage > 1 and x = 0 then Txt 14, 17, "ACT 2", CYELLOW
        end if
    case CLEARED
        Blit 6, 5, IHUD, HUDW, HUDH
        ' up and to the right: under it Charlie meets its family
        Panel 124, 4, 315, 36
        if Zone then
            Txt 2, 17, "CHROME WORKS CLEARED", CYELLOW
        else
            Txt 2, 18, "AMBER DOWNS CLEARED", CYELLOW
        end if
        Txt 3, 22, "BONUS " + Num$(g.bonus, 6), CWHITE
        Txt 4, 22, "SCORE " + Num$(g.score, 6), CWHITE
        if p.win then Say p, cx, cy, "WOOF!"
    case OVER
        Blit 6, 5, IHUD, HUDW, HUDH
        Panel 108, 68, 211, 92
        Banner 10, "GAME OVER", CYELLOW
    end select
end sub

' The far backdrop an eighth of the view's scroll, the near hills half,
' both wrapping; the hills sink as the view rises.
sub DrawBack (cx as integer, cy as integer)
    dim o as integer, y as integer
    o = (cx \ 8) mod SW
    put (-o, 0), Img(At(ZBack)), pset
    if o then put (SW - o, 0), Img(At(ZBack)), pset
    o = (cx \ 2) mod SW
    y = 132 - cy \ 6
    if y >= SH then exit sub
    put (-o, y), Img(At(ZBack + 2)), and
    put (-o, y), Img(At(ZBack + 1)), or
    if o then
        put (SW - o, y), Img(At(ZBack + 2)), and
        put (SW - o, y), Img(At(ZBack + 1)), or
    end if
end sub

' Each column in view: its top tile, then its earth in strips below.
sub DrawTiles (cx as integer, cy as integer)
    dim tx as integer, tx1 as integer, x as integer, y as integer, v as integer, k as integer, r as integer
    tx1 = cx \ TS + SW \ TS
    if tx1 > LW - 1 then tx1 = LW - 1
    for tx = cx \ TS to tx1
        y = ColTop(tx) * TS - cy
        if y < SH then
            x = tx * TS - cx
            v = ColVis(tx)
            r = ColTop(tx)
            if Zone then
                if y > -TS then
                    if v <= 2 then
                        put (x, y), Img(At(IDK + (tx and 3))), pset
                    else
                        Blit x, y, IZS + ((v - 4) * 4 + (tx and 3)) * 2, TS, TS
                    end if
                end if
                y = y + TS
                ' The works' wall hangs from the world, not the deck: single tiles down to its next 128, then strips.
                k = (tx and 3) * 8
                r = r + 1
                do while (r and 7) <> 0 and y < SH
                    if y > -TS then put (x, y), Img(At(IWT + k + (r and 7))), pset
                    r = r + 1
                    y = y + TS
                loop
                do while y < SH
                    if y > -128 then put (x, y), Img(At(IWS + (tx and 3))), pset
                    y = y + 128
                loop
                ' a slope's deck runs on over the wall under it
                y = (ColTop(tx) + 1) * TS - cy
                if v > 2 and y < SH and y > -TS then Blit x, y, IZU + ((v - 4) * 4 + (tx and 3)) * 2, TS, TS
            else
                ' The downs' comb hangs from the world too, of period 64 by 48: a slope's turf runs on into the tile
                ' under it; under a pipe, 0, the pipe's own picture is the ground.
                k = (tx and 3) * 3
                if y > -TS then
                    if v > 2 then
                        put (x, y), Img(At(IAS + ((v - 4) * 12 + k + r mod 3) * 2 + 1)), and
                        put (x, y), Img(At(IAS + ((v - 4) * 12 + k + r mod 3) * 2)), or
                    elseif v > 0 then
                        put (x, y), Img(At(IAF + k + r mod 3)), pset
                    end if
                end if
                y = y + TS
                if v > 2 and y < SH then
                    if y > -TS then put (x, y), Img(At(IAU + (v - 4) * 12 + k + r mod 3)), pset
                    y = y + TS
                    r = r + 1
                end if
                r = r + 1
                do while r mod 3 <> 0 and y < SH
                    if y > -TS then put (x, y), Img(At(IAC + k + r mod 3)), pset
                    r = r + 1
                    y = y + TS
                loop
                do while y < SH
                    if y > -STRIPH then put (x, y), Img(At(IAL + (tx and 3))), pset
                    y = y + STRIPH
                loop
                if FaceL(tx) then Bare x, ColTop(tx) * TS - cy, FaceL(tx), IAE
                if FaceR(tx) then Bare x, ColTop(tx) * TS - cy, FaceR(tx), IAE + 4
            end if
        end if
    next
end sub

' The things in view: back, the scenery behind the ground; else the rest.
sub DrawThings (cx as integer, cy as integer, back as integer)
    dim i as integer, c as integer, x as integer, y as integer, k as integer, f as integer, a as single
    c = (cx - WAVEL) \ TS
    if c < 0 then c = 0
    i = First(c)
    do while i < NObj
        x = ObX(i) - cx
        if x > SW + 96 then exit do
        k = ObK(i)
        if (k >= KLOOP) = back then
            y = ObY(i) - cy
            select case k
            case KBONE
                if ObS(i) then Blit x - 5, y - 5, IBONE + ((Ticks \ 6) and 3) * 2, 10, 10
            case KCAT
                if ObS(i) then Blit x - 10, y - 13, CatPic%(ObS(i)), 20, 14
            case KSPRING
                Blit x - 8, y - 15, ISPRING - 2 * (ObS(i) > 0), 16, 16
            case KBOOST
                Blit x - 12, y - 9, IBOOST + ((Ticks \ 4) and 1) * 2, 24, 10
            case KMARK
                Blit x - 6, y - 31, IMARK + 2 * ObS(i), 12, 32
            case KGOAL
                f = 0
                if ObS(i) then f = (Ticks \ 4) and 3
                Blit x - 12, y - 47, IGOAL + f * 2, 24, 48
            case KPLAT
                Blit x, y, ZTile + 18, TS, TS
            case KLOOP
                Blit x - LD \ 2, y - LOOPR - LD \ 2, ZLoop, LD, LD
            case KPIPE
                if Zone then
                    Blit x - PIPER, y - PIPER, ZPipe, PIPER, PIPER
                else
                    k = IAP + (((ObX(i) \ TS + 1) and 3) * 3 + (ObY(i) \ TS) mod 3) * 2
                    Blit x - PIPER, y - PIPER, k, PIPER, PIPER + TS
                end if
            case KMOVE
                if Zone then
                    Blit x, y, IMOVE, MOVEW, 12
                else
                    Blit x, y, IAM, MOVEW, 12
                end if
            case KPOST
                Blit x - 7, y - 47, IPOST, 16, 48
            case KTANK
                Blit x - 16, y - 39, ITANK, 32, 40
            case KVENT
                Blit x - 8, y - 27, IVENT, 16, 28
            case KTREE
                Blit x - 32, y - 78, ITREE, 64, 80
            case KBUSH
                Blit x - 20, y - 18, IBUSH, 40, 20
            case KFLOWER
                Blit x - 6, y - 14, IFLOWER + ((Ticks \ 30 + i) and 1) * 2, 12, 16
            case KROCK
                Blit x - 10, y - 10, IROCK, 20, 12
            case KTHORN
                Blit x - 8, y - 10, ITHORN, 16, 12
            case KFAM
                ' who meets Charlie: they cheer once it has come to them
                f = 0
                if Cheer then f = (Ticks \ 14) and 1
                Blit x - 90, y - 81, IFAM + f * 2, 180, 82
            case KLAMP
                Blit x - 10, y - 42, ILAMP, 20, 44
            case KJET
                Blit x - 8, y - 6, IJET, 16, 6
                if (Ticks + ObX(i)) mod 180 < 80 then Blit x - 6, y - 70, IJET + 2 + ((Ticks \ 3) and 1) * 2, 12, 64
            case KBUMP
                Blit x - 12, y - 12, IBUMP - 2 * (ObS(i) > 0), 24, 24
            case KSPIN
                line (x - 1, y - SPINH)-(x, y - 1), CINK, BF
                a = Ticks * .06 + ObX(i)
                for f = 1 to 3
                    pset (x + cint(SPINR * f / 4 * cos(a)), y - SPINH + cint(SPINR * f / 4 * sin(a))), CWHITE
                next
                Blit x - 4, y - SPINH - 4, ISPIN + 2, 8, 8
                Blit x + cint(SPINR * cos(a)) - 8, y - SPINH + cint(SPINR * sin(a)) - 8, ISPIN, 16, 16
            case KBUTTON
                Blit x - 8, y - 8, IBTN + 2 * ObS(i), 16, 8
            case KWAVE
                ' a twist's far half; its near one is drawn over Charlie
                Blit x, y - WAVEA - 22, ZWave, WAVEL, WAVEH
            case KHOOP
                Blit x - 6, y - 16, IHOOP - 2 * (ObS(i) > 0), 12, 32
            end select
        end if
        i = i + 1
    loop
end sub

' The cables and what their baskets hang by.
sub DrawCables (cx as integer, cy as integer)
    dim m as integer, x as integer, y as integer, x1 as integer, bx as integer, by as integer, i as integer
    for m = 0 to NMv - 1
        if MvK(m) = 3 then
            x = MvE(m) - 8 - cx
            y = MvY(m) - CABLEH - cy
            x1 = x + MvA(m) + MOVEW
            if x < SW and x1 >= 0 then
                line (x, y)-(x1, y + CABLED), CINK
                line (x, y + 1)-(x1, y + CABLED + 1), CYELLOW
                line (x - 1, y - 2)-(x, y + CABLEH), CINK, BF
                line (x1, y + CABLED - 2)-(x1 + 1, y + CABLED + CABLEH), CINK, BF
                bx = ObX(MvO(m)) - cx
                by = ObY(MvO(m)) - cy
                for i = 4 to MOVEW - 4 step MOVEW - 8
                    line (bx + i, by)-(bx + i, y + 1 + clng(bx + i - x) * CABLED \ (x1 - x)), CINK
                next
            end if
        end if
    next
end sub

sub DrawLoose (cx as integer, cy as integer, lo() as Loose)
    dim i as integer, k as integer
    k = IBONE + ((Ticks \ 6) and 3) * 2
    for i = 0 to NLOOSE - 1
        if lo(i).t > 1 or (lo(i).t > 0 and (Ticks and 4)) then
            Blit cint(lo(i).x) - 5 - cx, cint(lo(i).y) - 5 - cy, k, 10, 10
        end if
    next
end sub

' Charlie, and streaks of wind behind it at speed; running on a slope, the
' frame turned to it.
' What is nearer than Charlie: the near half of each ribbon's twist, which hides what runs behind it.
sub DrawOver (cx as integer, cy as integer)
    dim i as integer, c as integer, x as integer
    c = (cx - WAVEL) \ TS
    if c < 0 then c = 0
    i = First(c)
    do while i < NObj
        x = ObX(i) - cx
        if x > SW then exit do
        if ObK(i) = KWAVE2 then Blit x, ObY(i) - cy - WAVEA - 22, ZWave + 2, WAVEL, WAVEH
        i = i + 1
    loop
end sub

' A cat's picture by its state: startled, turned to what barked, or a stride of its prowl the way it goes.
function CatPic% (s as integer)
    if abs(s) > 1 then
        CatPic% = ICATF - 2 * (s > 0)
    else
        CatPic% = ICAT + ((Ticks \ 8) and 1) * 2 - 4 * (s < 0)
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

sub DrawHero (p as Hero, cx as integer, cy as integer)
    dim x as integer, y as integer, j as integer, d as integer, v as single, sy as integer, n as integer
    dim f as integer, t as integer, sx as integer, hu as single, hv as single, a as single, tw as integer
    if p.inv > 0 and not p.stun and (Ticks and 4) then exit sub
    x = cint(p.x) - cx
    y = cint(p.y) - cy
    ' the ground's lie under it; round a ribbon's twist it keeps to the inside, over the line then under it
    hu = 1
    if p.ground then
        hu = GU(p.sg)
        hv = GV(p.sg)
        if Twist!(p) >= 0 then
            a = Twist!(p)
            hu = cos(a)
            hv = 0
            tw = -1
        end if
    end if
    if p.ground then
        v = p.gsp
    else
        v = p.vx
    end if
    if abs(v) > 400 and not p.dead then
        d = sgn(v)
        for j = 0 to 3
            sy = y - cint(14 * hu) - 12 + (Ticks * 7 + j * 11) mod 22
            n = 12 + (Ticks * 5 + j * 9) mod 18
            line (x - d * 14, sy)-(x - d * (14 + n), sy), CWHITE
        next
    end if
    f = HeroFrame%(p)
    ' running round a twist: rolled with the ribbon, a twelfth of a turn a picture, its paws' line 12 off its middle
    if tw and f >= FRUN and f < FBALL then
        t = cint(a * 6 / PI) mod 12
        if f >= FDASH then
            j = f - FDASH
        else
            j = (f - FRUN) \ 3
        end if
        Blit x - 16, y - 16 - cint(12 * cos(t * PI / 6)), ITW + (p.face * 24 + t * 2 + j) * 2, 32, 32
        exit sub
    end if
    ' running, turned to the ground: t sixteenths of a turn, nose up
    if p.ground and f >= FRUN and f < FBALL then
        v = p.ang
        if p.face then v = -v
        t = cint(v * 8 / PI)
        if t < 0 then t = t + 16
        t = t and 15
    end if
    if t = 0 then
        ' curled on the ground: its middle 13 off it, whichever way that is
        if p.ground and f >= FBALL and f < FBALL + 4 then
            x = x + cint(13 * hv)
            y = y + 13 - cint(13 * hu)
        end if
        ' afraid, it shakes
        if f = FHURT or f = FSHIVER then x = x + ((Ticks \ 2) and 1) * 2 - 1
        Blit x - 16, y - 31, IHERO + (p.face * NF + f) * 2, 32, 32
        exit sub
    end if
    ' turned about its middle, 12 off the ground: the picture as drawn (0) or mirrored to face left (1); past
    ' half a turn the one half a turn back upside down, mirrored (3) or not (2)
    if f >= FDASH then
        f = 3 + f - FDASH
    else
        f = (f - FRUN) \ 2
    end if
    j = p.face
    if t >= 8 then
        t = t - 8
        j = 3 - j
    end if
    x = x + cint(12 * hv) - 16 + (j and 1)
    y = y - cint(12 * hu) - 18 - 5 * (j > 1)
    Blit x, y, ITILT + (j * 40 + t * 5 + f) * 2, 32, 32
end sub

function HeroFrame% (p as Hero)
    if p.dead or p.stun then
        HeroFrame% = FHURT
        if (Ticks \ 3) and 1 then HeroFrame% = FSHIVER
    elseif p.roll or p.charging then
        HeroFrame% = FBALL + (int(p.anim) and 3)
    elseif not p.ground then
        if p.sprung then
            HeroFrame% = FUP
        else
            HeroFrame% = FRUN + int(p.anim) mod 6
        end if
    elseif p.skid then
        HeroFrame% = FSKID
    elseif p.crouch then
        HeroFrame% = FCROUCH
    elseif p.win then
        HeroFrame% = FWIN + ((Ticks \ 20) and 1)
    elseif abs(p.gsp) < 5 then
        if p.idleT > 3 then
            HeroFrame% = FTAP + ((Ticks \ 12) and 1)
        elseif Ticks mod 180 < 8 then
            HeroFrame% = FBLINK
        else
            HeroFrame% = FIDLE
        end if
    elseif abs(p.gsp) >= TOP - 5 then
        HeroFrame% = FDASH + (int(p.anim) and 1)
    else
        HeroFrame% = FRUN + int(p.anim) mod 6
    end if
end function

sub DrawParts (pt() as Part, cx as integer, cy as integer)
    dim i as integer, x as integer, y as integer
    for i = 0 to NPART - 1
        if pt(i).t > 0 then
            x = cint(pt(i).x) - cx
            y = cint(pt(i).y) - cy
            if x >= 0 and x < SW - 1 and y >= 0 and y < SH - 1 then
                pset (x, y), pt(i).c
                if pt(i).t > .25 then pset (x + 1, y), pt(i).c
            end if
        end if
    next
end sub

' The score, time, bones and lives, top left over the scene: a picture
' and its mask, put together from the characters when a number moves.
' Called before the frame is drawn: it draws on the page's corner.
sub Hud (g as Game)
    dim s as integer, sig as long
    s = int(g.clock)
    sig = g.score * 7 + g.bones * 131& + g.lives * 100003 + s * 13& + 1
    if sig = g.sig then exit sub
    g.sig = sig
    line (0, 0)-(HUDW - 1, HUDH - 1), 0, BF
    line (0, 100)-(HUDW - 1, 100 + HUDH - 1), 255, BF
    HudLine 0, "SCORE " + Num$(g.score, 6), 6
    HudLine 1, "TIME  " + ltrim$(str$(s \ 60)) + ":" + Num$(clng(s mod 60), 2), 6
    HudLine 2, "BONES " + Num$(clng(g.bones), 3), 6
    HudLine 3, "CHARLIE x" + ltrim$(str$(g.lives)), 7
    get (0, 0)-(HUDW - 1, HUDH - 1), Img(At(IHUD))
    get (0, 100)-(HUDW - 1, 100 + HUDH - 1), Img(At(IHUD + 1))
end sub

' A row of the HUD's picture and mask: s, its first n characters yellow.
sub HudLine (row as integer, s as string, n as integer)
    dim i as integer, k as integer
    for i = 1 to len(s)
        k = instr("0123456789:xABCDEHILMNOPRST", mid$(s, i, 1)) - 1
        if k >= 0 then
            k = IGLYPH + (k - NGLYPH * (i > n)) * 2
            put (i * 9 - 9, row * 10), Img(At(k)), pset
            put (i * 9 - 9, 100 + row * 10), Img(At(k + 1)), pset
        end if
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
