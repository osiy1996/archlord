;;
;; aPLib compression library  -  the smaller the better :)
;;
;; fasm 64-bit assembler depacker
;;
;; Copyright (c) 1998-2014 Joergen Ibsen
;; All Rights Reserved
;;
;; http://www.ibsensoftware.com/
;;

;format MS64 COFF

public aP_depack_asm

; =============================================================

;section '.text' code readable executable
.code

aP_depack_asm PROC
    ; aP_depack_asm(const void *source, void *destination)

    push   rbx
    push   rsi
    push   rdi

    push   rdx

    mov    rsi, rcx
    mov    rdi, rdx

    cld
    mov    dl, 80h

literal:
    movsb
nexttag:
    call   getbit
    jnb    literal

    xor    ecx, ecx
    call   getbit
    jnb    codepair
    xor    eax, eax
    call   getbit
    jnb    shortmatch
    inc    ecx
    mov    al, 10h
  getmorebits:
    call   getbit
    adc    al, al
    jnb    getmorebits
    jnz    domatch
    stosb
    jmp    nexttag
codepair:
    call   getgamma_no_ecx
    dec    ecx
    ;jnz    normalcodepair
    loop   normalcodepair
    call   getgamma
    jmp    domatch_lastpos

shortmatch:
    lodsb
    shr    eax, 1
    jz     donedepacking
    adc    ecx, ecx
    jmp    domatch_with_2inc

normalcodepair:
    xchg   eax, ecx
    dec    eax
    shl    eax, 8
    lodsb
    call   getgamma

    cmp    eax, 7d00h
    jnb    short domatch_with_2inc
    cmp    ah, 5
    jnb    short domatch_with_inc
    cmp    eax, 7fh
    ja     short domatch_new_lastpos

domatch_with_2inc:
    inc    ecx

domatch_with_inc:
    inc    ecx

domatch_new_lastpos:
    xchg   eax, r8d
domatch_lastpos:
    mov    eax, r8d

domatch:
    push   rsi
    mov    rsi, rdi
    sub    rsi, rax
    rep    movsb
    pop    rsi
    jmp    nexttag

getbit:
    add    dl, dl
    jnz    stillbitsleft
    mov    dl, [rsi]
    inc    rsi
    adc    dl, dl
  stillbitsleft:
    ret

getgamma:
    xor    ecx, ecx
getgamma_no_ecx:
    inc    ecx
  getgammaloop:
    call   getbit
    adc    ecx, ecx
    call   getbit
    jb     getgammaloop
    ret

donedepacking:
    pop    rdx
    sub    rdi, rdx
    xchg   eax, edi

    pop    rdi
    pop    rsi
    pop    rbx

    ret
aP_depack_asm ENDP
END

; =============================================================
