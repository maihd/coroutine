#undef  _FORTIFY_SOURCE          /* No standard library checking     */
#define _FORTIFY_SOURCE 0

#define _XOPEN_SOURCE           /* Apple's ucontext                 */
#define _CRT_SECURE_NO_WARNINGS /* Allow Windows unsafe functions   */

#if __GNUC__
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#if __clang__
#pragma clang diagnostic ignored "-Wunused-parameter"
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
    int         stackSize;

    HANDLE      fiber;

    CoroutineFn func;
    void*       args;
};

/* Coroutine Fiber callback
 * @note: __stdcall insteadof CALLBACK because my compiler mark WINAPI is undefined 
 */
static void __stdcall Coroutine_Entry(void* params)
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

Coroutine* CoroutineCreate(int stackSize, CoroutineFn func, void* args)
{
    if (func)
    {
        Coroutine* coroutine = (Coroutine*)malloc(sizeof(Coroutine));
        if (coroutine)
        {
            coroutine->status    = COROUTINE_NORMAL;
            coroutine->stackSize = stackSize <= 0 ? COROUTINE_STACK_SIZE : stackSize;
            coroutine->func      = func;
            coroutine->args      = args;

            return coroutine;
        }
    }

    return NULL;
}

void CoroutineDestroy(Coroutine* coroutine)
{
    if (coroutine)
    {
        DeleteFiber(coroutine->fiber);
        free(coroutine);
    }
}

STATIC_INLINE bool CoroutineNativeStart(Coroutine* coroutine)
{
    if (!s_threadFiber)
    {
        s_threadFiber = ConvertThreadToFiber(NULL);
        assert(s_threadFiber != NULL && "Internal system error: OS cannot convert current thread to fiber.");
    }

    HANDLE handle = CreateFiber((SIZE_T)coroutine->stackSize, Coroutine_Entry, coroutine);
    if (handle)
    {
        coroutine->fiber = handle;
        SwitchToFiber(handle);
        return true;
    }
    else
    {
        return false;
    }
}

STATIC_INLINE bool CoroutineNativeResume(Coroutine* coroutine)
{
    SwitchToFiber(coroutine->fiber);
    return true;
}

STATIC_INLINE void CoroutineNativeYield(Coroutine* coroutine)
{
    (void)coroutine;

    SwitchToFiber(s_threadFiber);
}

#if 0
#if defined(_X86_)
#define CONTEXT_AMD64               0x00100000L
#define CONTEXT_CONTROL             (CONTEXT_AMD64 | 0x00000001L)
#define CONTEXT_INTEGER             (CONTEXT_AMD64 | 0x00000002L)
#define CONTEXT_SEGMENTS            (CONTEXT_AMD64 | 0x00000004L)
#define CONTEXT_FLOATING_POINT      (CONTEXT_AMD64 | 0x00000008L)
#define CONTEXT_DEBUG_REGISTERS     (CONTEXT_AMD64 | 0x00000010L)

#define CONTEXT_FULL                (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT)

#define CONTEXT_ALL                 (CONTEXT_CONTROL | CONTEXT_INTEGER | \
                                     CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT | \
                                     CONTEXT_DEBUG_REGISTERS)

#define CONTEXT_XSTATE              (CONTEXT_AMD64 | 0x00000040L)

#define CONTEXT_EXCEPTION_ACTIVE    0x08000000L
#define CONTEXT_SERVICE_ACTIVE      0x10000000L
#define CONTEXT_EXCEPTION_REQUEST   0x40000000L
#define CONTEXT_EXCEPTION_REPORTING 0x80000000L
#endif

typedef struct __stack 
{
	void*   ss_sp;
	size_t  ss_size;
	int     ss_flags;
} stack_t;

typedef CONTEXT         mcontext_t;
typedef unsigned long   __sigset_t;

typedef struct __ucontext 
{
	unsigned long int	uc_flags;
	struct __ucontext*  uc_link;
	stack_t				uc_stack;
	mcontext_t			uc_mcontext; 
	__sigset_t			uc_sigmask;
} ucontext_t;

STATIC_INLINE int getcontext(ucontext_t*ucp);
STATIC_INLINE int setcontext(const ucontext_t *ucp);
STATIC_INLINE int makecontext(ucontext_t*, void (*)(), int, ...);
STATIC_INLINE int swapcontext(ucontext_t*, const ucontext_t *);

