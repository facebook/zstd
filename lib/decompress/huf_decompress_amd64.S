/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "../common/portability_macros.h"

#if defined(__ELF__) && defined(__GNUC__)
/* Stack marking
 * ref: https://wiki.gentoo.org/wiki/Hardened/GNU_stack_quickstart
 */
.section .note.GNU-stack,"",%progbits

#if defined(__aarch64__)
/* Mark that this assembly supports BTI & PAC, because it is empty for aarch64.
 * See: https://github.com/facebook/zstd/issues/3841
 * See: https://gcc.godbolt.org/z/sqr5T4ffK
 * See: https://lore.kernel.org/linux-arm-kernel/20200429211641.9279-8-broonie@kernel.org/
 * See: https://reviews.llvm.org/D62609
 */
.pushsection .note.gnu.property, "a"
.p2align 3
.long 4                 /* size of the name - "GNU\0" */
.long 0x10              /* size of descriptor */
.long 0x5               /* NT_GNU_PROPERTY_TYPE_0 */
.asciz "GNU"
.long 0xc0000000        /* pr_type - GNU_PROPERTY_AARCH64_FEATURE_1_AND */
.long 4                 /* pr_datasz - 4 bytes */
.long 3                 /* pr_data - GNU_PROPERTY_AARCH64_FEATURE_1_BTI | GNU_PROPERTY_AARCH64_FEATURE_1_PAC */
.p2align 3              /* pr_padding - bring everything to 8 byte alignment */
.popsection
#endif

#endif

// There appears to be an unreconcilable syntax difference between Linux and Darwin assemblers.
// Name of a private label (i.e. not exported to symbol table) on Darwin has to start with "L",
// on Linux has to start with ".". There's no way to have a name start with both "." and "L", so
// we have to use a macro.
#if defined(__APPLE__)
#define LOCAL_LABEL(label) L_ ## label
#else
#define LOCAL_LABEL(label) .L_ ## label
#endif

#if ZSTD_ENABLE_ASM_X86_64_BMI2

/* Calling convention:
 *
 * %rdi (or %rcx on Windows) contains the first argument: HUF_DecompressAsmArgs*.
 * %rbp isn't maintained (no frame pointer).
 * %rsp contains the stack pointer that grows down.
 *      No red-zone is assumed, only addresses >= %rsp are used.
 * All register contents are preserved.
 */

ZSTD_HIDE_ASM_FUNCTION(HUF_decompress4X1_usingDTable_internal_fast_asm_loop)
ZSTD_HIDE_ASM_FUNCTION(HUF_decompress4X2_usingDTable_internal_fast_asm_loop)
ZSTD_HIDE_ASM_FUNCTION(_HUF_decompress4X2_usingDTable_internal_fast_asm_loop)
ZSTD_HIDE_ASM_FUNCTION(_HUF_decompress4X1_usingDTable_internal_fast_asm_loop)
.global HUF_decompress4X1_usingDTable_internal_fast_asm_loop
.global HUF_decompress4X2_usingDTable_internal_fast_asm_loop
.global _HUF_decompress4X1_usingDTable_internal_fast_asm_loop
.global _HUF_decompress4X2_usingDTable_internal_fast_asm_loop
.text

/* Sets up register mappings for clarity.
 * op[], bits[], dtable & ip[0] each get their own register.
 * ip[1,2,3] & olimit alias var[].
 * %rax is a scratch register.
 */

#define op0    rsi
#define op1    rbx
#define op2    rcx
#define op3    rdi

#define ip0    r8
#define ip1    r9
#define ip2    r10
#define ip3    r11

#define bits0  rbp
#define bits1  rdx
#define bits2  r12
#define bits3  r13
#define dtable r14
#define olimit r15

/* var[] aliases ip[1,2,3] & olimit
 * ip[1,2,3] are saved every iteration.
 * olimit is only used in compute_olimit.
 */
#define var0   r15
#define var1   r9
#define var2   r10
#define var3   r11

/* 32-bit var registers */
#define vard0  r15d
#define vard1  r9d
#define vard2  r10d
#define vard3  r11d

/* Calls X(N) for each stream 0, 1, 2, 3. */
#define FOR_EACH_STREAM(X) \
    X(0);                  \
    X(1);                  \
    X(2);                  \
    X(3)

/* Calls X(N, idx) for each stream 0, 1, 2, 3. */
#define FOR_EACH_STREAM_WITH_INDEX(X, idx) \
    X(0, idx);                             \
    X(1, idx);                             \
    X(2, idx);                             \
    X(3, idx)

/* Define both _HUF_* & HUF_* symbols because MacOS
 * C symbols are prefixed with '_' & Linux symbols aren't.
 */
