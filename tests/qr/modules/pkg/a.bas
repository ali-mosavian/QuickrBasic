option dialect quickr
from .b import twice
function quad(n as int) as int
    return twice(twice(n))
end function
var order = log("a")
private function log(s as string) as string
    print "init"; " "; s
    return s
end function
