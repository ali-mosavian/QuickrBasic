' OPTION DIALECT QUICKR
' a module's operators: public ones used where its type is
type Size
    w as int
    h as int
end type

function operator +(a as Size, b as Size) as Size
    return Size(a.w + b.w, a.h + b.h)
end function

private function operator -(a as Size, b as Size) as Size
    return Size(a.w - b.w, a.h - b.h)
end function

function shrink(a as Size) as Size
    return a - Size(1, 1)
end function
