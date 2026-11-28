option dialect quickr
from geom import Pt, scale, TOP
import geom
import tally

function far(p as Pt) as int
    var q = p
    scale(q, 3)
    return tally.twice(q.x + TOP + geom.count())
end function
