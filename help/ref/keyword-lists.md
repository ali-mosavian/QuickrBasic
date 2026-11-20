# Keyword Lists
keys: keyword lists, keywords, statements and functions, commands by task, reserved words

Every keyword by what it does. Each is a link to its topic.

See also [Contents](contents), [Operators](operators),
[Data types](data types), [Syntax notation](syntax notation)

## Details
Decisions and loops
[IF](IF)  [ELSE](ELSE)  [ELSEIF](ELSEIF)  [SELECT CASE](SELECT CASE)  [FOR](FOR)  [NEXT](NEXT)  [DO](DO)  [LOOP](LOOP)
[WHILE](WHILE)  [WEND](WEND)  [GOTO](GOTO)  [GOSUB](GOSUB)  [RETURN](RETURN)  [ON...GOTO](ON GOTO)  [EXIT](EXIT)  [END](END)
[STOP](STOP)  [SYSTEM](SYSTEM)  [SLEEP](SLEEP)  [CONTINUE](CONTINUE)

Procedures
[SUB](SUB)  [FUNCTION](FUNCTION)  [DECLARE](DECLARE)  [CALL](CALL)  [SHARED](SHARED)  [STATIC](STATIC)  [COMMON](COMMON)
[DEF FN](DEF FN)  [BYVAL](BYVAL)  [CHAIN](CHAIN)  [RUN](RUN)  [IMPORT](IMPORT)  [FROM](IMPORT)
[PRIVATE](IMPORT)  [PUBLIC](IMPORT)

Variables and types
[DIM](DIM)  [REDIM](REDIM)  [ERASE](ERASE)  [CONST](CONST)  [LET](LET)  [SWAP](SWAP)  [TYPE](TYPE)  [CLEAR](CLEAR)
[OPTION BASE](OPTION BASE)  [LBOUND](LBOUND)  [UBOUND](UBOUND)  [DEFINT](DEFINT)  [DEFBYTE](DEFBYTE)  [DEFSHORT](DEFSHORT)
[SHORT](SHORT)  [BYTE](BYTE)  [CBYTE](CBYTE)

Text output
[PRINT](PRINT)  [LPRINT](LPRINT)  [PRINT USING](PRINT USING)  [WRITE](WRITE)  [LOCATE](LOCATE)  [CLS](CLS)  [COLOR](COLOR)
[WIDTH](WIDTH)  [VIEW PRINT](VIEW PRINT)  [SPC](SPC)  [TAB](TAB)  [POS](POS)  [CSRLIN](CSRLIN)  [SCREEN()](SCREEN function)

Keyboard and input
[INPUT](INPUT)  [LINE INPUT](LINE INPUT)  [INPUT$](INPUT$)  [INKEY$](INKEY$)  [KEY](KEY)  [DATA](DATA)  [READ](READ)
[RESTORE](RESTORE)  [COMMAND$](COMMAND$)

Graphics
[SCREEN](SCREEN)  [PSET](PSET)  [PRESET](PRESET)  [LINE](LINE)  [CIRCLE](CIRCLE)  [PAINT](PAINT)  [DRAW](DRAW)  [GET](GET)
[PUT](PUT)  [POINT](POINT)  [PALETTE](PALETTE)  [PCOPY](PCOPY)  [VIEW](VIEW)  [WINDOW](WINDOW)  [PMAP](PMAP)

Sound
[BEEP](BEEP)  [SOUND](SOUND)  [PLAY](PLAY)  [PLAY()](PLAY function)

Mouse and joystick
[PEN](PEN)  [STICK](STICK)  [STRIG](STRIG)

Files and disks
[OPEN](OPEN)  [CLOSE](CLOSE)  [PRINT #](PRINT)  [INPUT #](INPUT)  [GET](GET)  [PUT](PUT)  [EOF](EOF)  [LOF](LOF)  [LOC](LOC)
[SEEK](SEEK)  [FREEFILE](FREEFILE)  [LOCK](LOCK)  [UNLOCK](UNLOCK)  [RESET](RESET)  [FIELD](FIELD)  [LSET](LSET)  [RSET](RSET)
[FILES](FILES)  [KILL](KILL)  [NAME](NAME)  [MKDIR](MKDIR)  [CHDIR](CHDIR)  [RMDIR](RMDIR)  [FILEATTR](FILEATTR)

Converting data
[MKI$](MKI$)  [CVI](CVI)  [CINT](CINT)  [CLNG](CLNG)  [CSNG](CSNG)  [CDBL](CDBL)  [CBYTE](CBYTE)  [ASC](ASC)  [CHR$](CHR$)
[VAL](VAL)  [STR$](STR$)  [HEX$](HEX$)  [OCT$](OCT$)

Math
[ABS](ABS)  [SGN](SGN)  [INT](INT)  [FIX](FIX)  [SQR](SQR)  [EXP](EXP)  [LOG](LOG)  [SIN](SIN)  [COS](COS)  [TAN](TAN)
[ATN](ATN)  [RND](RND)  [RANDOMIZE](RANDOMIZE)  [MOD](MOD)

Strings
[LEFT$](LEFT$)  [RIGHT$](RIGHT$)  [MID$](MID$)  [LEN](LEN)  [INSTR](INSTR)  [LCASE$](LCASE$)  [UCASE$](UCASE$)  [LTRIM$](LTRIM$)
[RTRIM$](RTRIM$)  [SPACE$](SPACE$)  [STRING$](STRING$)

Logic
[AND](AND)  [OR](OR)  [NOT](NOT)  [XOR](XOR)  [EQV](EQV)  [IMP](IMP)

Errors and events
[ON ERROR](ON ERROR)  [RESUME](RESUME)  [ERROR](ERROR)  [ERR](ERR)  [ERL](ERL)  [ERDEV](ERDEV)  [ON TIMER](TIMER event)  [TIMER](TIMER)
[ON KEY](ON event)  [ON PLAY](ON event)  [ON PEN](ON event)  [ON STRIG](ON event)

Debugging
[TRON](TRON)  [TROFF](TROFF)  [STOP](STOP)

System and memory
[SHELL](SHELL)  [ENVIRON](ENVIRON)  [ENVIRON$](ENVIRON$)  [DATE$](DATE$)  [TIME$](TIME$)  [FRE](FRE)  [DEF SEG](DEF SEG)  [PEEK](PEEK)
[POKE](POKE)  [VARPTR](VARPTR)  [VARSEG](VARSEG)  [VARPTR$](VARPTR$)  [SADD](SADD)  [INP](INP)  [OUT](OUT)  [WAIT](WAIT)
[BLOAD](BLOAD)  [BSAVE](BSAVE)  [IOCTL](IOCTL)

Left out: serial ports (COM), ISAM and the other PDS and Visual Basic
for DOS keywords QuickrBasic lacks.

## Example
    PRINT LEFT$("QuickrBasic", 5); " ";  UCASE$("basic")
    ' Quick BASIC
