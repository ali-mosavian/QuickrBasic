# IMPORT: Modules, Packages and COMMON
keys: import examples, module examples, package examples

Short recipes for programs of several files: modules, packages,
private names, setup code, COMMON and errors.

See also [IMPORT and FROM](IMPORT), [Procedures and Modules](procedures)

## Details
Save each file under the name its first line gives, all in one
folder, and run main.bas. Later recipes use the files of earlier ones.
Each main.bas ends with what it prints.

A module
Its SUBs, FUNCTIONs, CONSTs and TYPEs are used through its name.
    ' shapes.bas
    CONST sides = 4
    TYPE pt
      x AS SINGLE
      y AS SINGLE
    END TYPE
    FUNCTION area (w, h)
      area = w * h
    END FUNCTION

    ' main.bas
    IMPORT shapes
    DIM p AS shapes.pt
    p.x = 3
    PRINT shapes.area(p.x, 2); shapes.sides
    ' prints:
    '  6  4

Names of their own
FROM binds a module's names in the file; AS renames one.
    ' main.bas
    FROM shapes IMPORT area, pt AS spot
    IMPORT shapes AS s
    DIM p AS spot
    p.y = 5
    PRINT area(2, p.y); s.sides
    ' prints:
    '  10  4

Every name
* binds each SUB, FUNCTION, CONST and TYPE; the file's own win.
    ' main.bas
    CONST sides = 3
    FROM shapes IMPORT *
    PRINT area(sides, 2)
    ' prints:
    '  6

A package
A package is a folder. FROM's dots start at the importing file's.
    ' lib/pens.bas
    FUNCTION ink$
      ink$ = "red"
    END FUNCTION

    ' lib/draw.bas
    FROM . IMPORT pens
    SUB box (n)
      PRINT pens.ink$; n
    END SUB

    ' main.bas
    IMPORT lib.draw
    FROM lib IMPORT pens
    lib.draw.box 2
    PRINT pens.ink$
    ' prints:
    ' red 2
    ' red

Private and public
PRIVATE keeps a name in its file; DIM PUBLIC exports a variable.
    ' tally.bas
    DIM PUBLIC total
    SUB add (n)
      total = total + clamp(n)
    END SUB
    PRIVATE FUNCTION clamp (n)
      clamp = n
      IF n > 10 THEN clamp = 10
    END FUNCTION

    ' main.bas
    FROM tally IMPORT add, total
    add 4
    add 50
    PRINT total
    ' prints:
    '  14

Setup code
A module's own lines run once, at the first line importing it. READ
reads the module's DATA.
    ' days.bas
    DIM PUBLIC day$(1 TO 7)
    FOR i = 1 TO 7
      READ day$(i)
    NEXT
    PRINT "days read"
    DATA Mon, Tue, Wed, Thu, Fri, Sat, Sun

    ' week.bas
    FROM days IMPORT day$()
    FUNCTION first$
      first$ = day$(1)
    END FUNCTION

    ' main.bas
    PRINT "start"
    IMPORT days
    IMPORT week
    PRINT days.day$(7); " "; week.first$
    ' prints:
    ' start
    ' days read
    ' Sun Mon

COMMON
COMMON joins the files' variables by position, whatever their names.
    ' board.bas
    COMMON SHARED points, player$
    SUB show
      PRINT player$; points
    END SUB

    ' main.bas
    COMMON SHARED score, who$
    FROM board IMPORT show
    who$ = "Sam"
    score = 90
    show
    ' prints:
    ' Sam 90

## Example
An error in a file with no handler goes to the caller's file's, as
if it happened at the call.
    ' calc.bas
    FUNCTION half (x)
      IF x < 0 THEN ERROR 5
      half = x / 2
    END FUNCTION

    ' main.bas
    FROM calc IMPORT half
    ON ERROR GOTO oops
    PRINT half(8)
    PRINT half(-1)
    PRINT "done"
    END
    oops:
    PRINT "error"; ERR
    RESUME NEXT
    ' prints:
    '  4
    ' error 5
    ' done
