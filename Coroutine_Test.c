#include <stdio.h>
#include <stdlib.h>

#include "Coroutine.h"

void CoPrint(void* args)
{
    (void)args;

    printf("First print.\n");
    CoroutineYield();

    printf("Second print.\n");
    CoroutineYield();

    printf("Done.\n");
}

int main(void)
{
    // Create new coroutine with CoPrint and no args
    Coroutine* coroutine = CoroutineCreate(CoPrint, NULL);

    // Run coroutine until end
    while (CoroutineResume(coroutine));

    // Destroy coroutine
    CoroutineDestroy(coroutine);

    return 0;
}

