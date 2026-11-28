option dialect quickr
function fine() as int
    return 1
end function

function list(xs as list[int]) as int
    return len(xs)
end function

function ident[T](x as T) as T
    return x
end function

function maybe(x as int) as int or none
    return none
end function
