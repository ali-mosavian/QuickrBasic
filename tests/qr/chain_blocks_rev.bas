' OPTION DIALECT QUICKR
' chains: rev() after take, skip and zip; block lambdas
var a as int[]
for i in 0..8
    a.push(i * 10)
next
let k = 3
print a.take(3).rev().collect()[0]; " "; a.skip(5).rev().sum()
print a.skip(2).take(3).rev().map(fn(x) x + 1).collect()[0]
print (0..10).skip(7).rev().collect()[0]
print "hello".take(2).rev().collect()[0]
for x, y in a.zip(0..3).rev()
    print x; " "; y
next
let ab = "ab"
for x, s in a.skip(6).zip(ab).rev()
    print x; " "; s
next
print a.map(fn(x as int) as int
    let y = x + k
    return y * 2
end fn).sum()
print a.filter(fn(x as int)
    return x > 40
end fn).count()
print a.take(100).rev().count(); " "; a.skip(100).rev().count()
print a.take(-1).rev().count()
