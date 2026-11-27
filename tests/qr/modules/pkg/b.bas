option dialect quickr
function twice(n as int) as int
    return n * 2
end function
let order = say()
private function say() as string
    print "init b"
    return "b"
end function
