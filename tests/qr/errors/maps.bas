' OPTION DIALECT QUICKR
' maps: their errors
var a as int[double]
var b as int[][string]
var m as int[string]
m[3] = 1
print(m["x"] + "y")
for mut k in m
next
for k, v in m
    m["z"] = 1
    m.remove(k)
next
m.push(1)
print(m.keys(1))
let n = m
let o as int[string]
o["a"] = 1
var l as int[]
print(l.keys())
print(m)
