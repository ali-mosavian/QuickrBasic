option dialect quickr
' a runtime error in an instance: the generic's own line
function at[T](a as list[T], i as int) as T
    return a[i]
end function
var xs as list[int]
xs.push(1)
print at(xs, 0)
print at(xs, 5)
