option dialect quickr
' fn is a method's and a lambda's: a function, its end and its type
' are written function
fn twice(x as int) as int
    return x * 2
end fn
function half(x as int) as int
    return x div 2
end fn
fn third(x as int) as int
    return x div 3
end fn
native fn sqrt(x as double) as double
var f as fn(int) as int = twice
var a as list[int]
a.sort(function(x, y) x < y)
print twice(2); " "; half(4); " "; third(9)
