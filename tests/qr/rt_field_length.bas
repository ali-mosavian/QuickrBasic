' OPTION DIALECT QUICKR
' a field list's room past its length is out of range
type Bag
    items as int[]
end type
var bag as Bag
for i in 0..5
    bag.items.push(i)
next
print bag.items.pop(); " "; len(bag.items)
print bag.items[4]
