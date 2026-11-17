; setjmp and longjmp, cdecl (lib/include/setjmp.h).
        .386
        .model  flat
        public  _setjmp, _longjmp
        .code
_setjmp:
        mov     edx,[esp+4]
        mov     [edx],ebx
        mov     [edx+4],esi
        mov     [edx+8],edi
        mov     [edx+12],ebp
        lea     eax,[esp+4]             ; the caller's ESP after the return
        mov     [edx+16],eax
        mov     eax,[esp]
        mov     [edx+20],eax
        xor     eax,eax
        ret
_longjmp:
        mov     edx,[esp+4]
        mov     eax,[esp+8]
        test    eax,eax
        jnz     @F
        inc     eax
@@:
        mov     ebx,[edx]
        mov     esi,[edx+4]
        mov     edi,[edx+8]
        mov     ebp,[edx+12]
        mov     esp,[edx+16]
        jmp     dword ptr [edx+20]
        end
