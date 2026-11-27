' OPTION DIALECT QUICKR
type Cell
    v as int
end type

function d(c as Cell) as int
    return c.v + 3
end function
