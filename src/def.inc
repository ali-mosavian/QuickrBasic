; ==========================================================================
; def.inc - constants, the memory map
; ==========================================================================
; holds:    every constant but the O_* ops (vm/isa.inc numbers them)
; data:     Every value's type is known at compile time (cty). On the data
;           stack a value is an 8-byte cell: an INT, a FLOAT packed or a
;           string's Desc address in its low dword; a DOUBLE, an IEEE double. SI the code or token
;           cursor. A FLOAT packed: an IEEE single, s:1 e:8 f:23, e 0 zero. A
;           number variable is a FLOAT, a % an INT.
; ==========================================================================

FBIAS   = 127
T_INT   = 0                     ; a static type
T_STR   = 40h
T_FLT   = 80h
T_DBL   = 20h                   ; a DOUBLE: a whole cell
SK_DBL  = 4                     ; sinfo: a DOUBLE's
NM_LBL  = 1                     ; nflag: a label, in some scope
NM_FN   = 2                     ; nflag: DEF FN's, the name starts FN
NM_SUB  = 4                     ; nflag: a SUB's
NM_FUNC = 8                     ; nflag: a FUNCTION's
NPROC   = 256                   ; procedures, 1 .. NPROC-1
NPPAR   = 2048                  ; their parameters, all
B_SC    = 0                     ; a binding (mem/scope.asm): its scope, name,
B_NAM   = 2                     ; next in its chain, BF_* flags
B_NXT   = 4
B_FLG   = 6
B_ET    = 7                     ; the declared type, the array's next
B_REC   = 9                     ; the record's TYPE, the array's next
B_FIX   = 12                    ; STRING * n, the array's next (words)
B_LINE  = 16                    ; a label's line
SC_NUM  = 8000h                 ; a scope's line numbers: its labels too
BSZ     = 20
BF_SHR  = 1                     ; the module's, every procedure's
BF_REF  = 2                     ; SHARED in a procedure: the module's
BF_STAT = 4                     ; STATIC
BF_LBL  = 8                     ; a label
BF_CON  = 16                    ; a CONST
BF_USE  = 32                    ; alone: no binding, a use with a suffix before
ET_IMP  = 0                     ; a type: none given (QBasic's ET_*)
ET_I2   = 1
ET_I4   = 2
ET_R4   = 3
ET_R8   = 4
ET_SD   = 5
ET_SH   = 6                     ; SHORT: 2 bytes in a record, else an INT
EMAX    = 254
NDO     = 7                     ; digits flt_out shows
NTMP    = 48                    ; string temporaries: the data stack is full first
VS      = 4                     ; a variable's slot, a number's bytes
NB      = 4                     ; a stored FLOAT's bytes
MAXD    = 16

M_VAR   = 01h
M_INT   = 02h
REM_Q   = 03h                   ; after TK_REM: the comment was a ', not REM
HDR     = 6                     ; len:2, num:2, cp:2
NONUM   = 0FFFFh                ; a line's num: it has none
L_NUM   = 2
L_CP    = 4                     ; a line's code in the cache
LMAX    = 2047                  ; a line's record at most: its body offsets fit BOFF
LSRC    = 255                   ; a line's text at most; its body is 5 times at most
BOFF    = 7FFh                  ; a position's body offset: the low 11 bits of its
TAGSH   = 11                    ; word, a mode or a BK_* above; BOFF the line's end
IFNDEF IDE
IDE     = 0
ENDIF
XSLK    = 48                    ; cm_xlate: the most one op writes past its size
XLIM    = 2040                  ; compile room: ERR code, EOL still fit
NL      = 13
NCH     = 16                    ; channels: the console and 15 files (io/chan.asm)
CBSZ    = 512                   ; a channel's buffer
RECMAX  = 4096                  ; a record's bytes at most
CK_CON  = 1                     ; a channel's kind: the console, a DOS file,
CK_DISK = 2                     ; NUL
CK_NUL  = 3
FM_IN   = 1                     ; OPEN's modes
FM_OUT  = 2
FM_APP  = 4
FM_RND  = 8
FM_BIN  = 16
VA_DESC = 1                     ; a variable's place (cm_va): a Desc,
VA_RAW  = 2                     ; its bytes, an INT kept as 2
VA_I2   = 3

; a FOR frame on the control stack: mark, ln:2, ip:2, id:1, step:4, limit:4,
; cache:4. With whole bounds: step:2, count:2, limit:2, FFFFh (a NaN, no float).
F_MARK  = 0
F_LN    = 1
F_IP    = 3                     ; where the loop or RETURN goes on: absolute, or
                                ; an offset in its line's code (cm_flush)
F_ID    = 5                     ; a word
F_STEP  = 7
F_LIMIT = 11
F_CACHE = 15                    ; an INT counter's frame: the float last stored
FRSIZE  = 19
M_FR    = FRSIZE                ; a frame's mark is its size
M_GS    = 5
SF_BRK  = 1                     ; stpflg: the STOP key; an event to run
SF_EV   = 2
M_EV    = 6                     ; an event's GOSUB: mark, ln:2, ip:2, its slot
F_EVN   = 5
EK_TIMER = 0                    ; ONEV's, EVST's kinds
EK_KEY  = 1
EK_COM  = 2
EK_PEN  = 3
EK_STRIG = 4
PK_SUB  = 1                     ; dc_proc's: a SUB line, a FUNCTION line, their END
PK_FN   = 2
PK_END  = 3
DM_STEP = 1                     ; the debugger's stops: the next statement
DM_PSTEP = 2                    ; the next at the depth of pdep or less
DBBPSZ  = 65536/8               ; the breakpoints: a bit per line number
M_PC    = 22                    ; a procedure's frame: mark, ln:2, ip:2, then
F_PRC   = 5                     ; its procedure
F_LSP0  = 6                     ; lsp before its arguments, before its locals
F_LSP1  = 10
F_TSPB  = 14                    ; the caller's tspb
F_SB    = 16                    ; the data stack at its body's start
F_LHND  = 20                    ; ON LOCAL ERROR's line, 0 none
LSSZ    = 12                    ; a save entry: id:2, ref:2, the value:8
LSAVSZ  = 10000h                ; the save stack

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
E_USR   = 35                    ; ERROR n: n in uerr
E_OS    = 36                    ; Out of stack space: not trapped
E_AF    = 37                    ; Advanced feature unavailable
E_DL    = 38                    ; Duplicate label
E_VR    = 39                    ; Variable required
E_SPE   = 40                    ; Subprogram error: BC's, no ERR of its own
E_ASF   = 41                    ; AS clause required on first declaration: so
E_LAST  = E_ASF

; Memory map, flat: the code, then the data; the regions in .data? (main.asm).
; dsb is the 16-bit build's DS at the same offsets: the variables, the
; strings' Descs, the arrays' headers, the high buffers, the control stack.
; The program and the POKE memory are 64K each, addressed by offsets from
; progb and pokeb. The stream region: offsets from cachb, stored in 16 bits;
; the direct line's code at DCODE, the cache from CACHE0.
DLINE    = 0                    ; in progb: the direct line, len 0, num FFxxh
PROGBASE = 0110h                ; then the program, to PROGLIM
PROGLIM  = 0FFF0h
NSYM     = 4096                 ; variables, 0 none
NNAM     = 4096                 ; names, 0 none
HSIZE    = 1024                 ; the symbols' hash chains
NMSZ     = 10000h               ; the symbols' names
VARBASE  equ     <(offset g_varb)>          ; a variable's slot, 4 bytes per id
SVBASE   equ     <(offset g_svarb)>         ; a string variable's Desc, 8 bytes per id
PROGSZ   = PROGLIM-PROGBASE
HDRBASE  equ     <(offset g_dsb+0BC00h)>    ; the arrays' headers
HDRLIM   equ     <(offset g_dsb+0E000h)>
ifndef AC
AC       = IDE                  ; Tab completion in the editor
endif
DCODE    = 0100h                ; in cachb: the direct line's stream
CACHE0   = DCODE+1536           ; the cache, to CLIM
CLIM     = 0FFF0h
HMIN     = 40000h               ; the heap, arrays and strings: DPMI's, at least
HROOM    = 10000h               ; the free memory left to the DPMI host
CSTOP    equ     <(offset g_dsb+0FC00h)>    ; the control stack, down from here
STKSZ    = 10000h               ; the data stack, its own region
STKTOP   equ     <(offset g_stkb+STKSZ-16)>
STKROOM  = 8000h                ; the data stack's room for the run
CPROOM   = 4000h                ; the compiler's frames, below where it starts
CSLIM    equ     <(offset g_dsb+0E000h)>    ; the control stack's floor
OBSZ     = 256                  ; plat.asm's output buffer

; block entries (comp/block.asm): their fields, their kinds
NBLK    = 2048
NSTM    = 128                   ; statements a line's map holds
B_POS   = 0
B_NXT   = 4
B_END   = 8
B_LINK  = 12
B_AUX   = 14
BK_IF     = 1
BK_ELSEIF = 2
BK_ELSE   = 3
BK_ENDIF  = 4
BK_DO     = 5
BK_LOOP   = 6
BK_WHILE  = 7
BK_WEND   = 8
BK_SELECT = 9
BK_CASE   = 10
BK_ENDSEL = 11
BK_EXIT   = 12
BK_FOR    = 13

; a TYPE (g_typ, comp/decl.asm): its name, its first field, how many
T_NAM   = 0
T_FST   = 2
T_NF    = 4
NTYPE   = 64
T_SIZE  = 6                     ; its bytes, as QBasic lays them
F_NAM   = 0                     ; a field: its name, ET_*, its TYPE, STRING * n,
F_ET    = 2                     ; its offset
F_SUB   = 3
F_FIX   = 4
F_OFF   = 6
NFLD    = 512
ET_REC  = 40h                   ; vtyp: a record of TYPE t is ET_REC + t

; a procedure (g_prc, comp/block.asm): 32 bytes
PSIZE   = 32
P_NAM   = 0                     ; its name
P_KIND  = 2                     ; PK_SUB, PK_FUNC
P_FLAG  = 3                     ; PF_STAT
P_ENTRY = 4                     ; its body: a target
P_FIRST = 8                     ; its first line, its last
P_LAST  = 10
P_NPAR  = 12                    ; its parameters: their variables from P_PAR
P_RES   = 14                    ; FUNCTION's result variable
P_PAR   = 16                    ; in g_ppar
P_SKIP  = 20                    ; past its END: a target
PK_SUB  = 1
PK_FUNC = 2
PF_STAT = 1
PF_DEF  = 2                     ; a DEF FN's: its parameters and result its only locals
