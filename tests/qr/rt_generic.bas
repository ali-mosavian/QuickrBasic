' OPTION DIALECT QUICKR
' a runtime error in an instance: the generic's own line
function at[T](a as T[], i as int) as T
    return a[i]
end function
var xs as int[]
xs.push(1)
print(at(xs, 0))
print(at(xs, 5))