_HUF_decompress4X1_usingDTable_internal_fast_asm_loop:
HUF_decompress4X1_usingDTable_internal_fast_asm_loop:
    ZSTD_CET_ENDBRANCH
    .cfi_startproc
    .cfi_def_cfa_offset 8
    .cfi_offset %rip, -8
    /* Save all registers - even if they are callee saved for simplicity. */
    push %rax
    .cfi_def_cfa_offset 16
    .cfi_offset rax, -16
    push %rbx
    .cfi_def_cfa_offset 24
    .cfi_offset rbx, -24
    push %rcx
    .cfi_def_cfa_offset 32
    .cfi_offset rcx, -32
    push %rdx
    .cfi_def_cfa_offset 40
    .cfi_offset rdx, -40
    push %rbp
    .cfi_def_cfa_offset 48
    .cfi_offset rbp, -48
    push %rsi
    .cfi_def_cfa_offset 56
    .cfi_offset rsi, -56
    push %rdi
    .cfi_def_cfa_offset 64
    .cfi_offset rdi, -64
    push %r8
    .cfi_def_cfa_offset 72
    .cfi_offset r8, -72
    push %r9
    .cfi_def_cfa_offset 80
    .cfi_offset r9, -80
    push %r10
    .cfi_def_cfa_offset 88
    .cfi_offset r10, -88
    push %r11
    .cfi_def_cfa_offset 96
    .cfi_offset r11, -96
    push %r12
    .cfi_def_cfa_offset 104
    .cfi_offset r12, -104
    push %r13
    .cfi_def_cfa_offset 112
    .cfi_offset r13, -112
    push %r14
    .cfi_def_cfa_offset 120
    .cfi_offset r14, -120
    push %r15
    .cfi_def_cfa_offset 128
    .cfi_offset r15, -128

    /* Read HUF_DecompressAsmArgs* args from %rax */
#if defined(_WIN32)
    movq %rcx, %rax
#else
    movq %rdi, %rax
#endif
    movq  0(%rax), %ip0
    movq  8(%rax), %ip1
    movq 16(%rax), %ip2
    movq 24(%rax), %ip3
    movq 32(%rax), %op0
    movq 40(%rax), %op1
    movq 48(%rax), %op2
    movq 56(%rax), %op3
    movq 64(%rax), %bits0
    movq 72(%rax), %bits1
    movq 80(%rax), %bits2
    movq 88(%rax), %bits3
    movq 96(%rax), %dtable
    push %rax      /* argument */
    .cfi_def_cfa_offset 136
    push 104(%rax) /* ilowest */
    .cfi_def_cfa_offset 144
    push 112(%rax) /* oend */
    .cfi_def_cfa_offset 152
    push %olimit   /* olimit space */
    .cfi_def_cfa_offset 160

    subq $24, %rsp
    .cfi_def_cfa_offset 184

LOCAL_LABEL(4X1_compute_olimit):
    /* Computes how many iterations we can do safely
     * %r15, %rax may be clobbered
     * rbx, rdx must be saved
     * op3 & ip0 mustn't be clobbered
     */
    movq %rbx, 0(%rsp)
    movq %rdx, 8(%rsp)

    movq 32(%rsp), %rax /* rax = oend */
    subq %op3,    %rax  /* rax = oend - op3 */

    /* r15 = (oend - op3) / 5 */
    movabsq $-3689348814741910323, %rdx
    mulq %rdx
    movq %rdx, %r15
    shrq $2, %r15

    movq %ip0,     %rax /* rax = ip0 */
    movq 40(%rsp), %rdx /* rdx = ilowest */
    subq %rdx,     %rax /* rax = ip0 - ilowest */
    movq %rax,     %rbx /* rbx = ip0 - ilowest */

    /* rdx = (ip0 - ilowest) / 7 */
    movabsq $2635249153387078803, %rdx
    mulq %rdx
    subq %rdx, %rbx
    shrq %rbx
    addq %rbx, %rdx
    shrq $2, %rdx

    /* r15 = min(%rdx, %r15) */
    cmpq %rdx, %r15
    cmova %rdx, %r15

    /* r15 = r15 * 5 */
    leaq (%r15, %r15, 4), %r15

    /* olimit = op3 + r15 */
    addq %op3, %olimit

    movq 8(%rsp), %rdx
    movq 0(%rsp), %rbx

    /* If (op3 + 20 > olimit) */
    movq %op3, %rax    /* rax = op3 */
    cmpq %rax, %olimit /* op3 == olimit */
    je LOCAL_LABEL(4X1_exit)

    /* If (ip1 < ip0) go to exit */
    cmpq %ip0, %ip1
    jb LOCAL_LABEL(4X1_exit)

    /* If (ip2 < ip1) go to exit */
    cmpq %ip1, %ip2
    jb LOCAL_LABEL(4X1_exit)

    /* If (ip3 < ip2) go to exit */
    cmpq %ip2, %ip3
    jb LOCAL_LABEL(4X1_exit)

/* Reads top 11 bits from bits[n]
 * Loads dt[bits[n]] into var[n]
 */
