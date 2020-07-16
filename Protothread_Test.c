#include <stdio.h>

#include "Protothread.h"

static int counter = 0;
static int example(struct Protothread* thread)
{
    Protothread_begin(thread);

        Protothread_waitSeconds(thread, 1);
        printf("Threshold reached 1\n");

        Protothread_waitSeconds(thread, 2);
        printf("Threshold reached 2\n");

        Protothread_waitSeconds(thread, 3);
        printf("Threshold reached 3\n");

        // Protothread_waitUntil(thread, counter == 100000);
        // printf("Threshold reached\n");
        // counter = 0;

    Protothread_end(thread);
}

int main()
{
    struct Protothread thread;
    Protothread_init(&thread);

    while (example(&thread) == 0)
    {
        counter++;
    }

    return 0;
}