; ==========================================================================
; main.asm - the .COM image: startup, then every module
; ==========================================================================
; holds:    rt_start; the include list
; uses:     every module, as one translation unit
; data:     CS the code, below CSOFF, and the code cache over the data's
;           free part; DS=SS=ES CS+CSOFF/16, the data, its image moved there
;           at the start. FS the POKE segment; GS the program; the heap above.
; calls:
;   rt_start           the data moved, zeroed, plt_init, prg_init, rpl_ready
; ==========================================================================
;           Flat: CS, DS, ES, SS the one 4G segment. The regions at the end.
; ==========================================================================
        .386
        .387
        .model  flat
        .stack  4096
        option  casemap:none
include def.inc

        .code

; ::::::::::::::
; rt_start - the regions zeroed, the vectors, then the READY loop.
; ::::::::::::::
rt_start:
        cld
        mov     ax,ds
        mov     [dsv],ax
        mov     es,ax
        mov     fs,ax
        mov     gs,ax
        mov     [pspsel],es             ; DOS/32A: ES the PSP at the start
        mov     esp,STKTOP
        mov     edi,offset bss_s
        mov     ecx,offset bss_e
        sub     ecx,edi
        xor     al,al
        rep     stosb
        mov     edi,offset pokeb
        mov     ecx,10000h/4
        xor     eax,eax
        rep     stosd
        mov     [peekseg],0
        mov     [dslin],0
        mov     [tmplin],offset tmpd
        mov     [pglin],offset progb
        mov     [proglin],offset progb+PROGBASE
        mov     [hbot],offset heapb
        mov     [htop],offset heapb+HEAPSZ
        mov     [rsp],esp
        mov     [cstk],CSTOP
        mov     [stklim],STKTOP-STKROOM
        mov     [cplim],STKTOP-2*STKROOM   ; the compiler's frames are twice the 16-bit build's
        mov     dword ptr [progb+DLINE],0FF0000h
        mov     [cxb],offset xbuf
        mov     [rs],0FFFFFF01h         ; the 6502 build's boot state under sim65
        call    flt_finit
        call    plt_init
        call    prg_init
if IDE
        jmp     ui_start
else
        jmp     rpl_ready
endif

        .data
dsv     dw      ?                       ; DS, for code that runs without it
pspsel  dw      ?
old23   df      ?                       ; the vectors plt_init replaced
old1c   df      ?
        .code

include vm/isa.inc
include vm/exec.asm
include vm/flow.asm
include vm/calls.asm
include vm/builtins.asm
include comp/dfa.inc
include comp/lex.asm
include comp/comp.asm
include comp/expr.asm
include comp/link.asm
include vm/bc.asm
include comp/list.asm
if IDE
include ui/scr.asm
include ui/mouse.asm
if AC
include ui/compl.asm
endif
include ui/text.asm
include ui/edit.asm
include ui/draw.asm
include ui/menu.asm
include ui/dlg.asm
include ui/sync.asm
include ui/ui.asm
endif
include mem/far.asm
include mem/prog.asm
include mem/sym.asm
include mem/str.asm
include mem/arr.asm
include io/io.asm
include io/repl.asm
include io/file.asm
include io/plat.asm
include float/flt.asm
include float/ftr.asm
include float/fpu.asm
include gen.inc
include data.inc
        .data?
        align   4
dsb     db      10000h dup(?)           ; the 16-bit build's DS, as one block
progb   db      10000h dup(?)
pokeb   db      10000h dup(?)
cachb   db      10000h dup(?)
stkb    db      STKSZ dup(?)
heapb   db      HEAPSZ dup(?)
        .code

        end     rt_start
