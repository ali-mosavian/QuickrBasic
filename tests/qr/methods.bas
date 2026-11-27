option dialect quickr
' methods: self, mut self, the type's own, enums', generic ones, as values
type Vec
    x as double
    y as double

    fn length(self) as double
        return (self.x * self.x + self.y * self.y) ^ 0.5
    end fn

    fn scale(mut self, k as double = 2)
        self.x *= k
        self.y *= k
    end fn

    fn polar(r as double, a as double) as Vec
        return Vec(r * a, r)
    end fn

    fn unit(self) as Vec
        let n = self.length()
        return Vec(self.x / n, self.y / n)
    end fn

    fn sum(self) as double
        return self.x + self.y
    end fn

    fn map[U](self, f as function(double) as U) as list[U]
        var out as list[U]
        out.push(f(self.x))
        out.push(f(self.y))
        return out
    end fn
end type

' a function of the same name as a method: each its own
function length(v as Vec) as int
    return 7
end function

enum Shape
    circle(r as double)
    rect(w as double, h as double)
    dot

    fn area(self) as double
        select case self
            case circle(r)
                return 3 * r * r
            case rect(w, h)
                return w * h
            case dot
                return 0
        end select
    end fn
end enum

enum Mode
    walk
    run

    fn speed(self) as int
        return 1 if self = Mode.walk else 4
    end fn
end enum

type Button
    on as function(int) as int
    n as int

    fn press(mut self)
        self.n = self.on(self.n)
    end fn
end type

' a.count() is the chain's, never this
function count(xs as list[Vec]) as int
    return 99
end function

function twice(n as int) as int
    return n * 2
end function

function total[T](xs as list[T]) as double
    var t = 0.0
    for x in xs
        t += x.area()
    next
    return t
end function

var v = Vec(3, 4)
print v.length(); " "; length(v); " "; v.sum()
v.scale(k: 3)
v.scale()
print v.x; " "; v.y
let p = Vec.polar(2, 3)
print p.x; " "; p.y; " "; Vec(6, 8).unit().y
var vs as list[Vec]
vs.push(Vec(5, 12))
vs.push(Vec(0, 1))
print vs[0].length(); " "; vs.map(Vec.length).sum(); " "; vs.count()
for mut w in vs
    w.scale(10)
next
print vs[1].y; " "; v.map(fn(x) x > 10)[1]
let f = Vec.unit
print f(Vec(0, 9)).y
let s = Shape.rect(2, 5)
print s.area(); " "; Shape.circle(1).area(); " "; Shape.dot.area()
var shapes as list[Shape]
shapes.push(s)
shapes.push(Shape.circle(2))
print total(shapes); " "; Mode.run.speed(); " "; Mode.walk.speed()
var b = Button(on: twice, n: 3)
b.press()
b.press()
print b.n; " "; b.on(1)
