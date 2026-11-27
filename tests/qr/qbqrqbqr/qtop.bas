' OPTION DIALECT QUICKR
import cmid
from qleaf import Cell

function b(c as Cell) as int
    return cmid.c(c) + 1
end function
