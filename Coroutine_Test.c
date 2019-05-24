#include <stdio.h>
#include <stdlib.h>

#include "Coroutine.h"

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

void CoPrint(void* args)
{
    (void)args;
    int length = 1000 * 1000 * 1000;
    int calcul = 0;

    while (length--)
    {
        calcul++;
        //printf("First print.\n");
        CoroutineYield();
        
        calcul++;
        //printf("Second print.\n");
        CoroutineYield();

        //printf("Done.\n");
        calcul++;
    }
}

int main(void)
{
    // Create new coroutine with CoPrint and no args
    Coroutine* coroutine = CoroutineCreate(CoPrint, NULL);

    clock_t ticks = clock();
    // Run coroutine until end
    while (CoroutineResume(coroutine));

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
    CoroutineDestroy(coroutine);

    return 0;
}

