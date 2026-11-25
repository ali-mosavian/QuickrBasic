' OPTION DIALECT QUICKR
import early
function shown() as int
    return hidden()
end function
private function hidden() as int
    return 1
end function
private var count = 0
var late = 2
print "a module runs nothing"
function bad() as int
    return "no"
end function
