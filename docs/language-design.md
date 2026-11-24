# Quickr: the modern dialect

Quickr is a second language for QuickrBasic. It reads as BASIC, has one way to do each thing, and
replaces QBasic's hundreds of statements and functions with a standard library you import.
QBasic 1.1 stays as its own dialect, unchanged, and each file is one or the other. Execution speed
is a goal of the language itself, not only of the VM.

This is the spec an implementer works from. **In the VM** marks how the implementation does a
thing. **Not yet** marks what the implementation does not do; where it does something else today,
the note says what.

## A program

```basic
' OPTION DIALECT QUICKR
from keys import Key
from math import Vec
import keys
import random
import time

type Ball
    at as Vec
    vel as Vec
    r as double = 6

    fn move(mut self, dt as double, size as Vec)
        self.at = self.at + self.vel * dt
        if not self.r <= self.at.x <= size.x - self.r then self.vel.x = -self.vel.x
        if not self.r <= self.at.y <= size.y - self.r then self.vel.y = -self.vel.y
    end fn
end type

let size = Vec(320, 200)
var balls as Ball[]
for i = 1 to 10
    let vel = Vec(random.real(-80, 80), random.real(-80, 80))
    balls.push(Ball(at: Vec(160, 100), vel: vel))
next

var clock = time.Clock()
while not keys.down(Key.escape)
    let dt = clock.tick()
    for mut b in balls
        b.move(dt, size)
    next
    print f"{balls[0].at.x:.0f}", f"{balls[0].at.y:.0f}"
wend
```

## Choosing the dialect

