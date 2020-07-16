#pragma once

#include <time.h>

struct Protothread
{
    int     label;
    clock_t timer;
};

#define Protothread_begin(thread)                   switch ((thread)->label) { case 0: (void)0
#define Protothread_end(thread)                     }; (thread)->label = -1; return -1

#define Protothread_init(thread)                    (thread)->label = 0
#define Protothread_waitUntil(thread, cond)         (thread)->label = __LINE__; case __LINE__: if (!(cond)) return 0
#define Protothread_waitSeconds(thread, seconds)    (thread)->label = __LINE__; (thread)->timer = clock() + (seconds) * CLOCKS_PER_SEC; case __LINE__: if (clock() < (thread)->timer) return 0
