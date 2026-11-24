' OPTION DIALECT QUICKR
let s = "the quick brown fox jumps over the lazy dog"
var n = 0
for r in 0..20000
    for i in 0..len(s)
        if s[i] = 111 then
            n += 1
        end if
    end for
end for
print(n, s[4..9])
