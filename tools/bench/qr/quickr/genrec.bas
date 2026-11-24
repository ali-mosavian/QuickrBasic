' OPTION DIALECT QUICKR
type Acc[T]
    total as T
    n as T

    fn add(mut self, x as T)
        self.total += x
        self.n += 1
    end fn
end type

var a as Acc[int]
for i in 0..2000000
    a.add(i mod 7)
    a.total = a.total mod 1000
next
print a.total; " "; a.n
