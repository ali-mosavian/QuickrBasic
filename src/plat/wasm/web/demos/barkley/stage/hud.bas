' The HUD and the screen's words.
defint A-Z
option explicit

from engine.video import Checker, DrawBox, PicFill, PicPaste, Store
from engine.video import Txt
from barkley.model.images import IGLYPH, IHUD, NGLYPH
from barkley.model.hero import Hero
from barkley.model.level import NGOLD
from barkley.model.game import Game
from barkley.assets.palette import CGOLD, CHUD, CINK, CWHITE, CYELLOW
from barkley.assets.store import HUDH, HUDW

' Where in the HUD the golden bones' pips begin.
const PIPX = 84
const PIPY = 22

' The score, time, bones and lives, top left over the scene:
' a picture put together from the characters when a number
' moves. gold: the act's golden bones found, a bit each, a
' pip each after the bones; under 0, it has none.
sub Hud (art as Store, g as Game, gold as integer)
    dim s as integer, sig as long, i as integer, x as integer
    dim bit as integer

    s = int(g.clock)
    sig = g.score * 7 + g.bones * 131& + g.lives * 100003 + s * 13& _
        + 1 + (gold + 1) * 15485863&
    if sig = g.sig then exit sub

    g.sig = sig
    PicFill art, IHUD, 0, 0, HUDW - 1, HUDH - 1, 0
    HudLine art, 0, "SCORE " + Num$(g.score, 6), 6
    HudLine art, 1, _
        "TIME  " + ltrim$(str$(s \ 60)) + ":" _
        + Num$(clng(s mod 60), 2), 6
    HudLine art, 2, "BONES " + Num$(clng(g.bones), 3), 6
    HudLine art, 3, "CHARLIE x" + ltrim$(str$(g.lives)), 7
    bit = 1

    for i = 0 to NGOLD - 1
        if gold < 0 then exit for

        x = PIPX + i * 8
        PicFill art, IHUD, x, PIPY, x + 6, PIPY + 6, CINK

        if gold and bit then
            PicFill art, IHUD, x + 1, PIPY + 1, x + 5, PIPY + 5, _
                CYELLOW
        end if

        bit = bit * 2
    next
end sub

' A row of the HUD's picture: s, its first n characters
' yellow.
private sub HudLine ( _
    art as Store, _
    row as integer, _
    s as string, _
    n as integer _
)
    dim i as integer, k as integer

    for i = 1 to len(s)
        k = instr("0123456789:xABCDEHILMNOPRST", mid$(s, i, 1)) - 1
        if k < 0 then continue for

        k = IGLYPH + (k - NGLYPH * (i > n)) * 2
        PicPaste art, IHUD, i * 9 - 9, row * 10, k
    next
end sub

' What Charlie says, in a box over its head.
sub Say ( _
    art as Store, _
    p as Hero, _
    cx as integer, _
    cy as integer, _
    s as string _
)
    dim row as integer, col as integer

    col = (cint(p.at.x) - cx) \ 8 - len(s) \ 2 + 1
    row = (cint(p.at.y) - cy - 42) \ 8
    if col < 2 then col = 2
    if col > 39 - len(s) then col = 39 - len(s)
    if row < 3 then row = 3
    if row > 23 then row = 23
    Panel art, col * 8 - 12, row * 8 - 12, (col + len(s)) * 8 - 5, _
        row * 8 + 3
    Txt art, row, col, s, CWHITE
end sub

' A dark box with a gold edge, for words: every other pixel
' of it, the scene through the rest.
sub Panel ( _
    art as Store, _
    x1 as integer, _
    y1 as integer, _
    x2 as integer, _
    y2 as integer _
)
    Checker art, x1, y1, x2, y2, CHUD
    DrawBox art, x1, y1, x2, y2, CGOLD + 2
end sub

' s centred on row.
sub Banner (art as Store, row as integer, s as string, c as integer)
    Txt art, row, 21 - len(s) \ 2, s, c
end sub

' v in w digits, 0s first.
function Num$ (v as long, w as integer)
    Num$ = right$(string$(w, "0") + ltrim$(str$(v)), w)
end function
