' OPTION DIALECT QUICKR
' math: numbers' functions; sqrt .. floor are the VM's ops
public const pi = 3.141592653589793

public native fn sqrt(x as double) as double
public native fn sin(x as double) as double
public native fn cos(x as double) as double
public native fn tan(x as double) as double
public native fn atan(x as double) as double
public native fn exp(x as double) as double
public native fn log(x as double) as double
public native fn floor(x as double) as double
public native fn ceil(x as double) as double

public fn round(x as double) as double
    return floor(x + 0.5)
end fn

public fn atan2(y as double, x as double) as double
    if x > 0 then return atan(y / x)
    if x < 0 and y >= 0 then return atan(y / x) + pi
    if x < 0 then return atan(y / x) - pi
    if y > 0 then return pi / 2
    if y < 0 then return -pi / 2
    return 0
end fn

public fn abs[T](x as T) as T
    if x < 0 then return -x
    return x
end fn

public fn min[T](a as T, b as T) as T
    if b < a then return b
    return a
end fn

public fn max[T](a as T, b as T) as T
    if b > a then return b
    return a
end fn

public fn clamp[T](x as T, lo as T, hi as T) as T
    if x < lo then return lo
    if x > hi then return hi
    return x
end fn

public fn lerp(a as double, b as double, t as double) as double
    return a + (b - a) * t
end fn

' a 2-d vector
public type Vec
    x as double
    y as double
end type

public fn operator +(a as Vec, b as Vec) as Vec
    return Vec(a.x + b.x, a.y + b.y)
end fn

public fn operator -(a as Vec, b as Vec) as Vec
    return Vec(a.x - b.x, a.y - b.y)
end fn

public fn operator *(a as Vec, k as double) as Vec
    return Vec(a.x * k, a.y * k)
end fn

public fn operator /(a as Vec, k as double) as Vec
    return Vec(a.x / k, a.y / k)
end fn

public fn operator =(a as Vec, b as Vec) as bool
    return a.x = b.x and a.y = b.y
end fn

public fn dot(a as Vec, b as Vec) as double
    return a.x * b.x + a.y * b.y
end fn

public fn length(a as Vec) as double
    return sqrt(a.x * a.x + a.y * a.y)
end fn
