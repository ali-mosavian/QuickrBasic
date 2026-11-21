' A thing changed: by its rule, or met.
defint A-Z
option explicit

from barkley.model.level import KCAT, KCRAB, KPOUNCE, KSNATCH
from barkley.model.level import KSQUIRREL, KTHROW, KWALKER, Level, TS
from barkley.model.kinds import Kinds
from barkley.world.terrain.ground import Solid, Surf

' A cat's beat: how far from its home it prowls.
const BEAT = 40

' The steps a cat stands startled by a bark, a squirrel is
' cowed and a crab keeps to its shell; a snow-thrower's
' turn; a gull scared, and how near it hears.
private const STARTLED = 24
private const SQFRIGHT = 150
const SHELLT = 150
const THROWC = 75
const GSCARED = 4
private const GULLHEAR = 130

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

' A cat barked at from from_x stands startled STARTLED
' steps, then prowls away from there: its state's size is
' the steps left, its sign the way it will go.
private sub Flinch (lv as Level, i as integer, from_x as integer)
    lv.things(i).state = STARTLED
    if lv.things(i).at.x < from_x then lv.things(i).state = -STARTLED
end sub

' A bark from x, y, carrying r: every cat it reaches
' flinches, every crab goes into its shell, every gull is
' scared, every snow-thrower ducks, every squirrel is cowed,
' every pouncer leaps.
sub Heard ( _
    lv as Level, _
    kd as Kinds, _
    x as single, _
    y as single, _
    r as integer _
)
    dim i as integer, c as integer, dx as single, dy as single
    dim hung as integer, cowed as integer, early as integer

    c = (cint(x) - r - 40) \ TS
    if c < 0 then c = 0

    for i = lv.first(c) to lv.n_things - 1
        if lv.things(i).home > x + r + 40 then exit for

        dx = abs(lv.things(i).at.x - x)
        dy = abs(lv.things(i).at.y - y)
        ' an icicle or a ledge's load is loosed from further
        ' under it than he can jump
        hung = kd.k(lv.things(i).kind).hangs

        if hung and lv.things(i).state = 0 and dx <= r _
            and dy < 140 then
            lv.things(i).state = 1
        end if

        ' and a squirrel on its bough is cowed
        cowed = lv.things(i).kind = KSQUIRREL _
            and lv.things(i).state <> 0 and dx <= r and dy < 140

        if cowed then
            lv.things(i).state = sgn(lv.things(i).state) * SQFRIGHT
        end if

        ' and a pouncer in its bin leaps early, at where he
        ' is
        early = lv.things(i).kind = KPOUNCE _
            and lv.things(i).state = 0 and dx <= r and dy < 40

        if early then
            lv.things(i).state = 1 + 2 * (x < lv.things(i).at.x)
        end if

        if lv.things(i).state = 0 or dx > r _
            or dy >= 40 then continue for

        select case lv.things(i).kind
        case KCAT, KWALKER, KSNATCH
            Flinch lv, i, cint(x)
        case KCRAB
            lv.things(i).state = sgn(lv.things(i).state) * SHELLT
        case KTHROW
            lv.things(i).state = sgn(lv.things(i).state) * THROWC
        end select
    next

    for i = 0 to lv.n_gulls - 1
        if lv.gulls(i).st <= 0 then continue for
        if abs(lv.gulls(i).at.x - x) >= GULLHEAR _
            or abs(lv.gulls(i).at.y _
            - y) >= GULLHEAR then continue for

        lv.gulls(i).st = GSCARED
    next
end sub
