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

' Something let go at x, y with the speed vx, vy, to fly till it meets Charlie or the ground: its picture, and
' the colour it bursts in. None while NLOB are in the air.
sub Loft (lv as Level, x as single, y as single, vx as single, vy as single, pic as integer, c as integer)
    dim n as integer
    for n = 0 to NLOB - 1
        if not lv.lobs(n).live then exit for
    next
    if n = NLOB then exit sub
    lv.lobs(n).x = x
    lv.lobs(n).y = y
    lv.lobs(n).vx = vx
    lv.lobs(n).vy = vy
    lv.lobs(n).pic = pic
    lv.lobs(n).c = c
    lv.lobs(n).bedded = Solid%(lv, cint(x), cint(y))
    lv.lobs(n).live = -1
    lv.lobN = lv.lobN + 1
end sub

' What is in the air, n, bursts.
private sub Splat (lv as Level, mu as Deck, se as Session, n as integer, pt() as Part)
    lv.lobs(n).live = 0
    lv.lobN = lv.lobN - 1
    Burst se, pt(), lv.lobs(n).x, lv.lobs(n).y, 6, 90, lv.lobs(n).c
    Sfx mu, se, SPOP
end sub

' What is in the air, each on its arc till it meets Charlie, whom it hurts, or the ground.
sub Volley (lv as Level, mu as Deck, se as Session, g as Game, p as Hero, lo() as Loose, pt() as Part)
    dim n as integer, aground as integer
    for n = 0 to NLOB - 1
        if lv.lobs(n).live = 0 then continue for
        lv.lobs(n).vy = lv.lobs(n).vy + GRAV * STEPT
        lv.lobs(n).x = lv.lobs(n).x + lv.lobs(n).vx * STEPT
        lv.lobs(n).y = lv.lobs(n).y + lv.lobs(n).vy * STEPT
        aground = Solid%(lv, cint(lv.lobs(n).x), cint(lv.lobs(n).y))
        if not aground then lv.lobs(n).bedded = 0
        if abs(lv.lobs(n).x - p.at.x) < 10 and abs(lv.lobs(n).y - p.at.y + 13) < 14 then
            Hurt mu, se, g, p, cint(lv.lobs(n).x - sgn(lv.lobs(n).vx) * 10), lo(), SHURT
            Splat lv, mu, se, n, pt()
        elseif (aground and not lv.lobs(n).bedded) or lv.lobs(n).y > MH * TS then
            Splat lv, mu, se, n, pt()
        end if
    next
end sub

' What is in the air.
sub DrawLobs (lv as Level, art as Store, cx as integer, cy as integer)
    dim n as integer
    for n = 0 to NLOB - 1
        if lv.lobs(n).live = 0 then continue for
        Blit art, cint(lv.lobs(n).x) - cx - 4, cint(lv.lobs(n).y) - cy - 4, lv.lobs(n).pic, 8, 8
    next
end sub
