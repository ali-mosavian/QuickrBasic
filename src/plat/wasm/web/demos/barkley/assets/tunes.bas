' Which song plays when, and the sound effects.
' The tunes are in CHARLIE.DAT with the pictures, from tools/charliesong.txt.
defint A-Z
option explicit

from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck, HushSong, LoadSong
from barkley.model.level import Level, NONE
from barkley.model.game import CLEARED, Game, INTRO, OVER, Session, TITLE

const SJUMP = 0, SBONE = 1, SHURT = 2, SSPRING = 3, SPOP = 4, SROLL = 5
const SCHARGE = 6, SLAUNCH = 7, SCLEAR = 8, SDIE = 9, SMARK = 10, SBOOST = 11, SHISS = 12, SBARK = 13, SBUMP = 14
const SSQUAWK = 15, SCRACK = 16, SCLANK = 17
const CAUGHTUP = 2, PACKING = 10, LEFTBEHIND = 11
const ZTUNE = 0
private const THEME = 1
const CAUGHT = 2

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

' Beats of the level's end sounded: by the tune's own clock, by the scene's where no music plays; under none before
' it has begun.
function EndBeat! (mu as Deck, se as Session, g as Game)
    EndBeat! = g.modeT * mu.song(CAUGHT).tempo / 60
    if mu.muted or not mu.tune then exit function
    EndBeat! = -1
    if mu.slot = CAUGHT then EndBeat! = (se.ticks - mu.songAt) * STEPT * mu.song(CAUGHT).tempo / 60
end function

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
