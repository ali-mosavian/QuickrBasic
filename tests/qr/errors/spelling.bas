option dialect quickr
' spelling: its signature, and what print and f-strings can't spell
type Vec
    x as double
    fn text(self, n as int) as string
        return ""
    end fn
end type
type Bare
    n as int
end type
enum Shape
    dot
    ring(r as double)
end enum
enum Mode
    walk
end enum
function load() as int or Error
    return 1
end function
print Bare(1); " "; Shape.dot
print f"{Bare(2)} {load()}"
print (1, 2)
print Mode.walk.text(1)
