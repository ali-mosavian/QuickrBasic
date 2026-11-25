' OPTION DIALECT QUICKR
' a module: declarations alone, public ones exported
const MAXW = 319
var plotted = 0
private var hidden = 5
type Point
    x as int
    y as int

    fn norm(self) as int
        return self.x + self.y
    end fn
end type
enum Shade
    dark
    light
end enum
function plot(x as int, y as int)
    plotted += 1
    print "plot"; " "; clip(x); " "; y
end function
private function clip(x as int) as int
    return MAXW if x > MAXW else x
end function
function norm(p as Point) as int
    return p.x + p.y
end function
function largest[T](a as T[]) as T
    var best = a[0]
    for x in a
        if x > best then best = x
    next
    return best
end function
function evens(n as int) yields int
    for i = 0 to n - 1 step 2
        yield i
    next
end function
function apply(xs as int[], f as function(int) as int) as int
    var total = 0
    for x in xs
        total += f(x)
    next
    return total
end function
