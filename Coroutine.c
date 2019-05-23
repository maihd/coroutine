#include "Coroutine.h"

#include <stdlib.h>
#include <assert.h>

#ifdef _WIN32
/* End of windows version */
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

__declspec(thread) static void*      s_threadFiber;
__declspec(thread) static Coroutine* s_runningCoroutine;

struct Coroutine 
{
    HANDLE      fiber;

    CoroutineFn func;
    void*       args;
};

static void WINAPI Fiber_Entry(void* params)
{
    assert(params && "Internal logic error: params must be null.");
    assert(s_threadFiber && "Internal logic error: s_threadFiber must be initialized before run fiber.");

    Coroutine* coroutine = (Coroutine*)params;
    coroutine->func(coroutine->args);

    DeleteFiber(coroutine->fiber);
    coroutine->fiber = NULL;

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
            coroutine->func  = func;
            coroutine->args  = args;
            coroutine->fiber = handle;
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

int CoroutineResume(Coroutine* coroutine)
{
    if (coroutine && coroutine->fiber)
    {
        s_runningCoroutine = coroutine;
        SwitchToFiber(coroutine->fiber);
        return 1;
    }
    else
    {
        return 0;
    }
}

void CoroutineYield(void)
{
    s_runningCoroutine = NULL;
    SwitchToFiber(s_threadFiber);
}

Coroutine* CoroutineRunning(void)
{
    return s_runningCoroutine;
}

int CoroutineStatus(Coroutine* coroutine)
{
    if (!coroutine || !coroutine->fiber)
    {
        return COROUTINE_DEAD;
    }

    if (coroutine == s_runningCoroutine)
    {
        return COROUTINE_RUNNING;
    }

    return COROUTINE_SUSPENDED;
}
/* End of windows version */
#else
/* Begin of Unix family version */
#include <ucontext.h>

enum
{
    STACK_SIZE = 2097152 + 16384
};

enum 
{
    COFLAGS_END     = 1 << 0,
    COFLAGS_RUNNING = 1 << 1,  
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

static __thread Coroutine* s_runningCoroutine;

void Coroutine_Entry(Coroutine* coroutine)
{
    assert(coroutine && "coroutine must not be mull.");

    coroutine->func(coroutine->args);

    /* */
    coroutine->flags |= COFLAGS_END;

    /* Switch back to thread */
    swapcontext(&coroutine->callee, &coroutine->caller);
}

void CoroutineYield(void)
{
    if (s_runningCoroutine && (s_runningCoroutine->flags & COFLAGS_RUNNING) != 0)
    {
        s_runningCoroutine->flags &= ~(COFLAGS_RUNNING);
        swapcontext(&s_runningCoroutine->callee, &s_runningCoroutine->caller);
    }
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

            makecontext(&coroutine->callee, (void(*)())Coroutine_Entry, 1, coroutine);

            return coroutine;
        }
    }
    
    return NULL;
}

void CoroutineDestroy(Coroutine* coroutine)
{
    free(coroutine);
}

int CoroutineResume(Coroutine* coroutine)
{
    if (coroutine && !(coroutine->flags & COFLAGS_END))
    {
        s_runningCoroutine = coroutine;

        coroutine->flags |= COFLAGS_RUNNING;
        swapcontext(&coroutine->caller, &coroutine->callee);

        return 1;
    }
    else
    {
        return 0;
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
#endif