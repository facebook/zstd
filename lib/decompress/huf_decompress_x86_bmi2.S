.global HUF_decompress4X1_usingDTable_internal_bmi2_asm_loop
.global HUF_decompress4X2_usingDTable_internal_bmi2_asm_loop
.text


#define op0	rsi
#define op1	rbx
#define op2	rcx
#define op3	rdi

#define ip0	r8
#define ip1	r9
#define ip2	r10
#define ip3	r11

#define ipd0	r8d
#define ipd1	r9d
#define ipd2	r10d
#define ipd3	r11d

#define bits0	rbp
#define bits1	rdx
#define bits2	r12
#define bits3	r13
#define dtable	r14
#define olimit	r15

#define kUseStack 1

HUF_decompress4X1_usingDTable_internal_bmi2_asm_loop:
	push %rax
	push %rbx
	push %rcx
	push %rdx
	push %rbp
	push %rsi
	# push %rdi
	push %r8
	push %r9
	# push %r10
	# push %r11
	push %r12
	push %r13
	push %r14
	push %r15

	movq %rdi, %rax
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
	push %rax      # argument
	push 104(%rax) # ilimit
	push 112(%rax) # oend

.L_4X1_compute_olimit:
	# Computes how many iterations we can do savely
	# %r15, %rax may be clobbered
	# rbx, rdx must be saved
	# op3 & ip0 mustn't be clobbered
	movq %rbx, -8(%rsp)
	movq %rdx, -16(%rsp)

	movq 0(%rsp), %rax # rax = oend
	subq %op3,    %rax # rax = oend - op3

	# r15 = (oend - op3) / 5
	movabsq $-3689348814741910323, %rdx
	mulq %rdx
	movq %rdx, %r15
	shrq $2, %r15

	movq %ip0,    %rax # rax = ip0
	movq 8(%rsp), %rdx # rdx = ilimit
	subq %rdx,    %rax # rax = ip0 - ilimit
	movq %rax,    %rbx # rbx = ip0 - ilimit

	# rdx = (ip0 - ilimit) / 7
	movabsq $2635249153387078803, %rdx
	mulq %rdx
	subq %rdx, %rbx
	shrq %rbx
	addq %rbx, %rdx
	shrq $2, %rdx

	# r15 = min(%rdx, %r15)
	cmpq %rdx, %r15
	cmova %rdx, %r15

	# r15 = r15 * 5
	leaq (%r15, %r15, 4), %r15

	# olimit = op3 + r15
	addq %op3, %olimit

	movq -16(%rsp), %rdx
	movq -8(%rsp), %rbx

	# - if (op3 + 20 > olimit)
	movq %op3, %rax # rax = op3
	addq $20,  %rax # rax = op3 + 20
	cmpq %rax, %olimit # op3 + 20 > olimit
	jb .L_4X1_exit

	# - if (ip1 < ip0) go to exit
	cmpq %ip0, %ip1
	jb .L_4X1_exit

	# - if (ip2 < ip1) go to exit
	cmpq %ip1, %ip2
	jb .L_4X1_exit

	# - if (ip3 < ip2) go to exit
	cmpq %ip2, %ip3
	jb .L_4X1_exit

	.p2align 6


#if kUseStack

#define IF_NOT_4_0(...) __VA_ARGS__
#define IF_NOT_4_1(...) __VA_ARGS__
#define IF_NOT_4_2(...) __VA_ARGS__
#define IF_NOT_4_3(...) __VA_ARGS__
#define IF_NOT_4_4(...)
#define IF_NOT_4_(idx, ...) IF_NOT_4_##idx(__VA_ARGS__)
#define IF_NOT_4(idx, ...) IF_NOT_4_(idx, __VA_ARGS__)

