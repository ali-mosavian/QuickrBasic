option dialect quickr
type Land
    heights as int[8]
    sea as int
    names as list[string]
    marks as single[3]
end type

function fill(mut l as Land)
    l.heights[3] = 7
    l.sea = 2
    l.names.push("hill")
    l.marks[2] = 1.5
end function

function total(l as Land) as int
    var t = 0
    for h in l.heights
        t += h
    next
    return t + len(l.names)
end function
