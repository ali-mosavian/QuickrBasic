; The CPU's: CPUID, MSRs, a variable-range MTRR changed, and a fence (plat/dos32/dos.h), cdecl.
;
; # How it works
; An MTRR is changed as Intel's manual has it: interrupts off, the caches flushed and filling no
; more, the TLBs flushed, the MTRRs off while its pair is written, then all put back. A fence is a
; locked instruction, which sends a P6's write-combining buffers on, as Glide's P6FENCE.
        .686p
        .model  flat
        public  _pdos_cpuid, _pdos_rdmsr, _pdos_mtrr_write, _pdos_fence
        .code
; bool pdos_cpuid(uint32_t leaf, uint32_t out[4]): EAX, EBX, ECX and EDX; false, no CPUID.
_pdos_cpuid:
        pushfd
        pop     eax
        mov     ecx,eax
        xor     eax,200000h             ; ID: it toggles only where CPUID is
        push    eax
        popfd
        pushfd
        pop     eax
        push    ecx
        popfd
        xor     eax,ecx
        and     eax,200000h
        jz      @none
        push    ebx
        push    edi
        mov     eax,[esp+12]
        xor     ecx,ecx
        cpuid
        mov     edi,[esp+16]
        mov     [edi],eax
        mov     [edi+4],ebx
        mov     [edi+8],ecx
        mov     [edi+12],edx
        pop     edi
        pop     ebx
        mov     eax,1
@none:
        ret

; uint64_t pdos_rdmsr(uint32_t msr)
_pdos_rdmsr:
        mov     ecx,[esp+4]
        rdmsr
        ret

; void pdos_mtrr_write(uint32_t n, const MtrrPair *p): PHYSBASEn and PHYSMASKn.
_pdos_mtrr_write:
        push    ebx
        push    esi
        push    edi
        pushfd
        cli
        mov     edi,[esp+20]
        mov     esi,[esp+24]
        mov     ebx,cr4
        push    ebx
        and     ebx,not 80h             ; PGE off: global pages flushed too
        mov     cr4,ebx
        mov     eax,cr0
        push    eax
        or      eax,40000000h           ; CD on, NW off: the caches fill no more
        and     eax,not 20000000h
        mov     cr0,eax
        wbinvd
        mov     eax,cr3
        mov     cr3,eax
        mov     ecx,2FFh                ; MTRRdefType: the MTRRs off
        rdmsr
        push    edx
        push    eax
        and     eax,not 800h
        wrmsr
        lea     ecx,[edi*2+200h]
        mov     eax,[esi]
        mov     edx,[esi+4]
        wrmsr
        inc     ecx
        mov     eax,[esi+8]
        mov     edx,[esi+12]
        wrmsr
        wbinvd
        mov     eax,cr3
        mov     cr3,eax
        mov     ecx,2FFh
        pop     eax
        pop     edx
        wrmsr
        pop     eax
        mov     cr0,eax
        pop     ebx
        mov     cr4,ebx
        popfd
        pop     edi
        pop     esi
        pop     ebx
        ret

; void pdos_fence(void)
_pdos_fence:
        lock or dword ptr [esp],0
        ret
        end
