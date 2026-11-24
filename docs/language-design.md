# Quickr: the modern dialect

Quickr is a second language for QuickrBasic. It keeps BASIC's look, has one way to do each thing,
and replaces QBasic's hundreds of statements and functions with a standard library you import.
QBasic 1.1 stays as its own dialect, unchanged, and each file is one or the other. Execution speed
is a goal of the language itself, not only of the VM.

It takes the good parts of llrm's draft QuickrBASIC (`docs/frontends/qb/dialects.md` in llrm), spelled
as llrm spells them:

- f-strings;
- tuples;
- slices;
- `for … in`;
- the `a if c else b` conditional;
- chained comparisons and `in`;
- `break`, `continue` and `return value`.

Quickr leaves out that draft's VBDOS base.

Status: a draft for discussion. Items marked **Open** need a decision.

## A program

```basic
' OPTION DIALECT QUICKR
from gfx import Window
from keys import Key
from math import Vec
import keys
import random
import time

type Ball
    at as Vec
    vel as Vec
    r as single = 6
end type

fn step(mut b as Ball, dt as single, size as Vec)
    b.at.x += b.vel.x * dt
    b.at.y += b.vel.y * dt
    if not b.r <= b.at.x <= size.x - b.r then b.vel.x = -b.vel.x
    if not b.r <= b.at.y <= size.y - b.r then b.vel.y = -b.vel.y
end fn

var win = Window(320, 200)
var balls as Ball[]
for i in 0..10
    let vel = Vec(random.real(-80, 80), random.real(-80, 80))
    balls.push(Ball(at: Vec(160, 100), vel: vel))
end for

var clock = time.Clock()
while not keys.down(Key.escape)
    let dt = clock.tick()
    win.clear(0)
    for mut b in balls
        b.step(dt, win.size)
        win.circle(b.at, b.r, 14)
    end for
    win.text(4, 4, f"{1 / dt:.0f} fps")
    win.present()
end while
```

## Choosing the dialect

