#include <stdio.h>
#include <setjmp.h>

jmp_buf mainTask, childTask;

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

static int done;

void resume(void)
{
    if (!setjmp(mainTask))
    {
        longjmp(childTask, 1);
    }
}

void yield(void)
{
    if (!setjmp(childTask))
    {
        longjmp(mainTask, 1);
    }
}

void child(void)
{
    yield();

    int length = 1000 * 1000;
    int calcul = 0; 
    
    while (length-- >= 0)
    {
        printf("Length: %d\n", length);
        calcul++;    
        calcul++;
        calcul++;
    }

    done = 1;
    yield();
}

int main()
{
    if (!setjmp(mainTask))
    {
        child();
    }

    clock_t ticks = clock();
    while (!done)
    {
        resume();
    }

    #ifdef _WIN32
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    double clock2second = 1.0 / frequency.QuadPart;
#else
    double clock2second = 1.0 / CLOCKS_PER_SEC;
#endif

    printf("===============================================================\n");
    printf("=> Total time: %lfs\n", (double)(clock() - ticks) * clock2second);

    return 0;
}