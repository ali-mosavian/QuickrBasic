' OPTION DIALECT QUICKR
' integer types: byte short int long, ubyte ushort uint ulong
var b as byte = -128
var s as short = 32767
var l as long = 9_000_000_000
var ub as ubyte = 255
var us as ushort = 65535
var u as uint = 4_000_000_000
var ul as ulong = 18_000_000_000_000_000_000
print(b, s, l, ub, us, u, ul)
b += 100
s -= 30000
ub -= 5
us = us div 3
print(b, s, ub, us)
' widening: by itself where nothing is lost
let i as int = s
let wide as long = u
let fromub as short = ub
let d as double = l
print(i, wide, fromub, d)
print(l * 3, l div 7, l mod 7, -l, l + i)
print(ul - ub, b * l, -i - l, ul div us)
print(u + 1, u - 3_000_000_000, u * 1, u div 7, u mod 7)
print(ul div 3, ul - 1, ul > 17_000_000_000_000_000_000)
print(u > 3_000_000_000, l < 0, l = 9_000_000_000, l <> 1)
' conversions: written out, checked
print(int(l div 1000), byte(-5), ubyte(200), ushort(i), uint(i), long(2.9), ulong(u))
print(int(-2.9), short(-3.5), long(-1e18), double(ul), single(us))
print(string(u), string(l), f"{u:,} {l:>14,} {ul:x} {ub:08b} {b:+d}")
' wrap: arithmetic kept to its type's bits
print(wrap(ubyte(300)), wrap(ub + ub), wrap(b - 100), wrap(u * u), wrap(l * l))
var h as uint = 2166136261
for c in "abc"
    h = wrap((h ^ c) * 16777619)
end for
print(h)
print(wrap(int(2147483647) + 1), wrap(-l * 1_000_000_000_000))
' bits: & | ^ ~ << >>
print(ub & 15, ub | 256 - 1, ~ub, ~b, ~us, ~u, ~l)
print(ub << 1, ub >> 4, u >> 31, l << 2, -l >> 1, ul >> 60, b >> 2)
' a string's bytes are ubytes
let t = "Hi!"
print(t[0], t[0] + t[1], int(t[2]) * 1000)
' lists and records keep their narrow types
type Pixel
    r as ubyte
    g as ubyte
    a as byte
    z as ushort
    n as long
end type
var px = Pixel(r: 200, g: 7, a: -3, z: 60000, n: -5_000_000_000)
px.r -= 1
px.a -= 120
px.n *= 2
print(px.r, px.g, px.a, px.z, px.n)
var bs as ubyte[]
bs.push(250)
bs.push(1)
bs[1] = bs[0] + 4
var ls as long[]
ls.push(l)
ls.push(-1)
var ss as short[]
ss.push(-30000)
print(bs[0], bs[1], ls[0], ls[1], ss[0], len(bs), l in ls, 255 in bs)
print(bs.pop(), ls.pop(), ss.pop(), bs.sum(), ls.sum())
function twice(mut x as ubyte)
    x *= 2
end function
bs.push(3)
twice(mut px.g)
twice(mut bs[1])
print(px.g, bs[1])
function big(x as long) as long
    return x * 1000
end function
print(big(l), big(5))
