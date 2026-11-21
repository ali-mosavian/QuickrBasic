' Which song plays when, and the sound effects.
' The tunes are in CHARLIE.DAT with the pictures, from tools/charliesong.txt.
defint A-Z
option explicit

from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck, HushSong, LoadSong
from barkley.model.level import Level, NONE, Realm
from barkley.model.game import CLEARED, Game, INTRO, OVER, Session
from barkley.model.game import TITLE

const SJUMP = 0
const SBONE = 1
const SHURT = 2
const SSPRING = 3
const SPOP = 4
const SROLL = 5
const SCHARGE = 6
const SLAUNCH = 7
const SCLEAR = 8
const SDIE = 9
const SMARK = 10
const SBOOST = 11
const SHISS = 12
const SBARK = 13
const SBUMP = 14
const SSQUAWK = 15
const SCRACK = 16
const SCLANK = 17
const SGOLD = 18
const STWANG = 19
const SSNATCH = 20
const SMORTAR = 21
const CAUGHTUP = 2
const PACKING = 10
const LEFTBEHIND = 11
const WHISTLED = 19
const ZTUNE = 0
private const THEME = 1
const CAUGHT = 2

' A sound on the fourth voice, the tune's five left playing.
sub Sfx (mu as Deck, se as Session, id as integer)
    dim s as string
    if mu.muted then exit sub
    select case id
    case SJUMP
        s = "L64O4CEGO5C"
    case SBONE
        ' a bark: a low woof slid up into, two voices beating, through a mouth; one at a time
        if abs(se.ticks - mu.bark_at) < 8 then exit sub
        mu.bark_at = se.ticks
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
    case SGOLD
        ' a golden bone: a bright chord run up, its top held
        s = "L32O5EAO6C+EL8A"
    case STWANG
        ' a line trodden: plucked, and sprung back
        s = "L64O2GO3DGO4D"
    case SSNATCH
        ' a bone snatched: the bone's own notes, low and turned over
        s = "L64O3EO2B"
    case SMORTAR
        ' a mortar fired: a thump, and its shell's whistle up
        s = "L64O1CO0GL32O5CEG"
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
    mu.coming_bar = 0
    mu.ended = -1
    mu.tune = -1
end sub

' Song id into the zone's slot, unless it is there: what was playing from the slot is dropped.
sub ZoneTune (mu as Deck, id as integer)
    if mu.z_held = id + 1 then exit sub
    mu.z_held = id + 1
    LoadSong mu, ZTUNE, id
    mu.song(CAUGHT).lead_p = mu.song(ZTUNE).lead_p
    if mu.slot = ZTUNE then HushSong mu
end sub

' Song id into the level's end's slot, unless it is there, the zone's lead its own: what was playing from the
' slot is dropped.
sub SceneTune (mu as Deck, id as integer)
    if mu.c_held = id + 1 then exit sub
    mu.c_held = id + 1
    LoadSong mu, CAUGHT, id
    mu.song(CAUGHT).lead_p = mu.song(ZTUNE).lead_p
    if mu.slot = CAUGHT then HushSong mu
end sub

' The level's end for the level there is: after a chase the longer one, the cat sent packing first; in the quarter
' before its chase the whistled one, the guitar gone.
sub EndTune (lv as Level, zn as Realm, mu as Deck)
    if lv.chase_x <> NONE then
        SceneTune mu, PACKING
    elseif zn.id = 5 then
        SceneTune mu, WHISTLED
    else
        SceneTune mu, CAUGHTUP
    end if
end sub

' Beats of the level's end sounded: by the tune's own clock, by the scene's where no music plays; under none before
' it has begun.
function EndBeat! (mu as Deck, se as Session, g as Game)
    EndBeat! = g.mode_t * mu.song(CAUGHT).tempo / 60
    if mu.muted or not mu.tune then exit function
    EndBeat! = -1
    if mu.slot = CAUGHT then EndBeat! = (se.ticks - mu.song_at) * STEPT * mu.song(CAUGHT).tempo / 60
end function

' The zone tune's clock told the session: by the tune's own ticks while it plays, by the game's where it does not.
sub KeepBeat (mu as Deck, se as Session)
    se.tempo = mu.song(ZTUNE).tempo
    se.pulse = se.ticks * se.tempo
    if mu.muted or not mu.tune or mu.slot <> ZTUNE then exit sub
    se.pulse = (se.ticks - mu.song_at) * se.tempo
end sub

' The slot to play for the game's state: the theme for the title, the level's end or game over, else the zone's
' tune.
function SongFor% (g as Game)
    select case g.mode
    case TITLE
        SongFor% = THEME
    case CLEARED, OVER
        SongFor% = CAUGHT
    case else
        SongFor% = ZTUNE
    end select
end function

' Whether the state's song comes in at once and in full: the level's end, game over, the scene between zones.
function Sudden% (se as Session, g as Game)
    Sudden% = g.mode = CLEARED or g.mode = OVER or (g.mode = INTRO and se.scene_next > 0)
end function

' Slot n's song, when it is not the one playing, from its start or, late, from the bar it comes in at. The one
' playing fades over a bar (half of one, late) and the next fades in over one; late, it comes in full.
sub PickSong (mu as Deck, n as integer, late as integer)
    if n = mu.slot then
        mu.coming = -1
        mu.fade_out = 0
        exit sub
    end if
    if n = mu.coming then exit sub
    mu.coming = n
    mu.coming_bar = 0
    mu.fade_out = 2
    mu.fade_in = 2
    if late = 0 then exit sub
    mu.coming_bar = mu.song(n).entry
    mu.fade_out = 1
    mu.fade_in = 0
end sub
