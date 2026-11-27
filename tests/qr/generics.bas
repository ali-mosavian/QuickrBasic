option dialect quickr
' generic functions: monomorphized per use, checked at instantiation
type Vec
    x as int
    y as int
end type
function largest[T](a as T[], better as int) as T
    var best = a[0]
    for x in a
        if x > best then
            best = x
        end if
    next
    return best
end function
function pick[T](c as bool, a as T, b as T) as T
    return a if c else b
end function
function count[T](a as T[], v as T) as int
    var n = 0
    for x in a
        if x = v then
            n += 1
        end if
    next
    return n
end function
var ints as int[]
var strs as string[]
for i in 0..5
    ints.push((i * 7) mod 5)
    strs.push(f"s{(i * 3) mod 4}")
next
print largest(ints, 0); " "; largest(strs, 0)
print pick(true, 1, 2); " "; pick(false, "a", "b"); " "; pick(true, 1.5, 2.5)
print count(ints, 3); " "; count(strs, "s0")
let v = pick(false, Vec(1, 2), Vec(3, 4))
print v.x; " "; v.y
function first[T](a as T[], fallback as T) as T
    return a[0] if len(a) > 0 else fallback
end function
var e as single[]
print first(e, 1.5); " "; first(e, 2); " "; first(ints, 7)
let f = first(e, 1.5)
print f / 3
