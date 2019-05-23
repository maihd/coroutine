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

#if defined(__APPLE__) && defined(__MACH__)
#   define _XOPEN_SOURCE 700
#   include <sys/ucontext.h>
#else
#   include <ucontext.h>
#endif

enum
{
    STACK_SIZE = (1024 * 1024)
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
            coroutine->callee.uc_stack.ss_size  = STACK_SIZE;
            //coroutine->callee.uc_stack.ss_flags = 0;

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