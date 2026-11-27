option dialect quickr
type Foe
    hp as int
    tag as string
end type

function total(a as int[]) as int
    var s = 0
    for x in a
        s += x
    next
    return s
end function

function hurt(mut foes as Foe[], by as int)
    for i in 0..len(foes)
        foes[i].hp -= by
    next
    foes.push(Foe(hp: 1, tag: "imp"))
end function

function names(foes as Foe[]) as string
    var s = ""
    for f in foes
        s += f.tag
    next
    return s
end function

function shrink(mut a as int[])
    a.pop()
end function
