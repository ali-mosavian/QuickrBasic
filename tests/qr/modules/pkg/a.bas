' OPTION DIALECT QUICKR
from .b import twice
public function quad(n as int) as int
    return twice(twice(n))
end function
public var order = log("a")
function log(s as string) as string
    print("init", s)
    return s
end function
