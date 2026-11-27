option dialect quickr
function grow(mut a as int[])
    a.resize(len(a) + 1)
    a[len(a) - 1] = 1
end function
