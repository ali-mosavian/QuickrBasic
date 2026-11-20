' The HUD and the screen's words.
defint A-Z
option explicit

from barkley.engine.video import Store
from barkley.model.images import IGLYPH, IHUD, NGLYPH
from barkley.model.hero import Hero
from barkley.model.game import Game
from barkley.assets.palette import CGOLD, CHUD, CWHITE
from barkley.assets.store import HUDH, HUDW

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
private sub HudLine (art as Store, row as integer, s as string, n as integer)
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
