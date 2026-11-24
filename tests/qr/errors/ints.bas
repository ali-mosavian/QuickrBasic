' OPTION DIALECT QUICKR
' integer types: what the checker refuses
var b as byte = 1
var ub as ubyte = 2
var u as uint = 3
var l as long = 4
var i = 5
print(b + ub)
print(u + i)
let small as byte = 200
let neg as uint = -1
let narrow as short = i
let back as int = u
print(-u)
let ok as long = i
