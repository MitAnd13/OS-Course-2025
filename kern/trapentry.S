/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

#include <kern/picirq.h>

#ifdef CONFIG_KSPACE

.globl clock_thdlr
.type clock_thdlr, @function
clock_thdlr:
    call save_trapframe_trap
    # Set trap code for trapframe
    movl $(IRQ_OFFSET + IRQ_CLOCK), 136(%rsp)
    call trap
    jmp .

.globl timer_thdlr
.type timer_thdlr, @function
timer_thdlr:
    call save_trapframe_trap
    # Set trap code for trapframe
    movl $(IRQ_OFFSET + IRQ_TIMER), 136(%rsp)
    call trap
    jmp .

#else

# TRAPHANDLER defines a globally-visible function for handling a trap.
# It pushes a trap number onto the stack, then jumps to _alltraps.
# Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
#
# You shouldn't call a TRAPHANDLER function from C, but you may
# need to _declare_ one in C (for instance, to get a function pointer
# during IDT setup).  You can declare the function with
#   void NAME();
# where NAME is the argument passed to TRAPHANDLER.

#define TRAPHANDLER(name, num) \
.globl name;                   \
.type name, @function;         \
.align 2;                      \
name:                          \
    pushq $(num);              \
    jmp _alltraps

# Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
# It pushes a 0 in place of the error code, so the trap frame has the same
# format in either case.

#define TRAPHANDLER_NOEC(name, num) \
.globl name;                        \
.type name, @function;              \
.align 2;                           \
name:                               \
    pushq $0;                       \
    pushq $(num);                   \
    jmp _alltraps

.text

.globl _alltraps
.type _alltraps, @function;
.align 2
_alltraps:
    # LAB 8: Your code here
    # Complete `struct Trapframe' on stack
    # NOTE: Members after tf_paddind4 already on stack.
    #       For struct PushRegs you can use PUSHA macro.

    subq $16, %rsp
    PUSHA

    # LAB 8: Your code here
    # Invoke `trap' with a pointer to struct trap as an argument.
    movw $GD_KD, 120(%rsp)
    movw $GD_KD, 128(%rsp)

    movw $GD_KD, %ax
    movw %ax, %ds
    movw %ax, %es

    movq %rsp, %rdi
    call trap

    jmp .

# LAB 8: Your code here
# Use TRAPHANDLER or TRAPHANDLER_NOEC to setup
# all trap handlers' entry points
# (Look up their list in inc/trap.h)

# 0–7: без кода ошибки
TRAPHANDLER_NOEC(th_divide,  T_DIVIDE)   # 0: divide error
TRAPHANDLER_NOEC(th_debug,   T_DEBUG)    # 1: debug
TRAPHANDLER_NOEC(th_nmi,     T_NMI)      # 2: NMI
TRAPHANDLER_NOEC(th_brkpt,   T_BRKPT)    # 3: breakpoint
TRAPHANDLER_NOEC(th_oflow,   T_OFLOW)    # 4: overflow
TRAPHANDLER_NOEC(th_bound,   T_BOUND)    # 5: bounds
TRAPHANDLER_NOEC(th_illop,   T_ILLOP)    # 6: illegal opcode
TRAPHANDLER_NOEC(th_device,  T_DEVICE)   # 7: device not available

TRAPHANDLER(th_dblflt,  T_DBLFLT)        # 8: double fault

TRAPHANDLER(th_tss,     T_TSS)           # 10: invalid TSS
TRAPHANDLER(th_segnp,   T_SEGNP)         # 11: segment not present
TRAPHANDLER(th_stack,   T_STACK)         # 12: stack fault
TRAPHANDLER(th_gpflt,   T_GPFLT)         # 13: general protection
TRAPHANDLER(th_pgflt,   T_PGFLT)         # 14: page fault

TRAPHANDLER_NOEC(th_fperr,   T_FPERR)    # 16: floating point error
TRAPHANDLER(th_align,        T_ALIGN)    # 17: alignment check (имеет errcode)
TRAPHANDLER_NOEC(th_mchk,    T_MCHK)     # 18: machine check
TRAPHANDLER_NOEC(th_simderr, T_SIMDERR)  # 19: SIMD FP error

TRAPHANDLER_NOEC(th_syscall, T_SYSCALL)  # 48: system call
TRAPHANDLER_NOEC(trap_syscall, T_SYSCALL)

TRAPHANDLER_NOEC(trap_irq_timer,   IRQ_OFFSET + IRQ_TIMER)
TRAPHANDLER_NOEC(trap_irq_clock,   IRQ_OFFSET + IRQ_CLOCK)
TRAPHANDLER_NOEC(trap_irq_spurious, IRQ_OFFSET + IRQ_SPURIOUS)
TRAPHANDLER_NOEC(trap_irq_kbd,     IRQ_OFFSET + IRQ_KBD)
TRAPHANDLER_NOEC(trap_irq_serial,  IRQ_OFFSET + IRQ_SERIAL)
TRAPHANDLER_NOEC(trap_irq_ide,     IRQ_OFFSET + IRQ_IDE)
TRAPHANDLER_NOEC(trap_irq_error,   IRQ_OFFSET + IRQ_ERROR)

#endif