- Files are `.bas` in both dialects. A Quickr file's first line is `' OPTION DIALECT QUICKR`
  (case aside), a comment to QBasic; a file without it is QBasic's. The line is the file's line 1,
  required in every Quickr file, modules too. Nothing else is guessed from a file's contents.
- File > New asks which, and a Quickr program starts with that line, which the editor keeps as it
  is. The status bar shows the program's dialect. `basic` runs either.
- **Open:** whether a Quickr file may import a QBasic module. Proposed: yes, one way only. Its
  procedures and TYPEs appear with Quickr types, and a QBasic file never imports a Quickr one.

## What goes

| QBasic | Why it goes | Quickr |
|---|---|---|
| Line numbers, labels, GOTO, GOSUB, ON...GOTO | unstructured | blocks, `break`, `continue`, `return` |
| `%` `&` `!` `#` `$` suffixes, DEFINT... | the type hides in the name | `as type`, or inferred |
| Implicit variables | a typo makes a new variable | every variable is declared, as llrm's `quickr` requires |
| SUB vs FUNCTION, CALL, assigning to the function's name | two ways to call | one `fn`, called as `f(x)`, with `return` |
| OPTION BASE, `lower TO upper` | off-by-one | zero-based, as llrm's `quickr` has it |
| ON ERROR, RESUME | global jumps | see [Errors](#errors) |
| `:` chains, `_` continuation, REM | dense lines | one statement a line; an open bracket continues the line |
| WEND, NEXT, LOOP UNTIL, DO WHILE | four loop endings | `end while`, `end for`, `end do` |
| EXIT FOR/DO, a FOR's name after NEXT | loops exited by kind | `break` and `continue`, innermost loop |
| COMMON, SHARED, DIM SHARED | globals across files | module variables, imported by name |
| ~200 statements and functions | the language is its library | an import each |

## The language

### Names and lines

- Names are case-sensitive. Keywords are lower case only.
- A newline ends a statement, except inside `(`, `[` or `{`.
- Comments start with `'`.
- Number literals: `0xff`, `0b1010`, `1_000_000`, `2.5`.
- Other literals: `true`, `false`, `"plain"`, and `f"x = {x:.2f}"`. An f-string uses Python's
  format-spec rules, as in llrm. A value without a spec is spelled as `print` spells it; a bool
  takes a string's spec: `f"{ok:>5}"`.

### Types

| Type | Is |
|---|---|
| `bool` | true or false; `if` and `while` take only this |
| `byte` | 8 bits, always |
| `short` | 16 bits, always |
| `int` | the native int: 32 bits on every target (386 and up, 64-bit hosts, wasm) |
| `long` | twice `int`: 64 bits |
| `ubyte` `ushort` `uint` `ulong` | the same widths, unsigned |
| `single` | 32-bit floating point |
| `double` | 64-bit floating point |
| `string` | bytes, immutable; `s[i]` is a `ubyte`, `s[1..3]` a slice |
| `T[]` | a list: grows, zero-based |
| `T[n]` | a fixed array of `n`, inline in its record |
| `V[K]` | a map from `K` to `V` |
| `(T, U)` | a tuple: a function's results, as in llrm |
| `T?` | a `T` or `none` |
| `T!` | a `T` or an `Error` |
| `fn(T) as U` | a function value |
| `type` | a record: a value |
| `enum` | named values, each with or without data |

- `byte`, `short`, `int` and `long` are signed. `ubyte`, `ushort`, `uint` and `ulong` are their
  unsigned twins: `ubyte` is 0 to 255, for pixels and file bytes.
- An operator takes two values of one type and gives that type: `byte + byte` is a `byte`.
- A value widens by itself only where nothing can be lost:
  - a narrower integer to a wider one of the same signedness;
  - an unsigned integer to a wider signed one;
  - an integer to `double`;
  - `single` to `double`.
- `int` and `single` together give a `double`, the type both widen to.
- Signed and unsigned never mix by themselves, so C's `-1 < 0u` trap can't happen. Every other
  conversion is written out: `int(x)`, `ushort(n)`, `single(d)`, `string(n)`.
- A literal takes the type its place needs: `r as single = 6` stores a `single`. A literal that
  doesn't fit is a compile error. Alone, `2.5` is a `double` and `7` is an `int`.
- Every size is the same on every target, so a record laid out for a file reads the same
  everywhere.
- Integer overflow is a runtime error, Overflow, and so is a conversion that loses the value:
  `ubyte(300)`. `wrap(x)` asks for wrapping, keeping each operation and conversion in `x` to its
  type's bits, for hashes and random numbers: `h = wrap((h ^ c) * 16777619)`.
- `-` of an unsigned value is a compile error outside `wrap`. A shift keeps its type's bits,
  dropping those shifted out; its count is 0 up to the type's bits, else Illegal function call.
- `sum()` of a list of bytes or shorts adds them in an `int`.
- **In the VM**, an integer to 16 bits and an `int` live in an int cell and compute with the int
  ops, a result's range then checked. A `uint`'s bits are in an int cell too, with ops of its own.
  A `long` and a `ulong` are 8-byte cells loaded and stored by the double ops, with ops of their
  own: a `long`'s `+ - *`, compare-and-branch and `x += e` one op each. A power, and `div` in
  `wrap`, are CALL QX. The longs bench runs 0.94x the instructions of QBasic's double sum; `t += i`
  of an `int` costs 1.4x the same loop on ints, one op more.

### Ownership

- Records, lists and maps are values with one owner, and are freed with it. The collector never
  sees them.
- `b = a` copies a record of plain fields with one block copy.
- A list, a map, or a record holding one moves when `a` is not used again. Anywhere else,
  `b = a` is a compile error and you write `copy(a)`. A copy is never hidden.
  - "Not used again" is read from the source: no use of `a` follows the move, `a` is no module
    variable a function sees, and the move is not in a loop `a` is declared outside.
  - Only a whole `let` or `var` moves. A field, an element or a parameter takes `copy()`.
- A type with an `fn drop(mut x as T)` has it run when an owner of a `T` ends, at a known point:
  a `File` closes itself. There are no finalizers and no collector pauses.
  - `drop` goes in `T`'s module and is never called by name.
  - A local ends when its fn returns, by any `return` or `?`, or when its declaration runs again
    in a loop. A module's `var` or `let` ends when the program does. A record's fields end after
    its own `drop`, a list's elements with the list, or when `remove` or an unkept `pop` takes
    them. A place assigned drops its old value after the new one is made.
  - A value holding one with drop moves and is never copied: `copy()` of it is an error, and so
    is reading a fresh one in place, `make().name`: keep it in a `let` first. A statement's
    value that nobody keeps is dropped.
  - A map's values, an enum's data, a list of lists and a generator's locals can't hold one, nor
    can a type hold itself.
  - A runtime error or `os.exit()` runs no `drop`.
  - **In the VM**, each owner has a hidden flag, set when it takes a value and cleared when the
    value moves out; the fn's returns drop each owner whose flag is set. A list's elements are
    dropped by a loop in the line. A fn with no owner runs nothing more.
- There are no pointers: an element is referred to by its index. **Open:** whether a reference
  type is needed for graphs of objects. Proposed: not in the first version.

### Where values live

- A local lives on the stack, in its function's frame: numbers, records, tuples, optionals,
  enums, a generator's state and fixed arrays.
- A fixed array larger than a limit goes on the heap instead, owned by the frame and freed when
  the function returns. **Open:** the limit. Proposed: 4 KB, a compile-time constant.
- A list, map or string keeps its header in the frame and its elements on the heap, since they
  grow.
- A module `var` lives in the module's data, made once when the program starts.
- A frame is allocated and freed by moving the stack pointer. Nothing else is touched.
- **In the VM**, a `let`'s, a `var`'s or a temporary's record up to 4 KB lives in cells, as a
  local's: its strings and lists owned as a QBasic TYPE's are, gone with its frame. A number
  field in its first 256 bytes, at any offset, is a variable of its own, a byte's or a short's
  read and written by `N.LD` and `N.ST`; one past them is read through the record's address. A
  map and a larger record are boxes on the heap, reached by `R.ADDR`. A parameter is a ref to its
  argument, its number fields read and written through it by `P.FL.LD` and `P.FL.ST`. A fixed
  array variable is a dynamic array; as a field it is inline. An enum lays out each value's data
  after the tag, none overlapping.

### Declarations

```basic
const SPEED = 120.0         ' folded at compile time
var frames = 0              ' at module level: every function here sees it

fn run()
    let name = "Ada"        ' immutable
    var count = 0           ' mutable; int, from its value
    var pos as Vec          ' zeroed; a field takes its default if it has one
    var ages as int[string]
    var speed as single = 6 ' both: the type wins; 6 is a single
    let q, r = divmod(17, 5)
    count += 1
end fn
```

- Every declaration has one shape, `name [as type] [= value]`: `let`, `var`, a parameter, a
  record's field. At least one of the two is written; with no `as`, the type is the value's. A
  `let` always has a value.
- `as` belongs to the name before it: `var x as single, y = 0`.
- `let` binds an immutable value: it can't be assigned, nor its fields or elements changed.
- `var` declares a mutable variable; at module level, every function of the module sees it.
- `mut` is a borrow: see [Borrowing](#borrowing).
- A module's `const`s and `fn`s are visible in the whole file. Its `var`s and `let`s are visible
  from their line on, and in every `fn`.

### Records and enums

```basic
type Vec
    x as single
    y as single
end type

enum Mode
    walk
    jump
    roll
end enum

let v = Vec(3, 4)           ' fields in order
let b = Ball(r: 2, at: v)   ' or by name; the rest take their defaults

enum Shape
    circle(r as single)
    rect(w as single, h as single)
end enum

fn area(s as Shape) as single
    match s
        case circle(r)
            return math.pi * r * r
        case rect(w, h)
            return w * h
    end match
end fn

fn operator +(a as Vec, b as Vec) as Vec
    return Vec(a.x + b.x, a.y + b.y)
end fn
```

- An enum's values may carry data, and `match` takes them apart. A `match` must cover every
  value, or have `case else`: an enum's values, a bool's two; any other type's needs `case else`.
- A record's fields left out of `Vec(..)` take their defaults, else zero. An enum value with data
  takes all of it: `Shape.circle(2)`. A case names its data: `case circle(r)`, with or without
  `Shape.`.
- A plain enum's value is an int to `int(m)` and to `print`.
- An enum is stored inline: a tag and room for its largest value. Nothing is allocated.
- A record may define `+ - * / = <` with `fn operator`. Such a function is inlined like any
  small one. `<>`, `>`, `<=` and `>=` follow from `=` and `<`.
  - The left value's type picks the operator, its first parameter's; the right value converts
    to its second: `fn operator *(a as Vec, k as double)`. `=` and `<` give a `bool`.
  - Another module uses an operator only if it is `public`, as a fn.
  - A comparison chain of records, `a < b < c`, is an error.
  - **In the VM**, an operator whose body is one `return` is inlined, its parameters read where
    its arguments are; any other is called. A record built from its own fields, `p = p + v`, is
    built in place when no field is read after it is written, so it costs the field arithmetic.

### Functions

```basic
fn length(v as Vec) as single
    return math.sqrt(v.x * v.x + v.y * v.y)
end fn

fn scale(mut v as Vec, k as single)
    v.x *= k
    v.y *= k
end fn

fn divmod(a as int, b as int) as (int, int)
    return a div b, a mod b
end fn

scale(mut pos, 2)           ' the caller writes mut too
print(pos.length())         ' a.f(b) is f(a, b)
pos.scale(2)                ' before the dot, mut is implied
```

- A parameter is read-only and passed by address, so a record is not copied.
- `mut` makes a parameter writable. The caller then writes `mut` too, except before the dot of a
  method call.
- A parameter may have a default, written as a field's is: `level as single = 1`.
  - The default is a constant expression, folded at compile time. The caller passes it as if it
    were written, so a default costs nothing.
  - Parameters with defaults come last. A call leaves out trailing ones, or names the one it
    sets: `draw(p, color: 4)`.
  - A `mut` parameter has no default.
- `a.f(b)` calls `f(a, b)`. This gives methods without classes, and the compiler resolves it
  like any other call. A function of the module named `push`, `pop`, `insert` or `remove` comes
  before a list's own.
- A list's own: `a.push(x)`, `a.pop()` (the last, taken), `a.insert(i, x)`, `a.remove(i)`, and
  `len(a)`. `x in a` searches it.
- A map's own: `m[k] = v` adds `k` or changes its value, `m[k]` reads it (Subscript out of range
  when `k` is not there), `m.get(k)` gives a `V?` (`V` a number, a string or a bool), `k in m`, `m.remove(k)` (nothing when `k` is
  not there), `len(m)` and `m.keys()`, a new list. A key is an `int`, a `string` or an enum.
- `for k, v in m` and `for k in m` go in the keys' order: as added, except that a remove moves
  the last key into the removed one's place.
- **In the VM**, a map is a record of three lists: its keys, its values and its hash slots, each
  an entry's index. A key is found by its hash and a probe, half the slots at most in use. A
  list's growth on the heap leaves its old array for a sweep, which walks every string block, so
  the sweep waits until the dead arrays are half the heap in use or room runs short.

#### Results

```basic
fn squares(n as int) as int[]          ' a list
    var r as int[]
    for i in 0..n
        r.push(i * i)
    end for
    return r
end fn

' signatures only:
fn corners(b as Box) as Vec[4]        ' a fixed array, by value
fn find(names as string[], s as string) as int?
fn divmod(a as int, b as int) as (int, int)
```

- A function without `as` returns nothing. One with `as` that can reach `end fn` without a
  `return` is a compile error.
- Any type can be a result: a list, map, fixed array, record, tuple or optional.
- A result is built in the caller's place, as llrm does. `return r` moves `r` there, so
  `let s = squares(10)` copies nothing. A place the arguments read, or a module variable the
  function may see, gets the result in a temporary first, then a copy: QBasic's cost.
- A result is never a borrow, so no borrow outlives its call (below).

#### Generic functions

```basic
fn largest[T](a as T[]) as T
    var best = a[0]
    for x in a
        if x > best then best = x
    end for
    return best
end fn
```

- `[T]` after the name makes a function generic. The compiler makes one copy of it for each type
  used, so nothing is dispatched at run time.
- A type is checked when the function is used: `largest` of a type with no `<` is an error in
  its body, named for the copy: `can't compare a Vec with a Vec in largest[Vec]`.
- Each `T` is read from the values given. A generic never used is never checked.
- **In the VM**, each copy is the generic's tree parsed again with `T` bound, compiled onto copies
  of its lines, so an error or the debugger shows the generic's own line.

#### Function values and lambdas

```basic
let hot = temps.filter(fn(t) t > 30).collect()
balls.sort(fn(a, b) a.at.y < b.at.y)
let on_hit as fn(int) = flash        ' a named function as a value

let near = balls.filter(fn(b as Ball)
    let dx = b.at.x - hero.at.x
    let dy = b.at.y - hero.at.y
    return dx * dx + dy * dy < 400
end fn).collect()
```

- `fn(x) expr` is a one-line lambda. `fn(x as T) … end fn` is a longer one. Parameter types
  come from where the lambda is used.
- A lambda passed to a call may use the caller's variables: it borrows them for that call. It is
  inlined, and nothing is allocated.
- A function value kept in a variable or a record captures nothing: it is a code address.
- `fn(int) as int` is a function's type; `fn(string)` returns nothing. `b.on(5)` calls a record's
  function value.
- A lambda is only an argument for a function's parameter: `let f = fn(x) x` is an error. A
  function value is a named function that takes no `mut`, no types and no functions.
- A function taking functions is compiled once for each lambda or named function passed to it,
  as a generic is for each type. A lambda's result may type the generic: `map_all(xs, fn(x)
  f"{x}")` is a `string[]`.
- A lambda can't call another lambda its caller was given: it passes it on, `apply(f, x)`.
- `a.sort(fn(x, y) x < y)` orders a list or an array by a "less than" lambda, and is stable:
  equal elements keep their order. `a.sort()` orders numbers and strings by value; equal values
  can't be told apart, so stability doesn't arise. `a.swap(i, j)` exchanges two elements.
- **In the VM**, a one-line lambda's body is checked again and inlined at each call of its
  parameter. A block lambda is a hidden function: each call of its parameter calls it. A
  variable the lambda uses is a hidden parameter by reference, of the copy and of the block's
  function. A call through a function value is `CALL QX` with the value, a procedure's number,
  on top: the call `PCALL` makes, its target found from the procedure. No op is retired for it;
  it costs a `PCALL` and a load. A line with no statement, a comment's or a block lambda's where
  it is written, costs nothing to pass.
- **In the VM**, `sort` is C. With a lambda it is a natural merge sort over the elements' indexes:
  its ascending runs found, n - 1 compares, then merged; the order found is applied by swaps.
  Each compare runs the lambda's code, compiled after `CALL QX` and jumped over, on the two
  indexes. One buffer of 12 bytes an element is allocated for the sort and freed after it.
  Without one it is a quicksort, compares in C: a median-of-three or ninther pivot and a
  three-way partition, the smaller side recursed.

#### Generators

```basic
fn evens(n as int) yields int
    for i in 0..n step 2
        yield i
    end for
