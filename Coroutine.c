#include "Coroutine.h"

#include <stdlib.h>
#include <assert.h>

#if defined(_MSC_VER) || (defined(_WIN32) && defined(__clang__)) || defined(__MINGW32__) || defined(__CYGWIN__)
#   define THREAD_LOCAL     static __declspec(thread)
#   define STATIC_INLINE    static __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#   define THREAD_LOCAL     static __thread
#   define STATIC_INLINE    static __inline__ __attribute__((always_inline))
#else
#   define THREAD_LOCAL
#   define STATIC_INLINE    static /* Let the compiler do inline */
#endif

enum 
{
    COFLAGS_END     = 1 << 0,
    COFLAGS_START   = 1 << 1,
    COFLAGS_RUNNING = 1 << 2,  
};

#if defined(_WIN32)
/* End of Windows Fiber version */
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

THREAD_LOCAL void* s_threadFiber;

struct Coroutine 
{
    int         flags;

    HANDLE      fiber;

    CoroutineFn func;
    void*       args;
};

static void WINAPI Fiber_Entry(void* params)
{
    assert(params && "Internal logic error: params must be null.");
    assert(s_threadFiber && "Internal logic error: s_threadFiber must be initialized before run fiber.");

    /* Run the routine */
    Coroutine* coroutine = (Coroutine*)params;
    coroutine->func(coroutine->args);

    /* Mark the coroutine is end */
    coroutine->flags |= COFLAGS_END;

    /* Return to primary fiber */
    SwitchToFiber(s_threadFiber);
}

Coroutine* CoroutineCreate(CoroutineFn func, void* args)
{
    if (func)
    {
        if (!s_threadFiber)
        {
            s_threadFiber = ConvertThreadToFiber(NULL);
            assert(s_threadFiber != NULL && "Internal system error: OS cannot convert current thread to fiber.");
        }

        Coroutine* coroutine = (Coroutine*)malloc(sizeof(Coroutine));

        HANDLE handle = CreateFiber(0, Fiber_Entry, coroutine);
        if (handle)
        {
            coroutine->flags = 0;
            coroutine->fiber = handle;
            coroutine->func  = func;
            coroutine->args  = args;
            return coroutine;
        }
        else
        {
            free(coroutine);
            return NULL;
        }
    }
    else
    {
        return NULL;
    }
}

void CoroutineDestroy(Coroutine* coroutine)
{
    if (coroutine)
    {
        DeleteFiber(coroutine->fiber);
        free(coroutine);
    }
}

STATIC_INLINE void CoroutineNativeResume(Coroutine* coroutine)
{
    SwitchToFiber(coroutine->fiber);
}

STATIC_INLINE void CoroutineNativeYield(Coroutine* coroutine)
{
    (void)coroutine;

    SwitchToFiber(s_threadFiber);
}
/* End of Windows Fiber version */
#elif defined(__linux__) || defined(__APPLE__)
/* Begin of Unix's ucontext version */

#if defined(__APPLE__)
/* Begin of Apple ucontext implementation */

#   define _XOPEN_SOURCE
#   include <ucontext.h>

// #   include <stdarg.h>
// #   include <errno.h>
// #   include <stdlib.h>
// #   include <unistd.h>
// #   include <string.h>
// #   include <assert.h>
// #   include <time.h>
// #   include <sys/time.h>
// #   include <sys/types.h>
// #   include <sys/wait.h>
// #   include <sched.h>
// #   include <signal.h>
// #   include <sys/utsname.h>
// #   include <inttypes.h>
// #   include <sys/ucontext.h>
// #	define mcontext libthread_mcontext
// #	define mcontext_t libthread_mcontext_t
// #	define ucontext libthread_ucontext
// #	define ucontext_t libthread_ucontext_t

// #	if defined(__i386__) || defined(__x86_64__)
// /* Begin of Apple ucontext x86 version */
// #       define setcontext(u) setmcontext(&(u)->uc_mcontext)
// #       define getcontext(u) getmcontext(&(u)->uc_mcontext)
// typedef struct mcontext mcontext_t;
// typedef struct ucontext ucontext_t;

// typedef void (MakeContextCallback)(void);

