' OPTION DIALECT QUICKR
type Vec
    x as int
    y as int

    fn advance(mut self, v as Vec)
        self.x += v.x
        self.y += v.y
    end fn
end type

var p = Vec(0, 0)
let v = Vec(3, 1)
for i in 0..2000000
    p.advance(v)
    p.x = p.x mod 1000
next
print p.x; " "; p.y
