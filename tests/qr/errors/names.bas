option dialect quickr
' unknown and doubled names
print nope
let a = 1
let a = 2
function f()
end function
function f()
end function
f = 3
print f
g()
let h = 1
h()
const C = later_var
var later_var = 1
function uses()
    print local_of_other
end function
function other()
    let local_of_other = 1
end function
