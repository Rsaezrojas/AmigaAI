/*
 * stackswap.h - run the program on a stack that is big enough
 *
 * A TLS handshake in AmiSSL needs far more than the 4KB a Shell hands out by
 * default, and overflowing it does not fail cleanly: it corrupts whatever
 * memory sits below the stack and the machine goes down later, somewhere else.
 *
 * Asking for more through __stack is not enough. The Workbench icon honours
 * its own stack size and vbcc's startup reads the variable, but libnix ignores
 * it unless swapstack.o is linked, and that object overwrites the return
 * address of a program started from the Shell when it swaps back on exit.
 * So the program checks the stack it was given and, if it is too small, moves
 * to one of its own with exec's StackSwap().
 *
 * Header-only, so that every program that needs it (AmigaAI, SSLTest) gets it
 * without touching the build files. Include it from exactly one source file
 * per program.
 *
 * Usage:
 *     static int real_main(int argc, char **argv) __attribute__((noinline));
 *     int main(int argc, char **argv)
 *     {
 *         return run_with_stack(131072, real_main, argc, argv);
 *     }
 *
 * noinline matters: folded into main(), the locals of real_main would be laid
 * out on the small stack before the swap. And real_main must return rather
 * than call exit(), or the swap back and the FreeVec() are skipped.
 */

#ifndef STACKSWAP_H
#define STACKSWAP_H

#include <stdio.h>
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <proto/exec.h>

/* All state is static on purpose. Between the two StackSwap() calls the stack
 * pointer is on the new stack, and nothing addressed relative to the old one
 * (locals, arguments) can be trusted. */
static struct StackSwapStruct stk_swap;
static APTR   stk_mem;
static int  (*stk_fn)(int, char **);
static int    stk_argc;
static char **stk_argv;
static int    stk_rc;

static int run_with_stack(ULONG size, int (*fn)(int, char **),
                          int argc, char **argv)
{
    struct Task *me = FindTask(NULL);

    if ((ULONG)me->tc_SPUpper - (ULONG)me->tc_SPLower >= size)
        return fn(argc, argv);

    stk_mem = AllocVec(size, MEMF_PUBLIC);
    if (!stk_mem) {
        printf("Not enough memory for a %lu byte stack\n", (unsigned long)size);
        return 20;
    }
    stk_swap.stk_Lower   = stk_mem;
    stk_swap.stk_Upper   = (ULONG)stk_mem + size;
    stk_swap.stk_Pointer = (APTR)stk_swap.stk_Upper;
    stk_fn   = fn;
    stk_argc = argc;
    stk_argv = argv;

    StackSwap(&stk_swap);
    stk_rc = stk_fn(stk_argc, stk_argv);
    StackSwap(&stk_swap);

    FreeVec(stk_mem);
    return stk_rc;
}

#endif /* STACKSWAP_H */
