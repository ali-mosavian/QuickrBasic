' OPTION DIALECT QUICKR
public fn twice(x as int) as int
    return x * 2
end fn

public fn check_pos(x as int)
    assert x > 0
end fn

test "twice doubles"
    assert twice(3) = 6
end test

test "helper fails"
    assert twice(2) = 5
end test
