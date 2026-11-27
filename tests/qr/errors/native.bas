option dialect quickr
' native functions: an entry's name, plain values by value, no body
type P
    x as int
end type
native function no_such_entry(x as int) as int
native function sqrt(mut x as double) as double
native function floor(p as P) as P
native function find[T](s as T) as int
native type Q
native function upper(s as string) as string
print upper("ok")
