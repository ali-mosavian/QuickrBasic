option dialect quickr
' math: numbers' functions, each the VM's op
const pi = 3.141592653589793

native function sqrt(x as double) as double
native function sin(x as double) as double
native function cos(x as double) as double
native function tan(x as double) as double
native function atan(x as double) as double
native function exp(x as double) as double
native function log(x as double) as double
native function floor(x as double) as double
native function ceil(x as double) as double
' floor(x + 0.5)
native function round(x as double) as double
' atan(y / x) in x's and y's quadrant; pi / 2 or -pi / 2 on y's
' axis, 0 at 0
native function atan2(y as double, x as double) as double
' -x if x < 0 else x
native function abs[T](x as T) as T
' b if b < a else a
native function min[T](a as T, b as T) as T
' b if b > a else a
native function max[T](a as T, b as T) as T
' lo if x < lo, hi if x > hi, else x
native function clamp[T](x as T, lo as T, hi as T) as T
' a + (b - a) * t
native function lerp(a as double, b as double, t as double) as double

' a 2-d vector
type Vec
    x as double
    y as double

    fn dot(self, b as Vec) as double
        return self.x * b.x + self.y * b.y
    end fn

    fn length(self) as double
        return sqrt(self.x * self.x + self.y * self.y)
    end fn

    fn text(self) as string
        return f"({self.x}, {self.y})"
    end fn
end type

function operator +(a as Vec, b as Vec) as Vec
    return Vec(a.x + b.x, a.y + b.y)
end function

function operator -(a as Vec, b as Vec) as Vec
    return Vec(a.x - b.x, a.y - b.y)
end function

function operator *(a as Vec, k as double) as Vec
    return Vec(a.x * k, a.y * k)
end function

function operator *(k as double, a as Vec) as Vec
    return Vec(a.x * k, a.y * k)
end function

function operator -(a as Vec) as Vec
    return Vec(-a.x, -a.y)
end function

function operator /(a as Vec, k as double) as Vec
    return Vec(a.x / k, a.y / k)
end function

function operator =(a as Vec, b as Vec) as bool
    return a.x = b.x and a.y = b.y
end function
