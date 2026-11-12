; ==========================================================================
; errmsg.asm - QBasic's error numbers and messages
; ==========================================================================
; holds:    er_text, g_qberr
; data:     g_qberr is ERR by E_*; g_qbmsg the messages by ERR, each its
;           number, then its text, 0-ended.
; ==========================================================================

include x86f.inc

        .code

; ::::::::::::::
; er_text - the message of error AL, E_*: ERROR n's by uerr.
;
; out:    ESI  the text, 0-ended
; keeps:  all but ESI
; ::::::::::::::
er_text:
        push    eax
        push    edx
        movzx   eax,al
        mov     dl,[g_qberr+eax]
        cmp     al,E_USR
        jne     @F
        mov     ax,[g_state.rn.uerr]
        test    ah,ah
        jnz     @ert_u
        cmp     al,0FFh
        je      @ert_u
        mov     dl,al
@@:
        test    dl,dl
        jz      @ert_u
        mov     esi,offset g_qbmsg
@ert_l:
        lodsb
        test    al,al
        jz      @ert_u
        cmp     al,dl
        je      @ert_r
@@:
        lodsb
        test    al,al
        jnz     @B
        jmp     @ert_l
@ert_u:
        mov     esi,offset g_qbunp
@ert_r:
        pop     edx
        pop     eax
        ret

        .data
; ERR by E_*: 0 none; FFh not trapped, Out of stack space; F0h.. BC's compile
; errors, no ERR of their own.
g_qberr db      0,0,2,3,4,5,7,8,9,10,11,13,17,0,6,1,16,15,12,0,0,0,0
        db      67,55,52,53,68,54,54,64,52,0,13,18,0,0FFh,73,33,40,0F0h,0F1h
.erre ($ - g_qberr) EQ E_LAST+1
g_qbmsg db      1,"NEXT without FOR",0
        db      2,"Syntax error",0
        db      3,"RETURN without GOSUB",0
        db      4,"Out of DATA",0
        db      5,"Illegal function call",0
        db      6,"Overflow",0
        db      7,"Out of memory",0
        db      8,"Label not defined",0
        db      9,"Subscript out of range",0
        db      10,"Duplicate definition",0
        db      11,"Division by zero",0
        db      12,"Illegal in direct mode",0
        db      13,"Type mismatch",0
        db      14,"Out of string space",0
        db      16,"String formula too complex",0
        db      17,"Cannot continue",0
        db      18,"Function not defined",0
        db      19,"No RESUME",0
        db      20,"RESUME without error",0
        db      24,"Device timeout",0
        db      25,"Device fault",0
        db      26,"FOR without NEXT",0
        db      27,"Out of paper",0
        db      29,"WHILE without WEND",0
        db      30,"WEND without WHILE",0
        db      33,"Duplicate label",0
        db      35,"Subprogram not defined",0
        db      37,"Argument-count mismatch",0
        db      38,"Array not defined",0
        db      40,"Variable required",0
        db      50,"FIELD overflow",0
        db      51,"Internal error",0
        db      52,"Bad file name or number",0
        db      53,"File not found",0
        db      54,"Bad file mode",0
        db      55,"File already open",0
        db      56,"FIELD statement active",0
        db      57,"Device I/O error",0
        db      58,"File already exists",0
        db      59,"Bad record length",0
        db      61,"Disk full",0
        db      62,"Input past end of file",0
        db      63,"Bad record number",0
        db      64,"Bad file name",0
        db      67,"Too many files",0
        db      68,"Device unavailable",0
        db      69,"Communication-buffer overflow",0
        db      70,"Permission denied",0
        db      71,"Disk not ready",0
        db      72,"Disk-media error",0
        db      73,"Advanced feature unavailable",0
        db      74,"Rename across disks",0
        db      75,"Path/File access error",0
        db      76,"Path not found",0
        db      0F0h,"Subprogram error",0
        db      0F1h,"AS clause required on first declaration",0
        db      0FFh,"Out of stack space",0
        db      0
g_qbunp db      "Unprintable error",0
        .code

        end
