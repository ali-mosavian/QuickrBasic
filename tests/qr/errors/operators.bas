' OPTION DIALECT QUICKR
' function operator: its errors
from shapes import Size
type Vec
    x as int
end type
function operator +(a as int, b as int) as int
    return a + b
end function
function operator *(a as Vec) as Vec
    return a
end function
function operator =(a as Vec, b as Vec) as int
    return 0
end function
function operator -(mut a as Vec, b as Vec) as Vec
    return a
end function
function operator +(a as Vec, b as Vec) as Vec
    return Vec(a.x + b.x)
end function
function operator +(a as Vec, b as Vec) as Vec
    return a
end function
function operator +[T](a as T, b as T) as T
    return a
end function
function operator ^ (a as Vec, b as Vec) as Vec
end function
let v = Vec(1)
let w = v / v
let c = v < v
let d = v < v < v
let s = Size(1, 2) - Size(1, 1)
var u = Vec(2)
u -= v
function operator +(a as Size, b as int) as Size
    return a
end function
function operator *(a as Vec, k as double) as Vec
    return a
end function
function operator *(a as Vec, k as single) as Vec
    return a
end function
let x = v * 2
let y = v * "s"
let z = -v
