' OPTION DIALECT QUICKR
type Leaf
    v as int
end type
var made = 0
function mk(v as int) as Leaf
    made += 1
    return Leaf(v: v)
end function
