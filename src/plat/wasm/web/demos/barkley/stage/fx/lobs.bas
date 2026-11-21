' What is thrown through the air: snowballs and acorns.
defint A-Z
option explicit

from barkley.engine.video import Blit, Store
from barkley.engine.clock import STEPT
from barkley.engine.audio import Deck
from barkley.model.hero import Hero
from barkley.model.level import GRAV, Level, MH, TS
from barkley.model.game import Game, Loose, Part, Session
from barkley.model.world import NLOB
from barkley.assets.tunes import Sfx, SHURT, SPOP
from barkley.world.terrain.ground import Solid
from barkley.stage.fx.parts import Burst
from barkley.stage.fx.damage import Hurt

' Something let go at x, y with the speed vx, vy, to fly
' till it meets Charlie or the ground: its picture, and the
' colour it bursts in. None while NLOB are in the air.
sub Loft ( _
    lv as Level, _
    x as single, _
    y as single, _
    vx as single, _
    vy as single, _
    pic as integer, _
    c as integer _
)
    dim n as integer
    for n = 0 to NLOB - 1
        if not lv.lobs(n).live then exit for
    next
    if n = NLOB then exit sub
    lv.lobs(n).at.x = x
    lv.lobs(n).at.y = y
    lv.lobs(n).vel.x = vx
    lv.lobs(n).vel.y = vy
    lv.lobs(n).pic = pic
    lv.lobs(n).c = c
    lv.lobs(n).bedded = Solid%(lv, cint(x), cint(y))
    lv.lobs(n).live = -1
    lv.n_lobs = lv.n_lobs + 1
end sub

' What is in the air, n, bursts.
private sub Splat ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    n as integer, _
    pt() as Part _
)
    lv.lobs(n).live = 0
    lv.n_lobs = lv.n_lobs - 1
    Burst se, pt(), lv.lobs(n).at.x, lv.lobs(n).at.y, 6, 90, _
        lv.lobs(n).c
    Sfx mu, se, SPOP
end sub

' What is in the air, each on its arc till it meets Charlie,
' whom it hurts, or the ground.
sub Volley ( _
    lv as Level, _
    mu as Deck, _
    se as Session, _
    g as Game, _
    p as Hero, _
    lo() as Loose, _
    pt() as Part _
)
    dim n as integer, aground as integer
    for n = 0 to NLOB - 1
        if lv.lobs(n).live = 0 then continue for
        lv.lobs(n).vel.y = lv.lobs(n).vel.y + GRAV * STEPT
        lv.lobs(n).at.x = lv.lobs(n).at.x + lv.lobs(n).vel.x * STEPT
        lv.lobs(n).at.y = lv.lobs(n).at.y + lv.lobs(n).vel.y * STEPT
        aground = Solid%(lv, cint(lv.lobs(n).at.x), _
            cint(lv.lobs(n).at.y))
        if not aground then lv.lobs(n).bedded = 0
        if abs(lv.lobs(n).at.x - p.at.x) < 10 _
            and abs(lv.lobs(n).at.y - p.at.y + 13) < 14 then
            Hurt mu, se, g, p, _
                cint(lv.lobs(n).at.x - sgn(lv.lobs(n).vel.x) * 10), _
                lo(), SHURT
            Splat lv, mu, se, n, pt()
        elseif (aground and not lv.lobs(n).bedded) _
            or lv.lobs(n).at.y > MH * TS then
            Splat lv, mu, se, n, pt()
        end if
    next
end sub

' What is in the air.
sub DrawLobs (lv as Level, art as Store, cx as integer, cy as integer)
    dim n as integer
    for n = 0 to NLOB - 1
        if lv.lobs(n).live = 0 then continue for
        Blit art, cint(lv.lobs(n).at.x) - cx - 4, _
            cint(lv.lobs(n).at.y) - cy - 4, lv.lobs(n).pic, 8, 8
    next
end sub