#define GET_NEXT_DELT(n)                \
    movq $53, %var##n;                  \
    shrxq %var##n, %bits##n, %var##n;   \
    movzwl (%dtable,%var##n,2),%vard##n

/* var[n] must contain the DTable entry computed with GET_NEXT_DELT
 * Moves var[n] to %rax
 * bits[n] <<= var[n] & 63
 * op[n][idx] = %rax >> 8
 * %ah is a way to access bits [8, 16) of %rax
 */
#define DECODE_FROM_DELT(n, idx)       \
    movq %var##n, %rax;                \
    shlxq %var##n, %bits##n, %bits##n; \
    movb %ah, idx(%op##n)

/* Assumes GET_NEXT_DELT has been called.
 * Calls DECODE_FROM_DELT then GET_NEXT_DELT
 */
#define DECODE_AND_GET_NEXT(n, idx) \
    DECODE_FROM_DELT(n, idx);       \
    GET_NEXT_DELT(n)                \

/* // ctz & nbBytes is stored in bits[n]
 * // nbBits is stored in %rax
 * ctz  = CTZ[bits[n]]
 * nbBits  = ctz & 7
 * nbBytes = ctz >> 3
 * op[n]  += 5
 * ip[n]  -= nbBytes
 * // Note: x86-64 is little-endian ==> no bswap
 * bits[n] = MEM_readST(ip[n]) | 1
 * bits[n] <<= nbBits
 */
#define RELOAD_BITS(n)             \
    bsfq %bits##n, %bits##n;       \
    movq %bits##n, %rax;           \
    andq $7, %rax;                 \
    shrq $3, %bits##n;             \
    leaq 5(%op##n), %op##n;        \
    subq %bits##n, %ip##n;         \
    movq (%ip##n), %bits##n;       \
    orq $1, %bits##n;              \
    shlx %rax, %bits##n, %bits##n

    /* Store clobbered variables on the stack */
    movq %olimit, 24(%rsp)
    movq %ip1, 0(%rsp)
    movq %ip2, 8(%rsp)
    movq %ip3, 16(%rsp)

    /* Call GET_NEXT_DELT for each stream */
    FOR_EACH_STREAM(GET_NEXT_DELT)

    .p2align 6

LOCAL_LABEL(4X1_loop_body):
    /* Decode 5 symbols in each of the 4 streams (20 total)
     * Must have called GET_NEXT_DELT for each stream
     */
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 0)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 1)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 2)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 3)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_FROM_DELT, 4)

    /* Load ip[1,2,3] from stack (var[] aliases them)
     * ip[] is needed for RELOAD_BITS
     * Each will be stored back to the stack after RELOAD
     */
    movq 0(%rsp), %ip1
    movq 8(%rsp), %ip2
    movq 16(%rsp), %ip3

    /* Reload each stream & fetch the next table entry
     * to prepare for the next iteration
     */
    RELOAD_BITS(0)
    GET_NEXT_DELT(0)

    RELOAD_BITS(1)
    movq %ip1, 0(%rsp)
    GET_NEXT_DELT(1)

    RELOAD_BITS(2)
    movq %ip2, 8(%rsp)
    GET_NEXT_DELT(2)

    RELOAD_BITS(3)
    movq %ip3, 16(%rsp)
    GET_NEXT_DELT(3)

    /* If op3 < olimit: continue the loop */
    cmp %op3, 24(%rsp)
    ja LOCAL_LABEL(4X1_loop_body)

    /* Reload ip[1,2,3] from stack */
    movq 0(%rsp), %ip1
    movq 8(%rsp), %ip2
    movq 16(%rsp), %ip3

    /* Re-compute olimit */
    jmp LOCAL_LABEL(4X1_compute_olimit)

#undef GET_NEXT_DELT
#undef DECODE_FROM_DELT
#undef DECODE
#undef RELOAD_BITS
LOCAL_LABEL(4X1_exit):
    addq $24, %rsp
    .cfi_def_cfa_offset 160

    /* Restore stack (oend & olimit) */
    pop %rax /* olimit */
    .cfi_def_cfa_offset 152
    pop %rax /* oend */
    .cfi_def_cfa_offset 144
    pop %rax /* ilowest */
    .cfi_def_cfa_offset 136
    pop %rax /* arg */
    .cfi_def_cfa_offset 128

    /* Save ip / op / bits */
    movq %ip0,  0(%rax)
    movq %ip1,  8(%rax)
    movq %ip2, 16(%rax)
    movq %ip3, 24(%rax)
    movq %op0, 32(%rax)
    movq %op1, 40(%rax)
    movq %op2, 48(%rax)
    movq %op3, 56(%rax)
    movq %bits0, 64(%rax)
    movq %bits1, 72(%rax)
    movq %bits2, 80(%rax)
    movq %bits3, 88(%rax)

    /* Restore registers */
    pop %r15
    .cfi_restore r15
    .cfi_def_cfa_offset 120
    pop %r14
    .cfi_restore r14
    .cfi_def_cfa_offset 112
    pop %r13
    .cfi_restore r13
    .cfi_def_cfa_offset 104
    pop %r12
    .cfi_restore r12
    .cfi_def_cfa_offset 96
    pop %r11
    .cfi_restore r11
    .cfi_def_cfa_offset 88
    pop %r10
    .cfi_restore r10
    .cfi_def_cfa_offset 80
    pop %r9
    .cfi_restore r9
    .cfi_def_cfa_offset 72
    pop %r8
    .cfi_restore r8
    .cfi_def_cfa_offset 64
    pop %rdi
    .cfi_restore rdi
    .cfi_def_cfa_offset 56
    pop %rsi
    .cfi_restore rsi
    .cfi_def_cfa_offset 48
    pop %rbp
    .cfi_restore rbp
    .cfi_def_cfa_offset 40
    pop %rdx
    .cfi_restore rdx
    .cfi_def_cfa_offset 32
    pop %rcx
    .cfi_restore rcx
    .cfi_def_cfa_offset 24
    pop %rbx
    .cfi_restore rbx
    .cfi_def_cfa_offset 16
    pop %rax
    .cfi_restore rax
    .cfi_def_cfa_offset 8
    ret
    .cfi_endproc

