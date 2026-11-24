' OPTION DIALECT QUICKR
' private: a field or a method its module's alone
from bank import Account, Tag
var a = Account.open("ann")
a.add(50)
a.add(-500)
print a.name; " "; a.balance()
a.add(7)
let t = Tag("x")
print a.balance(); " "; t.label
