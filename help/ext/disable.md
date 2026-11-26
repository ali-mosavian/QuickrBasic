# OPTION DISABLE Statement
keys: OPTION DISABLE, disable, keywords as names

Syntax
    OPTION DISABLE keyword [, keyword]...

Makes QB's statements and functions for devices names in the module's
lines after it, the standard library's to give: `line`, `open`,
`point`. It is the module's first statement: after the file's doc
comment, before any other statement.

See also [IMPORT](IMPORT), [what QuickrBasic adds](QuickrBasic)

## Details
The keywords it takes: SCREEN, PRINT, INPUT, LINE, CIRCLE, PSET,
PRESET, PAINT, DRAW, PALETTE, VIEW, WINDOW, WIDTH, CLS, COLOR, LOCATE,
POINT, PCOPY, PMAP, OPEN, CLOSE, GET, PUT, READ, WRITE, DATA, RESTORE,
FIELD, SEEK, LOCK, EOF, LOF, LOC, FREEFILE, KILL, NAME, FILES, MKDIR,
CHDIR, RMDIR, BEEP, SOUND, PLAY, KEY, PEN, STICK, STRIG, INKEY$, INP,
OUT, WAIT, PEEK, POKE, TIMER, DATE$, TIME$, SLEEP, SHELL, CHAIN, RUN,
CLEAR, SYSTEM and the rest of their kind. With PRINT, ? is not PRINT.

The language's own words cannot be disabled: IF, FOR, SUB, DIM, the
types, the operators, and the functions of numbers and strings. Any
other word in the list is a Syntax error.

Each module disables its own words: a program that imports `gfx.draw`
keeps QB's LINE. Lines typed at the prompt or in the Immediate window
disable nothing.

## Example
    OPTION DISABLE POINT
    FUNCTION point (x AS INTEGER) AS INTEGER
      point = x * 2
    END FUNCTION
    PRINT point(21)                     ' 42
