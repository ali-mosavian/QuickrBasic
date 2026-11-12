; ==========================================================================
; theme.asm - the editor's colours: themes of attributes and palettes
; ==========================================================================
; holds:    th_set, th_qb, th_dark, th_in, th_out, th_ah, g_tha, g_thn
; uses:     the VGA's DAC, ports 3C8h, 3C9h
; data:     The editor draws with attribute slots (A_ED, A_KW, ..: ui.inc);
;           scr.asm makes each the theme's attribute, g_tha. A theme is
;           its NATTR attributes and the 16 text colours' RGB, 6 bits
;           each, for the DAC: a theme can change what a colour looks like.
;           The editor's screen shows the theme's colours; the program's
;           screen VGA's, theme 0's.
; ==========================================================================

include x86f.inc

        .code

; ::::::::::::::
; th_set - theme AL, shown.
; th_qb - Options, QBasic Colors; th_dark - Options, One Dark.
; ::::::::::::::
th_qb:
        xor     al,al
        jmp     th_set
th_dark:
        mov     al,1
th_set:
        pushad
        mov     [g_thn],al
        movzx   eax,al
        imul    esi,eax,TH_SZ
        add     esi,offset g_ths
        mov     edi,offset g_tha
        mov     ecx,NATTR
        rep     movsb
        popad

; ::::::::::::::
; th_in - the theme's colours: the editor's screen shown.
; ::::::::::::::
th_in:
        pushad
        movzx   eax,[g_thn]
        imul    esi,eax,TH_SZ
        add     esi,offset g_ths+NATTR
        jmp     @thd_g

; ::::::::::::::
; th_out - VGA's colours: the program's screen shown.
; ::::::::::::::
th_out:
        pushad
        mov     esi,offset g_ths+NATTR  ; theme 0's are VGA's
@thd_g:
        xor     ebx,ebx
@thd_l:
        mov     al,[g_acpal+ebx]        ; colour BL's DAC register
        mov     dx,3C8h
        out     dx,al
        inc     dx
        mov     ecx,3
@@:
        lodsb
        out     dx,al
        loop    @B
        inc     ebx
        cmp     ebx,16
        jb      @thd_l
        popad
        ret

; ::::::::::::::
; th_ah - AH, a slot, the theme's attribute.
;
; keeps:  all but AH
; ::::::::::::::
th_ah:
        push    ebx
        movzx   ebx,ah
        mov     ah,[g_tha+ebx]
        pop     ebx
        ret

        .data
; the attribute controller's palette as the BIOS sets it: colour n's DAC
g_acpal db      0,1,2,3,4,5,20,7,56,57,58,59,60,61,62,63
; ED SEL TTL BAR HOT STAT MSEL MHOT GRAY BPL CUR DLG DSEL DFOC SHD THB,
; KW STR NUM COM NAM; then the 16 colours
g_ths   label   byte
        db      17h,71h,71h,70h,7Fh,30h,07h,0Fh,78h,47h,1Fh,70h,07h,7Fh,08h,00h
        db      1Bh,1Eh,1Dh,1Ah,17h     ; QBasic's, VGA's colours
        db      0,0,0, 0,0,42, 0,42,0, 0,42,42, 42,0,0, 42,0,42, 42,21,0, 42,42,42
        db      21,21,21, 21,21,63, 21,63,21, 21,63,63, 63,21,21, 63,21,63
        db      63,63,21, 63,63,63
; One Dark's (Zed's): 0 the bars, 1 the text's ground, 3 a selection's, 5
; keywords, 2 strings, 6 numbers, 8 comments
        db      17h,37h,39h,07h,0Fh,07h,3Fh,39h,08h,4Fh,3Fh,07h,3Fh,0Fh,08h,30h
        db      15h,12h,16h,18h,17h
        db      8,9,10, 10,11,13, 38,48,30, 15,17,20, 56,27,29, 49,30,55, 52,38,25
        db      42,44,47, 23,24,28, 24,43,59, 38,48,30, 21,45,48, 56,27,29
        db      49,30,55, 57,48,30, 55,55,57
.erre ($ - g_ths) EQ 2*TH_SZ
g_thn   db      0                       ; the theme shown
        .data?
g_tha   db      NATTR dup(?)
        .code

        end
