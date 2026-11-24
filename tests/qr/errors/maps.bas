' OPTION DIALECT QUICKR
' maps: their errors
var a as dict[double, int]
var b as dict[string, int[]]
var m as dict[string, int]
m[3] = 1
print m["x"] + "y"
for mut k in m
next
for k, v in m
    m["z"] = 1
    m.remove(k)
next
m.push(1)
print m.keys(1)
let o as dict[string, int]
o["a"] = 1
var l as int[]
print l.keys()
print m