_HUF_decompress4X2_usingDTable_internal_fast_asm_loop:
HUF_decompress4X2_usingDTable_internal_fast_asm_loop:
    ZSTD_CET_ENDBRANCH
    .cfi_startproc
    .cfi_def_cfa_offset 8
    .cfi_offset %rip, -8
    /* Save all registers - even if they are callee saved for simplicity. */
    push %rax
    .cfi_def_cfa_offset 16
    .cfi_offset rax, -16
    push %rbx
    .cfi_def_cfa_offset 24
    .cfi_offset rbx, -24
    push %rcx
    .cfi_def_cfa_offset 32
    .cfi_offset rcx, -32
    push %rdx
    .cfi_def_cfa_offset 40
    .cfi_offset rdx, -40
    push %rbp
    .cfi_def_cfa_offset 48
    .cfi_offset rbp, -48
    push %rsi
    .cfi_def_cfa_offset 56
    .cfi_offset rsi, -56
    push %rdi
    .cfi_def_cfa_offset 64
    .cfi_offset rdi, -64
    push %r8
    .cfi_def_cfa_offset 72
    .cfi_offset r8, -72
    push %r9
    .cfi_def_cfa_offset 80
    .cfi_offset r9, -80
    push %r10
    .cfi_def_cfa_offset 88
    .cfi_offset r10, -88
    push %r11
    .cfi_def_cfa_offset 96
    .cfi_offset r11, -96
    push %r12
    .cfi_def_cfa_offset 104
    .cfi_offset r12, -104
    push %r13
    .cfi_def_cfa_offset 112
    .cfi_offset r13, -112
    push %r14
    .cfi_def_cfa_offset 120
    .cfi_offset r14, -120
    push %r15
    .cfi_def_cfa_offset 128
    .cfi_offset r15, -128

    /* Read HUF_DecompressAsmArgs* args from %rax */
#if defined(_WIN32)
    movq %rcx, %rax
#else
    movq %rdi, %rax
#endif
    movq  0(%rax), %ip0
    movq  8(%rax), %ip1
    movq 16(%rax), %ip2
    movq 24(%rax), %ip3
    movq 32(%rax), %op0
    movq 40(%rax), %op1
    movq 48(%rax), %op2
    movq 56(%rax), %op3
    movq 64(%rax), %bits0
    movq 72(%rax), %bits1
    movq 80(%rax), %bits2
    movq 88(%rax), %bits3
    movq 96(%rax), %dtable
    push %rax      /* argument */
    .cfi_def_cfa_offset 136
    push %rax      /* olimit */
    .cfi_def_cfa_offset 144
    push 104(%rax) /* ilowest */
    .cfi_def_cfa_offset 152

    movq 112(%rax), %rax
    push %rax /* oend3 */
    .cfi_def_cfa_offset 160

    movq %op3, %rax
    push %rax /* oend2 */
    .cfi_def_cfa_offset 168

    movq %op2, %rax
    push %rax /* oend1 */
    .cfi_def_cfa_offset 176

    movq %op1, %rax
    push %rax /* oend0 */
    .cfi_def_cfa_offset 184

    /* Scratch space */
    subq $8, %rsp
    .cfi_def_cfa_offset 192

LOCAL_LABEL(4X2_compute_olimit):
    /* Computes how many iterations we can do safely
     * %r15, %rax may be clobbered
     * rdx must be saved
     * op[1,2,3,4] & ip0 mustn't be clobbered
     */
    movq %rdx, 0(%rsp)

    /* We can consume up to 7 input bytes each iteration. */
    movq %ip0,     %rax  /* rax = ip0 */
    movq 40(%rsp), %rdx  /* rdx = ilowest */
    subq %rdx,     %rax  /* rax = ip0 - ilowest */
    movq %rax,    %r15   /* r15 = ip0 - ilowest */

    /* rdx = rax / 7 */
    movabsq $2635249153387078803, %rdx
    mulq %rdx
    subq %rdx, %r15
    shrq %r15
    addq %r15, %rdx
    shrq $2, %rdx

    /* r15 = (ip0 - ilowest) / 7 */
    movq %rdx, %r15

    /* r15 = min(r15, min(oend0 - op0, oend1 - op1, oend2 - op2, oend3 - op3) / 10) */
    movq 8(%rsp),  %rax /* rax = oend0 */
    subq %op0,     %rax /* rax = oend0 - op0 */
    movq 16(%rsp), %rdx /* rdx = oend1 */
    subq %op1,     %rdx /* rdx = oend1 - op1 */

    cmpq  %rax,    %rdx
    cmova %rax,    %rdx /* rdx = min(%rdx, %rax) */

    movq 24(%rsp), %rax /* rax = oend2 */
    subq %op2,     %rax /* rax = oend2 - op2 */

    cmpq  %rax,    %rdx
    cmova %rax,    %rdx /* rdx = min(%rdx, %rax) */

    movq 32(%rsp), %rax /* rax = oend3 */
    subq %op3,     %rax /* rax = oend3 - op3 */

    cmpq  %rax,    %rdx
    cmova %rax,    %rdx /* rdx = min(%rdx, %rax) */

    movabsq $-3689348814741910323, %rax
    mulq %rdx
    shrq $3,       %rdx /* rdx = rdx / 10 */

    /* r15 = min(%rdx, %r15) */
    cmpq  %rdx, %r15
    cmova %rdx, %r15

    /* olimit = op3 + 5 * r15 */
    movq %r15, %rax
    leaq (%op3, %rax, 4), %olimit
    addq %rax, %olimit

    movq 0(%rsp), %rdx

    /* If (op3 + 10 > olimit) */
    movq %op3, %rax    /* rax = op3 */
    cmpq %rax, %olimit /* op3 == olimit */
    je LOCAL_LABEL(4X2_exit)

    /* If (ip1 < ip0) go to exit */
    cmpq %ip0, %ip1
    jb LOCAL_LABEL(4X2_exit)

    /* If (ip2 < ip1) go to exit */
    cmpq %ip1, %ip2
    jb LOCAL_LABEL(4X2_exit)

    /* If (ip3 < ip2) go to exit */
    cmpq %ip2, %ip3
    jb LOCAL_LABEL(4X2_exit)

