option dialect quickr
' A record of a field with drop moves out after a one-return method
' of another module ran on that field: the inlined body's uses were
' placed at that module's tokens, past the move, so it was refused.
from res import Res
type Box
    r as Res
    n as int
end type
function boxed() as Box or Error
    var b = Box(r: Res("r"), n: 0)
    b.n = len(b.r.tag())
    return b
end function
function shown() as bool or Error
    let b = boxed()?
    print b.n; " "; b.r.tag()
    return true
end function
print shown() or else false
