option dialect quickr
' an f-string joined once, its parts sized first; past 255 parts in
' rounds
let a = "x"
let n = 7
let s = f"{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-{a}-"
print len(s); " "; s[0]; " "; s[1]; " "; s[298]; " "; s[299]
print f"n={n}, a={a}, {n * 1.5}/{n:4}."
