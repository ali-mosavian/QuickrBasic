option dialect quickr
const TITLE = "geo\tmap"
var kept as list[int[]]
' memory taken after the link, where its freed blocks were
function fill()
    for i in 0..3000
        var a = int[64]
        a[0] = -1
        kept.push(a)
    next
end function
