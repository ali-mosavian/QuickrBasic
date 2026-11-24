' OPTION DIALECT QUICKR
' test blocks and assert: --test runs every test of the file and its modules; a run, none
import helper
type Res
    name as string
end type

fn drop(mut r as Res)
    print("drop", r.name)
end fn

var count = 10
print("main runs")
count = 20
assert count = 20
print(helper.twice(count))

test "sums"
    var total = 0
    for i in 0..5
        total += i
    end for
    assert total = 10
end test

test "a failing assert"
    let r = Res("held")
    assert count = 20
    print("not reached")
end test

test "deep assert"
    helper.check_pos(-1)
end test

test "division"
    let z = count - 10
    print(100 div z)
end test

test "returns early"
    if count = 10 then return
    assert false
end test
