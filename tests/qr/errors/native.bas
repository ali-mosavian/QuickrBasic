' OPTION DIALECT QUICKR
' native fns: an entry's name, plain values by value, no body
type P
    x as int
end type
native fn no_such_entry(x as int) as int
native fn sqrt(mut x as double) as double
native fn floor(p as P) as P
native fn find[T](s as T) as int
native type Q
native fn upper(s as string) as string
print(upper("ok"))
