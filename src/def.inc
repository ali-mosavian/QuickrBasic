; ==========================================================================
; def.inc - constants, the memory map
; ==========================================================================
; holds:    every constant but the O_* ops (vm/isa.inc numbers them)
; data:     Every value's type is known at compile time (cty). On the data
;           stack a value is a 4-byte cell: an INT in its low word, a FLOAT
;           packed, a string its Desc's linear address. SI the code or token
;           cursor. A FLOAT packed: an IEEE single, s:1 e:8 f:23, e 0 zero. A
;           number variable is a FLOAT, a % an INT.
; ==========================================================================

FBIAS   = 127
T_INT   = 0                     ; a static type
T_STR   = 40h
T_FLT   = 80h
SI_INF  = 20h                   ; sinfo: a plain variable inferred INT
SI_FOR  = 10h                   ; sinfo: a FOR's variable, during cm_infer
EMAX    = 254
NDO     = 7                     ; digits flt_out shows
NTMP    = 48                    ; string temporaries: the data stack is full first
VS      = 4                     ; a variable's slot, a number's bytes
NB      = 4                     ; a stored FLOAT's bytes
MAXD    = 16

M_VAR   = 0CCh
M_INT   = 0CDh
HDR     = 5                     ; len:1, num:2, cp:2
L_CP    = 3                     ; a line's code in the cache
IFNDEF IDE
IDE     = 0
ENDIF
XSLK    = 48                    ; cm_xlate: the most one op writes past its size
XLIM    = 252                   ; compile room: ERR code, EOL still fit
NL      = 13
NFILE   = 10                    ; open files at most

; a FOR frame on the control stack: mark, ln:2, ip:2, id:1, step:4, limit:4,
; cache:4. With whole bounds: step:2, count:2, limit:2, FFFFh (a NaN, no float).
F_MARK  = 0
F_LN    = 1
F_IP    = 3                     ; where the loop or RETURN goes on: absolute, or
                                ; an offset in its line's code (cm_flush)
F_ID    = 5
F_STEP  = 6
F_LIMIT = 10
F_CACHE = 14                    ; an INT counter's frame: the float last stored
FRSIZE  = 18
M_FR    = FRSIZE                ; a frame's mark is its size
M_GS    = 5
M_FN    = 9                     ; an FN frame: mark, ln:2, ip:2, the old value:4
F_OLD   = 5

D_LEN   = 0                     ; a string Desc: len:2, ptr:4 linear
D_PTR   = 2
DSZ     = 6
SMAX    = 32767                 ; a string's chars at most: LEN is an INT
B_LEN   = 0                     ; a string block's tail, after the chars
B_OWN   = 2                     ; len:2, owner:4 linear
BHDR    = 6
AH_PTR  = 1                     ; an array's header: n, data:4, fast:2, the counts
AH_FAST = 5                     ; 1-D: the count, else 0
AH_CNT  = 7

TK_END  = 80h
TK_FOR  = 81h
TK_NEXT = 82h
TK_DATA = 83h
TK_INPUTN = 84h
TK_INPUT = 85h
TK_DIM  = 86h
TK_READ = 87h
TK_GOTO = 89h
TK_RUN  = 8Ah
TK_IF   = 8Bh
TK_RES  = 8Ch
TK_GOSUB = 8Dh
TK_RET  = 8Eh
TK_REM  = 8Fh
TK_STOP = 90h
TK_ON   = 91h
TK_WAIT = 92h
TK_LOAD = 93h
TK_DEF  = 96h
TK_POKE = 97h
TK_PRINTN = 98h
TK_PRINT = 99h
TK_CONT = 9Ah
TK_LIST = 9Bh
TK_CLR  = 9Ch
TK_CMD  = 9Dh
TK_SYS  = 9Eh
TK_OPEN = 9Fh
TK_CLOSE = 0A0h
TK_GET  = 0A1h
TK_NEW  = 0A2h
TK_TAB  = 0A3h
TK_TO   = 0A4h
TK_FN   = 0A5h
TK_SPC  = 0A6h
TK_THEN = 0A7h
TK_NOT  = 0A8h
TK_STEP = 0A9h
TK_PLUS = 0AAh
TK_MINUS = 0ABh
TK_GT   = 0B1h
TK_EQ   = 0B2h
TK_LT   = 0B3h
TK_SGN  = 0B4h
TK_FRE  = 0B8h
TK_LEN  = 0C3h
TK_MID  = 0CAh
TK_GO   = 0CBh
TK_PI   = 0FFh

E_BRK   = 0
MSG_READY = 1
E_SN    = 2
E_RG    = 3
E_OD    = 4
E_IQ    = 5
E_OM    = 6
E_US    = 7
E_BS    = 8
E_RD    = 9
E_DZ    = 10
E_TM    = 11
E_CN    = 12
MSG_OK  = 13
E_OV    = 14
E_NF    = 15
E_ST    = 16
E_LS    = 17
E_ID    = 18
MSG_ERR = 19
MSG_IN  = 20
MSG_REDO = 21
MSG_EXTRA = 22
E_TMF   = 23
E_VF    = 32
E_FD    = 33
E_UF    = 34

; Memory map, flat: the code, then the data; the regions in .data? (main.asm).
; dsb is the 16-bit build's DS at the same offsets: the variables, the
; strings' Descs, the arrays' headers, the high buffers, the control stack.
; The program and the POKE memory are 64K each, addressed by offsets from
; progb and pokeb. The stream region: offsets from cachb, stored in 16 bits;
; the direct line's code at DCODE, the cache from CACHE0.
DLINE    = 0                    ; in progb: the direct line, len 0, num FFxxh
PROGBASE = 0110h                ; then the program, to PROGLIM
PROGLIM  = 0FFF0h
VARBASE  equ     <(offset g_dsb+0B000h)>
VARLIM   equ     <(offset g_dsb+0B400h)>    ; 256 ids * 4
SVBASE   equ     <(offset g_dsb+0B400h)>    ; a string variable's Desc, 8 bytes per id
SVLIM    equ     <(offset g_dsb+0BC00h)>
PROGSZ   = PROGLIM-PROGBASE
HDRBASE  equ     <(offset g_dsb+0BC00h)>    ; the arrays' headers
HDRLIM   equ     <(offset g_dsb+0E000h)>
ifndef AC
AC       = IDE                  ; Tab completion in the editor
endif
DCODE    = 0100h                ; in cachb: the direct line's stream
CACHE0   = DCODE+1536           ; the cache, to CLIM
CLIM     = 0FFF0h
HEAPSZ   = 400000h              ; the heap: arrays and strings
CSTOP    equ     <(offset g_dsb+0FC00h)>    ; the control stack, down from here
STKSZ    = 10000h               ; the data stack, its own region
STKTOP   equ     <(offset g_stkb+STKSZ-16)>
STKROOM  = 191                  ; bytes either stack may take, as stock's
CSLIM    equ     <(offset g_dsb+0FC00h-STKROOM)>
OBSZ     = 256                  ; plat.asm's output buffer
