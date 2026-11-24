# The Quickr Language
keys: ~quickr language, ~quickr and, ~quickr as, ~quickr break, ~quickr case, ~quickr const, ~quickr continue, ~quickr div, ~quickr do, ~quickr else, ~quickr elseif, ~quickr end, ~quickr enum, ~quickr false, ~quickr fn, ~quickr function, ~quickr for, ~quickr if, ~quickr in, ~quickr is, ~quickr let, ~quickr select, ~quickr mod, ~quickr mut, ~quickr none, ~quickr not, ~quickr or, ~quickr return, ~quickr step, ~quickr then, ~quickr true, ~quickr type, ~quickr var, ~quickr while, ~quickr xor, ~quickr yield, ~quickr yields, ~quickr from, ~quickr import, ~quickr public, ~quickr native, ~quickr operator, ~quickr test, ~quickr assert, ~quickr shl, ~quickr shr, ~quickr to, ~quickr next, ~quickr wend, ~quickr loop, ~quickr until, ~quickr print, ~quickr len, ~quickr copy, ~quickr wrap, ~quickr Error, ~quickr bool, ~quickr byte, ~quickr short, ~quickr int, ~quickr long, ~quickr ubyte, ~quickr ushort, ~quickr uint, ~quickr ulong, ~quickr single, ~quickr double, ~quickr string

Names are case-sensitive; keywords are lower case. A line ends a
statement, except inside ( [ or {. A comment starts with '.

See also [Quickr](quickr index)

## Details
Declarations: name [as type] [= value].
    const N = 10          folded when compiled
    let x = 1             immutable
    var n as int          mutable; zero
    function f(a as int, mut b as P) as int ... end function
    type P ... end type   a record: x as int, one field a line
    enum Mode ... end enum

Types: bool, byte short int long, ubyte ushort uint ulong, single
double, string; T[] a list, T[n] an array, V[K] a map, (T, U) a
tuple, T? a T or none, T! a T or an Error, function(T) as U.

Control:
    if c then ... elseif c then ... else ... end if
    for i = a to b [step s] ... next     for x in xs   for i, x in xs
    for i in 0..n (n not counted)   while c ... wend
    do ... loop [while c | until c]   break   continue
    select case x / case a, b / case a to b / case is > c / case else
    / end select

Operators: + - * / div mod ^, = <> < <= > >=, and or not xor
(integers: bitwise), shl shr, a if c else b, ?? (a none's or an
Error's other value), x? (an error returned), += -= *= /=.

a.f(b) calls f(a, b). A parameter is read-only; mut makes it
writable, the caller writing mut too. Records, lists and maps have
one owner: copy(a) copies one.

Modules: import m, import m as n, from m import a, b. Only public
names are seen outside their file.

Built-ins: print(x, ...), len(a), copy(a), wrap(x) (integers wrap),
int(x) single(x) double(x) string(x) and the other types' names.

Tests: test "name" ... end test; assert c. basic --test FILE.BAS runs
them.

## Example
    ' OPTION DIALECT QUICKR
    type P
        x as int
        y as int
    end type

    function sum(p as P) as int
        return p.x + p.y
    end function

    let p = P(x: 2, y: 3)
    for i in 0..3
        print(f"{i}: {p.sum() * i}")
    next