end fn

for x in evens(10)
    print(x)
end for
```

- `yields T` makes a function a generator, and `yield` hands out each value.
- A generator compiles to a state machine in its caller's frame, so nothing is allocated.
- A generator kept in a `var` is resumed a frame at a time with `next()`, which suits cutscenes
  and enemy behaviour. Kept that way, it owns its arguments: they are moved or copied in, never
  borrowed.
- `it.next()` gives a `T?`: `none` once the generator ends. A kept generator is a value: `var b
  = it` copies its state, `for x in it` and a chain run it on, and a fn takes one by its
  generator's name, `fn drain(mut it as upto)`, or returns one, `fn make() as upto`. `next()`
  changes it, so it takes a `var` or a `mut` parameter. A lambda may use it; one whose lambda
  borrows its site's variables calls `next()` in that fn alone. A generic generator's state, or
  one taking a fn's, has no name to write.
- `return x` in a generator yields `x` last, then ends; a bare `return` ends it. A generator has
  no `as` type. A parameter may be `mut` for a `for`, which borrows it for the loop; a kept
  generator owns its arguments, so not there. A generator can't loop over itself.
- **In the VM**, each `for x in g(..)` gets its own copy of `g`, checked and compiled in the
  loop's frame. Its parameters and locals are the caller's locals. A constant argument is
  inlined, a record or list place is read where it is (borrowed for the loop), and the rest are
  stored when the loop starts. A `yield` stores `x` and jumps to the body; `end for` jumps back
  past the `yield`, on a resume index when there are several. When the only `yield` gives a
  variable or a constant, `x` names it and nothing is stored.
- A kept generator is a procedure, one for each generator (and its types). Its state is a
  record: its parameters, its resume index, its locals; each `next()` passes the state's fields by
  reference, and its entry jumps to the `yield` the index names.
- The link threads every jump and line end past what only passes control on: a line with no
  statement, a line that only jumps. A `yield` and its loop's `end for` cost no op.

### Borrowing

The rules make sure no write is ever seen through another name. The compiler checks all of them
at compile time.

1. **A plain parameter is a shared borrow.** The function reads the value in place, and the
   caller may lend the same value to any number of shared borrows at once.
2. **A `mut` argument is an exclusive borrow.** While the call runs, the value can't be reached
   any other way.
   - `f(mut a, a)` and `f(mut a, a.x)` are errors.
   - `f(mut a[i], a[j])` is an error, because `i` and `j` could be equal. `a.swap(i, j)` and
     `a.split(i)` give two parts.
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
   through a fn value may reach any fn used as a value, and a drop counts as a call.
10. **A moved value is gone.** Using it before it is assigned again is an error.

What this buys:

- A `let`, and anything borrowed shared, can stay in a register across calls.
- A `mut` parameter's fields can be held in registers through a loop, because nothing else
  writes them.
- No reference counts and no run-time checks.

### Control flow

```basic
if hp <= 0 then return            ' one line: one statement
if x < 0 then
    x = 0
