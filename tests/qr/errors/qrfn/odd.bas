' OPTION DIALECT QUICKR
public function fine() as int
    return 1
end function

public function list(xs as int[]) as int
    return len(xs)
end function

public function ident[T](x as T) as T
    return x
end function

public function maybe(x as int) as int or none
    return none
end function
