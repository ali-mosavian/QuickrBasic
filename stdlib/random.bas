' OPTION DIALECT QUICKR
' random: numbers from one seeded sequence, RND's
public native fn seed(n as double)
native fn unit() as double
' a whole number from 0 up to n
native fn below(n as int) as int

' a whole number from a to b, both included
public fn int(a as int, b as int) as int
    return a + below(b - a + 1)
end fn

' a number from a up to b
public fn real(a as double, b as double) as double
    return a + (b - a) * unit()
end fn

public fn pick[T](xs as T[]) as T
    return xs[below(len(xs))]
end fn
