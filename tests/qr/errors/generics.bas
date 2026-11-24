' OPTION DIALECT QUICKR
' generics: their errors, an instance's named
type Vec
    x as int
end type
fn largest[T](a as T[]) as T
    var best = a[0]
    for x in a
        if x > best then
            best = x
        end if
    end for
    return best
end fn
fn pair[T](a as T, b as T) as int
    return 0
end fn
fn none_of[T]() as int
    return 0
end fn
var vs as Vec[]
print(largest(vs).x)
print(pair(1, "a"))
print(none_of())
