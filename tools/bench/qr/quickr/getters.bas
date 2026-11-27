option dialect quickr
type Vec
    x as int
    y as int

    fn dot(self, o as Vec) as int
        return self.x * o.x + self.y * o.y
    end fn
end type

var p = Vec(0, 0)
let v = Vec(3, 1)
var s = 0
for i in 0..2000000
    p.x = i mod 100
    p.y = i mod 7
    s = (s + p.dot(v)) mod 1000
next
print s
