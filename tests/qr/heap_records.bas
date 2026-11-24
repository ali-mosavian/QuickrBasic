' OPTION DIALECT QUICKR
' records with strings and lists in cells: their parts owned as a
' box's were, through calls, copies, recursion, the heap's moves; a
' record over 256 bytes, its fields past them read by address
type Pair
    key as string
    n as int
end type

type Bag
    n as int
    tags as int[]
    name as string
    pair as Pair
end type

function make(i as int) as Bag
    var b = Bag(n: i, name: f"b{i}", pair: Pair(key: f"k{i}", n: i))
    for k in 0..i
        b.tags.push(k * 10)
    end for
    return b
end function

function churn() as int
    var t = 0
    for k in 0..300
        let s = f"garbage {k} and some more of it to fill the heap"
        t += len(s)
    end for
    return t
end function

function grow(mut b as Bag, depth as int)
    if depth = 0 then return
    var local = make(depth)
    churn()
    b.tags.push(len(local.tags))
    b.name += local.name
    grow(mut b, depth - 1)
end function

function parse(s as string) as int!
    if len(s) = 0 then return Error(1, "empty")
    return len(s)
end function

var x = make(1)
print(x.n, len(x.tags), x.name, x.pair.key)
x = make(3)
churn()
print(x.n, len(x.tags), x.tags[2], x.name, x.pair.key)
var y = copy(x)
y.tags[0] = 99
y.pair.key += "!"
churn()
print(x.tags[0], y.tags[0], x.pair.key, y.pair.key)
grow(mut y, 3)
print(len(y.tags), y.tags[4], y.name)
let r = parse("")
if r is Error then print(r.message, r.code)
let q = parse("four")
print(q ?? 0)
var all as Bag[]
for i in 0..6
    all.push(make(i))
    churn()
end for
var sum = 0
for b in all
    sum += b.n + len(b.tags) + len(b.name) + b.pair.n
end for
print(sum)

type Big
    a as int
    pad as int[70]
    b as int
    d as double
    s as string
end type

function fill(n as int) as Big
    var x = Big(a: n, b: n * 2, d: 1.5, s: f"s{n}")
    x.pad[69] = n * 3
    return x
end function

function weigh(b as Big) as int
    return b.a + b.b + b.pad[69] + len(b.s)
end function

var big = fill(4)
big.b += 1
let c = copy(big)
big.d *= 2.0
print(big.a, big.b, big.d, big.pad[69], big.s)
print(weigh(big), c.d)

function deep(n as int) as int
    if n = 0 then return 0
    let x = fill(n)
    return weigh(x) + deep(n - 1)
end function
print(deep(50))
