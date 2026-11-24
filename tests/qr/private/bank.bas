' OPTION DIALECT QUICKR
' an account: its balance private, changed by its methods alone
public type Account
    name as string
    private cents as int

    fn open(name as string) as Account
        return Account(name, 100)
    end fn

    fn add(mut self, n as int)
        self.cents += self.checked(n)
    end fn

    fn balance(self) as int
        return self.cents
    end fn

    private fn checked(self, n as int) as int
        return n if self.cents + n >= 0 else -self.cents
    end fn
end type

public type Tag
    label as string
end type