#define DECODE(n, idx)              \
    movq %bits##n, %rax;            \
    shrq $53, %rax;                 \
    movzwl 0(%dtable,%rax,4),%r8d;  \
    movzbl 2(%dtable,%rax,4),%r15d; \
    movzbl 3(%dtable,%rax,4),%eax;  \
    movw %r8w, (%op##n);            \
    shlxq %r15, %bits##n, %bits##n; \
    addq %rax, %op##n

#define RELOAD_BITS(n)              \
    bsfq %bits##n, %bits##n;        \
    movq %bits##n, %rax;            \
    shrq $3, %bits##n;              \
    andq $7, %rax;                  \
    subq %bits##n, %ip##n;          \
    movq (%ip##n), %bits##n;        \
    orq $1, %bits##n;               \
    shlxq %rax, %bits##n, %bits##n


    movq %olimit, 48(%rsp)

    .p2align 6

LOCAL_LABEL(4X2_loop_body):
    /* We clobber r8, so store it on the stack */
    movq %r8, 0(%rsp)

    /* Decode 5 symbols from each of the 4 streams (20 symbols total). */
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 0)
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 1)
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 2)
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 3)
    FOR_EACH_STREAM_WITH_INDEX(DECODE, 4)

    /* Reload r8 */
    movq 0(%rsp), %r8

    FOR_EACH_STREAM(RELOAD_BITS)

    cmp %op3, 48(%rsp)
    ja LOCAL_LABEL(4X2_loop_body)
    jmp LOCAL_LABEL(4X2_compute_olimit)

#undef DECODE
#undef RELOAD_BITS
LOCAL_LABEL(4X2_exit):
    addq $8, %rsp
    .cfi_def_cfa_offset 184
    /* Restore stack (oend & olimit) */
    pop %rax /* oend0 */
    .cfi_def_cfa_offset 176
    pop %rax /* oend1 */
    .cfi_def_cfa_offset 168
    pop %rax /* oend2 */
    .cfi_def_cfa_offset 160
    pop %rax /* oend3 */
    .cfi_def_cfa_offset 152
    pop %rax /* ilowest */
    .cfi_def_cfa_offset 144
    pop %rax /* olimit */
    .cfi_def_cfa_offset 136
    pop %rax /* arg */
    .cfi_def_cfa_offset 128

    /* Save ip / op / bits */
    movq %ip0,  0(%rax)
    movq %ip1,  8(%rax)
    movq %ip2, 16(%rax)
    movq %ip3, 24(%rax)
    movq %op0, 32(%rax)
    movq %op1, 40(%rax)
    movq %op2, 48(%rax)
    movq %op3, 56(%rax)
    movq %bits0, 64(%rax)
    movq %bits1, 72(%rax)
    movq %bits2, 80(%rax)
    movq %bits3, 88(%rax)

    /* Restore registers */
    pop %r15
    .cfi_restore r15
    .cfi_def_cfa_offset 120
    pop %r14
    .cfi_restore r14
    .cfi_def_cfa_offset 112
    pop %r13
    .cfi_restore r13
    .cfi_def_cfa_offset 104
    pop %r12
    .cfi_restore r12
    .cfi_def_cfa_offset 96
    pop %r11
    .cfi_restore r11
    .cfi_def_cfa_offset 88
    pop %r10
    .cfi_restore r10
    .cfi_def_cfa_offset 80
    pop %r9
    .cfi_restore r9
    .cfi_def_cfa_offset 72
    pop %r8
    .cfi_restore r8
    .cfi_def_cfa_offset 64
    pop %rdi
    .cfi_restore rdi
    .cfi_def_cfa_offset 56
    pop %rsi
    .cfi_restore rsi
    .cfi_def_cfa_offset 48
    pop %rbp
    .cfi_restore rbp
    .cfi_def_cfa_offset 40
    pop %rdx
    .cfi_restore rdx
    .cfi_def_cfa_offset 32
    pop %rcx
    .cfi_restore rcx
    .cfi_def_cfa_offset 24
    pop %rbx
    .cfi_restore rbx
    .cfi_def_cfa_offset 16
    pop %rax
    .cfi_restore rax
    .cfi_def_cfa_offset 8
    ret
    .cfi_endproc

#endif

#if ZSTD_ENABLE_ASM_X86_64_BMI2 && (STATIC_APXF || DYNAMIC_APXF)

