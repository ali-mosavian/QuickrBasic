' The player's keys made Charlie's pad; M and N for the
' music.
defint A-Z
option explicit

from engine.input import Held2, Keys, Poll, Saw
from engine.audio import Deck, Mute, ToggleTune
from barkley.model.hero import Pad

private const SCLEFT = 75
private const SCRIGHT = 77
private const SCUP = 72
private const SCDOWN = 80
private const SCSPACE = 57
private const SCZ = 44

sub ReadPad (kb as Keys, mu as Deck, k as Pad)
    dim c as string, t as single

    Poll kb
    t = timer

    do
        c = inkey$

        select case c
        case chr$(27)
            k.quit = -1
        case "m", "M"
            k.mute = -1
        case "n", "N"
            ToggleTune mu
        case chr$(13)
            k.start = -1
        case " "
            k.start = -1
            Saw kb, SCSPACE, t
        case "x", "X"
            k.bark = -1
        case "z", "Z"
            Saw kb, SCZ, t
        case chr$(0) + "K"
            Saw kb, SCLEFT, t
        case chr$(0) + "M"
            Saw kb, SCRIGHT, t
        case chr$(0) + "H"
            Saw kb, SCUP, t
        case chr$(0) + "P"
            Saw kb, SCDOWN, t
        end select
    loop while len(c)

    k.left = Held2%(kb, SCLEFT, t)
    k.right = Held2%(kb, SCRIGHT, t)
    k.down = Held2%(kb, SCDOWN, t)
    k.jump = Held2%(kb, SCSPACE, t) or Held2%(kb, SCZ, t) _
        or Held2%(kb, SCUP, t)
    if k.jump and not k.was then k.hit = -1
    k.was = k.jump
end sub

' M: the sound off, or on again.
sub MuteKey (mu as Deck, k as Pad)
    if k.mute = 0 then exit sub

    Mute mu
    k.mute = 0
end sub

' The presses a step has seen are spent.
sub Spent (k as Pad)
    k.hit = 0
    k.start = 0
    k.quit = 0
    k.bark = 0
end sub
