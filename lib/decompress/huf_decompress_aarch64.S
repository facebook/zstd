/**********************************************************************
  Copyright(c) 2022 Arm Corporation All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Arm Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************/

#include "../common/portability_macros.h"

#if defined(__ELF__) && defined(__GNUC__)
.section .note.GNU-stack,"",%progbits
#endif

#if ZSTD_ENABLE_ASM_AARCH64
ZSTD_HIDE_ASM_FUNCTION(HUF_decompress4X1_usingDTable_internal_asm_loop)
ZSTD_HIDE_ASM_FUNCTION(_HUF_decompress4X1_usingDTable_internal_asm_loop)

.global HUF_decompress4X1_usingDTable_internal_asm_loop
.global _HUF_decompress4X1_usingDTable_internal_asm_loop

.arch armv8-a

.text

.p2align 3,,7
/*
 * ArmV8-A calling conventions:
 * (https://developer.arm.com/documentation/den0024/a/The-ABI-for-ARM-64-bit-Architecture/Register-use-in-the-AArch64-Procedure-Call-Standard/Parameters-in-general-purpose-registers)
 * x30 (LR): Procedure link register, used to return from subroutines.
 * x29 (FP): Frame pointer.
 * x19 to x29: Callee-saved.
 * x18 (PR): Platform register. Used for some operating-system-specific special purpose, or an additional caller-saved register.
 * x16 (IP0) and x17 (IP1): Intra-Procedure-call scratch registers.
 * x9 to x15: Local variables, caller saved.
 * x8 (XR): Indirect return value address.
 * x0 to x7: Argument values passed to and results returned from a subroutine.
 */

#define coef7  x18
#define coef5  x17

#define oend   x16
#define ilimit x15
#define olimit x14

#define ip0    x13
#define ip1    x12
#define ip2    x11
#define ip3    x10

#define op0    x9
#define op1    x8
#define op2    x7
#define op3    x6

#define bits0  x5
#define bits1  x4
#define bits2  x3
#define bits3  x2

#define dtable x1

/* tmp variables registers: x19 - x24 */
#define iloop  x29
#define v0x    x20
#define v0w    w20
#define v1x    x21
#define v1w    w21
#define v2x    x22
#define v2w    w22
#define v3x    x23
#define v3w    w23
#define v4x    x19
#define v4w    w19
#define v5x    x24
#define v5w    w24

/* c prototype: https://www.godbolt.org/z/nTY38f9hY */
_HUF_decompress4X1_usingDTable_internal_asm_loop:
HUF_decompress4X1_usingDTable_internal_asm_loop:
        stp     x29, x30, [sp, -64]!
        mov     x29, sp
        mov     coef7, 37450
        mov     coef5, 52429
        ldp     ip0, ip1, [x0]
        ldp     op2, op3, [x0, 48]
        ldp     ilimit, oend, [x0, 104]
        ldp     ip2, ip3, [x0, 16]

        sub     iloop, ip0, ilimit
        sub     olimit, oend, op3
        stp     v4x, v5x, [sp, 16]
        mul     iloop, iloop, coef7
        mul     olimit, olimit, coef5
        ldp     op0, op1, [x0, 32]
        asr     iloop, iloop, 18
        asr     olimit, olimit, 18

        cmp     olimit, iloop
        csel    olimit, olimit, iloop, ls
        ldp     bits0, bits1, [x0, 64]
        cmp     ip0, ip1
        ccmp    ip1, ip2, 2, ls
        cset    v5w, ls
        cmp     olimit, 3
        ccmp    ip2, ip3, 2, hi
        cset    v4w, ls
        ldp     bits2, bits3, [x0, 80]
        tst     v5w, v4w
        ldr     dtable, [x0, 96]
        beq     .exit_4x1

        stp     v2x, v3x, [sp, 48]
        lsr     v2x, bits2, 53
        lsr     v3x, bits3, 53
        stp     v0x, v1x, [sp, 32]
        lsr     v0x, bits0, 53
        lsr     v1x, bits1, 53
        ldrh    v2w, [dtable, v2x, lsl 1]
        ldrh    v3w, [dtable, v3x, lsl 1]
        ldrh    v0w, [dtable, v0x, lsl 1]
        ldrh    v1w, [dtable, v1x, lsl 1]

.update_4x1_olimit:
        add     olimit, olimit, olimit, lsl 2
        add     olimit, op3, olimit

.decode_4x1_loop:
#if defined(__APPLE__) && (__APPLE__)
/* on Apple platforms '%%' is used as seperator instead of ';'
 * on other Linux systems.
 */
