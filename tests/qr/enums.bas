option dialect quickr
' enums, plain and with data; select case with values, ranges, is, else
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

function area(s as Shape) as single
    select case s
        case circle(r)
            return 3 * r * r
        case Shape.rect(w, h)
            return w * h
        case dot
            return 0
    end select
end function

function name(m as Mode) as string
    select case m
        case walk
            return "walk"
        case Mode.jump, roll
            return "air"
    end select
end function

print area(Shape.circle(2)); " "; area(Shape.rect(3, 4)); " "; area(Shape.dot)
print name(Mode.walk); " "; name(Mode.roll)
var m = Mode.jump
print m = Mode.jump; " "; m <> Mode.jump; " "; int(m)
for i in 0..10
    select case i
        case 1, 2
            print i; " "; "small"
        case 3 to 5
            print i; " "; "mid"
        case is > 7
            print i; " "; "big"
        case else
            print i; " "; "other"
    end select
next
let k = "b"
select case k
    case "a"
        print "A"
    case "b"
        print "B"
    case else
        print "?"
end select
' two different constant enum values compare unequal
print Mode.jump = Mode.walk; " "; Mode.jump <> Mode.walk; " "; Mode.roll = Mode.roll
