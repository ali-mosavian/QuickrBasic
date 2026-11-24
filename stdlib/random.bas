' OPTION DIALECT QUICKR
' random: numbers from one seeded sequence, RND's
public native function seed(n as double)
native function unit() as double
' a whole number from 0 up to n
native function below(n as int) as int

' a whole number from a to b, both included
public function int(a as int, b as int) as int
    return a + below(b - a + 1)
end function

' a number from a up to b
public function real(a as double, b as double) as double
    return a + (b - a) * unit()
end function

public function pick[T](xs as T[]) as T
    return xs[below(len(xs))]
end function
