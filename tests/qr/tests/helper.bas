option dialect quickr
function twice(x as int) as int
    return x * 2
end function

function check_pos(x as int)
    assert x > 0
end function

test "twice doubles"
    assert twice(3) = 6
end test

test "helper fails"
    assert twice(2) = 5
end test
