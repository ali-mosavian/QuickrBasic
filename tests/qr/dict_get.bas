' OPTION DIALECT QUICKR
' d.get(k) for any value type: a V or none
type Pet
    name as string
    age as int
end type
enum Mode
    walk
    run
end enum
enum Shape
    circle(r as double)
    square(side as double)
end enum
var pets as dict[string, Pet]
pets["rex"] = Pet("Rex", 4)
let p = pets.get("rex")
if p is not none then print p.name; " "; p.age
let q = pets.get("tom")
print q is none; " "; (pets.get("tom") or else Pet("nobody", 0)).name
var modes as dict[int, Mode]
modes[1] = Mode.run
print modes.get(1) or else Mode.walk; " "; modes.get(2) or else Mode.walk
var shapes as dict[string, Shape]
shapes["c"] = Shape.circle(2)
let s = shapes.get("c")
if s is not none then
    select case s
        case circle(r)
            print "circle "; r
        case square(side)
            print "square "; side
    end select
end if
var pairs as dict[string, (int, string)]
pairs["a"] = (1, "one")
let t = pairs.get("a")
if t is not none then
    let n, w = t
    print n; " "; w
end if
var names as dict[int, string]
names[3] = "three"
print names.get(3) or else "?"; " "; names.get(4) or else "?"
var maybe as dict[string, int or none]
maybe["a"] = none
maybe["b"] = 2
print maybe.get("a") is none; " "; maybe.get("b") or else 0; " "; maybe.get("c") is none
