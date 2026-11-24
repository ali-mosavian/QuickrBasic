# math Module
keys: ~quickr math, ~quickr math.pi, ~quickr math.sqrt, ~quickr math.sin, ~quickr math.cos, ~quickr math.tan, ~quickr math.atan, ~quickr math.exp, ~quickr math.log, ~quickr math.floor, ~quickr math.ceil, ~quickr math.round, ~quickr math.atan2, ~quickr math.abs, ~quickr math.min, ~quickr math.max, ~quickr math.clamp, ~quickr math.lerp, ~quickr math.Vec, ~quickr math.dot, ~quickr math.length

Numbers' functions, and a 2-d vector.

    pi                             3.141592653589793
    sqrt sin cos tan atan exp log  of a double, as the VM's ops
    floor(x), ceil(x), round(x)    to a whole number, as a double
    atan2(y, x)                    the angle of (x, y)
    abs(x), min(a, b), max(a, b)   of any number type
    clamp(x, lo, hi)               x kept from lo to hi
    lerp(a, b, t)                  a + (b - a) * t

See also [Quickr](quickr index), [random](quickr random)

## Details
Vec has x and y, both double, and the operators + - (two Vecs),
* / (a Vec and a number) and =. dot(a, b) and length(a) take Vecs:
a.length() is length(a).

## Example
    ' OPTION DIALECT QUICKR
    import math
    let v = math.Vec(3, 4)
    print(v.length())
    print(math.clamp(12, 0, 10))
