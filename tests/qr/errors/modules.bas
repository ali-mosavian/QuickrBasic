' OPTION DIALECT QUICKR
' modules' errors: each at its own file and line
import lib
import nope
from lib import hidden, shown, missing
function lib()
end function
print lib.shown(); " "; lib.hidden(); " "; lib.count; " "; lib.nothing
var x as lib.Nope
print hidden()
public print 1
if true then
    import lib
end if
