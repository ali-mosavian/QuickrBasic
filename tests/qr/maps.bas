option dialect quickr
' maps: set, read, in, remove, len, keys, for; records, enums, copies
var ages as dict[string, int]
ages["ada"] = 36
ages["bob"] = 7
ages["cy"] = 12
print len(ages); " "; ages["bob"]
ages["bob"] += 1
print ages["bob"]; " "; "ada" in ages; " "; "zed" in ages; " "; "zed" not in ages
for k, v in ages
    print k; " "; v
next
ages.remove("ada")
ages.remove("nope")
print len(ages)
for k in ages
    print k
next
let ks = ages.keys()
print len(ks); " "; ks[0]
var sq as dict[int, int]
for i in 0..1000
    sq[i * 7] = i * i
next
var sum = 0
for i in 0..1000
    sum += sq[i * 7]
next
print len(sq); " "; sum; " "; sq[700]
for i in 0..1000
    if i mod 3 <> 0 then
        sq.remove(i * 7)
    end if
next
var ok = true
for i in 0..1000
    ok = ok and ((i * 7 in sq) = (i mod 3 = 0))
next
print len(sq); " "; ok
var names as dict[int, string]
for i in 0..200
    names[i] = f"n{i}"
next
print names[150]; " "; len(names)
type Pet
    name as string
    legs as int
    tags as list[string]
end type
type Zoo
    pets as dict[string, Pet]
    count as int
end type
enum Color
    red
    green
end enum
function census(z as Zoo) as dict[string, int]
    var out as dict[string, int]
    for k, p in z.pets
        out[k] = p.legs
    next
    return out
end function
function add(mut z as Zoo, name as string, legs as int)
    z.pets[name] = Pet(name, legs)
    z.pets[name].tags.push(f"{legs} legs")
    z.count += 1
end function
var zoo as Zoo
add(zoo, "rex", 4)
add(zoo, "tweety", 2)
add(zoo, "nemo", 0)
print zoo.count; " "; len(zoo.pets); " "; zoo.pets["tweety"].tags[0]
let c = census(zoo)
for k, v in c
    print k; " "; v
next
var z2 = zoo
z2.pets["rex"].legs = 3
z2.pets.remove("nemo")
print zoo.pets["rex"].legs; " "; z2.pets["rex"].legs; " "; len(zoo.pets); " "; len(z2.pets)
var paint as dict[Color, string]
paint[Color.green] = "grass"
paint[Color.red] = "rose"
print paint[Color.red]; " "; len(paint)
var big as dict[int, Pet]
for i in 0..60
    big[i] = Pet(f"p{i}", i)
    big[i].tags.push("t")
next
for i in 0..60
    if i mod 2 = 0 then
        big.remove(i)
    end if
next
var total = 0
for k, p in big
    total += p.legs + len(p.tags)
next
print len(big); " "; total; " "; big[59].name
var moved = big
print len(moved)
function build(n as int) as int
    var m as dict[int, string]
    for i in 0..n
        m[i] = f"v{i}"
    next
    return len(m)
end function
function made(n as int) as dict[int, int]
    var m as dict[int, int]
    for i in 0..n
        m[i] = i
    next
    return m
end function
var t = 0
for r in 0..300
    t += build(200)
    let q = made(50)
    t += len(q)
next
print t
for k, v in made(5)
    print k; " "; v
next
print len(made(9).keys())
