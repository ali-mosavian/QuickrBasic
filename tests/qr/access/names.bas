option dialect quickr
' public is the default: written or not, a name is exported
public function said() as int
    return 1
end function
function unsaid() as int
    return 2
end function
public type Pair
    public a as int
    b as int
end type
const both = 3
public const either = 4
private function own() as int
    return 5
end function
