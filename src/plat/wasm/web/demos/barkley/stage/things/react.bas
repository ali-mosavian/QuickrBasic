' A thing changed: by its rule, or met.
defint A-Z
option explicit

from barkley.model.level import Level
from barkley.model.kinds import Kinds
from barkley.world.terrain.ground import Solid, Surf

' A cat's beat: how far from its home it prowls.
const BEAT = 40

' Cat i a pixel on the way it faces, where its beat, the
' ground and no wall let it: whether it went.
function Paced% (lv as Level, i as integer)
    dim x as integer, y as integer, s as integer

    Paced% = 0
    x = lv.things(i).at.x + lv.things(i).state
    y = lv.things(i).at.y
    s = Surf%(lv, x + lv.things(i).state * 8, y)
    if abs(x - lv.things(i).home) > BEAT or abs(s - y) > 6 _
        or Solid%(lv, x + lv.things(i).state * 8, _
        y - 6) then exit function

    lv.things(i).at.x = x
    s = Surf%(lv, x, y)
    if abs(s - y) <= 6 then lv.things(i).at.y = s
    Paced% = -1
end function

' A cat's step: startled, it stands and counts its state
' down to a prowl's; prowling, a pixel along its beat,
' turned at the beat's end, a drop or a wall.
sub Prowl (lv as Level, i as integer)
    if abs(lv.things(i).state) > 1 then
        lv.things(i).state = lv.things(i).state _
            - sgn(lv.things(i).state)
        exit sub
    end if

    if not Paced%(lv, i) then lv.things(i).state = -lv.things(i).state
end sub

' Thing i to state s.
sub Turn (lv as Level, i as integer, s as integer)
    lv.things(i).state = s
end sub

' Thing i met, to rest: its state its row's rest.
sub Rest (lv as Level, kd as Kinds, i as integer)
    lv.things(i).state = kd.k(lv.things(i).kind).rest
end sub
