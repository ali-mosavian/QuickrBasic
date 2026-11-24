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
' bounce.qr
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

- The file's extension picks the dialect: `.bas` is QBasic and `.qr` is Quickr. Nothing guesses
  from a file's contents.
- File > New asks which. The status bar shows the file's dialect. `basic` runs either.
- **Open:** whether a `.qr` file may import a `.bas` module. Proposed: yes, one way only. Its
  procedures and TYPEs appear with Quickr types, and a `.bas` never imports a `.qr`.

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
  format-spec rules, as in llrm.

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
| `string` | bytes, immutable; `s[i]` is a byte, `s[1..3]` a slice |
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
- Signed and unsigned never mix by themselves, so C's `-1 < 0u` trap can't happen. Every other
  conversion is written out: `int(x)`, `ushort(n)`, `single(d)`, `string(n)`.
- A literal takes the type its place needs: `r as single = 6` stores a `single`. A literal that
  doesn't fit is a compile error. Alone, `2.5` is a `double` and `7` is an `int`.
- Every size is the same on every target, so a record laid out for a file reads the same
  everywhere.
- **Open:** integer overflow. Proposed: an error, which costs one `jo` an op. `wrap(a + b)`
  asks for wrapping where it is wanted, such as in hashes and random numbers.

### Ownership

- Records, lists and maps are values with one owner, and are freed with it. The collector never
  sees them.
- `b = a` copies a record of plain fields with one block copy.
- A list, a map, or a record holding one moves when `a` is not used again. Anywhere else,
  `b = a` is a compile error and you write `copy(a)`. A copy is never hidden.
- A type with an `fn drop(mut x as T)` has it run when an owner of a `T` ends, at a known point:
  a `File` closes itself. There are no finalizers and no collector pauses.
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
  value, or have `case else`.
- An enum is stored inline: a tag and room for its largest value. Nothing is allocated.
- A record may define `+ - * / = <` with `fn operator`. Such a function is inlined like any
  small one. `<>`, `>`, `<=` and `>=` follow from `=` and `<`.

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
  like any other call.

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

- A function without `as` returns nothing.
- Any type can be a result: a list, map, fixed array, record, tuple or optional.
- A result is built in the caller's place, as llrm does. `return r` moves `r` there, so
  `let s = squares(10)` copies nothing.
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
- A type is checked when the function is used: `largest` of a type with no `<` is an error at
  that call.

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
5. **`for mut x in a` borrows `a` exclusively for the loop.** The body changes elements through
   `x`, but can't push to `a` or remove from it.
6. **`for x in a` borrows `a` shared for the loop.** The body can't change `a`.
7. **A lambda borrows what it uses for the call it is passed to**, exclusively if it assigns
   to it.
8. **A generator borrows its arguments** until its loop ends.
9. **Module `var`s count too.** A call can't borrow a module variable that the callee, or
   anything it calls, writes. The compiler knows which ones each function writes.
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
- `match` also takes `case 3 to 5` and `case is > 7`, as SELECT CASE does.

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
- A runtime error in the language itself, such as an index out of range or a division by zero,
  stops the program at its line, as today. A program handles only the errors a function
  returns as `T!`.

### Modules

- A file is a module and a folder is a package, as `IMPORT` works today: `import gfx`,
  `from barkley.model.hero import Hero`, and relative `from .feet import Land`.
- **Open:** what is exported by default. Proposed: nothing; exports are marked `public`, the
  opposite of llrm's `PRIVATE`.
- Only the main file runs top-level statements. A module holds declarations, `var`s included,
  and runs nothing when imported.

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
| Allocation happens in few, known places | only a list growing past its room, a map insert, building a string, and `copy` allocate; a frame loop that does none of these allocates nothing |
| A string is built in one allocation | an f-string sizes its result first; loops use `text.Builder` |
| Owned values free deterministically | lists, maps and records free with their owner; only strings are collected |
| `for x in a` checks no bounds | the loop owns the range; `a[i]` checks |
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
  gives `1 2.5 x true`.
- A module is written in Quickr where possible. Its machine-level parts are native entries in the
  table built into the interpreter (the extensions' inittab), so DOS loads nothing extra.
- A library call is a direct call, or an op where the entry is marked as one. Moving a statement
  out of the language costs nothing at run time.

## Building it

- **A new front end.** Quickr gets its own lexer, parser and semantic pass. They produce the
  checked tree that codegen already takes. The VM, runtime, collector and debugger are shared.
- **llrm can compile it too.** Nothing in the language needs the VM. If llrm takes Quickr up,
  each front end runs the other's test programs.
- **The IDE takes both dialects.** Each has its own highlighting, completion and help (`help/qr/`).
  Lines are still checked one at a time.

Each step is usable before the next starts:

1. Lexer, parser and sema for the core, with `io` and `math`. Tests are `.qr` programs and their
   output.
2. `text`, `lists`, `maps`, `random`, `time`, `fs` and `os`.
3. `keys`, `gfx` and `synth`, on the extensions already built.
4. The IDE: New asks for the dialect, then highlighting, completion and help.
5. A `.qr` program imports a `.bas` module, if that is decided.
