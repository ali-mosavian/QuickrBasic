' OPTION DIALECT QUICKR
import text
let s = "the quick brown fox jumps over the lazy dog"
var n = 0
for r in 0..50000
    if text.find(s, "lazy") >= 0 then n += 1
end for
print(n)
