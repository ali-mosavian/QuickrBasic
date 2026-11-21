' The picture store's layout, as charlieart.py's: each picture's number.
defint A-Z
option explicit

const NF = 23

' Images in art.img from art.at(k); a masked one's mask at k + 1. As charlieart.py's.
const IBG = 0
private const IMID = 1
const ILOOP = 5, ITITLE = 7
const ITILE = 10, IHERO = 34, IBONE = IHERO + NF * 4
const ICAT = IBONE + 8, ISPRING = ICAT + 8, IBOOST = ISPRING + 4, IMARK = IBOOST + 4
const IGOAL = IMARK + 4, ITREE = IGOAL + 8, IBUSH = ITREE + 2, IFLOWER = IBUSH + 2
const IROCK = IFLOWER + 4, ITHORN = IROCK + 2, ILAMP = ITHORN + 2, ITILT = ILAMP + 2
const IDASH = ITILT + 324
const ICARD = IDASH + 2, IGLYPH = ICARD + 2, NGLYPH = 27, IHUD = IGLYPH + NGLYPH * 4
private const IPIPE = IHUD + 2

' The Chrome Works' own: built here, then charlieart.py's.
const IZ2 = IPIPE + 2, ITILE2 = IZ2 + 9, IMOVE = ITILE2 + 24, IPOST = IMOVE + 2, ITANK = IPOST + 2, IVENT = ITANK + 2
const ICARD2 = IVENT + 2
private const IBLD = ICARD2 + 2, IPLANT = IBLD + 12
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

const ICART = IBASKET + 2, ICARTW = ICART + 12

' Sprocket's crane as it goes and bumped, a length of its rail, its crate, the toolbox.
const ICRANE = ICARTW + 4, IRAIL = ICRANE + 4, IWBOX = IRAIL + 2, ITOOLS = IWBOX + 2

const IWET = ITOOLS + 2, ISURF = IWET + 278, ICRAB = ISURF + 2
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
const IFAM = IDUNKED + 2, NFAM = 5, FAMF = 9, IBARON = IFAM + NFAM * FAMF * 2

' The pass's load of snow on its ledge, and down in a heap.
const ILOAD = IBARON + 12, IHEAP = ILOAD + 2

' The wood's keepsake, the Pomchi's ball; a sapling bowed, and sprung; bramble grown, and cut; a hollow log; a
' burrow's mound; a squirrel to the left, and with an acorn held out, and both to the right; an acorn.
const IBALL = IHEAP + 2, ISAPLING = IBALL + 2, IBRAMBLE = ISAPLING + 4, ILOG = IBRAMBLE + 4, IMOUND = ILOG + 2
const ISQUIRREL = IMOUND + 2, IACORN = ISQUIRREL + 8

' The wood's spring, a toadstool, and trodden; a length of the great oak's trunk and its foot; a limb of it to
' the right, near and far, then their mirrors; its hollow; a bracket of fungus and its mirror; the Bramble
' twins, five pictures each: grinning, reaching, a paw out, shocked, sulking.
const ITOAD = IACORN + 2, IOAK = ITOAD + 4, IOAKFOOT = IOAK + 2, ILIMB = IOAKFOOT + 2, IKNOT = ILIMB + 8
const IBRACKET = IKNOT + 2, ITWIN = IBRACKET + 4
' A golden bone, turned as a bone is.
const IGOLD = ITWIN + 20

' The quarter's washing on its lines: a shirt, trousers, a sock; an awning, and trodden; a chimney; a tram.
const ISHIRT = IGOLD + 8, ITROUSERS = ISHIRT + 2, ISOCK = ITROUSERS + 2, IAWNING = ISOCK + 2
const ICHIMNEY = IAWNING + 4, ITRAM = ICHIMNEY + 2, NIMG = ITRAM + 2

' the picture store's LONGs: for what every zone shows and the largest zone's own
const IMGSZ = 490000