// /*extern*/ int swapcontext(ucontext_t *, ucontext_t *);
// //  /*extern*/ void makecontext(ucontext_t*, void(*)(), int, ...);
// /*extern*/ void makecontext(ucontext_t *, MakeContextCallback *, int, ...);
// /*extern*/ int  getmcontext(mcontext_t *);
// /*extern*/ void setmcontext(mcontext_t *);

// /* #include <machine/ucontext.h> */

// struct mcontext {
// 	/*
// 	 * The first 20 fields must match the definition of
// 	 * sigcontext. So that we can support sigcontext
// 	 * and ucontext_t at the same time.
// 	 */
// 	long mc_onstack; /* XXX - sigcontext compat. */
// 	long mc_gs;
// 	long mc_fs;
// 	long mc_es;
// 	long mc_ds;
// 	long mc_edi;
// 	long mc_esi;
// 	long mc_ebp;
// 	long mc_isp;
// 	long mc_ebx;
// 	long mc_edx;
// 	long mc_ecx;
// 	long mc_eax;
// 	long mc_trapno;
// 	long mc_err;
// 	long mc_eip;
// 	long mc_cs;
// 	long mc_eflags;
// 	long mc_esp; /* machine state */
// 	long mc_ss;

// 	long mc_fpregs[28]; /* env87 + fpacc87 + u_long */
// 	long __spare__[17];
// };

// struct ucontext {
// 	/*
// 	 * Keep the order of the first two fields. Also,
// 	 * keep them the first two fields in the structure.
// 	 * This way we can have a union with struct
// 	 * sigcontext and ucontext_t. This allows us to
// 	 * support them both at the same time.
// 	 * note: the union is not defined, though.
// 	 */
// 	sigset_t    uc_sigmask;
// 	mcontext_t  uc_mcontext;

// 	struct __ucontext *uc_link;
// 	stack_t            uc_stack;
// 	long               __spare__[8];
// };

// void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
// {
// 	uintptr_t *sp;

// 	sp = (uintptr_t *)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size / sizeof(void *);
// 	sp -= argc;
// 	sp = (void*)((uintptr_t)sp - (uintptr_t)sp % 16);	/* 16-align for OS X */
// 	memmove(sp, &argc + 1, argc * sizeof(uintptr_t));

// 	*--sp = 0;		/* return address */
// 	ucp->uc_mcontext.mc_eip = (long)func;
// 	ucp->uc_mcontext.mc_esp = (long)sp;
// }
// /* End of Apple ucontext x86 version */
// #   else
// /* Begin of Apple ucontext powerpc version */
// #       define setcontext(u) _setmcontext(&(u)->mc)
// #       define getcontext(u) _getmcontext(&(u)->mc)
// typedef struct mcontext mcontext_t;
// typedef struct ucontext ucontext_t;
// struct mcontext
// {
// 	ulong pc;      /* lr */
// 	ulong cr;      /* mfcr */
// 	ulong ctr;     /* mfcr */
// 	ulong xer;     /* mfcr */
// 	ulong sp;      /* callee saved: r1 */
// 	ulong toc;     /* callee saved: r2 */
// 	ulong r3;      /* first arg to function, return register: r3 */
// 	ulong gpr[19]; /* callee saved: r13-r31 */
// /*
// // XXX: currently do not save vector registers or floating-point state
// //	ulong   pad;
// //	uvlong  fpr[18];    / * callee saved: f14-f31 * /
// //	ulong   vr[4*12];   / * callee saved: v20-v31, 256-bits each * /
// */
// };

// struct ucontext
// {
// 	struct {
// 		void *ss_sp;
// 		uint ss_size;
// 	} uc_stack;
// 	sigset_t uc_sigmask;
// 	mcontext_t mc;
// };

// void makecontext(ucontext_t*, void(*)(void), int, ...);
// int swapcontext(ucontext_t*, ucontext_t*);
// int _getmcontext(mcontext_t*);
// void _setmcontext(mcontext_t*);

// void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
// {
// 	unsigned long *sp, *tos;
// 	va_list arg;

// 	tos = (unsigned long*)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size / sizeof(unsigned long);
// 	sp = tos - 16;
// 	ucp->mc.pc = (long)func;
// 	ucp->mc.sp = (long)sp;
// 	va_start(arg, argc);
// 	ucp->mc.r3 = va_arg(arg, long);
// 	va_end(arg);
// }
// /* End of Apple ucontext powerpc version */
// #   endif

