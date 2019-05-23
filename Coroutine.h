#pragma once

//
// Portable coroutine focus on common platforms based on Lua's Coroutine, but with native support.
// Target platforms: Windows, Linux, Mac, Android, iOS
// Reference: 
//      - https://www.lua.org/manual/5.1/manual.html#2.11
//      - https://www.lua.org/manual/5.1/manual.html#5.2
//

#ifndef COROUTINE_API
#define COROUTINE_API
#endif

#if __cplusplus
extern "C" {
#endif

enum
{
    COROUTINE_DEAD,
    COROUTINE_NORMAL,
    COROUTINE_RUNNING,
    COROUTINE_SUSPENDED,
};

typedef void(*CoroutineFn)(void* args);

typedef struct Coroutine Coroutine;

/**
 * Creates a new coroutine, with body func and args.
 * Return NULL if func is not valid or creation failed.
 */
COROUTINE_API Coroutine*    CoroutineCreate(CoroutineFn func, void* args);

/**
 * Release coroutine memory usage
 */
COROUTINE_API void          CoroutineDestroy(Coroutine* coroutine);

/**
 *  Returns the status of coroutine.
 *      - COROUTINE_DEAD: the coroutine has finished its body function, or if it has stopped with an error. 
 *      - COROUTINE_NORMAL: the coroutine is active but not running (that is, it has resumed another coroutine).
 *      - COROUTINE_RUNNING: the coroutine is running (that is, it is CoroutineRunning()).
 *      - COROUTINE_SUSPENDED: the coroutine is suspended in a call to CoroutineYield, or it has not started running yet.
 */
COROUTINE_API int           CoroutineStatus(Coroutine* coroutine);

/**
 * Returns the running coroutine, or NULL when called by the main thread.
 */
COROUTINE_API Coroutine*    CoroutineRunning(void);

/**
 * Suspends the execution of the calling coroutine.
 */
COROUTINE_API void          CoroutineYield(void);

/**
 * Starts or continues the execution of coroutine.
 * Return 1 if resume success, 0 is otherwise.
 */
COROUTINE_API int           CoroutineResume(Coroutine* coroutine);

#if __cplusplus
}
#endif