#define LOAD(n) \
	movq $53, %ip##n; \
	shrxq %ip##n, %bits##n, %ip##n; \
	movzwl (%dtable,%ip##n,2),%ipd##n; \

#define USE(n, idx) \
	movq %ip##n, %rax; \
	shlxq %ip##n, %bits##n, %bits##n; \
	movb %ah, idx(%op##n); \

#define DECODE(n, idx) \
	USE(n, idx) \
	IF_NOT_4(idx, LOAD(n))

#else
#define DECODE(n, idx) \
	movq %bits##n, %rax;            \
	shrq $53, %rax;                 \
	movzwl (%dtable,%rax,2),%eax;   \
	shlxq %rax, %bits##n, %bits##n; \
	movb %ah, idx(%op##n)
#endif

#define RELOAD(n)                \
	bsfq %bits##n, %bits##n; \
	movq %bits##n, %rax;     \
	andq $7, %rax;           \
	shrq $3, %bits##n;       \
	leaq 5(%op##n), %op##n;  \
	subq %bits##n, %ip##n;   \
	movq (%ip##n), %bits##n; \
	orq $1, %bits##n;        \
	shlx %rax, %bits##n, %bits##n;


#if kUseStack
	movq %ip0, -8(%rsp)
	movq %ip1, -16(%rsp)
	movq %ip2, -24(%rsp)
	movq %ip3, -32(%rsp)
	LOAD(0)
	LOAD(1)
	LOAD(2)
	LOAD(3)
#endif

.L_4X1_loop_body:
# LLVM-MCA-BEGIN decode-4X1

	DECODE(0, 0)
	DECODE(1, 0)
	DECODE(2, 0)
	DECODE(3, 0)

	DECODE(0, 1)
	DECODE(1, 1)
	DECODE(2, 1)
	DECODE(3, 1)

	DECODE(0, 2)
	DECODE(1, 2)
	DECODE(2, 2)
	DECODE(3, 2)

	DECODE(0, 3)
	DECODE(1, 3)
	DECODE(2, 3)
	DECODE(3, 3)

	DECODE(0, 4)
	DECODE(1, 4)
	DECODE(2, 4)
	DECODE(3, 4)

#if kUseStack
	movq -8(%rsp),  %ip0
	movq -16(%rsp), %ip1
	movq -24(%rsp), %ip2
	movq -32(%rsp), %ip3

	RELOAD(0)
	movq %ip0, -8(%rsp)
	LOAD(0)

	RELOAD(1)
	movq %ip1, -16(%rsp)
	LOAD(1)

	RELOAD(2)
	movq %ip2, -24(%rsp)
	LOAD(2)

	RELOAD(3)
	movq %ip3, -32(%rsp)
	LOAD(3)
#else
	RELOAD(0)
	RELOAD(1)
	RELOAD(2)
	RELOAD(3)

#endif

#undef DECODE
#undef RELOAD

	cmp %op3, %olimit
	ja .L_4X1_loop_body

#if kUseStack
	movq -8(%rsp),  %ip0
	movq -16(%rsp), %ip1
	movq -24(%rsp), %ip2
	movq -32(%rsp), %ip3
#endif

	jmp .L_4X1_compute_olimit
# LLVM-MCA-END
.L_4X1_exit:
	# Restore stack (oend & olimit)
	pop %rax # oend
	pop %rax # ilimit
	pop %rax # arg

	# Save ip / op / bits
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

	# Restore registers
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	# pop %r11
	# pop %r10
	pop %r9
	pop %r8
	# pop %rdi
	pop %rsi
	pop %rbp
	pop %rdx
	pop %rcx
	pop %rbx
	pop %rax
	ret

HUF_decompress4X2_usingDTable_internal_bmi2_asm_loop:
	push %rax
	push %rbx
	push %rcx
	push %rdx
	push %rbp
	push %rsi
	# push %rdi
	push %r8
	push %r9
	# push %r10
	# push %r11
	push %r12
	push %r13
	push %r14
	push %r15

	movq %rdi, %rax
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
	push %rax      # argument
	push 104(%rax) # ilimit

	movq 112(%rax), %rax
	subq $4, %rax
	push %rax # oend3 - 4

	movq %op3, %rax
	subq $4, %rax
	push %rax # oend2 - 4

	movq %op2, %rax
	subq $4, %rax
	push %rax # oend1 - 4

	movq %op1, %rax
	subq $4, %rax
	push %rax # oend0 - 4

.L_4X2_compute_olimit:
	# Computes how many iterations we can do savely
	# %r15, %rax may be clobbered
	# rdx must be saved
	# op* & ip0 mustn't be clobbered

	# We can only actually consume 6 bytes per
	# iteration max, but computing 8 is much simpler.
	movq %ip0,     %r15  # r15 = ip0
	movq 32(%rsp), %rax  # rax = ilimit
	subq %rax,     %r15  # r15 = ip0 - ilimit
	shrq $3,       %r15  # r15 = (ip0 - ilimit) / 8

	movq 0(%rsp), %rax # rax = oend0
	subq %op0,    %rax # rax = oend0 - op0
	shrq $3,      %rax # rax = (oend0 - op0) / 8

	# r15 = min(%rax, %r15)
	cmpq  %rax, %r15
	cmova %rax, %r15

	movq 8(%rsp), %rax # rax = oend1
	subq %op1,    %rax # rax = oend1 - op1
	shrq $3,      %rax # rax = (oend1 - op1) / 8

	# r15 = min(%rax, %r15)
	cmpq  %rax, %r15
	cmova %rax, %r15

	movq 16(%rsp), %rax # rax = oend2
	subq %op2,     %rax # rax = oend2 - op2
	shrq $3,       %rax # rax = (oend2 - op2) / 8

	# r15 = min(%rax, %r15)
	cmpq  %rax, %r15
	cmova %rax, %r15

	movq 24(%rsp), %rax # rax = oend3
	subq %op3,     %rax # rax = oend3 - op3
	shrq $3,       %rax # rax = (oend3 - op3) / 8

	# r15 = min(%rax, %r15)
	cmpq  %rax, %r15
	cmova %rax, %r15

	# olimit = op3 + 4 * r15
	leaq (%op3, %r15, 4), %olimit

	# - if (op3 + 16 > olimit)
	movq %op3, %rax # rax = op3
	addq $16,  %rax # rax = op3 + 16
	cmpq %rax, %olimit # op3 + 16 > olimit
	jb .L_4X2_exit

	# - if (ip1 < ip0) go to exit
	cmpq %ip0, %ip1
	jb .L_4X2_exit

	# - if (ip2 < ip1) go to exit
	cmpq %ip1, %ip2
	jb .L_4X2_exit

	# - if (ip3 < ip2) go to exit
	cmpq %ip2, %ip3
	jb .L_4X2_exit

	.p2align 6

.L_4X2_loop_body:
# LLVM-MCA-BEGIN decode-4X2
	# Index 0
	# movq %bits##n, %rax;
	# shrq $53, %rax;
#define DECODE(n, idx) \
	movq %bits##n, %rax; \
	shrq $52, %rax; \
	movl 0(%dtable,%rax,4),%r8d; \
	movzwl 2(%dtable,%rax,4),%r9d; \
	movzbl 3(%dtable,%rax,4),%eax; \
	movl %r8d, (%op##n); \
	shlxq %r9, %bits##n, %bits##n; \
	addq %rax, %op##n

#define RELOAD(n) \
	bsfq %bits##n, %bits##n; \
	movq %bits##n, %rax; \
	shrq $3, %bits##n; \
	andq $7, %rax; \
	subq %bits##n, %ip##n; \
	movq (%ip##n), %bits##n; \
	orq $1, %bits##n; \
	shlxq %rax, %bits##n, %bits##n; \

	movq %r8, -8(%rsp)
	movq %r9, -16(%rsp)

	DECODE(0, 0)
	DECODE(1, 0)
	DECODE(2, 0)
	DECODE(3, 0)

	DECODE(0, 1)
	DECODE(1, 1)
	DECODE(2, 1)
	DECODE(3, 1)

	DECODE(0, 2)
	DECODE(1, 2)
	DECODE(2, 2)
	DECODE(3, 2)

	DECODE(0, 3)
	DECODE(1, 3)
	DECODE(2, 3)
	DECODE(3, 3)

	movq -16(%rsp), %r9
	movq -8(%rsp), %r8

	RELOAD(0)
	RELOAD(1)
	RELOAD(2)
	RELOAD(3)

#undef DECODE
#undef RELOAD

	cmp %op3, %olimit
	ja .L_4X2_loop_body
	jmp .L_4X2_compute_olimit
# LLVM-MCA-END
.L_4X2_exit:
	# Restore stack (oend & olimit)
	pop %rax # oend0
	pop %rax # oend1
	pop %rax # oend2
	pop %rax # oend3
	pop %rax # ilimit
	pop %rax # arg

	# Save ip / op / bits
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

	# Restore registers
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	# pop %r11
	# pop %r10
	pop %r9
	pop %r8
	# pop %rdi
	pop %rsi
	pop %rbp
	pop %rdx
	pop %rcx
	pop %rbx
	pop %rax
	ret
