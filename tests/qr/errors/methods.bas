option dialect quickr
' methods: their names, self, and calls on a value or on the type
type Vec
    x as double
    y as double
    fn x(self) as double
        return 1
    end fn
    fn length(self) as double
        return self.x
    end fn
    fn length(self) as double
        return self.y
    end fn
    fn scale(mut self, k as double)
        self.x *= k
    end fn
    fn polar(r as double) as Vec
        return Vec(r, 0)
    end fn
    fn flip(n as int, self) as Vec
        return self
    end fn
    fn typed(self as Vec) as double
        return 0
    end fn
    function old(self) as double
        return 0
    end fn
    fn ended(self) as double
        return 0
    end function
end type
enum Mode
    walk
    fn walk(self) as int
        return 0
    end fn
end enum
function far(self) as int
    return 0
end function
fn top(x as int) as int
    return x
end fn
function length2(v as Vec) as double
    return v.x * v.x
end function
let v = Vec(3, 4)
v.scale(2)
print Vec.length(v); " "; v.polar(1).x; " "; v.nothing(); " "; Vec.nothing(1)
print v.length2(); " "; v.length
var a as list[int]
print a.len()
let s = Vec.scale
print self
