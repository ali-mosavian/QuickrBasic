option dialect quickr
function bump(mut a as int[])
    a[1] = 7
end function

function grow(mut a as int[])
    a.resize(len(a) + 1)
end function
