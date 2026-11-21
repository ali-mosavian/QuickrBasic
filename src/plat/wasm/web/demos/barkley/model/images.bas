' The picture store's layout, as charlieart.py's: each
' picture's number.
defint A-Z
option explicit

const NF = 23

' Images in art.img from art.at(k); a masked one's mask at k
' + 1. As charlieart.py's.
const IBG = 0
private const IMID = 1
const ILOOP = 5
const ITITLE = 7
const ITILE = 10
const IHERO = 34
const IBONE = IHERO + NF * 4
const ICAT = IBONE + 8
const ISPRING = ICAT + 8
const IBOOST = ISPRING + 4
const IMARK = IBOOST + 4
const IGOAL = IMARK + 4
const ITREE = IGOAL + 8
const IBUSH = ITREE + 2
const IFLOWER = IBUSH + 2
const IROCK = IFLOWER + 4
const ITHORN = IROCK + 2
const ILAMP = ITHORN + 2
const ITILT = ILAMP + 2
const IDASH = ITILT + 324
const ICARD = IDASH + 2
const IGLYPH = ICARD + 2
const NGLYPH = 27
const IHUD = IGLYPH + NGLYPH * 4
private const IPIPE = IHUD + 2

' The Chrome Works' own: built here, then charlieart.py's.
const IZ2 = IPIPE + 2
const ITILE2 = IZ2 + 9
const IMOVE = ITILE2 + 24
const IPOST = IMOVE + 2
const ITANK = IPOST + 2
const IVENT = ITANK + 2
const ICARD2 = IVENT + 2
private const IBLD = ICARD2 + 2
private const IPLANT = IBLD + 12
const IDK = IPLANT + 8
const IWT = IDK + 4
const IWS = IWT + 32
const IJET = IWS + 4
const IBUMP = IJET + 6
const ISPIN = IBUMP + 4
const IHOOP = ISPIN + 4
const IWAVE = IHOOP + 4
const IBTN = IWAVE + 8

' Amber Downs' ground, by the comb's column of 4 and row of
' 3: flats, slopes, the tile under each slope, the comb
' alone and in strips, quarter pipes; its moving platform.
const IAF = IBTN + 4
const IAS = IAF + 12
const IAU = IAS + 144
const IAC = IAU + 72
const IAL = IAC + 12
const IAP = IAL + 4
const IAM = IAP + 24
const IAE = IAM + 2

' The works' slopes, by the deck's four 16s: each kind's
' tile, then what of its deck runs on into the tile under.
const IZS = IAE + 8
const IZU = IZS + 48

' Charlie round a ribbon's twist, 8 pictures of two frames,
' then facing left: stood on it, pressed flatter twice, seen
' from over its back, hung under it flatter twice and whole,
' seen from under.
const ITW = IZU + 48
const ICATF = ITW + 96

' the intro's: the van, a wheel at two turns, the van's back
' door open, Charlie at its window and looking back from it;
' the old house, the boxes and one thrown, a hedge, the
' Mob's float, the basket
const IVAN = ICATF + 4
const IWHEEL = IVAN + 2
const IDOOR = IWHEEL + 4
const IPEEK = IDOOR + 2
const IHOUSE = IPEEK + 4
const IBOXES = IHOUSE + 2
const ICRATE = IBOXES + 2
const IHEDGE = ICRATE + 2
const IFLOAT = IHEDGE + 2
const IBASKET = IFLOAT + 6

const ICART = IBASKET + 2
const ICARTW = ICART + 12

' Sprocket's crane as it goes and bumped, a length of its
' rail, its crate, the toolbox.
const ICRANE = ICARTW + 4
const IRAIL = ICRANE + 4
const IWBOX = IRAIL + 2
const ITOOLS = IWBOX + 2

const IWET = ITOOLS + 2
const ISURF = IWET + 278
const ICRAB = ISURF + 2
const IJELLY = ICRAB + 6
const IBUOY = IJELLY + 4
const ICHAIR = IBUOY + 4
const IGULL = ICHAIR + 4
const IPEDAL = IGULL + 8
const ITEDDY = IPEDAL + 4

' The flume: its fall and its end, four frames each of
' running water, the water falling from it, its tank.
const IFLUME = ITEDDY + 2
const ISPOUT = IFLUME + 8
const IRUNNEL = ISPOUT + 8
const ICISTERN = IRUNNEL + 8

