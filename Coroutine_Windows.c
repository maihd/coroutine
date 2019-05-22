#include "Coroutine.h"

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdlib.h>
#include <assert.h>

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