- Files are `.bas` in both dialects. A Quickr file's first line is `' OPTION DIALECT QUICKR`
  (case aside), a comment to QBasic; a file without it is QBasic's. The line is the file's line 1,
  required in every Quickr file, modules too. Nothing else is guessed from a file's contents.
- File > New asks which, and a Quickr program starts with that line, which the editor keeps as it
  is. The status bar shows the program's dialect. `basic` runs either.
- A Quickr file importing a QBasic module is an error at the import. A QBasic file never imports
  a Quickr one.

## What goes, what stays

| QBasic | Quickr |
|---|---|
| Line numbers, labels, GOTO, GOSUB, ON...GOTO | blocks, `break`, `continue`, `return` |
| `%` `&` `!` `#` `$` suffixes, DEFINT... | `as type`, or inferred |
| Implicit variables | every variable is declared |
| SUB, CALL, assigning to the function's name | one `function`, called as `f(x)`, with `return` |
| OPTION BASE, `DIM a(lower TO upper)` | zero-based lists and arrays |
| ON ERROR, RESUME | errors are values: see [Errors](#errors) |
| `:` chains, `_` continuation, REM | one statement a line; an open bracket continues the line |
| EXIT FOR/DO, a FOR's name after NEXT | `break` and `continue`, innermost loop |
| COMMON, SHARED, DIM SHARED | module variables, imported by name |
| ~200 statements and functions | an import each |

These stay as QBasic has them: `print` with `;` and `,`; `for i = a to b step s … next`;
`while … wend`; `do … loop`; `select case`; `if … then … elseif … else … end if`; `type … end
type`; `^` for power; `and`, `or`, `xor`, `not` on integers; `'` comments; `end x` for every block
that is not a loop.

## The language

### Names and lines

- Names are case-sensitive. Keywords are lower case only.
- A newline ends a statement, except inside `(`, `[` or `{`.
- Comments start with `'`.
- Number literals: `0xff`, `0b1010`, `1_000_000`, `2.5`, `1e9`.
- `true`, `false` and `none`.
- A string is `"plain"`. Its escapes are `\n`, `\t`, `\"` and `\\`; any other is a compile error.
- `f"x = {x:.2f}"` is an f-string. A part in braces is an expression, then an optional `:` and a
  spec by Python's format-spec rules. A value without a spec is spelled as `print` spells it. A
  bool, and a value `print` spells as text (an enum, a record with `text`, a `T or none`), takes a
  string's spec: `f"{ok:>5}"`.
- A character constant is a string constant indexed by a constant: `"0"[0]` is the `ubyte` 48,
  folded at compile time, so a `const` or a `case` takes it. An index outside the string is a
  compile error.

### Types

| Type | Is |
|---|---|
| `bool` | true or false; `if`, `while` and `until` take only this |
| `byte` `short` `int` `long` | signed integers of 8, 16, 32 and 64 bits |
| `ubyte` `ushort` `uint` `ulong` | the same widths, unsigned |
| `single` `double` | 32-bit and 64-bit floating point |
| `string` | bytes, immutable; `s[i]` is a `ubyte`, `s[1..3]` a slice |
| `T[]` | a list: grows, zero-based |
| `T[n]` | a fixed array of `n`, a constant; inline in its record |
| `dict[K, V]` | a map from `K` to `V` |
| `(T, U)` | a tuple |
| `T or none` | a `T` or `none` |
| `T or Error` | a `T` or an `Error` |
| `function(T) as U` | a function value; `function(T)` returns nothing |
| a `type`'s name | a record: a value |
| an `enum`'s name | named values, each with or without data |
| `Name[T]` | a generic record with its type given |

- In a type, `or` binds last: `int[] or none` is a list or none, `(int or none)[]` a list of
  optionals, and `function(int) as int or none` a function whose result is optional. A type has
  one `or`.
- Inside `[ ]` after a type's name, a constant makes a fixed array and a type fills a generic.
  Only a generic type takes a type.
- `T[][]` is a list of lists; any type can be an element or a dict's value.
- `int` is the native int, 32 bits on every target. Every size is the same on every target, so a
  record laid out for a file reads the same everywhere.
- An operator takes two values of one type and gives that type: `byte + byte` is a `byte`.
- A value widens by itself only where nothing can be lost:
  - a narrower integer to a wider one of the same signedness;
  - an unsigned integer to a wider signed one;
  - an integer to `double`;
  - `single` to `double`.
- `int` and `single` together give a `double`, the type both widen to.
- Signed and unsigned never mix by themselves. Every other conversion is written out: `int(x)`,
  `ushort(n)`, `single(d)`, `string(n)`. `int(x)` of a real truncates toward zero. A conversion
  takes a number; a string is read by `text.to_int` and `text.number`.
- A literal takes the type its place needs: `r as single = 6` stores a `single`. A literal that
  doesn't fit is a compile error. Alone, `2.5` is a `double` and `7` is an `int`.
- Integer overflow is a runtime error, Overflow, and so is a conversion that loses the value:
  `ubyte(300)`. `wrap(x)` asks for wrapping, keeping each operation and conversion in `x` to its
  type's bits: `h = wrap((h xor c) * 16777619)`.
- `-` of an unsigned value is a compile error outside `wrap`.
- `T?`, `T!`, `V[K]` and `??` are errors that name `T or none`, `T or Error`, `dict[K, V]` and
  `or else`.
- **Not yet:** generic records.
- **In the VM**, an integer to 16 bits and an `int` live in an int cell and compute with the int
  ops, a result's range then checked. A `uint`'s bits are in an int cell too, with ops of its own.
  A `long` and a `ulong` are 8-byte cells loaded and stored by the double ops, with ops of their
  own: a `long`'s `+ - *`, compare-and-branch and `x += e` one op each. A power, and `div` in
  `wrap`, are CALL QX. The longs bench runs 0.94x the instructions of QBasic's double sum; `t += i`
  of an `int` costs 1.4x the same loop on ints, one op more.

### Declarations

```basic
const SPEED = 120.0         ' folded at compile time
var frames = 0              ' at module level: every function here sees it

function run()
    let name = "Ada"        ' immutable
    var count = 0           ' mutable; int, from its value
    var pos as Vec          ' zeroed; a field takes its default if it has one
    var ages as dict[string, int]
    var speed as single = 6 ' both: the type wins; 6 is a single
    var x as int, y = 5     ' two variables
    let q, r = divmod(17, 5)
    count += 1
end function
```

- Every declaration has one shape, `name [as type] [= value]`: `let`, `var`, a parameter, a
  record's field. At least one of the two is written; with no `as`, the type is the value's. A
  `let` always has a value.
- A comma separates declarations, and `as` belongs to the name before it. A name with neither
  `as` nor `=` takes a part of the next value, a tuple: `let q, r = divmod(17, 5)`.
- `a, b = b, a` assigns each part. A tuple has no index: it is read by taking it apart.
- `const` is a constant expression, folded at compile time.
- `let` binds an immutable value: it can't be assigned, nor its fields or elements changed.
- `var` declares a mutable variable; at module level, every function of the module sees it.
- A name declared in a block ends with the block, and may hide an outer one.
- A module's `const`s, `function`s, `type`s and `enum`s are visible in the whole file. Its `var`s
  and `let`s are visible from their line on, and in every `function`.

### Operators

Lowest first. Each row binds tighter than the one above.

| Operators | Takes | Notes |
|---|---|---|
| `a if c else b` | `c` a bool | evaluates one arm; the arms are one type |
| `a or else b` | `a` a `T or none` or `T or Error` | `b` when `a` holds no `T`; `b` is evaluated only then |
| `or` | two bools, or two integers | bools: short-circuit; integers: bitwise |
| `xor` | two bools, or two integers | |
| `and` | two bools, or two integers | bools: short-circuit; integers: bitwise |
| `not` | a bool, or an integer | integers: every bit flipped |
| `= <> < <= > >=`, `in`, `not in`, `is`, `is not` | | chained: `a < b <= c`, each value evaluated once |
| `shl`, `shr` | integers | |
| `+ -` | numbers; `+` joins two strings | |
| `* / div mod` | numbers | |
| unary `-` | a signed number | |
| `^` | numbers | power; right to left: `2 ^ 3 ^ 2` is 512, `-2 ^ 2` is -4 |
| `x?`, `f(x)`, `a[i]`, `a.b` | | |

- A bool and an integer never mix: `x and 0xff = 0` is a compile error, written
  `(x and 0xff) = 0`.
- `/` gives a `double`, or a `single` when both sides are. `div` takes integers.
- `div`, `mod` and `int(x)` truncate toward zero: `-7 div 2` is -3, `-7 mod 2` is -1.
- `int ^ int` is an `int`.
- A shift keeps its type's bits, dropping those shifted out. `shr` of a signed value keeps its
  sign. A count outside 0 up to the type's bits is a runtime error, Illegal function call.
- `x in a..b`, `x in a_list`, `k in a_dict`. A range `a..b` runs from `a` up to, not including,
  `b`.
- Compound assignment: `+= -= *= /=`. The target's subscripts are evaluated once. `/=` takes a
  `single` or `double` target.

### print

```basic
print "total: "; n          ' ; joins with nothing between
print name, age, town       ' , moves to the next zone
print "loading";            ' a trailing ; or , holds the line
print                       ' an empty line
```

- `print` is a statement and needs no import. It takes values separated by `;` or `,` and writes
  them to the console, then a newline.
- `;` writes the next value where the last ended. `,` first moves to the next 14-column zone, as
  QBasic's PRINT does. A separator after the last value leaves the line open.
- `print (a + b) * 2` prints one value. `print(a, b)` is a compile error that names `print a, b`.
- An integer is spelled in decimal, with a `-` when negative and no leading space. A `single`
  shows 7 significant digits and a `double` 15, as `%g` does, and a whole number keeps its `.0`:
  `print 2.0; " "; 1e20` gives `2.0 1e+20`. A bool is `true` or `false`.
- A plain enum value is spelled by its name. A record or an enum with a `text` method is spelled
  by it (see [Methods](#methods)). A `T or none` is `none` or its `T`.
- Any other value is a compile error: a list, a dict, a tuple, a `T or Error`, a record without
  `text`.

### Control flow

```basic
if hp <= 0 then return                    ' one line: one statement
if x < 0 then x = 0 else x = x mod w      ' with an else
if x < 0 then
    x = 0
elseif x > w then
    x = w
else if x = w then                        ' the same as elseif
    x = w - 1
else
    x += 1
end if

for i = 0 to n - 1 ... next               ' 0 up to n - 1, both included
for i = n to 1 step -1 ... next
for name in names ... next
for i, name in names ... next             ' with its index
for mut b in balls ... next               ' elements writable
for key, age in ages ... next
for i in 0..n ... next                    ' a range: 0 up to n - 1
while busy ... wend
do ... loop                               ' until a break
do ... loop until done
do ... loop while busy

select case mode
    case Mode.walk, Mode.roll
        move(hero)
    case else
        fly(hero)
end select

let sign = 1 if x >= 0 else -1
```

- A condition is a `bool`.
- A one-line `if` holds one statement after `then` and, optionally, one after `else`.
- `for i = a to b [step s]` counts an `int` from `a` to `b`, both included. `b` and `s` are
  evaluated once. Without `step` it counts up by 1. It runs while `i <= b` when `s` is positive,
  while `i >= b` when negative; it ends at an `int`'s last value without overflow. The counter
  can't be assigned. A constant step of 0 is a compile error.
- `for … in` takes any iterable: see [Iteration](#iteration).
- `next` ends a `for` and takes no name. `wend` ends a `while`. `loop` ends a `do`.
- `loop until c` leaves when `c` is true after a pass; `loop while c` when it is false. The body
  runs at least once. `continue` in a `do` goes to that test. The test is outside the body: it
  doesn't see the body's names.
- `next`, `wend` and `loop` are written alone; `end for`, `end while` and `end do` are errors.
  `next` is a keyword only where a statement starts: `it.next()` is a method.
- `break` and `continue` act on the innermost loop.
- `select case x` takes a number, a string, a bool or an enum. A case is a list of:
  - a value: `case 3, 5`, `case "q", "x"`;
  - `a to b`, both included: `case 3 to 5`;
  - `is` and a comparison: `case is > 7`;
  - an enum's value, with or without its enum's name: `case walk`, `case Mode.walk`;
  - an enum's value with names for its data: `case circle(r)`.
- A bare name in a case of a plain enum is its value when the enum has one of that name.
  `match` is an error naming `select case`.
- The first case that fits runs, and no other. A `select case` must cover every value or have
  `case else`: an enum's values, a bool's two; any other type needs `case else`.
- `a..b` is a range: from `a` up to, not including, `b`. It is a loop's, a slice's (`s[1..3]`,
  `a[..n]`, `a[n..]`), `in`'s and a chain's source. It has no step and counts up; `(a..b).rev()`
  counts down.
- A slice is a new value: a string's a new string, a list's a new list of copies. Its ends are
  clamped to the elements, so `s[..100]` of a shorter `s` is all of it. `s[i]` past the end is a
  runtime error, Subscript out of range.
- **Not yet:** a `for` stepping past an `int`'s last value stops with Overflow.

### Records

```basic
type Vec
    x as double
    y as double
end type

type Ball
    at as Vec
    r as double = 6
    private hits as int
end type

let v = Vec(3, 4)           ' fields in order
let b = Ball(at: v)         ' or by name; the rest take their defaults
```

- A record is a value. Its fields are laid out in order, each inline.
- `T(…)` builds one: values in field order, or `name: value`, by position first. A field left out
  takes its default, else zero.
- A field's default is a constant expression.
- A record can't hold itself; it holds a list of itself.
- A field is seen wherever its type is, unless marked `private`: then only its module reads or
  writes it. A type with a private field is built, `T(…)`, only in its module; elsewhere one comes
  from its methods, or is declared `var x as T`, its fields their defaults.
- `private` goes before a field or a method alone; a top-level name is its module's unless
  `public`.

### Enums

```basic
enum Mode
    walk
    jump
    roll
end enum

enum Shape
    circle(r as double)
    rect(w as double, h as double)
    dot

    fn area(self) as double
        select case self
            case circle(r)
                return math.pi * r * r
            case rect(w, h)
                return w * h
            case dot
                return 0
        end select
    end fn
end enum

let m = Mode.jump
let s = Shape.circle(2)
print m; " "; int(m); " "; s.area()     ' jump 1 12.5663706143592
```

- A value is written `Enum.name`; one with data takes all of it: `Shape.circle(2)`.
- A plain enum, one with no data, numbers its values from 0: `int(m)`. It compares with `=` and
  `<>`, is a dict's key, and has a built-in `text()` that gives its value's name.
- An enum with data is taken apart by `select case`. It has no `=`.
- An enum is stored inline: a tag and room for its largest value. Nothing is allocated.
- **In the VM**, an enum lays out each value's data after the tag, none overlapping.

### Methods

```basic
type Vec
    x as double
    y as double

    fn length(self) as double
        return math.sqrt(self.x * self.x + self.y * self.y)
    end fn

    fn scale(mut self, k as double)
        self.x *= k
        self.y *= k
    end fn

    fn polar(r as double, a as double) as Vec    ' no self: called on the type
        return Vec(r * math.cos(a), r * math.sin(a))
    end fn

    fn text(self) as string
        return f"({self.x}, {self.y})"
    end fn
end type

var v = Vec.polar(1, 0)
v.scale(2)
print v.length(); " "; v                ' 2.0 (2.0, 0.0)
```

- A method is written inside its `type … end type` or `enum … end enum`, after the fields or
  values, as `fn name(…) [as T] … end fn`.
- A first parameter named `self` makes it a method on a value: `v.length()`. `self` has no `as`;
  its type is the type it is in. `self` reads the value. `mut self` changes it, and the value it
  is called on is a `var`, a `mut` parameter or a `for mut` element. `self` is a keyword, a
  parameter nowhere else.
- A method without `self` is called on the type: `Vec.polar(1, 0)`. One with `self` is called on
  a value only: `Vec.length(v)` is a compile error.
- A field is `self.x`, never bare. Another method is `self.f()` or `Vec.f()`.
- A method is a function in all else: defaults, named arguments, `[T]`, `yields`, a result.
- `v.f(x)` looks in the methods and fields of `v`'s type and nowhere else. `f(v, x)` calls a
  function and never a method.
- A type's methods, fields and, in an enum, values share one set of names; two of one name is a
  compile error. `b.on(5)` calls a field's function value.
- A method is seen wherever its type is, unless marked `private fn`: then only its module calls
  it. A method is never imported by name.
- Only its own `type` block gives a type methods. A program gives none to `string`, a list, a
  dict or another module's type; it writes a function: `text.upper(s)`.
- `Vec.length` is a function value, a `function(Vec) as double`. A `mut self` method is no value.
  Passed to a chain or `sort` by name, as `vs.map(Vec.length)`, it is called directly, as a
  function's name is.
- Two names are reserved:
  - `fn text(self) as string`: `print` and an f-string spell the value by it. A `text` with
    another signature is a compile error.
  - `fn drop(mut self)`: runs when the value's owner ends (see [Ownership](#ownership)). It is
    never called by name, and only a `type` has one: a top-level `function drop` is a compile
    error.
- The built-in types' methods are the core's: a list's, a dict's, an iterable's (below) and a
  kept generator's `next()`. `len(x)` is a function, not a method.
- There is no inheritance, no interface and no dispatch at run time. A generic function is
  checked against each type it is used with, methods included, so `x.area()` in a generic needs
  no interface. A list of mixed shapes is an enum with data.
- **In the VM**, a method is a procedure with `self` its first parameter, and a call of it is the
  call a function's is. A method whose body is one `return` is inlined.
- **Not yet:** a method whose body is one `return` is called, as a function is.

### Operators on records

```basic
function operator +(a as Vec, b as Vec) as Vec
    return Vec(a.x + b.x, a.y + b.y)
end function

function operator *(a as Vec, k as double) as Vec
    return Vec(a.x * k, a.y * k)
end function

function operator *(k as double, a as Vec) as Vec
    return a * k
end function

function operator -(a as Vec) as Vec
    return Vec(-a.x, -a.y)
end function
```

- `function operator` defines `+ - * / = <` of two values, or unary `-` of one, at the top level
  of a record's module. One of its values is that record.
- A type has one operator for each pair of value types. The values' types pick it; a value
  widens to a parameter's type when no operator takes it as it is. Two that take it so are a
  compile error: with `Vec * single` and `Vec * double`, `v * 2` is one.
- `=` and `<` give a `bool`. `<>`, `>`, `<=` and `>=` follow from them.
- `v += w` is `v = v + w`.
- Another module uses an operator only if it is `public`.
- An operator is not generic and yields nothing. A comparison chain of records, `a < b < c`, is
  an error.
- **In the VM**, an operator whose body is one `return` is inlined, its parameters read where
  its arguments are; any other is called. A record built from its own fields, `p = p + v`, is
  built in place when no field is read after it is written, so it costs the field arithmetic.

### Generic records

```basic
type Stack[T]
    items as T[]

    fn push(mut self, x as T)
        self.items.push(x)
    end fn

    fn top(self) as T or none
        if len(self.items) = 0 then return none
        return self.items[len(self.items) - 1]
    end fn
end type

var s as Stack[int]
s.push(3)
```

- `[T]` after a type's name makes it generic; `[T, U]` takes two.
- Its type is written out where it is declared: `Stack[int]`. `Stack[int]()` builds one. Built
  with values, its types are read from them: `Pair(1, "a")`.
- The compiler makes one copy of the type and its methods for each type used.
- **Not yet:** all of it.

### Functions

```basic
function length2(v as Vec) as double
    return v.x * v.x + v.y * v.y
end function

function grow(mut v as Vec, k as double = 2)
    v.x *= k
    v.y *= k
end function

function divmod(a as int, b as int) as (int, int)
    return a div b, a mod b
end function

var pos = Vec(3, 4)
grow(pos)
grow(pos, k: 3)
```

- `function name(parameters) [as T] … end function`, at the top level only. It is called as
  `f(x)`, as a statement or in an expression.
- A parameter is read-only. A number, a bool, a plain enum, a string and a function value pass by
  value: a call that changes the variable passed leaves the parameter as it was. Any other passes
  by address, so a record or a list is not copied.
- `mut` makes a parameter writable: the function changes the caller's value. It is written on the
  parameter only. Its argument is a `var`, a `var`'s field or element, a `mut` parameter or a
  `for mut` element, of the parameter's exact type.
- A parameter may have a default, written as a field's is: `level as single = 1`.
  - The default is a constant expression. The caller passes it as if it were written, so a
    default costs nothing.
  - Parameters with defaults come last. A call leaves out trailing ones, or names the one it
    sets: `draw(p, color: 4)`. A value by position never follows one by name.
  - A `mut` parameter has no default.
- A function without `as` returns nothing. One with `as` that can reach `end function` without a
  `return` is a compile error. `return` outside a function is a compile error.
- Any type can be a result. A result is never a borrow.
- A result is built in the caller's place. `return r` moves `r` there, so `let s = squares(10)`
  copies nothing. A place the arguments read, or a module variable the function may see, gets
  the result in a temporary first, then a copy: QBasic's cost.
- `function` is the word for a top-level definition and for a function's type. `fn` is the word
  for a method and a lambda. One where the other belongs is a compile error that names the right
  word.
- `grow(mut pos)` is a compile error: the call passes `pos` as it is.
- **In the VM**, a parameter by address is a ref to its argument, its number fields read and
  written through it by `P.FL.LD` and `P.FL.ST`. An argument by value that is a variable's place is
  passed by its address too, as cheap, unless the call may change that variable: a module var the
  callee writes, or one lent `mut` in the same call. Then its value goes in a cell of its own,
  `BYVAL`; a string's is copied.

#### Generic functions

```basic
function largest[T](a as T[]) as T
    var best = a[0]
    for x in a
        if x > best then best = x
    next
    return best
end function
```

- `[T]` after the name makes a function generic. The compiler makes one copy of it for each type
  used, so nothing is dispatched at run time.
- Each `T` is read from the values given; a call never writes it.
- A type is checked when the function is used: `largest` of a type with no `<` is an error in
  its body, named for the copy: `can't compare a Vec with a Vec in largest[Vec]`. A generic
  never used is never checked.
- **In the VM**, each copy is the generic's tree parsed again with `T` bound, compiled onto copies
  of its lines, so an error or the debugger shows the generic's own line.

#### Function values and lambdas

```basic
let hot = temps.filter(fn(t) t > 30).collect()
balls.sort(fn(a, b) a.at.y < b.at.y)
let on_hit as function(int) = flash        ' a named function as a value

let near = balls.filter(fn(b as Ball)
    let dx = b.at.x - hero.at.x
    let dy = b.at.y - hero.at.y
    return dx * dx + dy * dy < 400
end fn).collect()
```

- `fn(x) expr` is a one-line lambda. `fn(x as T) … end fn` is a longer one. Parameter types
  come from where the lambda is used.
- A lambda is only an argument for a function's parameter: `let f = fn(x) x` is an error.
- A lambda may use the variables of the function it is written in: it borrows them for the call
  it is passed to. It is inlined, and nothing is allocated.
- A function value is a named function, or a method by `Type.name`, that takes no `mut`, no
  generic types and no functions. Kept in a variable or a record it captures nothing: it is a
  code address.
- A function taking functions is compiled once for each lambda or named function passed to it,
  as a generic is for each type. A lambda's result may type the generic: `map_all(xs, fn(x)
  f"{x}")` is a `string[]`.
- A lambda can't call another lambda its caller was given: it passes it on, `apply(f, x)`.
- **In the VM**, a one-line lambda's body is checked again and inlined at each call of its
  parameter. A block lambda is a hidden function: each call of its parameter calls it. A
  variable the lambda uses is a hidden parameter by reference, of the copy and of the block's
  function. A call through a function value is `CALL QX` with the value, a procedure's number,
  on top: the call `PCALL` makes, its target found from the procedure. It costs a `PCALL` and a
  load. A line with no statement, a comment's or a block lambda's where it is written, costs
  nothing to pass.

#### Generators

```basic
function evens(n as int) yields int
    for i = 0 to n - 1 step 2
        yield i
    next
end function

for x in evens(10)
    print x
next
```

- `yields T` makes a function a generator, and `yield` hands out each value. A generator has no
  `as` type.
- `return x` in a generator yields `x` last, then ends; a bare `return` ends it.
- In a `for`, a generator compiles to a state machine in its caller's frame, so nothing is
  allocated. It borrows its arguments until the loop ends; a parameter may be `mut`.
- A generator kept in a `var` is resumed with `it.next()`, which gives a `T or none`: `none` once
  it ends. Kept that way it owns its arguments: they are moved or copied in, and none is `mut`.
- A kept generator is a value: `var b = it` copies its state, and `for x in it` and a chain run
  it on. Its type is its generator's name: `function drain(mut it as upto)`, `function make() as
  upto`. `next()` changes it, so it takes a `var` or a `mut` parameter. A generic generator's
  state, or one taking a function, has no name to write.
- A generator can't loop over itself, and can't own a value with `drop`.
- **In the VM**, each `for x in g(..)` gets its own copy of `g`, checked and compiled in the
  loop's frame. Its parameters and locals are the caller's locals. A constant argument is
  inlined, a record or list place is read where it is, and the rest are stored when the loop
  starts. A `yield` stores `x` and jumps to the body; `next` jumps back past the `yield`, on a
  resume index when there are several. When the only `yield` gives a variable or a constant, `x`
  names it and nothing is stored.
- A kept generator is a procedure, one for each generator (and its types). Its state is a
  record: its parameters, its resume index, its locals; each `next()` passes the state's fields by
  reference, and its entry jumps to the `yield` the index names.
- The link threads every jump and line end past what only passes control on: a line with no
  statement, a line that only jumps. A `yield` and its loop's `next` cost no op.

### Lists and dicts

- A list's methods: `a.push(x)`, `a.pop()` (the last, taken), `a.insert(i, x)`, `a.remove(i)`,
  `a.swap(i, j)`, `a.sort()` and `a.sort(fn(x, y) x < y)`. `len(a)` is its length and `x in a`
  searches it. `a[i]` outside its elements is a runtime error, Subscript out of range.
- A fixed array has `swap` and `sort`; its size never changes.
- `a.sort(fn(x, y) …)` orders by a "less than" lambda and is stable. `a.sort()` orders numbers
  and strings by value.
- A dict's methods: `d.get(k)` gives a `V or none`, a copy of the value; when `V` is a `T or
  none` it gives that, `none` too when `k` is not there. A dict of `T or Error` has no `get`.
  `d.remove(k)` does nothing when `k` is not there, and `d.keys()` gives a new list. `d[k] = v`
  adds `k` or changes its value. `d[k]` reads it, a runtime error, Subscript out of range, when
  `k` is not there. `k in d`, `len(d)`.
- A key is an `int`, a `string` or a plain enum.
- `for k, v in d` and `for k in d` go in the keys' order: as added, except that a remove moves
  the last key into the removed one's place.
- **In the VM**, a list whose elements are lists is a list of one-field TYPEs, each holding its
  element's cell, as a record holding a list does; a dict of lists keeps its values so.
- **In the VM**, a dict is a record of three lists: its keys, its values and its hash slots, each
  an entry's index. A key is found by its hash and a probe, half the slots at most in use. A
  list's growth on the heap leaves its old array for a sweep, which walks every string block, so
  the sweep waits until the dead arrays are half the heap in use or room runs short.
- **In the VM**, `sort` is C. With a lambda it is a natural merge sort over the elements' indexes:
  its ascending runs found, n - 1 compares, then merged; the order found is applied by swaps.
  Each compare runs the lambda's code, compiled after `CALL QX` and jumped over, on the two
  indexes. One buffer of 12 bytes an element is allocated for the sort and freed after it.
  Without one it is a quicksort, compares in C: a median-of-three or ninther pivot and a
  three-way partition, the smaller side recursed.

### Iteration

`for … in` takes any iterable: a list, a fixed array, a dict, a string, a range `a..b` or a
generator. Each has the same lazy methods:

| Kind | Methods |
|---|---|
| Change | `map`, `filter`, `take`, `skip`, `zip`, `rev`; the index comes from `for i, x in` |
| Reduce | `sum`, `min`, `max`, `count`, `any`, `all`, `find`, `fold` |
| Collect | `collect()`: builds a list, the only method here that allocates |

```basic
let total = orders.filter(fn(o) o.paid).map(fn(o) o.price).sum()
for b in balls.filter(fn(b) b.r > 4)
    print b.r
next
```

- A chain compiles to one loop, with its lambdas inlined. No list is built between the steps.
- A chain is used up in the statement that makes it, by a `for`, a reducing method or
  `collect()`. It borrows its source only for that statement.
- A string's elements are its bytes; a dict's are its keys. A range is written `(a..b)` before a
  method, and `zip(0..n)` takes one bare.
- A step or a reducing method takes a lambda, or a name to call: a function, a function value,
  a lambda passed in. A one-line lambda is inlined; a block lambda is called, so it names its
  result's type unless it's a `filter`'s.
- `min()`, `max()` and `find(p)` give a `T or none`. `count()` counts the elements, `count(p)`
  those `p` holds for. `fold(first, fn(acc, x) ..)` starts from `first`. `sum()` of bytes or
  shorts adds them in an `int`.
- `rev()` goes anywhere in a chain over a list, array, string or range, once. It can't follow a
  generator, nor a `filter`'s `take`, `skip` or `zip`. On a range it counts down.
- `zip(b)` takes a variable's list or string, or a range. Its pair's parts are the next
  lambda's two parameters, `fn(x, y)`, or a `for`'s two names: `for a, b in xs.zip(ys)`. A
  second `zip` adds a third part. `sum`, `min`, `max`, `find` and `collect` take one value: map
  the pair first.
- A generator's call in a chain is kept as a `var`'s would be: each element is a `next()`.
- There are no comprehensions.
- **In the VM**, a chain is a loop within its statement's line: its start a label, its counter
  stepped and tested by the ops a `for`'s `next` uses, fused to one `I.NEXT`. With a string
  on the stack under it, its jumps are `QJ`s. A `for` over a chain starts it and runs its steps
  on the `for`'s line; `next` steps its counter.

### Ownership

```basic
var a as int[]
a.push(1)
var b = a           ' a copy: a is used below
b.push(2)
print len(a)        ' 1
var c = b           ' a move: b is not used again
```

- Records, lists and dicts are values with one owner, and are freed with it. The collector never
  sees them.
- Assignment copies: after `b = a`, changing one never shows in the other. A record of plain
  fields is one block copy; a list or a dict copies its elements.
- When `a` is not used again, `b = a` moves it and nothing is copied.
  - "Not used again" is read from the source: `a` is a whole `let` or `var`, no use of it
    follows, no function sees it, and the assignment is not in a loop `a` is declared outside.
  - A field, an element and a parameter are always copied.
- An argument kept by what it is passed to is assigned by the same rule: `a.push(x)`, `Ball(at:
  v)`, a kept generator's arguments. So is a returned module variable or parameter.
- There is no `copy()`; `copy(a)` is a compile error that names assignment. `basic --copies
  FILE.bas` lists each copy kept of a list, a dict or a record holding one, by its line, column
  and type, and runs nothing.
- A type with an `fn drop(mut self)` has it run when an owner of one ends, at a known point: a
  `File` closes itself. There are no finalizers and no collector pauses.
  - A local ends when its function returns, by any `return` or `?`, or when its declaration runs
    again in a loop. A module's `var` or `let` ends when the program does. A record's fields end
    after its own `drop`, a list's elements with the list, or when `remove` or an unkept `pop` takes
    them. A place assigned drops its old value after the new one is made.
  - A value holding one with `drop` only moves: assigning it and using it again is a compile
    error. Reading a fresh one in place, `make().name`, is an error too: keep it in a `let`
    first. A statement's value that nobody keeps is dropped.
  - A dict's values, an enum's data, a list of lists and a generator's locals can't hold one, nor
    can a type hold itself.
  - A runtime error or `os.exit()` runs no `drop`.
- There are no pointers: an element is referred to by its index.
- **In the VM**, each owner of a value with `drop` has a hidden flag, set when it takes a value
  and cleared when the value moves out; the function's returns drop each owner whose flag is set.
  A list's elements are dropped by a loop in the line. A function with no owner runs nothing more.

### Where values live

- A local lives on the stack, in its function's frame: numbers, records, tuples, optionals,
  enums, a generator's state and fixed arrays.
- A fixed array larger than 4 KB goes on the heap instead, owned by the frame and freed when
  the function returns.
- A list, dict or string keeps its header in the frame and its elements on the heap.
- A module `var` lives in the module's data, made once when the program starts.
- A frame is allocated and freed by moving the stack pointer. Nothing else is touched.
- **In the VM**, a `let`'s, a `var`'s or a temporary's record up to 4 KB lives in cells, as a
  local's: its strings and lists owned as a QBasic TYPE's are, gone with its frame. A number
  field in its first 256 bytes, at any offset, is a variable of its own, a byte's or a short's
  read and written by `N.LD` and `N.ST`; one past them is read through the record's address. A
  dict and a larger record are boxes on the heap, reached by `R.ADDR`. A fixed array variable is
  a dynamic array; as a field it is inline. A `T or none` and a `T or Error` live in cells as a
  record does.

### Borrowing

The rules make sure no write is ever seen through another name. The compiler checks all of them
at compile time. They concern records, lists, dicts, tuples and optionals; a value passed by
value is never borrowed.

1. **A plain parameter is a shared borrow.** The function reads the value in place, and the
   caller may lend the same value to any number of shared borrows at once.
2. **A `mut` parameter is an exclusive borrow.** While the call runs, its argument can't be
   reached any other way.
   - With `function f(mut a as Vec, b as Vec)`, `f(v, v)` is an error.
   - `f(a[i], a[j])` is an error, because `i` and `j` could be equal. `a.swap(i, j)` exchanges
     two elements.
   - `self` follows the same rules.
3. **Only a `var` can be borrowed `mut`**, never a `let`, a `const` or a plain parameter.
4. **A borrow ends with its call.** No record, list or variable holds one, and no function
   returns one, so there are no lifetimes to write.
5. **`for mut x in a` borrows `a` exclusively for the loop.** The body reaches `a` only through
   `x`: it can't read `a`, push to it or remove from it.
6. **`for x in a` borrows `a` shared for the loop.** The body can't change `a`.
7. **A lambda borrows what it uses for the call it is passed to**, exclusively if it assigns
   to it.
8. **A generator borrows its arguments** until its loop ends.
9. **Module `var`s count too.** A call can't borrow a module variable that the callee, or
   anything it calls, writes. The compiler knows which ones each function writes. A call
   through a function value may reach any function used as a value, and a drop counts as a call.
10. **A value with `drop` that moved is gone.** Using it before it is assigned again is an error.

What this buys:

- A `let`, and anything borrowed shared, can stay in a register across calls.
- A `mut` parameter's fields can be held in registers through a loop, because nothing else
  writes them.
- No reference counts and no run-time checks.

### Optional values

```basic
function oldest(ages as dict[string, int], name as string) as int
    let age = ages.get(name)            ' int or none
    if age is none then return -1
    return age + 1                      ' an int from here on
end function

let n = ages.get("bo") or else 0
```

- `T or none` holds a `T` or `none`. It is a `T` and a flag, inline, with nothing allocated.
- `x is none` and `x is not none` test it.
- After `if x is none then return`, or any guard that leaves, `x` is a `T`. So is it in the
  branch a test proves: the `else` of `if x is none`, the `then` of `if x is not none`.
- Narrowing takes a `let` or a parameter, which can't change. A `var` stays a `T or none`: read
  it with `or else`, or bind a `let` to it first.
- `a or else b` gives `b` when `a` is `none`.
- `none` needs a place that says its type: `var x as int or none = none`.

### Errors

Errors are values: nothing unwinds, in the VM or in native code.

```basic
const EMPTY = 1

function load(path as string) as string or Error
    let text = fs.read_text(path)?      ' an error returns from load
    if len(text) = 0 then return Error(EMPTY, f"{path} is empty")
    return text
end function

function show(path as string)
    let r = load(path)
    if r is Error then
        print r.message
        return
    end if
    print r                             ' a string from here on
end function
```

- `T or Error` holds a `T` or an `Error`: a code, a message and a line. It is inline, a tag
  beside the value.
- `return` gives either one: `return text` or `return Error(code, message)`. `Error(…)` needs a
  `T or Error` place.
- `x?` gives the value, or returns the error from the function it is in. That function's result
  is a `T or Error` too. `parse(a)? + parse(b)?` returns the first error.
- `x is Error` and `x is not Error` test it, and narrow as `is none` does.
- `r.code`, `r.message` and `r.line` read its error; they are 0 and "" when there is none.
  `r.line` is the line in its module's file.
- `a or else b` takes a `T or Error` too.
- A runtime error in the language itself, such as an index out of range or a division by zero,
  stops the program at its line. A program handles only the errors a function returns.

### Modules

- A file is a module and a folder is a package, found as QBasic's `IMPORT` finds one (the main
  file's folder, each folder of `BASICLIB`, then `lib/` beside the interpreter), as a `.bas`
  whose first line says it is Quickr's.
- `import m`, `import m as n`, `import a.b` (then `a.b.f()`), `from a.b import X, Y as Z`,
  `from pkg import m` (a package's module) and relative `from .feet import Land`: `.` is the
  importing file's folder, `..` its parent. An import goes at the top level.
- A top-level name is exported only if marked `public`: a `function`, `type`, `enum`, `const`,
  `var` or `let`. A name a module imports is not exported again.
- A type's fields and methods come with it, the `private` ones aside. `clock.tick()` needs only
  a `Clock`, however it got here.
- Only the main file runs top-level statements. A module's top level holds declarations, imports
  and `var`s; anything else is a compile error.
- A module's `var`s and `let`s start once, before the main file's first line: each module after
  those it imports, in the order its imports are written, the first met first in a cycle. Reading
  the var of a module that starts later, at another's top level, is a compile error.
- Across a cycle, declarations refer to each other freely: every function, type and const is known
  before any body is checked.
- An error, at compile time or run time, names the module's own file and line.
- **In the VM**, each module's lines are the program store's. The run goes through the modules
  in the order above, the main's last: each module's last line goes on to the next one's first.

### Tests

```basic
test "a ball bounces off the left wall"
    var b = Ball(at: Vec(1, 50), vel: Vec(-10, 0))
    b.move(0.1, Vec(320, 200))
    assert b.vel.x > 0
end test
```

- `test` blocks sit at any module's top level. `basic --test` runs them; a program's build leaves
  them out.
- A test runs as a function of no parameters: `return` ends it.
- `assert c` stops with the file, the line and the condition's text when `c` is false.
- `basic --test FILE.bas` runs the tests of the file and of each module it imports, a module's
  after those it imports, each in source order. Each prints `pass name`, or `FAIL name:
  file:line: why`, an assert's why its condition's text; then `3 passed, 1 failed`, the exit 1
  when one failed.
- A false assert or a runtime error ends its test, its owners dropped, and the next one runs.
- Under `--test` the main file's top level runs its declarations alone; modules start as in a
  run.
- Outside a test, a false assert stops the program: `prog.bas:5: assert failed`, then the line.
- **In the VM**, a test is a procedure whose handler, ON LOCAL ERROR's, says the failure and
  RESUMEs at its `end test`. Each test's run is a line of its own after the program's.

## Execution efficiency

The rule: a spelling change costs nothing, and what the source does not ask for is not run.

| Rule | Consequence |
|---|---|
| Every type is known at compile time | each op is typed (`iadd`, `fadd`), and no op checks a type at run time |
| Every name resolves at compile time | calls, fields and methods are direct addresses; nothing is searched while running |
| No virtual dispatch, no boxing | a `Ball[]` is one block of `Ball`s, laid out as a C array |
| A method is a function | `v.length()` costs what `length(v)` costs |
| Generic functions and types compile once per type used | no boxing, no dispatch |
| Lambdas passed to a call inline | a `filter` costs what the loop it replaces costs |
| Iterator chains fuse | `map`, `filter` and `sum` make one loop, with no list built between them |
| Generators are state machines | a `yield` is a jump, and nothing is allocated |
| Enums and optionals are inline | a tag beside the value, never a heap block |
| Records and lists pass by address | passing one costs a pointer, never a copy |
| Numbers and strings pass by value | a call may change the variable passed; its value is copied only then |
| A last use moves | `b = a` copies only when both are used after; `--copies` lists where |
| Locals live on the stack | a call's locals cost one stack-pointer move; only large arrays and growing containers reach the heap |
| Results are built in place | a returned record or list is not copied |
| Borrows can't alias | values stay in registers across calls and loops |
| Allocation happens in few, known places | a list growing past its room, a dict insert, building a string, and a copy of a list or dict. **In the VM:** a push past a list's room doubles it; a pop gives nothing back until the list is empty |
| A string is built in one allocation | an f-string sizes its result first; loops use `text.Builder`. **In the VM:** up to 255 parts at a time |
| `print a; b` builds no string | each value is written as it is |
| Owned values free deterministically | lists, dicts and records free with their owner; only strings are collected |
| `for x in a` checks no bounds | the loop owns the range; `a[i]` checks. **In the VM:** `x` is a copy its loop's `next` loads, unchecked: a number, an enum, a record of them. A string, a record holding strings or lists, and a `for mut`'s `x` are `a(i)`, checked. A resumable generator's loop copies numbers of 4 or 8 bytes |
| `for i = a to b` is QBasic's FOR | its `next` is one fused op, `I.NEXT.UP` or `I.NEXT.DN`; a step whose sign is known only when run, `I.PAST` |
| Errors are values | a tag and a value; no unwinding |
| Heavily used library calls are ops | `math.sqrt` compiles to the VM's sqrt op, not a call |
| Small functions inline | `Vec` arithmetic costs what the field arithmetic costs |
| `const` folds | constants, and expressions of them, fold before codegen |

- `int` is the 386's register width, so its arithmetic is one instruction; `long` takes two.
  `single` and `double` compute at the same speed on the 486's FPU; `single` halves an array's
  memory.
- Each rule gets a `tools/bench.py` benchmark against QBasic-dialect code doing the same work.

## The standard library

The core has `print`, the type conversions, `len`, `wrap`, `Error`, and the methods of lists,
dicts and iterables. Everything else is in a module.

| Module | Has | Replaces |
|---|---|---|
| `io` | `read_line`, `at`, `color`, `clear` | LINE INPUT, LOCATE, COLOR, CLS |
| `text` | `find`, `find_from`, `upper`, `lower`, `trim`, `split`, `join`, `chr`, `code`, `hex`, `number`, `to_int`, `Builder` | INSTR, UCASE$, LCASE$, LTRIM$, RTRIM$, CHR$, ASC, HEX$, VAL |
| `math` | `pi`, `sqrt`, `sin`, `cos`, `tan`, `atan`, `atan2`, `exp`, `log`, `floor`, `ceil`, `round`, `abs`, `min`, `max`, `clamp`, `lerp`, `Vec` | SQR, SIN, COS, TAN, ATN, EXP, LOG, INT, ABS |
| `random` | `seed`, `int(a, b)`, `real(a, b)`, `pick` | RND, RANDOMIZE |
| `time` | `now`, `ticks`, `sleep`, `Clock` | TIMER, DATE$, TIME$, SLEEP |
| `fs` | `File`, `read_text`, `write_text`, `list` | OPEN, GET, PUT, SEEK, LOF, CLOSE, FILES |
| `keys` | `Key`, `next`, `pressed`, `down`, `held_for`, `Mouse`, `mouse`, `clicked` | INKEY$, PEN |
| `os` | `args`, `env`, `exit`, `run` | COMMAND$, ENVIRON$, END, SHELL |
| `gfx` | windows, canvases, drawing, sprites, fonts | SCREEN, PSET, LINE, CIRCLE, PAINT, GET, PUT, PALETTE |
| `synth` | instruments and songs | PLAY, SOUND, BEEP |

- A module is written in Quickr, in `stdlib/`; the build puts it in `lib/` beside the
  interpreter, where a module is looked for last: the Mac app's in `Contents/Resources/lib`, the
  web page's in `/lib`.
- Its machine-level parts are `native function` declarations: `public native function sqrt(x as
  double) as double`. A native function has no body; its entry, by its name, is in
  `src/core/qr/qnat.c`. A call compiles to the entry's code, which is what QBasic's statement
  compiles to, so a library call costs what the built-in does. A native takes and gives numbers,
  bools and strings, by value. It is a top-level function, never a method; a method calls it.
- `math.sqrt` against `SQR`, and `text.find` against `INSTR`, run the same ops as QBasic (benches
  sqrt and find); host instructions are 4% and 5% more.

`text`:

- `find(s, sub)` and `find_from(s, sub, start)` give an index from 0, -1 when not there.
- `number(s) as double` reads as VAL does: what it can't read is 0.
- `to_int(s) as int or Error` reads a whole decimal number with an optional `-`. Anything else,
  a space or nothing at all too, is an error of code 5, Illegal function call; a number past an
  `int`, of code 6, Overflow. `text.trim` cuts the spaces first.
- `Builder` builds a string in parts: `b.add(s)`, then `b.text()`.

`math`:

- `abs`, `min`, `max` and `clamp` are generic functions.
- `Vec` is `x` and `y`, `double`s. It has `+`, `-` and `=`, `*` and `/` by a `double`, `double *
  Vec`, unary `-`, and the methods `dot(b)`, `length()` and `text()`.

`time`:

- `ticks()` is seconds since midnight. `now()` is `2026-11-24 09:30:00`. `sleep(seconds)` waits
  on `ticks()`.
- `Clock` is a frame clock: `c.tick()` gives the seconds since the last tick, 0 the first.

`fs`:

- `File.open(path, mode = "r") as File or Error`: mode `"r"` to read, `"w"` to write it anew,
  `"a"` to add to its end, `"rw"` both.
- `f.read(count) as string` gives `count` bytes at most. `f.write(s)` and `f.seek(at)` give a
  `bool or Error`. `f.size() as int`. `f.close()` closes it sooner than its `drop` does.
- A `File`'s number is private. A file's error is a value, its `code` QBasic's ERR.
- `read_text(path) as string or Error` reads at most 32767 bytes, a string's most.
  `write_text(path, s)`. `list(pattern = "*.*")` gives the platform's DOS names, upper case on
  the host.

`keys`:

- `Key` is a plain enum; `k.code()` gives its code as `next()` gives it.
- `next()` is the next key's code, 0 none waiting. `pressed(k)` is true once for each press.
- The platforms report key presses and repeats, never releases: `down(k)` is true for `held_for`
  (0.55 s) after `k`'s last press or repeat.

`os` and `io`:

- `os.args()` is COMMAND$'s words, so upper case. `os.exit()` has no exit code, as END has none.
- `os.run(cmd)` runs `cmd` as SHELL does: `/bin/sh -c` on the host, `COMSPEC /C` on DOS. It
  returns nothing; a command that can't start is Illegal function call. **In the VM**, the
  command is cut at 122 bytes.
- `io.read_line` echoes the line's end, as LINE INPUT does. `io.at(row, col)` counts from 1.
  `io.color(fg, bg = 0)`.

**Not yet:** `gfx` waits on the graphics extension; `synth` on `lib/synth.qbx`.

## Building it

- **A front end of its own.** Quickr has its own lexer, parser and semantic pass, in
  `src/core/qr/`. They produce the checked tree that codegen takes. The VM, runtime, collector
  and debugger are shared with QBasic.
- **llrm can compile it too.** Nothing in the language needs the VM.
- **The IDE takes both dialects.** Each has its own highlighting, completion and help
  (`help/qr/`). Lines are checked one at a time; the whole program before a run. The debugger
  steps and stops by line, names spelled as typed (docs/ide-design.md).
- Tests are Quickr programs and their output, in `tests/qr/`; what is rejected, in
  `tests/qr/errors/`.
