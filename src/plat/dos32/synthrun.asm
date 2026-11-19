; synth_run: a voice's run of samples for synth.c's voice_run, its wave and filter fixed, added to the sum.
;
; # How it works
; One loop for each wave and filter, the kind's by a jump table: the filter's state in registers, the rest in the
; SynthRun. Each sample as voice_run's in C, to the bit.
        .386
        .model  flat
        public  _synth_run

SynthRun struc
        sr_phase        dd      ?
        sr_step         dd      ?
        sr_level        dd      ?
        sr_delta        dd      ?
        sr_low          dd      ?
        sr_band         dd      ?
        sr_coef            dd      ?
        sr_damping         dd      ?
        sr_gain         dd      ?
        sr_sum          dd      ?
        sr_n            dd      ?
        sr_noise        dd      ?
        sr_wide         dd      ?
        sr_kind         dd      ?               ; wave * 4 + filter
        sr_input           dd      ?
        sr_sine         dd      ?
        sr_ratio        dd      ?
        sr_depth        dd      ?
        sr_bend         dd      ?
        sr_slope        dd      ?
        sr_fed0         dd      ?
        sr_fed1         dd      ?
        sr_feedback     dd      ?
SynthRun ends

; eax: the wave's next sample, -32767 to 32767; the phase stepped
WAVE    macro   w
        local   l_hi, l_keep
  if w eq 3                                     ; noise: the register steps as bit 27 rises
        mov     ecx, [ebx].SynthRun.sr_phase
        mov     eax, ecx
        add     eax, [ebx].SynthRun.sr_step
        mov     [ebx].SynthRun.sr_phase, eax
        not     ecx
        and     ecx, eax
        test    ecx, 08000000h
        jz      l_keep
        mov     eax, [ebx].SynthRun.sr_noise
        mov     edx, eax
        shr     edx, 22
        mov     ecx, eax
        shr     ecx, 17
        xor     edx, ecx
        and     edx, 1
        add     eax, eax
        or      eax, edx
        and     eax, 07FFFFFh
        mov     [ebx].SynthRun.sr_noise, eax
l_keep:
        mov     eax, [ebx].SynthRun.sr_noise
        shr     eax, 15
        and     eax, 0FFh
        imul    eax, eax, 257
        sub     eax, 32767
  else
        mov     eax, [ebx].SynthRun.sr_phase
        add     eax, [ebx].SynthRun.sr_step
        mov     [ebx].SynthRun.sr_phase, eax
    if w eq 0                                   ; pulse
        shr     eax, 20
        cmp     eax, [ebx].SynthRun.sr_wide
        mov     eax, 32767
        jb      l_hi
        neg     eax
l_hi:
    elseif w eq 1                               ; triangle
        shr     eax, 16
        cmp     eax, 32768
        jb      l_hi
        xor     eax, 0FFFFh
l_hi:
        add     eax, eax
        sub     eax, 32767
    elseif w eq 2                               ; saw
        shr     eax, 16
        sub     eax, 32768
    else                                        ; FM: a sine bent by a sine, which bends itself
        mov     ecx, eax
        imul    eax, [ebx].SynthRun.sr_ratio
        mov     edx, [ebx].SynthRun.sr_fed0
        add     edx, [ebx].SynthRun.sr_fed1
        imul    edx, [ebx].SynthRun.sr_feedback
        add     eax, edx
        shr     eax, 20
        mov     edx, [ebx].SynthRun.sr_sine
        movsx   eax, word ptr [edx+eax*2]
        mov     edx, [ebx].SynthRun.sr_bend
        sar     edx, 9
        imul    eax, edx
        sar     eax, 15
        mov     edx, [ebx].SynthRun.sr_fed0     ; its last two samples, the bend a step down
        mov     [ebx].SynthRun.sr_fed1, edx
        mov     [ebx].SynthRun.sr_fed0, eax
        mov     edx, [ebx].SynthRun.sr_bend
        sub     edx, [ebx].SynthRun.sr_slope
        mov     [ebx].SynthRun.sr_bend, edx
        imul    eax, [ebx].SynthRun.sr_depth
        add     eax, ecx
        shr     eax, 20
        mov     edx, [ebx].SynthRun.sr_sine
        movsx   eax, word ptr [edx+eax*2]
    endif
  endif
        endm

; the filter once: low in ebp, band in edi, high left in ecx
FSTEP   macro
        mov     eax, [ebx].SynthRun.sr_coef                 ; low += f * band
        imul    edi
        shrd    eax, edx, 16
        add     ebp, eax
        mov     eax, [ebx].SynthRun.sr_damping              ; high = in - low - damping * band
        imul    edi
        shrd    eax, edx, 16
        mov     ecx, [ebx].SynthRun.sr_input
        sub     ecx, ebp
        sub     ecx, eax
        mov     eax, [ebx].SynthRun.sr_coef                 ; band += f * high
        imul    ecx
        shrd    eax, edx, 16
        add     edi, eax
        endm

; a run of wave w through filter flt (0 none, 1 low, 2 band, 3 high)
VOICE   macro   w, flt
        local   l_top, l_done
        mov     esi, [ebx].SynthRun.sr_sum
        mov     edi, [ebx].SynthRun.sr_band
        mov     ebp, [ebx].SynthRun.sr_low
l_top:
        WAVE    w
        mov     ecx, [ebx].SynthRun.sr_level
        sar     ecx, 9
        imul    eax, ecx
        sar     eax, 15
  if flt eq 0
        imul    eax, [ebx].SynthRun.sr_gain
        sar     eax, 15
  else
        shl     eax, 8
        mov     [ebx].SynthRun.sr_input, eax
        FSTEP
        FSTEP
    if flt eq 1
        mov     eax, ebp
    elseif flt eq 2
        mov     eax, edi
    else
        mov     eax, ecx
    endif
        sar     eax, 8
        imul    dword ptr [ebx].SynthRun.sr_gain
        shrd    eax, edx, 15
  endif
        add     [esi], eax
        add     esi, 4
        dec     dword ptr [ebx].SynthRun.sr_n
        jz      l_done
        mov     eax, [ebx].SynthRun.sr_delta
        add     dword ptr [ebx].SynthRun.sr_level, eax
        jmp     l_top
l_done:
        mov     [ebx].SynthRun.sr_band, edi
        mov     [ebx].SynthRun.sr_low, ebp
        jmp     finish
        endm

        .code
; void synth_run(SynthRun *r): r in eax (regparm)
_synth_run:
        push    ebx
        push    esi
        push    edi
        push    ebp
        mov     ebx, eax
        mov     eax, [ebx].SynthRun.sr_kind
        jmp     kinds[eax*4]
finish:
        pop     ebp
        pop     edi
        pop     esi
        pop     ebx
        ret

kinds   dd      p0, p1, p2, p3, t0, t1, t2, t3, s0, s1, s2, s3, n0, n1, n2, n3, f0, f1, f2, f3
p0:     VOICE   0, 0
p1:     VOICE   0, 1
p2:     VOICE   0, 2
p3:     VOICE   0, 3
t0:     VOICE   1, 0
t1:     VOICE   1, 1
t2:     VOICE   1, 2
t3:     VOICE   1, 3
s0:     VOICE   2, 0
s1:     VOICE   2, 1
s2:     VOICE   2, 2
s3:     VOICE   2, 3
n0:     VOICE   3, 0
n1:     VOICE   3, 1
n2:     VOICE   3, 2
n3:     VOICE   3, 3
f0:     VOICE   4, 0
f1:     VOICE   4, 1
f2:     VOICE   4, 2
f3:     VOICE   4, 3
        end
