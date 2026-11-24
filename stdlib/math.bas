' OPTION DIALECT QUICKR
' math: numbers' functions; sqrt .. floor are the VM's ops
public const pi = 3.141592653589793

public native function sqrt(x as double) as double
public native function sin(x as double) as double
public native function cos(x as double) as double
public native function tan(x as double) as double
public native function atan(x as double) as double
public native function exp(x as double) as double
public native function log(x as double) as double
public native function floor(x as double) as double
public native function ceil(x as double) as double

public function round(x as double) as double
    return floor(x + 0.5)
end function

public function atan2(y as double, x as double) as double
    if x > 0 then return atan(y / x)
    if x < 0 and y >= 0 then return atan(y / x) + pi
    if x < 0 then return atan(y / x) - pi
    if y > 0 then return pi / 2
    if y < 0 then return -pi / 2
    return 0
end function

public function abs[T](x as T) as T
    if x < 0 then return -x
    return x
end function

public function min[T](a as T, b as T) as T
    if b < a then return b
    return a
end function

public function max[T](a as T, b as T) as T
    if b > a then return b
    return a
end function

public function clamp[T](x as T, lo as T, hi as T) as T
    if x < lo then return lo
    if x > hi then return hi
    return x
end function

public function lerp(a as double, b as double, t as double) as double
    return a + (b - a) * t
end function

' a 2-d vector
public type Vec
    x as double
    y as double
end type

public function operator +(a as Vec, b as Vec) as Vec
    return Vec(a.x + b.x, a.y + b.y)
end function

public function operator -(a as Vec, b as Vec) as Vec
    return Vec(a.x - b.x, a.y - b.y)
end function

public function operator *(a as Vec, k as double) as Vec
    return Vec(a.x * k, a.y * k)
end function

public function operator /(a as Vec, k as double) as Vec
    return Vec(a.x / k, a.y / k)
end function

public function operator =(a as Vec, b as Vec) as bool
    return a.x = b.x and a.y = b.y
end function

public function dot(a as Vec, b as Vec) as double
    return a.x * b.x + a.y * b.y
end function

public function length(a as Vec) as double
    return sqrt(a.x * a.x + a.y * a.y)
end function