#define DECODE_STREAM(idx)                  \
        lsl     bits0, bits0, v0x%%         \
        ubfx    v0x, v0x, 8, 8%%            \
        lsr     v4x, bits0, 53%%            \
        lsl     bits1, bits1, v1x%%         \
        strb    v0w, [op0, -##idx]%%        \
        lsr     v5x, bits1, 53%%            \
        ldrh    v0w, [dtable, v4x, lsl 1]%% \
        ubfx    v1x, v1x, 8, 8%%            \
        lsl     bits2, bits2, v2x%%         \
        strb    v1w, [op1, -##idx]%%        \
        lsr     v4x, bits2, 53%%            \
        ldrh    v1w, [dtable, v5x, lsl 1]%% \
        ubfx    v2x, v2x, 8, 8%%            \
        lsl     bits3, bits3, v3x%%         \
        strb    v2w, [op2, -##idx]%%        \
        ubfx    v3x, v3x, 8, 8%%            \
        lsr     v5x, bits3, 53%%            \
        ldrh    v2w, [dtable, v4x, lsl 1]%% \
        strb    v3w, [op3, -##idx]%%        \
        ldrh    v3w, [dtable, v5x, lsl 1]

#define PROCESS_EACH_STREAM(FN) \
        FN(5)%%                 \
        FN(4)%%                 \
        FN(3)%%                 \
        FN(2)
#else
#define DECODE_STREAM(idx)                  \
        lsl     bits0, bits0, v0x;          \
        ubfx    v0x, v0x, 8, 8;             \
        lsr     v4x, bits0, 53;             \
        lsl     bits1, bits1, v1x;          \
        strb    v0w, [op0, -##idx];         \
        lsr     v5x, bits1, 53;             \
        ldrh    v0w, [dtable, v4x, lsl 1];  \
        ubfx    v1x, v1x, 8, 8;             \
        lsl     bits2, bits2, v2x;          \
        strb    v1w, [op1, -##idx];         \
        lsr     v4x, bits2, 53;             \
        ldrh    v1w, [dtable, v5x, lsl 1];  \
        ubfx    v2x, v2x, 8, 8;             \
        lsl     bits3, bits3, v3x;          \
        strb    v2w, [op2, -##idx];         \
        ubfx    v3x, v3x, 8, 8;             \
        lsr     v5x, bits3, 53;             \
        ldrh    v2w, [dtable, v4x, lsl 1];  \
        strb    v3w, [op3, -##idx];         \
        ldrh    v3w, [dtable, v5x, lsl 1]

#define PROCESS_EACH_STREAM(FN) \
        FN(5);                  \
        FN(4);                  \
        FN(3);                  \
        FN(2)
#endif

        add     op3, op3, 5
        add     op0, op0, 5
        add     op1, op1, 5
        add     op2, op2, 5

        PROCESS_EACH_STREAM(DECODE_STREAM)

        // DECODE_FROM_DELT and PREPARE_NEXT_ITER
        lsl     bits0, bits0, v0x
        ubfx    v4x, v0x, 8, 8
        rbit    bits0, bits0
        ubfx    v5x, v1x, 8, 8
        lsl     bits1, bits1, v1x
        strb    v4w, [op0, -1]

        clz     bits0, bits0
        rbit    bits1, bits1
        ubfx    v4x, v2x, 8, 8
        lsl     bits2, bits2, v2x
        sub     ip0, ip0, bits0, lsr 3
        clz     bits1, bits1
        rbit    bits2, bits2
        strb    v5w, [op1, -1]

        lsl     bits3, bits3, v3x
        ubfx    v5x, v3x, 8, 8
        sub     ip1, ip1, bits1, lsr 3
        rbit    bits3, bits3
        clz     bits2, bits2
        strb    v4w, [op2, -1]

        clz     bits3, bits3
        sub     ip2, ip2, bits2, lsr 3
        strb    v5w, [op3, -1]
        sub     ip3, ip3, bits3, lsr 3
        ubfx    v0x, bits0, 0, 3
        ldr     bits0, [ip0]
        ubfx    v1x, bits1, 0, 3
        ubfx    v2x, bits2, 0, 3
        ldr     bits1, [ip1]

        ubfx    v3x, bits3, 0, 3
        ldr     bits2, [ip2]
        orr     bits0, bits0, 1
        ldr     bits3, [ip3]
        lsl     bits0, bits0, v0x
        orr     bits1, bits1, 1
        lsr     v4x, bits0, 53
        lsl     bits1, bits1, v1x
        ldrh    v0w, [dtable, v4x, lsl 1]

        orr     bits2, bits2, 1
        lsr     v5x, bits1, 53
        lsl     bits2, bits2, v2x
        ldrh    v1w, [dtable, v5x, lsl 1]
        orr     bits3, bits3, 1
        lsr     v4x, bits2, 53
        lsl     bits3, bits3, v3x
        ldrh    v2w, [dtable, v4x, lsl 1]
        lsr     v5x, bits3, 53

        cmp     olimit, op3
        ldrh    v3w, [dtable, v5x, lsl 1]
        bhi     .decode_4x1_loop

        sub     olimit, oend, op3
        sub     iloop, ip0, ilimit
        mul     olimit, olimit, coef5
        mul     iloop, iloop, coef7
        asr     olimit, olimit, 18
        asr     iloop, iloop, 18

        cmp     olimit, iloop
        csel    olimit, olimit, iloop, ls
        cmp     ip0, ip1
        ccmp    ip1, ip2, 2, ls
        cset    v5w, ls
        cmp     olimit, 3
        ccmp    ip2, ip3, 2, hi
        cset    v4w, ls
        tst     v5w, v4w
        bne     .update_4x1_olimit

        ldp     v2x, v3x, [sp, 48]
        ldp     v0x, v1x, [sp, 32]

.exit_4x1:
        stp     ip0, ip1, [x0]
        stp     ip2, ip3, [x0, 16]
        stp     op0, op1, [x0, 32]
        stp     op2, op3, [x0, 48]
        stp     bits0, bits1, [x0, 64]
        stp     bits2, bits3, [x0, 80]
        ldp     v4x, v5x, [sp, 16]
        ldp     x29, x30, [sp], 64
        ret

#undef DECODE_STREAM
#undef PROCESS_EACH_STREAM
#endif
