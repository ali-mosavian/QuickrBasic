# Arrays and records as values

Dynamic arrays and records follow the rules strings already follow: they grow at run time, are
copied on assignment, can be returned from a FUNCTION and are freed with their owner. There are no
pointers.

```basic
type Vec
    x as single
    y as single
end type

type Mesh
    title as string            ' a variable-length string
    corner(1 to 4) as Vec      ' a fixed array, inline
    pts() as Vec               ' a dynamic array, empty until redim
    kids() as Mesh             ' a type may hold an array of itself
end type

function Add (a as Vec, b as Vec) as Vec
    Add.x = a.x + b.x
    Add.y = a.y + b.y
end function

function Squares (n as integer) as long()
    redim Squares(1 to n)
    for i = 1 to n: Squares(i) = i * i: next
end function

dim m as Mesh, s() as long
redim m.pts(1 to 100)
m.pts(1) = Add(m.corner(1), m.corner(2))
s() = Squares(10)
```

## Rules

- `b = a` copies a record and everything in it. `b(:) = a(:)` gives `b` the bounds and a copy of
  the elements of `a`; `b() = a()` moves the array, leaving `a` empty. A record's array field
  written alone copies: `c.pts = m.pts`.
- `swap a, b` of two records of one type swaps everything in them.
- A fixed array's bounds in a type are integer constants: numbers, CONSTs and `+ - * \ mod`.
  `lbound`, `ubound`, `erase` and the graphics `get` and `put` take it whole: `ubound(m.corner)`.
- A whole array is written `a()`: the scalar `a` and the array `a()` are separate variables.
- Procedures receive references, as before.
- A FUNCTION's name is its result: `Add.x = ...`, `redim Squares(...)`, `Squares(i) = ...`. With
  arguments, or outside the FUNCTION, the name is a call.
- A value lives as long as its owner: a local until its procedure returns, a field as long as its
  record, an element as long as its array.
- A type whose fields are all fixed keeps its byte layout, so `put`, `get` and `len` are unchanged.
  `put` and `get` refuse a type with a dynamic part.

## Ownership

Every array has one owner, and the owner's end frees it. Nothing is shared, so nothing is counted.

- A module's array lives for the run; RUN, CLEAR and END reset the heap.
- A procedure's local array is freed when the procedure returns (`PF_ARRAYS`, `end_call`).
- A parameter is the caller's; the callee never frees it.
- A call's result goes to a hidden variable of the caller's, one per call site: a local inside a
  procedure, freed on return; at module level, replaced by the next call there.
- Assignment frees the target's old array. `b(:) = a(:)` copies; `b() = a()` and `s() = f(10)`
  move, leaving the source empty. A local's frame or arena array moved to a cell its procedure may
  outlive is copied to the heap first.
- `erase` and `redim` free as before.
- Freeing a record frees its dynamic fields, by its TYPE's slots. An array header names its cell,
  so the compacting heap can update a cell inside a record that moved.

## Steps

1. `function ... as type` for the scalar types and records; `declare function ... as type`.
2. `dim a()`, `b() = a()`, `b(:) = a(:)` and array results.
3. Fixed arrays in a type.
4. Dynamic arrays and `string` in a type.

## How a record result is returned

The caller passes the place for the result as a hidden last argument, a reference like any
other. Inside the FUNCTION its name is that reference, so `Add.x = ...` writes the caller's
place. Each call site has its own hidden variable in its scope, so recursion works as it does for
any local.
