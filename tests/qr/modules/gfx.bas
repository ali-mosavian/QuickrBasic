' OPTION DIALECT QUICKR
' a module: declarations alone, public ones exported
public const MAXW = 319
public var plotted = 0
var hidden = 5
public type Point
    x as int
    y as int
end type
public enum Shade
    dark
    light
end enum
public function plot(x as int, y as int)
    plotted += 1
    print("plot", clip(x), y)
end function
function clip(x as int) as int
    return MAXW if x > MAXW else x
end function
public function norm(p as Point) as int
    return p.x + p.y
end function
public function largest[T](a as T[]) as T
    var best = a[0]
    for x in a
        if x > best then best = x
    next
    return best
end function
public function evens(n as int) yields int
    for i = 0 to n - 1 step 2
        yield i
    next
end function
public function apply(xs as int[], f as function(int) as int) as int
    var total = 0
    for x in xs
        total += f(x)
    next
    return total
end function