// int swapcontext(ucontext_t *oucp, ucontext_t *ucp)
// {
// 	if(getcontext(oucp) == 0)
// 		setcontext(ucp);
// 	return 0;
// }

/* End of Apple ucontext implementation */
#else
#   include <ucontext.h>
#endif

enum
{
    STACK_SIZE = 2097152 + 16384
};

struct Coroutine
{
    int         flags;

    CoroutineFn func;
    void*       args;

    ucontext_t  caller;
    ucontext_t  callee;

    char        stackBuffer[STACK_SIZE];
};

void Coroutine_Entry(unsigned int hiPart, unsigned int loPart)
{
    Coroutine* coroutine = (Coroutine*)(((long long)hiPart << 32) | (long long)loPart);
    assert(coroutine && "coroutine must not be mull.");

    /* Run the routine */
    coroutine->func(coroutine->args);

    /* Mark the coroutine is end */
    coroutine->flags |= COFLAGS_END;

    /* Return to primary fiber */
    swapcontext(&coroutine->callee, &coroutine->caller);
}

Coroutine* CoroutineCreate(CoroutineFn func, void* args)
{
    if (func)
    {
        Coroutine* coroutine = (Coroutine*)malloc(sizeof(Coroutine));
        if (coroutine)
        {
            getcontext(&coroutine->callee);

            coroutine->flags                    = 0;
            coroutine->func                     = func;
            coroutine->args                     = args;

            coroutine->callee.uc_link           = &coroutine->caller;
            coroutine->callee.uc_stack.ss_sp    = coroutine->stackBuffer;
            coroutine->callee.uc_stack.ss_size  = STACK_SIZE - sizeof(long);
            coroutine->callee.uc_stack.ss_flags = 0;

        #if defined(__x86_64__)
            unsigned int hiPart = (unsigned int)((long long)coroutine >> 32);
            unsigned int loPart = (unsigned int)((long long)coroutine & 0xFFFFFFFF);
            makecontext(&coroutine->callee, (void(*)())Coroutine_Entry, 2, hiPart, loPart);
        #else
            makecontext(&coroutine->callee, (void(*)())Coroutine_Entry, 1, coroutine);
        #endif

            return coroutine;
        }
    }
    
    return NULL;
}

void CoroutineDestroy(Coroutine* coroutine)
{
    free(coroutine);
}

STATIC_INLINE void CoroutineNativeResume(Coroutine* coroutine)
{
    swapcontext(&coroutine->caller, &coroutine->callee);
}

STATIC_INLINE void CoroutineNativeYield(Coroutine* coroutine)
{
    swapcontext(&coroutine->callee, &coroutine->caller);
}
/* End of Unix's ucontext version */
#else
#error Unsupport version
#endif

/* Running coroutine, NULL mean current is primary coroutine */
THREAD_LOCAL Coroutine* s_runningCoroutine;

int CoroutineResume(Coroutine* coroutine)
{
    if (CoroutineStatus(coroutine) == COROUTINE_SUSPENDED)
    {
        s_runningCoroutine = coroutine;
        coroutine->flags |= COFLAGS_RUNNING;
        CoroutineNativeResume(coroutine);
        return 1;
    }
    else
    {
        return 0;
    }
}

void CoroutineYield(void)
{
    if (s_runningCoroutine && (s_runningCoroutine->flags & COFLAGS_RUNNING) != 0)
    {
        Coroutine* coroutine = s_runningCoroutine;
        s_runningCoroutine = NULL;

        coroutine->flags &= ~(COFLAGS_RUNNING);
        CoroutineNativeYield(coroutine);
    }
}

Coroutine* CoroutineRunning(void)
{
    return s_runningCoroutine && (s_runningCoroutine->flags & COFLAGS_RUNNING) ? s_runningCoroutine : NULL;
}

int CoroutineStatus(Coroutine* coroutine)
{
    if (!coroutine || (coroutine->flags & COFLAGS_END))
    {
        return COROUTINE_DEAD;
    }

    if (coroutine->flags & COFLAGS_RUNNING)
    {
        return COROUTINE_RUNNING;
    }

    return COROUTINE_SUSPENDED;
}