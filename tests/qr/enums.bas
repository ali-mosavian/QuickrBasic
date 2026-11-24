' OPTION DIALECT QUICKR
' enums, plain and with data; match with values, ranges, is, else
enum Mode
    walk
    jump
    roll
end enum

enum Shape
    circle(r as single)
    rect(w as single, h as single)
    dot
end enum

fn area(s as Shape) as single
    match s
        case circle(r)
            return 3 * r * r
        case Shape.rect(w, h)
            return w * h
        case dot
            return 0
    end match
end fn

fn name(m as Mode) as string
    match m
        case Mode.walk
            return "walk"
        case Mode.jump, Mode.roll
            return "air"
    end match
end fn

print(area(Shape.circle(2)), area(Shape.rect(3, 4)), area(Shape.dot))
print(name(Mode.walk), name(Mode.roll))
var m = Mode.jump
print(m = Mode.jump, m <> Mode.jump, int(m))
for i in 0..10
    match i
        case 1, 2
            print(i, "small")
        case 3 to 5
            print(i, "mid")
        case is > 7
            print(i, "big")
        case else
            print(i, "other")
    end match
end for
let k = "b"
match k
    case "a"
        print("A")
    case "b"
        print("B")
    case else
        print("?")
end match