/* APX-optimized variant of HUF_decompress4X1_usingDTable_internal_fast_asm_loop.
 *
 * APX features used:
 *   EGPR      – r16=ip1, r17=ip2, r18=ip3, r19=olimit, r20/r21 scratch.
 *               Eliminates the 24-byte stack-spill area that the base loop
 *               needs because ip[1,2,3] and olimit aliased var[].
 *   PUSH2/POP2 – Saves/restores register pairs in a single instruction,
 *               halving the number of push/pop uops in the prologue/epilogue.
 *   NDD        – 3-operand "andq $7, %bits, %rax" in RELOAD_BITS replaces
 *               the mov+and sequence, leaving bits## unchanged for the
 *               following shrq.
 *   NF         – {nf} prefix on shifts/adds in RELOAD_BITS and
 *               compute_olimit that do not feed into a flag-consuming
 *               instruction, reducing flag-write pressure.
 *   CCMP       – The three ip-ordering checks (ip1>=ip0, ip2>=ip1, ip3>=ip2)
 *               are chained into a single cmpq + two ccmpq + one jb,
 *               eliminating two branch instructions.
 *
 * All BMI/BMI2 instructions (shrxq, shlxq) are preserved.
 */

ZSTD_HIDE_ASM_FUNCTION(HUF_decompress4X1_usingDTable_internal_fast_asm_loop_apx)
ZSTD_HIDE_ASM_FUNCTION(_HUF_decompress4X1_usingDTable_internal_fast_asm_loop_apx)
.global HUF_decompress4X1_usingDTable_internal_fast_asm_loop_apx
.global _HUF_decompress4X1_usingDTable_internal_fast_asm_loop_apx

/* Override ip[1,2,3] and olimit to use APX EGPRs.
 * The base macros (op0-op3, ip0, bits0-bits3, dtable, var0-var3, vard0-vard3)
 * remain in effect from the BMI2 block above.
 */
#undef ip1
#undef ip2
#undef ip3
#undef olimit
#define ip1    r16
#define ip2    r17
#define ip3    r18
#define olimit r19

/* GET_NEXT_DELT: reads top 11 bits of bits[n] and loads dt[bits[n]] into var[n].
 * var[n] uses r15/r9/r10/r11 -- no longer aliased with ip[1,2,3] or olimit,
 * so no stack spill is needed in the hot loop. */
