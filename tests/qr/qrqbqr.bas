option dialect quickr
' Quickr imports a QBasic module that imports a Quickr one: leaf's
' record crosses to QBasic and back, its code run once
import twice
import leaf
let l = leaf.mk(21)
print twice.twice(l); " "; leaf.made
