#undef  _FORTIFY_SOURCE          /* No standard library checking     */
#define _FORTIFY_SOURCE 0

#define _XOPEN_SOURCE           /* Apple's ucontext                 */
#define _CRT_SECURE_NO_WARNINGS /* Allow Windows unsafe functions   */

#if __GNUC__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#if __clang__
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

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

#if defined(_WIN32)
/* End of Windows Fiber version */
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

THREAD_LOCAL void* s_threadFiber;

struct Coroutine 
{
    int         status;

    HANDLE      fiber;

    CoroutineFn func;
    void*       args;
};

static void WINAPI Coroutine_Entry(void* params)
{
    assert(params && "Internal logic error: params must be null.");
    assert(s_threadFiber && "Internal logic error: s_threadFiber must be initialized before run fiber.");

    /* Run the routine */
    Coroutine* coroutine = (Coroutine*)params;
    coroutine->func(coroutine->args);

    /* Mark the coroutine is end */
    coroutine->status = COROUTINE_DEAD;

    /* Return to primary fiber */
    SwitchToFiber(s_threadFiber);
}

Coroutine* CoroutineCreate(CoroutineFn func, void* args)
{
    if (func)
    {
        Coroutine* coroutine = (Coroutine*)malloc(sizeof(Coroutine));
        if (coroutine)
        {
            coroutine->status = COROUTINE_NORMAL;
            coroutine->func   = func;
            coroutine->args   = args;
        }
        return coroutine;
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

STATIC_INLINE int CoroutineNativeStart(Coroutine* coroutine)
{
    if (!s_threadFiber)
    {
        s_threadFiber = ConvertThreadToFiber(NULL);
        assert(s_threadFiber != NULL && "Internal system error: OS cannot convert current thread to fiber.");
    }

    HANDLE handle = CreateFiber(0, Coroutine_Entry, coroutine);
    if (handle)
    {
        coroutine->fiber = handle;
        SwitchToFiber(handle);
        return 1;
    }
    else
    {
        return 0;
    }
}

STATIC_INLINE int CoroutineNativeResume(Coroutine* coroutine)
{
    SwitchToFiber(coroutine->fiber);
    return 1;
}

STATIC_INLINE void CoroutineNativeYield(Coroutine* coroutine)
{
    (void)coroutine;

    SwitchToFiber(s_threadFiber);
}
/* End of Windows Fiber version */
#elif defined(__linux__) || defined(__APPLE__)
/* Begin of Unix's ucontext version */

#include <setjmp.h>
#include <ucontext.h>

#define STORE_CALLER_CONTEXT 0

enum
{
#if defined(__APPLE__)
    STACK_SIZE = 32 * 1024 // 32 KB
#else
    STACK_SIZE = 2 * 1024 // 2 KB
#endif
};

struct Coroutine
{
    int         status;

    CoroutineFn func;
    void*       args;

#if STORE_CALLER_CONTEXT
    ucontext_t  caller;
    ucontext_t  callee;
#else
    ucontext_t  context;
    jmp_buf     jmpPoint;
#endif

    char        stack[STACK_SIZE];
};

#if !STORE_CALLER_CONTEXT
typedef struct CoroutineRunner
{
    Coroutine*  coroutine;

    ucontext_t* context;
    jmp_buf*    jmpPoint;
} CoroutineRunner;

THREAD_LOCAL jmp_buf s_threadJmpPoint;
#endif

static void Coroutine_Entry(unsigned int hiPart, unsigned int loPart)
{
#if !STORE_CALLER_CONTEXT
    CoroutineRunner* runner = (CoroutineRunner*)(((long long)hiPart << 32) | (long long)loPart);
    assert(runner && "context must not be mull.");

    Coroutine* coroutine = runner->coroutine;
    assert(coroutine && "coroutine must not be mull.");

    /* Run the routine */
    if (_setjmp(*runner->jmpPoint) == 0)
    { 
        ucontext_t tmp;
        swapcontext(&tmp, runner->context);
    }

    coroutine->func(coroutine->args);

    /* Mark the coroutine is end */
    coroutine->status = COROUTINE_DEAD;
    
    /* Return to primary thread */
    _longjmp(s_threadJmpPoint, 1);
#else
    Coroutine* coroutine = (Coroutine*)(((long long)hiPart << 32) | (long long)loPart);
    assert(coroutine && "coroutine must not be mull.");

    coroutine->func(coroutine->args);

    /* Mark the coroutine is end */
    coroutine->status = COROUTINE_DEAD;
    
    /* Return to primary thread */
    swapcontext(&coroutine->callee, &coroutine->caller);
#endif
}

Coroutine* CoroutineCreate(CoroutineFn func, void* args)
{
    if (func)
    {
        Coroutine* coroutine = (Coroutine*)malloc(sizeof(Coroutine));
        if (coroutine)
        {
            coroutine->status = COROUTINE_NORMAL;
            coroutine->func   = func;
            coroutine->args   = args;

#if !STORE_CALLER_CONTEXT
            getcontext(&coroutine->context);

            coroutine->context.uc_stack.ss_sp    = coroutine->stack;
            coroutine->context.uc_stack.ss_size  = sizeof(coroutine->stack);

            ucontext_t tmp;
            CoroutineRunner runner = { coroutine, &tmp, &coroutine->jmpPoint};

            unsigned int hiPart = (unsigned int)((long long)(&runner) >> 32);
            unsigned int loPart = (unsigned int)((long long)(&runner) & 0xFFFFFFFF);
            makecontext(&coroutine->context, (void(*)())Coroutine_Entry, 2, hiPart, loPart);
            swapcontext(&tmp, &coroutine->context);
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

static int CoroutineNativeStart(Coroutine* coroutine)
{
#if STORE_CALLER_CONTEXT
    getcontext(&coroutine->callee);

    coroutine->callee.uc_link           = &coroutine->caller;
    coroutine->callee.uc_stack.ss_sp    = coroutine->stack;
    coroutine->callee.uc_stack.ss_size  = sizeof(coroutine->stack);
    coroutine->callee.uc_stack.ss_flags = 0;

    unsigned int hiPart = (unsigned int)((long long)coroutine >> 32);
    unsigned int loPart = (unsigned int)((long long)coroutine & 0xFFFFFFFF);
    makecontext(&coroutine->callee, (void(*)())Coroutine_Entry, 2, hiPart, loPart);
    swapcontext(&coroutine->caller, &coroutine->callee);

    return 1;
#else
    if (_setjmp(s_threadJmpPoint) == 0)
    { 
        _longjmp(coroutine->jmpPoint, 1);
    }

    return 1;
#endif
}

static int CoroutineNativeResume(Coroutine* coroutine)
{
#if STORE_CALLER_CONTEXT
    swapcontext(&coroutine->caller, &coroutine->callee);
    return 1;
#else
    if (_setjmp(s_threadJmpPoint) == 0)
    { 
        _longjmp(coroutine->jmpPoint, 1);
    }
    return 1;
#endif
}

static void CoroutineNativeYield(Coroutine* coroutine)
{
#if STORE_CALLER_CONTEXT
    swapcontext(&coroutine->callee, &coroutine->caller);
#else
    if (_setjmp(coroutine->jmpPoint) == 0)
    { 
        _longjmp(s_threadJmpPoint, 1);
    }
#endif
}
/* End of Unix's ucontext version */
#else
#error Unsupport version
#endif

/* Running coroutine, NULL mean current is primary coroutine */
THREAD_LOCAL Coroutine* s_runningCoroutine;

int CoroutineResume(Coroutine* coroutine)
{
    switch (CoroutineStatus(coroutine))
    {
    case COROUTINE_NORMAL:
        s_runningCoroutine = coroutine;
        coroutine->status = COROUTINE_RUNNING;

        return CoroutineNativeStart(coroutine);

    case COROUTINE_SUSPENDED:
        s_runningCoroutine = coroutine;
        coroutine->status = COROUTINE_RUNNING;

        return CoroutineNativeResume(coroutine);

    default:
        return 0;
    }
}

void CoroutineYield(void)
{
    if (s_runningCoroutine && s_runningCoroutine->status == COROUTINE_RUNNING)
    {
        Coroutine* coroutine = s_runningCoroutine;
        s_runningCoroutine = NULL;

        coroutine->status = COROUTINE_SUSPENDED;
        CoroutineNativeYield(coroutine);
    }
}

Coroutine* CoroutineRunning(void)
{
    return s_runningCoroutine && (s_runningCoroutine->status == COROUTINE_RUNNING) ? s_runningCoroutine : NULL;
}

int CoroutineStatus(Coroutine* coroutine)
{
    return coroutine ? coroutine->status : COROUTINE_DEAD;
}