#define GET_NEXT_DELT(n)                \
    movq $53, %var##n;                  \
    shrxq %var##n, %bits##n, %var##n;   \
    movzwl (%dtable,%var##n,2),%vard##n

/* DECODE_FROM_DELT: var[n] holds the DTable entry from GET_NEXT_DELT.
 * bits[n] <<= var[n] & 63;  op[n][idx] = bits shifted right by 56. */
#define DECODE_FROM_DELT(n, idx)       \
    movq %var##n, %rax;                \
    shlxq %var##n, %bits##n, %bits##n; \
    movb %ah, idx(%op##n)

#define DECODE_AND_GET_NEXT(n, idx) \
    DECODE_FROM_DELT(n, idx);       \
    GET_NEXT_DELT(n)

/* APX-enhanced RELOAD_BITS:
 *   tzcntq                -- replaces bsfq; produces the same result when src!=0
 *                            (bits##n is always non-zero in the hot loop because
 *                            the loop guard ensures enough input data remains)
 *   andq $7, .., %rax     -- NDD: rax = bits##n & 7, bits##n unchanged (no mov needed)
 *   {nf} shrq $3          -- no flag update; flags not consumed before next cmpq
 *   {nf} orq  $1          -- no flag update
 *   shlxq                 -- BMI2 preserved
 */
#define RELOAD_BITS(n)                  \
    tzcntq %bits##n, %bits##n;          \
    andq $7, %bits##n, %rax;            \
    {nf} shrq $3, %bits##n;             \
    leaq 5(%op##n), %op##n;             \
    subq %bits##n, %ip##n;              \
    movq (%ip##n), %bits##n;            \
    {nf} orq $1, %bits##n;              \
    shlxq %rax, %bits##n, %bits##n

.text
_HUF_decompress4X1_usingDTable_internal_fast_asm_loop_apx:
HUF_decompress4X1_usingDTable_internal_fast_asm_loop_apx:
    ZSTD_CET_ENDBRANCH
    .cfi_startproc
    .cfi_def_cfa_offset 8
    .cfi_offset %rip, -8
    /* Save all registers using PUSH2 (APX: 2 regs per instruction) and PUSH.
     * Stack pointer must be 16-byte aligned before the first PUSH2 instruction.
     * Standard ABI entry leaves RSP congruent to 8 mod 16. A single PUSH corrects this.
     *
     * push2 %r1, %r2 stores r1 at [old_RSP-8] (high) and r2 at [old_RSP-16]
     * (low), then decrements RSP by 16.  For CFI purposes this is identical
     * to two consecutive push instructions.
     *
     * Stack layout after the save sequence (offsets from RSP):
     *  [RSP+0]   rax     [RSP+8]   rbx    -- push2 %rax, %rbx
     *  [RSP+16]  rcx     [RSP+24]  rdx    -- push2 %rcx, %rdx
     *  [RSP+32]  rbp     [RSP+40]  rsi    -- push2 %rbp, %rsi
     *  [RSP+48]  rdi     [RSP+56]  r8     -- push2 %rdi, %r8
     *  [RSP+64]  r9      [RSP+72]  r10    -- push2 %r9,  %r10
     *  [RSP+80]  r11     [RSP+88]  r12    -- push2 %r11, %r12
     *  [RSP+96]  r13     [RSP+104] r14    -- push2 %r13, %r14
     *  EGPRs used by this function:
     *  [RSP+112] r16     [RSP+120] r17    -- push2 %r16, %r17
     *  [RSP+128] r18     [RSP+136] r19    -- push2 %r18, %r19
     *  [RSP+144] r20     [RSP+152] r21    -- push2 %r20, %r21
     *  [RSP+160] r15                      -- push  %r15
     * Total: 168 bytes.  CFA_offset = 8 + 168 = 176 after saves.
     */
    push %r15
    .cfi_def_cfa_offset 16
    .cfi_offset r15, -16
    push2 %rax, %rbx
    .cfi_def_cfa_offset 32
    .cfi_offset rax, -24
    .cfi_offset rbx, -32
    push2 %rcx, %rdx
    .cfi_def_cfa_offset 48
    .cfi_offset rcx, -40
    .cfi_offset rdx, -48
    push2 %rbp, %rsi
    .cfi_def_cfa_offset 64
    .cfi_offset rbp, -56
    .cfi_offset rsi, -64
    push2 %rdi, %r8
    .cfi_def_cfa_offset 80
    .cfi_offset rdi, -72
    .cfi_offset r8, -80
    push2 %r9, %r10
    .cfi_def_cfa_offset 96
    .cfi_offset r9, -88
    .cfi_offset r10, -96
    push2 %r11, %r12
    .cfi_def_cfa_offset 112
    .cfi_offset r11, -104
    .cfi_offset r12, -112
    push2 %r13, %r14
    .cfi_def_cfa_offset 128
    .cfi_offset r13, -120
    .cfi_offset r14, -128
    /* EGPRs r16-r21: saved for ABI compliance.
     * CFI annotations for r16+ are omitted as they are not universally
     * supported by older unwinders. */
    push2 %r16, %r17
    .cfi_def_cfa_offset 144
    push2 %r18, %r19
    .cfi_def_cfa_offset 160
    push2 %r20, %r21
    .cfi_def_cfa_offset 176

    /* Read HUF_DecompressAsmArgs* into %rax */
#if defined(_WIN32)
    movq %rcx, %rax
#else
    movq %rdi, %rax
#endif
    movq  0(%rax), %ip0
    movq  8(%rax), %ip1        /* ip1 = r16 (EGPR) */
    movq 16(%rax), %ip2        /* ip2 = r17 (EGPR) */
    movq 24(%rax), %ip3        /* ip3 = r18 (EGPR) */
    movq 32(%rax), %op0
    movq 40(%rax), %op1
    movq 48(%rax), %op2
    movq 56(%rax), %op3
    movq 64(%rax), %bits0
    movq 72(%rax), %bits1
    movq 80(%rax), %bits2
    movq 88(%rax), %bits3
    movq 96(%rax), %dtable
    push %rax          /* argument pointer */
    .cfi_def_cfa_offset 184
    push 104(%rax)     /* ilowest */
    .cfi_def_cfa_offset 192
    push 112(%rax)     /* oend */
    .cfi_def_cfa_offset 200
    /* No olimit stack slot: olimit lives in r19 (EGPR).
     * No 24-byte spill area: ip1/ip2/ip3 live in r16/r17/r18. */

LOCAL_LABEL(4X1_apx_compute_olimit):
    /* Compute how many 5-symbol iterations we can perform safely.
     * r20/r21 are used as scratch to preserve op1 (rbx) and bits1 (rdx)
     * without a stack store/load round-trip.
     *
     * Stack layout at this point:
     *   0(%rsp)  = oend
     *   8(%rsp)  = ilowest
     *  16(%rsp)  = arg pointer
     */
    movq %rbx, %r20         /* r20 = save op1 */
    movq %rdx, %r21         /* r21 = save bits1 */

    movq 0(%rsp),  %rax     /* rax = oend */
    subq %op3,     %rax     /* rax = oend - op3 */

    /* olimit (r19) = (oend - op3) / 5 */
    movabsq $-3689348814741910323, %rdx
    mulq %rdx
    movq %rdx, %olimit
    {nf} shrq $2, %olimit

    movq %ip0,    %rax      /* rax = ip0 */
    movq 8(%rsp), %rdx      /* rdx = ilowest */
    subq %rdx,    %rax      /* rax = ip0 - ilowest */
    movq %rax,    %rbx      /* rbx = ip0 - ilowest */

    /* rdx = (ip0 - ilowest) / 7 */
    movabsq $2635249153387078803, %rdx
    mulq %rdx
    {nf} subq %rdx, %rbx
    {nf} shrq $1, %rbx
    {nf} addq %rbx, %rdx
    {nf} shrq $2, %rdx

    /* olimit = min(rdx, olimit) */
    cmpq %rdx, %olimit
    cmova %rdx, %olimit

    /* olimit = olimit * 5 */
    leaq (%olimit, %olimit, 4), %olimit

    /* olimit = op3 + olimit */
    addq %op3, %olimit

    movq %r21, %rdx         /* restore bits1 */
    movq %r20, %rbx         /* restore op1 */

    /* Exit if op3 == olimit (no full iterations possible) */
    cmpq %op3, %olimit
    je LOCAL_LABEL(4X1_apx_exit)

    /* Chain ip ordering checks with CCMP (APX feature).
     *
     * cmpq  ip0, ip1           => CF=1 if ip1 < ip0
     * ccmpq {nb} $1, ip1, ip2  => if nb (CF=0, ip1>=ip0): compare ip2 vs ip1;
     *                              else set EFLAGS from dfv=1 (CF=1, keep fail)
     * ccmpq {nb} $1, ip2, ip3  => if nb (CF=0, ip2>=ip1): compare ip3 vs ip2;
     *                              else set EFLAGS from dfv=1 (CF=1, keep fail)
     * jb    exit               => taken whenever CF=1 (any ordering violated)
     *
     * This replaces three separate cmpq/jb pairs with one branch.
     */
    cmpq %ip0, %ip1
    ccmpnbq {dfv=cf} %ip1, %ip2
    ccmpnbq {dfv=cf} %ip2, %ip3
    jb LOCAL_LABEL(4X1_apx_exit)

    /* Pre-fetch the first DTable entry for each stream */
    FOR_EACH_STREAM(GET_NEXT_DELT)

    .p2align 6

LOCAL_LABEL(4X1_apx_loop_body):
    /* Decode 5 symbols in each of the 4 streams (20 symbols total).
     * ip[1,2,3] stay in r16/r17/r18 throughout -- no stack spills needed. */
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 0)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 1)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 2)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_AND_GET_NEXT, 3)
    FOR_EACH_STREAM_WITH_INDEX(DECODE_FROM_DELT, 4)

    /* Reload bit buffers and pre-fetch next DTable entries.
     * After RELOAD_BITS, ip##n holds the updated pointer (EGPR for n=1,2,3). */
    RELOAD_BITS(0)
    GET_NEXT_DELT(0)

    RELOAD_BITS(1)
    GET_NEXT_DELT(1)

    RELOAD_BITS(2)
    GET_NEXT_DELT(2)

    RELOAD_BITS(3)
    GET_NEXT_DELT(3)

    /* Continue if op3 < olimit (olimit in r19, no memory load needed) */
    cmp %op3, %olimit
    ja LOCAL_LABEL(4X1_apx_loop_body)

    /* Recompute olimit for the next chunk */
    jmp LOCAL_LABEL(4X1_apx_compute_olimit)

