option dialect quickr
' generic records: their errors; a method is checked for an instance once called
type Vec
    x as int
end type
type Box[T]
    v as T

    fn bigger(self, o as Box[T]) as bool
        return self.v > o.v
    end fn
end type
type Pair[A, B]
    a as A
    b as B
end type
enum Shape[T]
    dot
end enum
var b1 as Box
var b2 as Box[int, int]
var b3 as Vec[int]
let b4 = Box[Vec](Vec(1))
print b4.bigger(b4)
let b5 = Pair(1)
let b6 = Box(1).v + "a"
let b7 = largest[int](3)
let b8 = Box[int]
type Twice[T, T]
    v as T
end type
var tw as Twice[int, int]
type Grow[T]
    more as Grow[T[]][]
end type
var g as Grow[int]
type Self[T]
    me as Self[T]
end type
var s as Self[int]
var unused as Box[Box[Vec]]