STATIC_INLINE int getcontext(ucontext_t *ucp)
{
	int ret;

	/* Retrieve the full machine context */
    ucp->uc_mcontext.ContextFlags = CONTEXT_FULL;
	ret = GetThreadContext(GetCurrentThread(), &ucp->uc_mcontext);

	return (ret == 0) ? -1 : 0;
}

STATIC_INLINE int setcontext(const ucontext_t *ucp)
{
	int ret;
	
	/* Restore the full machine context (already set) */
	ret = SetThreadContext(GetCurrentThread(), &ucp->uc_mcontext);
	return (ret == 0) ? -1 : 0;
}

STATIC_INLINE int makecontext(ucontext_t* ucp, void (*func)(), int argc, ...)
{
    /* Stack pointer */
	char* sp;

	/* Stack grows down */
	sp = (char*)(size_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size;	

	/* Reserve stack space for the arguments (maximum possible: argc * (sizeof(DWORD64) bytes per argument)) */
	sp -= argc * sizeof(DWORD64);

	if (sp < (char*)ucp->uc_stack.ss_sp) 
    {
		/* errno = ENOMEM;*/
		return -1;
	}

	/* Set the instruction and the stack pointer */
#if defined(_X86_)
	ucp->uc_mcontext.Eip = (DWORD32)func;
	ucp->uc_mcontext.Esp = (DWORD32)(sp - 4);
#else
	ucp->uc_mcontext.Rip = (DWORD64)func;
	ucp->uc_mcontext.Rsp = (DWORD64)(sp - 40);
#endif
	/* Save/Restore the full machine context */
	ucp->uc_mcontext.ContextFlags = CONTEXT_FULL;

	/* Copy the arguments */
    va_list  src;
    DWORD64* dst;

    dst = (DWORD64*)(sp);
	va_start(src, argc);
	//for (int i = argc - 1; i > -1; i--)
    for (int i = 0; i < argc; i++)
    {
        DWORD64  v = va_arg(src, DWORD64);
        dst[i] = v;
	}
	va_end(src);

	return 0;
}

STATIC_INLINE int swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
{
	int ret;

	if ((oucp == NULL) || (ucp == NULL)) {
		/*errno = EINVAL;*/
		return -1;
	}

	ret = getcontext(oucp);
	if (ret == 0) {
		ret = setcontext(ucp);
	}
	return ret;
}

#define _longjmp longjmp
#endif

/* End of Windows Fiber version */
#elif defined(__linux__) || defined(__APPLE__)
/* Begin of Unix's ucontext version */

#if defined(__linux__)
#   ifndef __stack_t_defined
#   define __stack_t_defined 1
#   define __need_size_t
#   include <stddef.h>
/* Structure describing a signal stack.  */
typedef struct
{
    void*   ss_sp;
    int     ss_flags;
    size_t  ss_size;
} stack_t;
#   endif
#endif

#include <setjmp.h>
#include <unistd.h>
#include <ucontext.h>

#if !defined(_WIN32) || defined(_X86_)
#define DUMMYARGS
#else
#define DUMMYARGS long long _0, long long _1, long long _2, long long _3, 
#endif


#define STORE_CALLER_CONTEXT 0

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

    int         stackSize;
    char        stack[];
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

static void Coroutine_Entry(DUMMYARGS unsigned int hiPart, unsigned int loPart)
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

Coroutine* CoroutineCreate(int stackSize, CoroutineFn func, void* args)
{
    if (func)
    {
        stackSize = stackSize <= 0 ? COROUTINE_STACK_SIZE : stackSize;
        Coroutine* coroutine = (Coroutine*)malloc(sizeof(Coroutine) + stackSize);
        if (coroutine)
        {
            coroutine->status       = COROUTINE_NORMAL;
            coroutine->func         = func;
            coroutine->args         = args;
            coroutine->stackSize    = stackSize;

#if !STORE_CALLER_CONTEXT
            getcontext(&coroutine->context);

            coroutine->context.uc_stack.ss_sp    = coroutine->stack;
            coroutine->context.uc_stack.ss_size  = coroutine->stackSize;

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
    coroutine->callee.uc_stack.ss_size  = coroutine->stackSize;
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

bool CoroutineResume(Coroutine* coroutine)
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
        return false;
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