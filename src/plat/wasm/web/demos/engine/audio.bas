' The song player: songs read, played a half bar at a time,
' faded.
defint A-Z
option explicit

from engine.datafile import Chars, DataFile, OpenData, Word

' Half a bar as PLAY strings: its drums and their letters,
' the bass, the two chord voices, the lead, the first voice
' an octave down, and the third voice. strike: the drums'
' second sounds, on their own voice.
type Half
    drums as string
    strike as string
    kit as string
    bass as string
    one as string
    lead as string
    two as string
    under as string
    third as string
end type

' A song: its half bars, its bars' order and voicings, and
' its kit: the sounds of its lead, bass, low double and
' third voice; by voicing, its letter's place in vo_let, the
' two chord voices' sounds and whether the double plays; by
' drum, its letter's place in dr_let, its sound, how loud,
' what a fade rounds it up by, and its note.
type Song
    half() as Half
    order as string
    voicing as string
    tempo as integer
    bass_in as integer
    lvl(5) as integer
    fade as integer
    entry as integer
    ends as integer
    swing as integer
    lead_p as string
    bass_p as string
    under_p as string
    mid_p as string
    vo_let as string
    dr_let as string
    vo_one() as string
    vo_two() as string
    vo_under() as integer
    dr_patch() as string
    dr_note() as string
    dr_vol() as integer
    dr_up() as integer
    layered as integer
end type

' The music: the slot playing, its bar and half; ended: it
' has ended and nothing plays. z_held: the song in the
' zone's slot, one more than its number in the data file: 0
' before any. dat: the file its songs are in.
type Deck
    song() as Song
    dat as DataFile
    slot as integer
    bar as integer
    half as integer
    ended as integer
    coming as integer
    coming_bar as integer
    fade_out as integer
    fade_in as integer
    stage_tune as integer
    tune as integer
    z_held as integer
    c_held as integer
    song_at as long
    muted as integer
    bark_at as long
end type

' The songs held at once, each in its slot: the zone's tune,
' Charlie's theme, the level's end. A song's most bars,
' voicings and drums.
const NSLOT = 3
const NBAR = 30
const NVOICE = 4
const NDRUM = 8

' Every voice a song plays on cut short, a rest to each.
sub CutVoices ()
    play "P64", "P64", "P64", , "P64", "P64", "P64", _
        "P64", "P64"
end sub

' The song playing dropped where it is: the next picked
' starts at once.
sub HushSong (mu as Deck)
    mu.slot = -1
    mu.ended = -1
    mu.fade_out = 0
end sub

' Song id of the deck's data file into slot s: its order and
' levels, its kit, and its bars as PLAY strings.
sub LoadSong (mu as Deck, s as integer, id as integer)
    dim i as integer, n as integer, bars as integer, octs as integer
    dim songs as long
    dim bas(7) as string, hits(7) as string, pat(7) as string

    OpenData mu.dat
    songs = Word&(mu.dat)
    seek #mu.dat.num, songs + 4 + id * 4
    seek #mu.dat.num, songs + Word&(mu.dat)
    mu.song(s).order = Chars$(mu.dat)
    mu.song(s).voicing = Chars$(mu.dat)
    bars = Word&(mu.dat)
    mu.song(s).tempo = Word&(mu.dat)
    mu.song(s).bass_in = Word&(mu.dat)

    for i = 0 to 5
        mu.song(s).lvl(i) = Word&(mu.dat)
    next

    mu.song(s).fade = Word&(mu.dat)
    mu.song(s).entry = Word&(mu.dat)
    mu.song(s).ends = Word&(mu.dat)
    octs = Word&(mu.dat)
    mu.song(s).swing = Word&(mu.dat)
    ReadKit mu, s
    ReadList mu.dat, bas()
    ReadList mu.dat, hits()
    ReadList mu.dat, pat()

    for n = 0 to bars - 1
        ReadBar mu, s, n, bas(), hits(), pat(), 27 + 12 * octs
    next

    close #mu.dat.num
end sub

