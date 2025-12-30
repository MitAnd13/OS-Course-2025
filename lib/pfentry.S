#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

# Page fault upcall entrypoint.

# This is where we ask the kernel to redirect us to whenever we cause
# a page fault in user space (see the call to sys_set_pgfault_handler
# in pgfault.c).
#
# When a page fault actually occurs, the kernel switches our RSP to
# point to the user exception stack if we're not already on the user
# exception stack, and then it pushes a UTrapframe onto our user
# exception stack:
#
#  utf_rsp
#  utf_rflags
#  utf_rip
#  utf_regs.reg_rax
#  ...
#  utf_regs.reg_r15
#  utf_err (error code)
#  utf_fault_va <-- %rsp
#
# If this is a recursive fault, the kernel will reserve for us a
# blank word above the trap-time rsp for scratch work when we unwind
# the recursive call.
#
# We then have call up to the appropriate page fault handler in C
# code, pointed to by the global variable '_pgfault_handler'.

.text
.globl _pgfault_upcall
_pgfault_upcall:
    # Call the C page fault handler.
    movq  %rsp,%rdi # passing the function argument in rdi
    movabs $_handle_vectored_pagefault, %rax
    call *%rax

    # Now the C page fault handler has returned and you must return
    # to the trap time state.
    # Push trap-time %eip onto the trap-time stack.
    #
    # Explanation:
    #   We must prepare the trap-time stack for our eventual return to
    #   re-execute the instruction that faulted.
    #   Unfortunately, we can't return directly from the exception stack:
    #   We can't call 'jmp', since that requires that we load the address
    #   into a register, and all registers must have their trap-time
    #   values after the return.
    #   We can't call 'ret' from the exception stack either, since if we
    #   did, %esp would have the wrong value.
    #   So instead, we push the trap-time %eip onto the *trap-time* stack!
    #   Below we'll switch to that stack and call 'ret', which will
    #   restore %eip to its pre-fault value.
    #
    #   In the case of a recursive fault on the exception stack,
    #   note that the word we're pushing now will fit in the
    #   blank word that the kernel reserved for us.
    #
    # Throughout the remaining code, think carefully about what
    # registers are available for intermediate calculations.  You
    # may find that you have to rearrange your code in non-obvious
    # ways as registers become unavailable as scratch space.

    # LAB 9: Your code here

    # 1 Push trap-time %rip onto the trap-time stack (and update utf_rsp to the adjusted value)
    movq    UTRAP_RSP(%rsp), %rax        # %rax = utf_rsp (trap-time RSP)
    movq    UTRAP_RIP(%rsp), %rcx        # %rcx = utf_rip (trap-time RIP)
    subq    $8, %rax                     # make room for return address on trap-time stack
    movq    %rcx, (%rax)                 # *(utf_rsp - 8) = utf_rip
    movq    %rax, UTRAP_RSP(%rsp)        # utf_rsp = utf_rsp - 8 (save adjusted trap-time RSP)

    # 2 Restore trap-time registers (POPA expects %rsp to point to utf_regs)
    leaq    16(%rsp), %rsp               # skip utf_fault_va + utf_err -> now %rsp points to utf_regs
    POPA                                   # restores r15..rax; after this %rsp points to utf_rip

    # 3 Restore rflags (utf_rflags is 8 bytes above utf_rip)
    pushq   8(%rsp)                      # push utf_rflags
    popfq                                # restore RFLAGS, no further flag-clobbering ops

    # 4 Switch back to the adjusted trap-time stack (utf_rsp is 16 bytes above utf_rip)
    movq    16(%rsp), %rsp               # %rsp = adjusted utf_rsp (trap-time stack with rip already pushed)

    # Return to re-execute the instruction that faulted.
    ret