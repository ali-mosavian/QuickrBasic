' OPTION DIALECT QUICKR
' spelling: print and f-strings spell a record or an enum by its text(), a plain enum by
' its name, a T or none as none or its T
type Vec
    x as double
    y as double

    fn text(self) as string
        return f"({self.x}, {self.y})"
    end fn
end type

enum Mode
    walk
    jump
    roll
end enum

enum Shape
    circle(r as double)
    dot

    fn text(self) as string
        select case self
            case circle(r)
                return f"circle {r}"
            case dot
                return "dot"
        end select
    end fn
end enum

enum Coin
    heads
    tails

    fn text(self) as string
        return "H" if self = Coin.heads else "T"
    end fn
end enum

function pick(n as int) as Mode
    return Mode.roll if n > 2 else Mode.jump
end function

let v = Vec(1, 2.5)
let m = Mode.jump
print v; " "; m; " "; Mode.walk; " "; pick(5); " "; m.text(); " "; Shape.circle(2); " "; Shape.dot
print f"[{v}] [{m:>6}] [{pick(1)}] [{Coin.tails}] {Coin.heads.text()}"
var o as int or none = none
print o; " "; f"{o}"
o = 4
print o
var w as Vec or none = none
print w
w = Vec(0, 1)
print w; " "; int(m)
var ms as Mode or none = Mode.roll
print ms
var d as dict[string, int]
d["a"] = 1
print d.get("a"); " "; d.get("b")