' Slot s's kit, the data file's next: four sounds, its
' voicings and its drums.
private sub ReadKit (mu as Deck, s as integer)
    dim i as integer, n as integer

    mu.song(s).lead_p = Chars$(mu.dat)
    mu.song(s).bass_p = Chars$(mu.dat)
    mu.song(s).under_p = Chars$(mu.dat)
    mu.song(s).mid_p = Chars$(mu.dat)
    mu.song(s).vo_let = ""
    n = Word&(mu.dat)

    for i = 0 to n - 1
        mu.song(s).vo_let = mu.song(s).vo_let + Chars$(mu.dat)
        mu.song(s).vo_one(i) = Chars$(mu.dat)
        mu.song(s).vo_two(i) = Chars$(mu.dat)
        mu.song(s).vo_under(i) = Word&(mu.dat)
    next

    mu.song(s).dr_let = ""
    n = Word&(mu.dat)

    for i = 0 to n - 1
        mu.song(s).dr_let = mu.song(s).dr_let + Chars$(mu.dat)
        mu.song(s).dr_patch(i) = Chars$(mu.dat)
        mu.song(s).dr_vol(i) = Word&(mu.dat)
        mu.song(s).dr_up(i) = Word&(mu.dat)
        mu.song(s).dr_note(i) = Chars$(mu.dat)
    next

    mu.song(s).layered = Doubled%(mu.song(s).dr_let)
end sub

' Where the letter at place n of a comes again, or -1.
private function Again% (a as string, n as integer)
    Again% = instr(n + 2, a, mid$(a, n + 1, 1)) - 1
end function

' Whether any of a's letters comes twice: a drum given
' twice is two sounds at once, the second its strike.
private function Doubled% (a as string)
    dim i as integer

    Doubled% = 0

    for i = 0 to len(a) - 2
        if Again%(a, i) >= 0 then Doubled% = -1
    next
end function

' The data file's next list of strings, after its count.
private sub ReadList (df as DataFile, a() as string)
    dim i as integer, n as integer

    n = Word&(df)

    for i = 0 to n - 1
        a(i) = Chars$(df)
    next
end sub

' Slot s's bar n, the data file's next: its root, bass riff
' and drum pattern, and four riffs, made its two half bars'
' PLAY strings, the lead's lowest note `lead`. A second
' voice of two characters is a chord pattern, its x the
' note, an octave down. A third voice of no loudness is left
' out.
private sub ReadBar ( _
    mu as Deck, _
    s as integer, _
    n as integer, _
    bas() as string, _
    hits() as string, _
    pat() as string, _
    lead as integer _
)
    dim root as integer, nb as integer, nd as integer, low as integer
    dim j as integer, h as integer
    dim st as string, br as string, sol as string, mid as string

    root = Word&(mu.dat)
    nb = Word&(mu.dat)
    nd = Word&(mu.dat)
    st = Chars$(mu.dat)
    br = Chars$(mu.dat)
    sol = Chars$(mu.dat)
    mid = Chars$(mu.dat)
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
        mu.song(s).half(h).drums = _
            Beat$(mu, s, mu.song(s).half(h).kit, 8, 0)
        mu.song(s).half(h).strike = ""

        if mu.song(s).layered then
            mu.song(s).half(h).strike = _
                Beat$(mu, s, mu.song(s).half(h).kit, 8, 1)
        end if

        mu.song(s).half(h).bass = Riff$(bas(nb - 1), j, 15 + root, _
            mu.song(s).swing)
        mu.song(s).half(h).one = Riff$(br, j, 27, mu.song(s).swing)
        mu.song(s).half(h).lead = Riff$(sol, j, lead, _
            mu.song(s).swing)
        mu.song(s).half(h).two = Riff$(st, j, low, mu.song(s).swing)
        mu.song(s).half(h).under = Riff$(br, j, 15, mu.song(s).swing)
        mu.song(s).half(h).third = Riff$(mid, j, 27, mu.song(s).swing)
    next
end sub

