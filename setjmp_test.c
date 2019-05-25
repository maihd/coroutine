#include <setjmp.h>

enum 
{
    Coroutine_Done    = 1,
    Coroutine_Working = 2,
};

typedef struct Coroutine
{
    int     status;
    jmp_buf callee;
    jmp_buf caller;
} Coroutine;

typedef void (*CoFunc)(void*);

void CoStart(Coroutine* c, CoFunc f, void* arg, void* sp);
void CoYield(Coroutine* c);
int  CoResume(Coroutine* c);

#include <stdio.h>

#ifdef _WIN32
#include <Windows.h>
#define clock_t long long
static long long clock(void)
{
    LARGE_INTEGER value;
    return QueryPerformanceCounter(&value) ? value.QuadPart : 0;
}
#else
#include <time.h>
#endif

Coroutine coroutine;

void CoPrint(void* args)
{
    (void)args;
    int length = 10 * 1000 * 1000;
    int calcul = 0;

    while (length--)
    {
        calcul++;
        //printf("First print.\n");
        CoYield(&coroutine);
        
        calcul++;
        //printf("Second print.\n");
        CoYield(&coroutine);

        //printf("Done.\n");
        calcul++;
    }
}

int main(void)
{
    char stack[1024];

    // Create new coroutine with CoPrint and no args
    CoStart(&coroutine, CoPrint, 0, stack);

    clock_t ticks = clock();
    // Run coroutine until end
    while (CoResume(&coroutine) != Coroutine_Done);

#ifdef _WIN32
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    double clock2second = 1.0 / frequency.QuadPart;
#else
    double clock2second = 1.0 / CLOCKS_PER_SEC;
#endif

    printf("===============================================================\n");
    printf("=> Total time: %lfs\n", (double)(clock() - ticks) * clock2second);

    // Destroy coroutine
    //CoroutineDestroy(coroutine);

    return 0;
}

void CoYield(Coroutine* c) 
{
    if (!setjmp(c->callee)) 
    {
        longjmp(c->caller, 1);
    }
}

int CoResume(Coroutine* c) 
{
    if (Coroutine_Done == c->status) return Coroutine_Done;

    if (!setjmp(c->caller))
    {
        c->status = Coroutine_Working;
        longjmp(c->callee, 1);
    }
    else 
    {
        return c->status;
    }
}

typedef struct {
  Coroutine*    c;
  CoFunc        f;
  void*         arg;
  void*         old_sp;
  void*         old_fp;
} CoStartParams;

#ifdef __GNUC__
#   define get_sp(p)        __asm__ volatile("movq %%rsp, %0" : "=r"(p))
#   define get_fp(p)        __asm__ volatile("movq %%rbp, %0" : "=r"(p))
#   define set_sp(p)        __asm__ volatile("movq %0, %%rsp" : : "r"(p))
#   define set_fp(p)        __asm__ volatile("movq %0, %%rbp" : : "r"(p))
#   define set_spfp(p1, p2) set_sp( p1 ); set_fp( p2 )
#elif defined(_WIN64)
// kevin:
// see https://github.com/halayli/lthread/blob/master/src/lthread.c
// for _switch CoFunction, might be a better alternative than set_sp set_fp
#include <intrin.h>
#include <windows.h>
inline __int64 getRSP();
inline __int64 getRBP();
inline void setRSP(__int64 v);
inline void setRBP(__int64 v);
inline void setRSPRBP(__int64 v1, __int64 v2);
#   define get_sp(var) var = (void*)getRSP()
#   define get_fp(var) var = (CoStartParams*)getRBP()
#   define set_sp(var) setRSP((__int64)(var))
#   define set_fp(var) setRBP((__int64)(var))
#   define set_spfp(var1, var2) setRSPRBP((__int64)(var1), (__int64)(var2))
#elif defined(_WIN32)
#   define get_sp(var) { __int32 t1918; __asm { \
           mov t1918, esp \
   }; var = (void*)t1918; }
#   define get_fp(var) { __int32 t1918; __asm { \
      mov t1918, ebp \
   }; var = (CoStartParams*)t1918; }
#   define set_sp(var) { __int32 t1918 = (__int32)(var); __asm { \
   mov esp, t1918 \
}; }
#   define set_fp(var) { __int32 t1918 = (__int32)(var); __asm { \
   mov ebp, t1918 \
}; }
#   define set_spfp(p1, p2) set_sp( p1 ); set_fp( p2 )
#else
#endif

enum { FRAME_SZ=5 }; //fairly arbitrary

void CoStart(Coroutine* c, CoFunc f, void* arg, void* sp)
{
    c->status = Coroutine_Working;
    CoStartParams* p = ((CoStartParams*)sp) - 1;

    //save params before stack switching
    p->c = c;
    p->f = f;
    p->arg = arg;
    /*
    CONTEXT context;
    memset( &context, 0, sizeof( CONTEXT ) );
    EXCEPTION_RECORD econtext;
    memset( &econtext, 0, sizeof( EXCEPTION_RECORD ) );
    RtlCaptureContext( &context );
    p->old_sp = (void*)context.Rsp;
    p->old_fp = (void*)context.Rbp;
    */
    get_sp(p->old_sp);
    get_fp(p->old_fp);

    //char** retaddr = (char**)_AddressOfReturnAddress();
    //*retaddr = (char*)(p - FRAME_SZ);
    /*RtlCaptureContext( &context );
    context.Rsp = DWORD64(p - 5);
    context.Rbp = (DWORD64)p;
    econtext.ExceptionAddress = (void*)context.Rsp;
    RtlRestoreContext( &context, &econtext );
    */
    //set_spfp( p - FRAME_SZ, p );
    set_sp(p - FRAME_SZ);
    set_fp(p); //effectively clobbers p
    //(and all other locals)
    /*RtlCaptureContext( &context );
    p = (CoStartParams*)context.Rbp;
    */
    get_fp(p); //…so we read p back from $fp

    //and now we read our params from p
    if (!setjmp(p->c->callee)) 
    {
        //set_spfp( p->old_sp, p->old_fp );
        /*RtlCaptureContext( &context );
        context.Rsp = (DWORD64)p->old_sp;
        context.Rbp = (DWORD64)p->old_fp;
        econtext.ExceptionAddress = (void*)context.Rsp;
        RtlRestoreContext( &context, &econtext );
        */
        set_sp(p->old_sp);
        set_fp(p->old_fp);
        return;
    }
    (*p->f)(p->arg);
    longjmp(p->c->caller, Coroutine_Done);
}