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
public fn plot(x as int, y as int)
    plotted += 1
    print("plot", clip(x), y)
end fn
fn clip(x as int) as int
    return MAXW if x > MAXW else x
end fn
public fn norm(p as Point) as int
    return p.x + p.y
end fn
public fn largest[T](a as T[]) as T
    var best = a[0]
    for x in a
        if x > best then best = x
    end for
    return best
end fn
public fn evens(n as int) yields int
    for i in 0..n step 2
        yield i
    end for
end fn
public fn apply(xs as int[], f as fn(int) as int) as int
    var total = 0
    for x in xs
        total += f(x)
    end for
    return total
end fn
