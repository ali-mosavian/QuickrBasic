option dialect quickr
' a one-line if: one statement after then, one after else
var x = 1
if x = 1 then x = 2 else
if x = 1 then x = 2 else if x = 2 then x = 3
if x = 1 then x = 2 else x = 3 else x = 4
function f(a as bool) as int
    if a then return 1 else x = 2
end function
