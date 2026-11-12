; ==========================================================================
; theme.asm - the editor's colours: themes of attributes and palettes
; ==========================================================================
; holds:    th_set, th_qb, th_dark, th_pick, th_in, th_out, th_ah, g_tha, g_thn
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
; th_qb - Options, QBasic Colors; th_dark - One Dark; th_pick - the menu's
; theme action AL, from A_TH2 on.
; ::::::::::::::
th_qb:
        xor     al,al
        jmp     th_set
th_dark:
        mov     al,1
        jmp     th_set
th_pick:
        sub     al,A_TH2-2
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

; Zed's themes' attributes, over one layout of the colours
zeda    macro
        db      17h,37h,39h,07h,0Fh,07h,3Fh,39h,08h,4Fh,3Fh,07h,3Fh,0Fh,08h,30h
        db      15h,12h,16h,18h,17h,19h,1Dh,1Bh,1Ah
        endm

        .data
; the attribute controller's palette as the BIOS sets it: colour n's DAC
g_acpal db      0,1,2,3,4,5,20,7,56,57,58,59,60,61,62,63
; ED SEL TTL BAR HOT STAT MSEL MHOT GRAY BPL CUR DLG DSEL DFOC SHD THB,
; KW STR NUM COM NAM FNM PAR TYP OP; then the 16 colours
g_ths   label   byte
        db      17h,71h,71h,70h,7Fh,30h,07h,0Fh,78h,47h,1Fh,70h,07h,7Fh,08h,00h
        db      1Bh,1Eh,1Dh,1Ah,17h,1Fh,1Ch,13h,13h ; QBasic's, VGA's colours
        db      0,0,0, 0,0,42, 0,42,0, 0,42,42, 42,0,0, 42,0,42, 42,21,0, 42,42,42
        db      21,21,21, 21,21,63, 21,63,21, 21,63,63, 63,21,21, 63,21,63
        db      63,63,21, 63,63,63
; 0 bars, 1 the text's ground, 2 strings, 3 a selection, 4 errors, 5 keywords, 6 numbers,
; 7 text, 8 comments, 9 functions, 10 operators, 11 types, 12 punctuation, 13 parameters,
; 14 warnings, 15 strong text; from Zed's theme files
; One Dark
        zeda
        db      14,16,19, 10,11,12, 40,48,32, 17,18,21, 52,28,29, 45,29,51
        db      47,37,26, 43,44,47, 23,24,27, 28,43,58, 27,45,47, 27,45,47
        db      43,44,47, 52,28,29, 55,48,33, 55,56,57
; One Light
        zeda
        db      55,55,55, 62,62,62, 25,39,21, 50,50,50, 52,24,20, 41,18,42
        db      43,27,9, 9,9,10, 40,40,41, 22,30,56, 14,32,45, 14,32,45
        db      9,9,10, 52,24,19, 41,34,6, 3,3,4
; Ayu Mirage
        zeda
        db      17,18,20, 9,10,13, 53,63,31, 20,21,23, 60,33,30, 63,43,25
        db      55,47,63, 51,50,48, 23,25,28, 63,52,28, 60,39,29, 28,51,63
        db      45,44,43, 55,47,63, 63,51,28, 58,58,57
; Ayu Light
        zeda
        db      55,55,55, 63,63,63, 33,44,0, 51,52,52, 59,28,28, 62,35,15
        db      40,30,51, 23,24,25, 42,44,45, 60,43,18, 59,36,25, 14,39,57
        db      28,29,30, 40,30,51, 60,43,18, 9,9,10
; Gruvbox Dark Soft
        zeda
        db      19,17,16, 12,12,11, 46,46,9, 22,20,19, 62,18,13, 62,18,12
        db      52,33,38, 58,54,44, 42,38,33, 46,46,9, 35,48,31, 62,47,11
        db      53,49,40, 58,54,44, 62,47,11, 61,60,56
; Gruvbox Light
        zeda
        db      54,50,41, 62,60,49, 30,29,3, 50,46,38, 39,0,2, 39,0,1
        db      35,15,28, 10,10,10, 31,27,25, 30,29,3, 16,30,22, 45,29,4
        db      15,14,13, 10,10,10, 45,29,5, 4,4,4
; JetBrains Dark
        zeda
        db      9,10,10, 7,7,8, 26,42,28, 16,17,18, 62,25,29, 51,35,27
        db      10,43,46, 47,47,49, 30,31,33, 26,40,53, 47,47,49, 41,46,29
        db      51,35,27, 47,47,49, 60,49,23, 57,57,57
; JetBrains Light
        zeda
        db      61,62,62, 63,63,63, 1,31,5, 55,56,57, 63,0,0, 0,12,44
        db      5,20,58, 2,2,2, 35,35,35, 0,24,30, 2,2,2, 40,18,2
        db      0,12,44, 2,2,2, 61,38,4, 0,0,0
.erre ($ - g_ths) EQ NTHEME*TH_SZ
g_thn   db      0                       ; the theme shown
        .data?
g_tha   db      NATTR dup(?)
        .code

        end