#undef GET_NEXT_DELT
#undef DECODE_FROM_DELT
#undef DECODE_AND_GET_NEXT
#undef RELOAD_BITS

LOCAL_LABEL(4X1_apx_exit):
    /* Pop the three bookkeeping values pushed before the loop */
    pop %rax        /* oend -- value discarded */
    .cfi_def_cfa_offset 192
    pop %rax        /* ilowest -- value discarded */
    .cfi_def_cfa_offset 184
    pop %rax        /* argument pointer */
    .cfi_def_cfa_offset 176

    /* Write back ip / op / bits to HUF_DecompressFastArgs via %rax */
    movq %ip0,   0(%rax)
    movq %ip1,   8(%rax)    /* ip1 = r16 */
    movq %ip2,  16(%rax)    /* ip2 = r17 */
    movq %ip3,  24(%rax)    /* ip3 = r18 */
    movq %op0,  32(%rax)
    movq %op1,  40(%rax)
    movq %op2,  48(%rax)
    movq %op3,  56(%rax)
    movq %bits0, 64(%rax)
    movq %bits1, 72(%rax)
    movq %bits2, 80(%rax)
    movq %bits3, 88(%rax)

    /* Restore registers in reverse push order using POP2 */
    pop2 %r21, %r20
    .cfi_def_cfa_offset 160
    pop2 %r19, %r18
    .cfi_def_cfa_offset 144
    pop2 %r17, %r16
    .cfi_def_cfa_offset 128
    pop2 %r14, %r13
    .cfi_restore r13
    .cfi_restore r14
    .cfi_def_cfa_offset 112
    pop2 %r12, %r11
    .cfi_restore r11
    .cfi_restore r12
    .cfi_def_cfa_offset 96
    pop2 %r10, %r9
    .cfi_restore r9
    .cfi_restore r10
    .cfi_def_cfa_offset 80
    pop2 %r8, %rdi
    .cfi_restore rdi
    .cfi_restore r8
    .cfi_def_cfa_offset 64
    pop2 %rsi, %rbp
    .cfi_restore rbp
    .cfi_restore rsi
    .cfi_def_cfa_offset 48
    pop2 %rdx, %rcx
    .cfi_restore rcx
    .cfi_restore rdx
    .cfi_def_cfa_offset 32
    pop2 %rbx, %rax
    .cfi_restore rax
    .cfi_restore rbx
    .cfi_def_cfa_offset 16
    pop %r15
    .cfi_restore r15
    .cfi_def_cfa_offset 8
    ret
    .cfi_endproc

/* Restore the original ip1/ip2/ip3/olimit register aliases so subsequent
 * includes of this file see a consistent state. */
#undef ip1
#undef ip2
#undef ip3
#undef olimit
#define ip1    r9
#define ip2    r10
#define ip3    r11
#define olimit r15

#endif /* ZSTD_ENABLE_ASM_X86_64_BMI2 && (STATIC_APXF || DYNAMIC_APXF) */
