# The Quickr Language
keys: ~quickr language, ~quickr and, ~quickr as, ~quickr break, ~quickr case, ~quickr const, ~quickr continue, ~quickr div, ~quickr do, ~quickr else, ~quickr elseif, ~quickr end, ~quickr enum, ~quickr false, ~quickr fn, ~quickr function, ~quickr for, ~quickr if, ~quickr in, ~quickr is, ~quickr let, ~quickr select, ~quickr mod, ~quickr mut, ~quickr none, ~quickr not, ~quickr or, ~quickr return, ~quickr step, ~quickr then, ~quickr true, ~quickr type, ~quickr var, ~quickr while, ~quickr xor, ~quickr yield, ~quickr yields, ~quickr from, ~quickr import, ~quickr public, ~quickr native, ~quickr operator, ~quickr test, ~quickr assert, ~quickr shl, ~quickr shr, ~quickr to, ~quickr next, ~quickr wend, ~quickr loop, ~quickr until, ~quickr print, ~quickr self, ~quickr private, ~quickr len, ~quickr wrap, ~quickr Error, ~quickr bool, ~quickr byte, ~quickr short, ~quickr int, ~quickr long, ~quickr ubyte, ~quickr ushort, ~quickr uint, ~quickr ulong, ~quickr single, ~quickr double, ~quickr string, ~quickr dict

Names are case-sensitive; keywords are lower case. A line ends a
statement, except inside ( [ or {. A comment starts with '.

See also [Quickr](quickr index)

## Details
Declarations: name [as type] [= value].
    const N = 10          folded when compiled
    let x = 1             immutable
    var n as int          mutable; zero
    function f(a as int, mut b as P) as int ... end function
    type P ... end type   a record: x as int, one field a line,
                          then its methods: fn f(self) ... end fn
    type Stack[T] ...     generic: Stack[int](), Pair(1, "a")
    enum Mode ... end enum

Types: bool, byte short int long, ubyte ushort uint ulong, single
double, string; T[] a list, T[n] an array, dict[K, V] a map, (T, U)
a tuple, T or none, T or Error, function(T) as U. Any type is an
element or a map's value: int[][] is a list of lists. s[i] is a
ubyte; "0"[0] is the constant 48.

Control:
    if c then ... elseif c then ... else ... end if   (else if = elseif)
    if c then s else s    one line
    for i = a to b [step s] ... next     for x in xs   for i, x in xs
    for i in 0..n (n not counted)   while c ... wend
    do ... loop [while c | until c]   break   continue
    select case x / case a, b / case a to b / case is > c / case else
    / end select

Operators: + - * / div mod ^, = <> < <= > >=, and or not xor
(integers: bitwise), shl shr, a if c else b, a or else b (b when a
holds none or an Error), x? (an error returned), += -= *= /=.

Methods: fn f(self, ..) in a type or an enum is called a.f(..);
mut self changes a. fn g(..) without self is called P.g(..). P.f is
a function value. fn drop(mut self) runs when the owner ends.
A parameter is read-only; mut on it makes it writable: f(a) then
changes a. Numbers, bools, enums and strings pass by value.
Records, lists and maps have one owner: b = a copies a, or moves it
when a is not used again. basic --copies FILE.BAS lists each copy of
a list or a map.

Modules: import m, import m as n, from m import a, b. A top-level
name, a type's field or method is seen outside its file unless
private. A type with a private field is built in its file.
A QBasic module, or a native one (a .qbx), is imported the same way:
its SUBs and FUNCTIONs are functions, its TYPEs records, and a list
is passed to an array parameter by reference.

print a; b, c: ; joins, , to the next 14-column zone, a separator
last holds the line; print alone, an empty line.
An enum prints its value's name, a T or none none or its T, and a
type with fn text(self) as string what it gives; f-strings too.

Built-ins: len(a), wrap(x) (integers wrap),
int(x) single(x) double(x) string(x) and the other types' names.

Tests: test "name" ... end test; assert c. basic --test FILE.BAS runs
them.

## Example
    ' OPTION DIALECT QUICKR
    type P
        x as int
        y as int

        fn sum(self) as int
            return self.x + self.y
        end fn
    end type

    let p = P(x: 2, y: 3)
    for i in 0..3
        print f"{i}: {p.sum() * i}"
    next