elseif x > w then
    x = w
end if

for i in 0..n ... end for         ' 0 to n - 1
for i in 0..n step 2 ... end for
for i in (0..n).rev() ... end for  ' n - 1 down to 0
for name in names ... end for
for i, name in names ... end for  ' with its index
for mut b in balls ... end for    ' elements writable
for key, age in ages ... end for
while busy ... end while
do ... end do                     ' until a break

match mode
    case Mode.walk, Mode.roll
        move(mut hero)
    case else
        fly(mut hero)
end match

let sign = 1 if x >= 0 else -1
if key in ("q", "x") then break
```

- `break` and `continue` act on the innermost loop, as in llrm.
- `a..b` is a range: from `a` up to, not including, `b`. The same syntax is a loop's, a slice's
  (`s[1..3]`, `a[..n]`, `a[n..]`) and `in`'s: `if x in 0..w then`.
- A slice is a new value: a string's a new string, a list's a new list of copies. Its ends are
  clamped to the elements, so `s[..100]` of a shorter `s` is all of it. `s[i]` past the end is
  Subscript out of range. `s[i]` is a `ubyte`.
- `match` also takes `case 3 to 5` and `case is > 7`, as SELECT CASE does.
- A `for` over a range counts in `int`s. Its counter can't be assigned, and a constant step of 0
  is a compile error.

### Iteration

`for … in` takes any iterable: a list, a fixed array, a map, a string, a range `a..b` or a
generator. Each has the same lazy methods:

| Kind | Methods |
|---|---|
| Change | `map`, `filter`, `take`, `skip`, `zip`, `rev`; the index comes from `for i, x in` |
| Reduce | `sum`, `min`, `max`, `count`, `any`, `all`, `find`, `fold` |
| Collect | `collect()`: builds a list, the only method here that allocates |

```basic
let total = orders.filter(fn(o) o.paid).map(fn(o) o.price).sum()
for b in balls.filter(fn(b) b.r > 4)
    win.circle(b.at, b.r, 14)