' Half a bar of slot s's drums, `loud` eighths as loud:
' each of x's letters its drum, any other a rest. layer 1:
' the drums' second sounds, a rest where a drum has none.
function Beat$ ( _
    mu as Deck, _
    s as integer, _
    x as string, _
    loud as integer, _
    layer as integer _
)
    dim q as integer, n as integer, d as string, ln as string

    for q = 1 to 8
        n = instr(mu.song(s).dr_let, mid$(x, q, 1)) - 1
        if layer and n >= 0 then n = Again%(mu.song(s).dr_let, n)
        ' a swung song's sixteenths: a beat's first two a
        ' twelfth each, its last two a twenty-fourth
        ln = "16"
        if mu.song(s).swing then ln = Triplet$(Swung%(q, q))

        if n < 0 then
            d = d + "P" + ln
        else
            if mu.song(s).swing then d = d + "L" + ln
            d = d + mu.song(s).dr_patch(n) _
                + Vol$(mu.song(s).dr_vol(n), loud, _
                mu.song(s).dr_up(n)) + mu.song(s).dr_note(n)
        end if
    next

    Beat$ = d
end function

' PLAY's volume for a level `loud` eighths as loud, rounded
' up by `up` eighths.
private function Vol$ ( _
    level as integer, _
    loud as integer, _
    up as integer _
)
    Vol$ = "V" + ltrim$(str$((level * loud + up) \ 8))
end function

' A bar's sixteenths from its i-th to its j-th, swung, in
' twenty-fourths of a bar: a beat's first two are two each
' and its last two one, so its off-beat eighth comes on the
' triplet's last.
private function Swung% (i as integer, j as integer)
    dim k as integer, n as integer

    for k = i to j
        n = n + 1
        if ((k - 1) and 3) < 2 then n = n + 1
    next

    Swung% = n
end function

' PLAY's length for n twenty-fourths of a bar, of those one
' length says.
private function Triplet$ (n as integer)
    select case n
    case 1
        Triplet$ = "24"
    case 2
        Triplet$ = "12"
    case 3
        Triplet$ = "8"
    case 4
        Triplet$ = "6"
    case 6
        Triplet$ = "4"
    case 8
        Triplet$ = "3"
    case 9
        Triplet$ = "4."
    case else
        Triplet$ = "2"
    end select
end function

' A swung note, a rest where it is 0, n twenty-fourths of a
' bar long; slid: into from the note before. A length no one
' PLAY length says is the next shorter, then a rest.
private function Shuffle$ ( _
    note as integer, _
    slid as integer, _
    n as integer _
)
    dim cut as integer, ln as string, dot as string

    select case n
    case 5, 7, 10
        cut = 1
    case 11
        cut = 2
    end select

    ln = Triplet$(n - cut)

    if right$(ln, 1) = "." then
        dot = "."
        ln = left$(ln, len(ln) - 1)
    end if

    if note then
        ln = "L" + ln + "N" + ltrim$(str$(note)) + dot
    else
        ln = "P" + ln + dot
    end if

    if note > 0 and slid then ln = "K6" + ln + "K0"
    if cut then ln = ln + "P" + Triplet$(cut)
    Shuffle$ = ln
end function

' Half h of a bar's riff as PLAY's notes, its lowest note
' `low`: at most eight, as the drums' always are. swing: its
' sixteenths swung.
private function Riff$ ( _
    p as string, _
    h as integer, _
    low as integer, _
    swing as integer _
)
    dim i as integer, j as integer, r as integer, last as integer
    dim note as integer
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
            note = low _
                + instr("0123456789abcdefghijklmnopqrstuvwxyz", _
                lcase$(c)) - 1

            do while i + r <= last and mid$(p, i + r, 1) = "-"
                r = r + 1
            loop
        end if

        i = i + r

        if swing then
            s = s + Shuffle$(note, c <> lcase$(c), _
                Swung%(i - r, i - 1))
            continue do
        end if

        ' five and seven sixteenths: four and six, then a
        ' rest
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

' The next half bar, once the last is nearly played: the
' drums are the first voice, eight sixteenths every time, so
' PLAY(0) counts the tune's own clock. The last four bars
' fade, an eighth a half bar. The music kept fed: when
' little is left to play, its next half bar.
sub FeedSong (mu as Deck, ticks as long)
    if mu.muted or not mu.tune then exit sub
    if play(0) > 2 then exit sub

    NextHalf mu, ticks
end sub

