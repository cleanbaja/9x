#ifndef SYSCALL_H
#define SYSCALL_H

/*
 * syscall.h stolen from ninex's mlibc fork
 * WARNING: DON'T MODIFY HERE, DO IT UPSTREAM FIRST
 */
#include <arch/irqchip.h>

// Numerical constants for syscalls
#define SYS_DEBUG_LOG  0
#define SYS_OPEN       1
#define SYS_VM_MAP     2
#define SYS_VM_UNMAP   3
#define SYS_EXIT       4
#define SYS_ARCHCTL    5
#define SYS_READ       6
#define SYS_WRITE      7
#define SYS_SEEK       8
#define SYS_CLOSE      9
#define SYS_GETPID     10
#define SYS_GETPPID    11
#define SYS_FCNTL      12
#define SYS_IOCTL      13
#define SYS_GETCWD     14

// Arch-Specific constants for SYS_ARCHCTL
#ifdef __x86_64__
#define   ARCHCTL_WRITE_FS   0xB0
#define   ARCHCTL_READ_MSR   0xB1
#define   ARCHCTL_WRITE_MSR  0xB2
#define   ARCHCTL_EMBED_PKEY 0xB3
#endif // __x86_64__

// Macros to ease getting args from a context
#ifdef __x86_64__
#define CALLNUM(regs) regs->rax
#define ARG0(regs)    regs->rdi
#define ARG1(regs)    regs->rsi
#define ARG2(regs)    regs->rdx
#define ARG3(regs)    regs->r10
#define ARG4(regs)    regs->r9
#define ARG5(regs)    regs->r8
#endif // __x86_64__

// Defined in arch/<ARCH>/arch.c
void syscall_archctl(cpu_ctx_t* context);

#endif
