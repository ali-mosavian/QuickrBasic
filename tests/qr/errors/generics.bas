' OPTION DIALECT QUICKR
' generics: their errors, an instance's named
type Vec
    x as int
end type
function largest[T](a as T[]) as T
    var best = a[0]
    for x in a
        if x > best then
            best = x
        end if
    next
    return best
end function
function pair[T](a as T, b as T) as int
    return 0
end function
function none_of[T]() as int
    return 0
end function
var vs as Vec[]
print(largest(vs).x)
print(pair(1, "a"))
print(none_of())
