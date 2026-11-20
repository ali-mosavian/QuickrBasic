' The player's keys made Charlie's pad; M and N for the music.
defint A-Z
option explicit

from barkley.engine.input import Held2, Keys, Poll
from barkley.engine.audio import Deck
from barkley.model.hero import Pad

private const SCLEFT = 75, SCRIGHT = 77, SCUP = 72, SCDOWN = 80, SCSPACE = 57, SCZ = 44

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

' M: the sound off, or on again.
sub MuteKey (mu as Deck, k as Pad)
    if k.mute = 0 then exit sub
    mu.muted = not mu.muted
    k.mute = 0
    if mu.muted then play "P64", "P64", "P64", , "P64", "P64", "P64", "P64"
end sub

' The presses a step has seen are spent.
sub Spent (k as Pad)
    k.hit = 0
    k.start = 0
    k.quit = 0
    k.bark = 0
end sub