' The playing song's next half bar to PLAY, its voices each
' their own string; or the song that was coming, begun.
' Nothing once a song has ended.
sub NextHalf (mu as Deck, ticks as long)
    dim b as integer, loud as integer, n as integer

    ' the song picked comes in once this one has faded, or
    ' at once when it has ended
    if mu.coming >= 0 and (mu.fade_out = 0 or mu.ended) then
        StartSong mu, ticks
    end if

    if mu.ended then exit sub

    n = len(mu.song(mu.slot).order)
    b = asc(mid$(mu.song(mu.slot).order, mu.bar + 1, 1)) - 97
    loud = (n - mu.bar) * 2 - mu.half
    if loud > 8 or mu.bar < n - mu.song(mu.slot).fade then loud = 8

    if mu.fade_out > 0 then
        loud = loud * mu.fade_out \ 3
        mu.fade_out = mu.fade_out - 1
    elseif mu.fade_in > 0 then
        loud = loud * (3 - mu.fade_in) \ 3
        mu.fade_in = mu.fade_in - 1
    end if

    PlayHalf mu, mu.song(mu.slot), b, loud
    mu.half = 1 - mu.half
    if mu.half then exit sub

    mu.bar = mu.bar + 1
    if mu.bar < n then exit sub

    mu.bar = 0
    mu.ended = mu.song(mu.slot).ends
end sub

' Song sg's bar b, the half the deck is at, to PLAY, `loud`
' eighths as loud: its voices each their own string. The
' drums' strikes are the ninth.
private sub PlayHalf ( _
    mu as Deck, _
    sg as Song, _
    b as integer, _
    loud as integer _
)
    dim v as integer, h as integer
    dim t as string, d as string, bass as string, under as string
    dim one as string, two as string, solo as string, midv as string
    dim strike as string

    t = "T" + ltrim$(str$(sg.tempo))
    h = b * 2 + mu.half
    d = sg.half(h).drums
    strike = sg.half(h).strike
    if loud < 8 then d = Beat$(mu, mu.slot, sg.half(h).kit, loud, 0)

    if loud < 8 and sg.layered then
        strike = Beat$(mu, mu.slot, sg.half(h).kit, loud, 1)
    end if

    ' the bass waits its bars; the low double plays under
    ' the voicings that have it
    bass = Vol$(sg.lvl(4), loud, 0) + sg.half(h).bass
    if mu.bar < sg.bass_in then bass = "P2"
    v = instr(sg.vo_let, mid$(sg.voicing, b + 1, 1)) - 1
    under = "P2"

    if sg.vo_under(v) then
        under = Vol$(sg.lvl(3), loud, 0) + sg.half(h).under
    end if

    one = t + sg.vo_one(v) + Vol$(sg.lvl(0), loud, 0) + sg.half(h).one
    solo = t + sg.lead_p + Vol$(sg.lvl(1), loud, 7) + sg.half(h).lead
    two = t + sg.vo_two(v) + Vol$(sg.lvl(2), loud, 0) + sg.half(h).two
    midv = t + Vol$(sg.lvl(5), loud, 0) + sg.half(h).third
    play t + d, t + bass, one, , solo, two, t + under, midv, _
        t + strike
end sub

' The song that was coming begins: its kit to the voices
' that keep theirs. In stereo: the first chord voice to the
' left, the second and the low double to the right; the room
' answers little.
sub StartSong (mu as Deck, ticks as long)
    ' its first note sounds once what is queued has: the
    ' sixteenths left of the song before
    mu.song_at = ticks

    if mu.slot >= 0 then
        mu.song_at = ticks + play(0) * 900& \ mu.song(mu.slot).tempo
    end if

    mu.slot = mu.coming
    mu.bar = mu.coming_bar
    mu.half = 0
    mu.coming = -1
    mu.ended = 0
    mu.fade_out = 0
    Patch mu.song(mu.slot)
end sub

' Song sg's kit to the voices that keep theirs.
private sub Patch (sg as Song)
    dim t as string

    t = "T" + ltrim$(str$(sg.tempo))
    play "MB" + t + "Z1,0L16", t + sg.bass_p, t + "R2S4", , _
        t + "R2" + sg.lead_p, t + "R2S12", t + sg.under_p, _
        t + sg.mid_p, t + "L16"
end sub