' The works' sweeper, two frames going left and their
' mirrors; a spark's carriage dark and two frames alight, a
' tile of its rail; a press's housing, a length of its rod,
' its head.
const ISWEEP = ICISTERN + 2
const ISPARK = ISWEEP + 8
const ISRAIL = ISPARK + 6
const IPRESS = ISRAIL + 2

' The pass's keepsake, the fair one's camera. A sled as the
' ground lies, from a steep fall to the right to a steep
' rise; deep snow.
const ICAMERA = IPRESS + 6
const ISLED = ICAMERA + 2
const IDRIFT = ISLED + 10

' A geyser, where the works have a jet: its mouth, its
' column at two beats. The snowball he is in, at two turns.
const IGEYSER = IDRIFT + 2
const ISNOWBALL = IGEYSER + 6

' A snow-thrower ducked and up, facing left, then their
' mirrors; its snowball.
const IFORT = ISNOWBALL + 4
const ILOB = IFORT + 8
const IICICLE = ILOB + 2
const ISEAT = IICICLE + 2

' A goat's: at graze, then at a charge as the ground lies,
' five ways, two strides each; each with its mirror.
const IGOAT = ISEAT + 2
const IGOATR = IGOAT + 4
const IWALL = IGOATR + 40

' Frost on her toboggan as the ground lies, five ways; and
' knocked into deep snow.
const IFROST = IWALL + 4
const IDUNKED = IFROST + 10

' Who meets Charlie, NFAM of them FAMSTEP apart, each in
' FAMF pictures: three of a wait, six of a cheer. The
' Baron's: four sat, his tail swaying and a blink, then two
' pointing.
const IFAM = IDUNKED + 2
const NFAM = 5
const FAMF = 9
const IBARON = IFAM + NFAM * FAMF * 2

' The pass's load of snow on its ledge, and down in a heap.
const ILOAD = IBARON + 12
const IHEAP = ILOAD + 2

' The wood's keepsake, the Pomchi's ball; a sapling bowed,
' and sprung; bramble grown, and cut; a hollow log; a
' burrow's mound; a squirrel to the left, and with an acorn
' held out, and both to the right; an acorn.
const IBALL = IHEAP + 2
const ISAPLING = IBALL + 2
const IBRAMBLE = ISAPLING + 4
const ILOG = IBRAMBLE + 4
const IMOUND = ILOG + 2
const ISQUIRREL = IMOUND + 2
const IACORN = ISQUIRREL + 8

' The wood's spring, a toadstool, and trodden; a length of
' the great oak's trunk and its foot; a limb of it to the
' right, near and far, then their mirrors; its hollow; a
' bracket of fungus and its mirror; the Bramble twins, five
' pictures each: grinning, reaching, a paw out, shocked,
' sulking.
const ITOAD = IACORN + 2
const IOAK = ITOAD + 4
const IOAKFOOT = IOAK + 2
const ILIMB = IOAKFOOT + 2
const IKNOT = ILIMB + 8
const IBRACKET = IKNOT + 2
const ITWIN = IBRACKET + 4
' A golden bone, turned as a bone is.
const IGOLD = ITWIN + 20

' The quarter's washing on its lines: a shirt, trousers, a
' sock; an awning, and trodden; a chimney; a tram; a paper
' lantern; a dustbin with eyes under its lid, and shut; a
' snatcher, laid out as the cat: two strides right, two
' left, then startled right and left; a mortar; a firework's
' burst opening in three, in two colours; the guitar; the
' relay's five cats, each two strides and bumped; a tipper's
' house, its cat sat and with a paw out.
const ISHIRT = IGOLD + 8
const ITROUSERS = ISHIRT + 2
const ISOCK = ITROUSERS + 2
const IAWNING = ISOCK + 2
const ICHIMNEY = IAWNING + 4
const ITRAM = ICHIMNEY + 2
const IPAPER = ITRAM + 2
const IBIN = IPAPER + 2
const ISNATCH = IBIN + 4
const IMORTAR = ISNATCH + 12
const IBURST = IMORTAR + 2
const IGUITAR = IBURST + 12
const IRELAY = IGUITAR + 2
const ITALL = IRELAY + 30
const ITIPPER = ITALL + 2
const NIMG = ITIPPER + 4

' the picture store's LONGs: for what every zone shows and
' the largest zone's own
const IMGSZ = 490000
