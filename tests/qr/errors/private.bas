option dialect quickr
' private: out of its module, not read, written, called or built
from bank import Account
var a = Account.open("bo")
print a.cents
a.cents = 5
print a.checked(1)
let f = Account.checked
let b = Account("cy", 3)
private var x = 1
type Local
    private n as int
    private fn twice(self) as int
        return self.n * 2
    end fn
end type
enum Mode
    private walk
end enum
let l = Local(4)
print l.n; " "; l.twice()
