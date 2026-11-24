' OPTION DIALECT QUICKR
var s = 0
for i in 0..3000000
    match i mod 10
        case 1, 2
            s += 1
        case 3 to 5
            s += 2
        case is > 7
            s += 3
        case else
            s += 4
    end match
end for
print(s)
