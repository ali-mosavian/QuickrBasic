# What QuickrBasic Adds to QBasic
keys: QuickrBasic, additions, extensions, differences

QuickrBasic runs QBasic 1.1 programs and adds the following.

## Language
    BYTE, DEFBYTE, CBYTE   unsigned 0-255, 1 byte in records
                           [BYTE](BYTE), [DEFBYTE](DEFBYTE), [CBYTE](CBYTE)
    SHORT, DEFSHORT        QBasic's 16-bit integer
                           [SHORT](SHORT), [DEFSHORT](DEFSHORT)
    32-bit INTEGER         one type with LONG; 32767 + 1 is fine
                           [no limits](no limits)
    PLAY a$, b$, ...       six voices; V, W, Q, H and Y commands
                           [play voices](play voices)
    PEN(n)                 the mouse: left button n, right n + 10
                           [PEN](PEN)
    Video pages            pages and PCOPY in every SCREEN mode
                           [video pages](video pages)
    No 64 KB walls         programs, arrays, strings grow
                           [no limits](no limits)

From QuickBASIC PDS and VB-DOS, also accepted: REDIM PRESERVE,
OPTION EXPLICIT (an undeclared variable is "Variable not declared")
and ON LOCAL ERROR.

## Editor
    Syntax highlighting    by the compiler's own lexer
    Completion             only what fits at the cursor
                           [completion](completion)
    Debug pane             Watch, Locals, Call stack; Step Out
                           [debug pane](debug pane)
    Themes                 nine, and 25, 43 or 50 lines
                           [themes](themes)
    Split windows          horizontal or vertical; drag lines between
                           [IDE additions](ide additions)

## Command line and testing
    basic --check FILE     list every compile error, run nothing
                           [command line](command line)
    QB_KEYS, QB_MOUSE ...  scripted input in the window build
                           [test scripts](test scripts)

See also [contents](contents)
