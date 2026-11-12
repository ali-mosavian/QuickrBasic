; ==========================================================================
; comp/comp.inc - the parser's actions: under A_BASE a CALL, from it cmact's
; ==========================================================================

; r = r in upper case, a letter; r a 32-bit register.
upcase  macro   r
        local   @upc
        cmp     r,'a'
        jb      @upc
        cmp     r,'z'
        ja      @upc
        sub     r,32
@upc:
        endm

A_BASE  = 0C0h                  ; above the CALLs, under the PS_ codes (FCh)
A_E     = A_BASE
A_LV    = A_BASE+1
A_L     = A_BASE+2
A_TOB   = A_BASE+3
A_TOA   = A_BASE+4
A_TOS   = A_BASE+5
A_STR   = A_BASE+6
A_FV    = A_BASE+7
A_FST   = A_BASE+8
A_FK    = A_BASE+9
A_FOR   = A_BASE+10
A_FOR1  = A_BASE+11
A_NV    = A_BASE+12
A_IF    = A_BASE+13
A_ON    = A_BASE+14
A_ONL   = A_BASE+15
A_PR    = A_BASE+16
A_DEF   = A_BASE+17
A_LST   = A_BASE+18
A_DV    = A_BASE+19
A_DATA  = A_BASE+20
A_REM   = A_BASE+21
A_LET   = A_BASE+22
A_IN    = A_BASE+23
A_RD    = A_BASE+24
A_GET   = A_BASE+25
A_GOTO  = A_BASE+26
A_GOSUB = A_BASE+27
A_RUN   = A_BASE+28
A_RES   = A_BASE+29
A_RET   = A_BASE+30
A_N0    = A_BASE+31
A_N1    = A_BASE+32
A_NARG  = A_BASE+33
A_FOP   = A_BASE+34
A_SWAP  = A_BASE+35
A_TOI   = A_BASE+36
A_TOD   = A_BASE+37
.erre K_N LE A_BASE                     ; a CALL's K_ is never an action
.erre A_TOD LT 0FCh
