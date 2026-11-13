; ==========================================================================
; main.asm - the .COM image: startup, then every module
; ==========================================================================
; holds:    rt_start; the regions at the end
; uses:     plt_init, prg_init, flt_finit, rpl_ready
; data:     CS the code, below CSOFF, and the code cache over the data's
;           free part; DS=SS=ES CS+CSOFF/16, the data, its image moved there
;           at the start. FS the POKE segment; GS the program; the heap above.
; calls:
;   rt_start           the data moved, zeroed, plt_init, prg_init, rpl_ready
; ==========================================================================
;           Flat: CS, DS, ES, SS the one 4G segment. The regions at the end.
; ==========================================================================
include x86f.inc
        .stack  4096

        .code

; ::::::::::::::
; rt_start - the regions zeroed, the vectors, then the READY loop.
; ::::::::::::::
rt_start:
        cld
        mov     [g_plat.pspsel],es      ; DOS/32A: ES the PSP at the start
        mov     ax,ds
        mov     [g_plat.dsv],ax
        mov     es,ax
        mov     fs,ax
        mov     gs,ax
        mov     esp,STKTOP
        mov     edi,offset g_bss_s
        mov     ecx,offset g_bss_e
        sub     ecx,edi
        xor     al,al
        rep     stosb
        mov     edi,offset g_pokeb
        mov     ecx,10000h/4
        xor     eax,eax
        rep     stosd
        mov     [g_state.mm.peekseg],0
        mov     [g_state.mm.dslin],0
        mov     [g_state.sg.tmplin],offset g_tmpd
        call    plt_heap
        mov     ecx,10000h              ; the cache, then the program store
        xor     ebx,ebx
        call    alloc_new
        mov     [g_state.mm.cachebase],edx
        mov     ecx,10000h
        mov     ebx,offset prg_place
        call    alloc_new
        call    prg_place
        mov     [g_state.rn.rsp],esp
        mov     [g_state.rn.cstk],CSTOP
        mov     [g_state.rn.stklim],STKTOP-STKROOM
        mov     dword ptr gs:[DLINE+TLINE.len],0  ; the direct line: len 0, num FF00h
        mov     word ptr gs:[DLINE+TLINE.num],0FF00h
        mov     [g_state.cm.cxb],offset g_xbuf
        mov     [g_state.ft.rs],50000h  ; QBasic's first seed
        call    flt_finit
        call    plt_init
        call    sys_init
        call    prg_init
if IDE
        jmp     ui_start
else
        call    rpl_file
        jmp     rpl_ready
endif

        .data?
g_bss_s label   byte                    ; from here to g_bss_e (regions.asm): zeroed
g_state TSTATE  <>
        .code


        end     rt_start