end for
```

- A chain compiles to one loop, with its lambdas inlined. No list is built between the steps.
- A chain is used up in the statement that makes it, by a `for`, a reducing method or
  `collect()`. It borrows its source only for that statement.
- A string's elements are its bytes, `int`s as `s[i]` gives them; a map's are its keys. A range
  is written `(a..b)` before a method, and `zip(0..n)` takes one bare.
- A step or a reducing method takes a lambda, or a name to call: a function, a function value,
  a lambda passed in. A one-line lambda is inlined; a block lambda is called, so it names its
  result's type unless it's a `filter`'s.
- `min()`, `max()` and `find(p)` give a `T?`. `count(p)` counts the elements `p` holds for.
  `fold(first, fn(acc, x) ..)` starts from `first`.
- `rev()` goes anywhere in a chain over a list, array, string or range, once. It can't follow a
  generator, nor a `filter`'s `take`, `skip` or `zip`: one loop counting down can't know how many
  the filter keeps. On a range it counts down.
- `zip(b)` takes a variable's list or string, or a range. Its pair's parts are the next
  lambda's two parameters, `fn(x, y)`, or a `for`'s two names: `for a, b in xs.zip(ys)`. A
  second `zip` adds a third part. `sum`, `min`, `max`, `find` and `collect` take one value: map
  the pair first.
- A generator's call in a chain is kept as a `var`'s would be: each element is a `next()`.
- **In the VM**, a chain is a loop within its statement's line: its start a label, its counter
  stepped and tested by the ops a `for`'s `end for` uses, fused to one `I.NEXT`. With a string
  on the stack under it, its jumps are `QJ`s. A `for` over a chain starts it and runs its steps
  on the `for`'s line; `end for` steps its counter.
- There are no comprehensions: one way to do it.

### Operators

| Kind | Operators |
|---|---|
| Arithmetic | `+ - * /` (`/` gives a `double`, or a `single` when both sides are), `div` (int), `mod`, `**` |
| Compare | `= <> < <= > >=`, chained as in llrm: `a < b <= c`; `in`, `not in` |
| Logic, `bool` only | `and or` (short-circuit), `not`, `xor` |
| Bits, integers only | `& | ^ ~ << >>` |
| Strings | `+` |
| Conditional | `a if c else b`, evaluating one arm |
| Compound | `+= -= *= /=`; the target's subscripts are evaluated once |

- `div`, `mod` and `int(x)` truncate toward zero: `-7 div 2` is -3, `-7 mod 2` is -1.
- `int ** int` is an `int`. A shift count outside 0 to 31 is a runtime error.

### Optional values

```basic
let age = ages.get("ann")             ' int?
if age is none then return
print(age + 1)                        ' an int from here on

