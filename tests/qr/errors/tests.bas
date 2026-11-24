' OPTION DIALECT QUICKR
' test blocks and assert: their errors
test sums
end test
test "returns"
    return 1
end test
test "asserts"
    assert 1
end test
assert "yes"
function f()
    test "inside"
    end test
end function
test "ends wrong"
end fn
