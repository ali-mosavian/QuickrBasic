' OPTION DIALECT QUICKR
' a module's operators: public ones used where its type is
public type Size
    w as int
    h as int
end type

public function operator +(a as Size, b as Size) as Size
    return Size(a.w + b.w, a.h + b.h)
end function

function operator -(a as Size, b as Size) as Size
    return Size(a.w - b.w, a.h - b.h)
end function

public function shrink(a as Size) as Size
    return a - Size(1, 1)
end function