let n = ages.get("bo") ?? 0
```

- `T?` holds a `T` or `none`. It is a `T` and a flag, inline, with nothing allocated.
- After `if x is none then return`, or any guard that leaves, `x` is a `T`. That fits the
  guard-first style.
- `a ?? b` gives `b` when `a` is `none`.
- So does the branch a test proves: the `else` of `if x is none`, the `then` of `if x is not
  none`, and the same of `is Error`. Narrowing takes a `let` or a parameter, which can't
  change. A `var` stays a `T?`: read it with `??`, or bind a `let` to it first.
- **In the VM**, a `T?` and a `T!` live in cells as a record does (see Where values live).

### Errors

Errors are values, as in Nib: nothing unwinds, in the VM or in native code.

```basic
fn load(path as string) as string!
    let text = fs.read_text(path)?    ' an error returns from load
    if len(text) = 0 then return Error(EMPTY, f"{path} is empty")
    return text
end fn

let r = load("save.dat")
if r is Error then
    print(r.message)
    return
end if
print(r)                              ' a string from here on
```

- `T!` holds a `T` or an `Error`: a code, a message, a file and a line. Like `T?`, it is inline,
  a tag beside the value.
- `return` gives either one: `return text` or `return Error(code, message)`.
- `x?` gives the value, or returns the error from the function it is in. That function's result
  must be a `!` type too.
- After a guard such as `if r is Error then return`, `r` is a `T`.
- `r.code`, `r.message` and `r.line` read a `T!`'s error; they are 0 and "" when there is none.
  `a ?? b` takes a `T!` too. `r.line` is the line in its module's file; an error has no file yet.
- A runtime error in the language itself, such as an index out of range or a division by zero,
  stops the program at its line, as today. A program handles only the errors a function
  returns as `T!`.

### Modules

- A file is a module and a folder is a package, found as QBasic's `IMPORT` finds one (the main
  file's folder, each folder of `BASICLIB`, then `lib/` beside the interpreter), as a `.bas`
  whose first line says it is Quickr's.
- `import gfx`, `import m as n`, `import a.b` (then `a.b.f()`), `from a.b import X, Y as Z`,
  `from pkg import m` (a package's module) and relative `from .feet import Land`: `.` is the
  importing file's folder, `..` its parent.
- Nothing is exported unless marked `public`: a `fn`, `type`, `enum`, `const`, `var` or `let`. A
  name a module imports is not exported again.
- `a.f(b)` takes the `f` in sight, else the public `f` of the module declaring `a`'s type:
  `clock.tick()` needs only `import time`.
- Only the main file runs top-level statements. A module's top level holds declarations, imports
  and `var`s; anything else is a compile error.
- A module's `var`s and `let`s start once, before the main file's first line: each module after
  those it imports, in the order its imports are written, the first met first in a cycle. Reading
  the var of a module that starts later, at another's top level, is a compile error.
- Across a cycle, declarations refer to each other freely: every fn, type and const is known
  before any body is checked.
- An error, at compile time or run time, names the module's own file and line.
- A Quickr file importing a QBasic module is an error at the import.
- **In the VM**, each module's lines are the program store's. The run goes through the modules
  in the order above, the main's last: each module's last line goes on to the next one's first.

### Tests

```basic
test "a ball bounces off the left wall"
    var b = Ball(at: Vec(1, 50), vel: Vec(-10, 0))
    b.step(0.1, Vec(320, 200))
    assert b.vel.x > 0
