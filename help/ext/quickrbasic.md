# What QuickrBasic Adds to QBasic
keys: QuickrBasic, additions, extensions, differences

QuickrBasic runs QBasic 1.1 programs and adds new types, 32-voice
PLAY, PEN, video pages, no 64 KB walls, and a better editor.

See also [contents](contents)

## Details
Language
    BYTE, DEFBYTE, CBYTE   unsigned 0-255, 1 byte in records
                           [BYTE](BYTE), [DEFBYTE](DEFBYTE), [CBYTE](CBYTE)
    SHORT, DEFSHORT        QBasic's 16-bit integer
                           [SHORT](SHORT), [DEFSHORT](DEFSHORT)
    CONTINUE, EXIT WHILE   skip a loop's round, leave a WHILE or outer FOR
                           [CONTINUE](CONTINUE), [EXIT](EXIT)
    IMPORT gfx             modules and packages, from files and folders
                           [IMPORT](IMPORT)
    OPTION DISABLE LINE    QB's words for devices as names
                           [OPTION DISABLE](OPTION DISABLE)
    FUNCTION f () AS Vec   a result's type by AS; records, arrays
                           [FUNCTION](FUNCTION)
    b() = a(), b(:) = a(:) an array moved; copied
                           [array assignment](array assignment)
    pts() AS Vec in TYPE   records with arrays and strings that grow
                           [records as values](records as values)
    max_count              `_` in names, after the first letter
                           [names](data types)
    32-bit INTEGER         one type with LONG; 32767 + 1 is fine
                           [no limits](no limits)
    PLAY a$, b$, ...       32 synthesizer voices: waves, FM, reverb
                           [play voices](play voices)
    PEN(n)                 the mouse: left button n, right n + 10
                           [PEN](PEN)
    No 64 KB walls         programs, arrays, strings grow
                           [no limits](no limits)

From QuickBASIC PDS and VB-DOS, also accepted: REDIM PRESERVE,
OPTION EXPLICIT (an undeclared variable is "Variable not declared"),
ON LOCAL ERROR and fixed arrays in a [TYPE](TYPE).

Editor
    Syntax highlighting    by the compiler's own lexer
    Completion             only what fits at the cursor
                           [completion](completion)
    Debug pane             Watch, Locals, Call stack; Step Out
                           [debug pane](debug pane)
    Themes                 nine, and 25, 43 or 50 lines
                           [themes](themes)
    Split windows          horizontal or vertical; drag lines between
                           [IDE additions](ide additions)

Command line and testing
    basic --check FILE     list every compile error, run nothing
                           [command line](command line)
    QB_KEYS, QB_MOUSE ...  scripted input in the window build
                           [test scripts](test scripts)

## Example
    DEFBYTE B
    DIM v AS SHORT
    b = 255
    v = 32767
    PRINT b, v, 32767 + 1
    ' 255          32767         32768