end test
```

- `test` blocks sit in any module. `basic --test` runs them; a program's build leaves them out.
- `assert c` stops with the file, the line and the condition's text when `c` is false.
- A test goes at a module's top level and runs as a fn of no parameters: `return` ends it.
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

The rule: anything that costs is visible in the source, and anything that reads as free is free.

| Rule | Consequence |
|---|---|
| Every type is known at compile time | each op is typed (`iadd`, `fadd`), and no op checks a type at run time |
| Every name resolves at compile time | calls, fields and methods are direct addresses; nothing is searched while running |
| No virtual dispatch, no boxing | a `Ball[]` is one block of `Ball`s, laid out as a C array |
| Generic functions compile once per type used | no boxing, no dispatch |
| Lambdas passed to a call inline | a `filter` costs what the loop it replaces costs |
| Iterator chains fuse | `map`, `filter` and `sum` make one loop, with no list built between them |
| Generators are state machines | a `yield` is a jump, and nothing is allocated |
| Enums and optionals are inline | a tag beside the value, never a heap block |
| Read-only parameters go by address | passing a record costs a pointer, never a copy |
| Copies are written out | `copy(a)`; otherwise a heap value moves |
| Locals live on the stack | a call's locals cost one stack-pointer move; only large arrays and growing containers reach the heap |
| Results are built in place | a returned record or list is not copied, as in llrm |
| Borrows can't alias | values stay in registers across calls and loops |
| Allocation happens in few, known places | only a list growing past its room, a map insert, building a string, and `copy` allocate; a frame loop that does none of these allocates nothing. **In the VM:** a push past a list's room doubles it; a pop gives nothing back until the list is empty |
| A string is built in one allocation | an f-string sizes its result first; loops use `text.Builder`. **In the VM:** up to 255 parts at a time |
| Owned values free deterministically | lists, maps and records free with their owner; only strings are collected |
| `for x in a` checks no bounds | the loop owns the range; `a[i]` checks. **In the VM:** `x` is a copy its loop's next loads, unchecked: a number, an enum, a record of them. A string, a record holding strings or lists, and a `for mut`'s `x` are `a(i)`, checked: a copy would allocate each step or outlive a move of the heap, and `for mut` writes through. A resumable generator's loop copies numbers of 4 or 8 bytes |
| Errors are values | `T!` is a tag and a value; no unwinding, in the VM or in native code |
| Heavily used library calls are ops | `math.sqrt` compiles to the VM's sqrt op, not a call |
| Small functions inline | `Vec` arithmetic costs what the field arithmetic costs |
| `const` folds | constants, and expressions of them, fold before codegen |

- `int` is the 386's register width, so its arithmetic is one instruction; `long` takes two. `single` and `double`
  compute at the same speed on the 486's FPU; `single` halves an array's memory.
- Each rule gets a `tools/bench.py` benchmark against QBasic-dialect code doing the same work.

## The standard library

The core has no built-in functions except `print`, the type conversions, `len` and `copy`.
Everything else is in a module.

| Module | Has | Replaces |
|---|---|---|
| `io` | `print`, `read_line`, the text console: `at`, `color`, `clear` | PRINT, INPUT, LOCATE, COLOR, CLS, PRINT USING |
| `text` | `find`, `upper`, `lower`, `trim`, `split`, `join`, `chr`, `code`, `Builder` | INSTR, UCASE$, LTRIM$, CHR$, ASC, STR$, VAL, HEX$ |
| `math` | `sqrt`, `sin`, `cos`, `atan2`, `abs`, `floor`, `clamp`, `lerp`, `pi`, `Vec` | SQR, SIN, ATN, ABS, INT, FIX |
| `random` | `seed`, `int(a, b)`, `real(a, b)`, `pick` | RND, RANDOMIZE |
| `lists`, `maps` | `push`, `pop`, `insert`, `remove`, `sort`, `find`, `keys` | REDIM PRESERVE, SWAP |
| `time` | `now`, `ticks`, `sleep`, `Clock` | TIMER, DATE$, TIME$, SLEEP |
| `fs` | `read_text`, `write_text`, `File` with `read`, `write`, `seek`; `list` | OPEN, GET, PUT, CLOSE, FILES, KILL, NAME |
| `keys` | `down`, `pressed`, `next`, `Key`; the mouse | INKEY$, ON KEY, PEN, STICK |
| `gfx` | windows, canvases, drawing, sprites, fonts (PR #47's design) | SCREEN, PSET, LINE, CIRCLE, PAINT, GET, PUT, PALETTE |
| `synth` | instruments and songs (`docs/native-extensions-design.md`) | PLAY, SOUND, BEEP |
| `os` | `args`, `env`, `exit`, `run` | COMMAND$, ENVIRON$, SHELL, END |

- `print` needs no import: it is the one library function every module sees. The rest of `io` is
  imported.
- `print` writes its values separated by one space, then a newline: `print(1, 2.5, "x", true)`
  gives `1 2.5 x true`. A `single` shows 7 significant digits and a `double` 15, as `%g` does,
  and a whole number keeps its `.0`: `print(2.0, 1e20)` gives `2.0 1e+20`.
- A module is written in Quickr where possible, in `stdlib/`; the build puts it in `lib/` beside
  the interpreter, where a module is looked for last: the Mac app's in `Contents/Resources/lib`
  (the app's files are its Resources'), the web page's in `/lib`.
- Its machine-level parts are `native fn` declarations: `public native fn sqrt(x as double) as
  double`. A native fn has no body; its entry, by its name, is in `src/core/qr/qnat.c`, the VM's
  services and no module's. A call compiles to the entry's code, which is what QBasic's statement
  compiles to, so a library call costs what the built-in does. A native takes and gives numbers,
  bools and strings, by value.
- `math.sqrt` against `SQR`, and `text.find` against `INSTR`, run the same ops as QBasic (benches
  sqrt and find); host instructions are 4% and 5% more.

| Entry | Replaces |
|---|---|
| `io.read_line`, `at`, `color`, `clear` | LINE INPUT, LOCATE r, c, COLOR f, b, CLS |
| `text.find`, `find_from` | INSTR, from 0, -1 none (CALL QX) |
| `text.upper`, `lower`, `trim`, `chr`, `code`, `hex`, `number` | UCASE$, LCASE$, LTRIM$(RTRIM$()), CHR$, ASC, HEX$, VAL |
| `math.sqrt`, `sin`, `cos`, `tan`, `atan`, `exp`, `log`, `floor`, `ceil` | SQR, SIN, COS, TAN, ATN, EXP, LOG, INT, -INT(-x) |
| `random.seed`; `int`, `real`, `pick` over RND | RANDOMIZE, RND |
| `time.ticks`; `now`, `sleep`, `Clock` over it | TIMER; DATE$ and TIME$ |
| `fs.open`, `read`, `write`, `seek`, `size`, `close`, `list` | OPEN, GET, PUT, SEEK, LOF, CLOSE, FILES (CALL QX) |
| `os.env`, `exit`, `run`; `args` over COMMAND$ | ENVIRON$, END, SHELL, COMMAND$ |
| `keys.next`, `pressed`, `down` over INKEY$; `mouse`, `clicked` | INKEY$, PEN |

- `math`'s `abs`, `min`, `max` and `clamp` are generic Quickr fns. `Vec` has `+`, `-` and `=`,
  `*` and `/` by a `double`, `dot` and `length`.
- `os.run(cmd)` runs `cmd` as SHELL does: `/bin/sh -c` on the host, `COMSPEC /C` on DOS. It
  returns nothing, as SHELL gives no exit code; a command that can't start is Illegal function
  call. **In the VM**, the command is cut at 122 bytes, as SHELL's is to fit DOS's command tail.
- A `File` closes itself when it ends; `close(mut f)` closes it sooner. A file's error is a
  value, its `code` QBasic's ERR. `read_text` reads at most 32767 bytes, a string's most.
  `fs.list` gives the platform's DOS names, upper case on the host.
- `os.args()` is COMMAND$'s words, so upper case; `os.exit()` has no exit code, as END has none.
- `io.read_line` echoes the line's end, as LINE INPUT does. `io.color(fg)` sets the background 0.
- The platforms report key presses and repeats, never releases: `keys.down(k)` is true for
  `keys.held_for` (0.55 s) after k's last press or repeat. `time.sleep` waits on `ticks()`.
- `gfx` waits on the graphics extension being designed; `synth` on `lib/synth.qbx`, which is not
  on this branch. Neither is in M4.

## Building it

- **A new front end.** Quickr gets its own lexer, parser and semantic pass. They produce the
  checked tree that codegen already takes. The VM, runtime, collector and debugger are shared.
- **llrm can compile it too.** Nothing in the language needs the VM. If llrm takes Quickr up,
  each front end runs the other's test programs.
- **The IDE takes both dialects.** Each has its own highlighting, completion and help (`help/qr/`).
  Lines are still checked one at a time; the whole program before a run. The debugger steps and
  stops by line, names spelled as typed (docs/ide-design.md).

Each step is usable before the next starts:

1. Lexer, parser and sema for the core, with `io` and `math`. Tests are Quickr programs and their
   output.
2. `text`, `lists`, `maps`, `random`, `time`, `fs` and `os`.
3. `keys`, `gfx` and `synth`, on the extensions already built.
4. The IDE: New asks for the dialect, then highlighting, completion and help.
5. A Quickr program imports a QBasic module, if that is decided.
