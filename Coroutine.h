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

#ifdef __cplusplus
extern "C" {
#elif !defined(__bool_true_and_false_defined)
typedef int bool;
enum
{
    true = 1, false = 0
};
#endif

enum
{
    COROUTINE_DEAD,
    COROUTINE_NORMAL,
    COROUTINE_RUNNING,
    COROUTINE_SUSPENDED,
};

enum
{
    /* Recommended stack size */
    COROUTINE_STACK_SIZE =
#if defined(_WIN32)
    1024 * 1024 /* 1 MB */
#elif defined(__APPLE__)
    32 * 1024 /* 32 KB   */
#else
    2 * 1024  /* 2 KB    */
#endif
};

typedef void(*CoroutineFn)(void* args);

typedef struct Coroutine Coroutine;

/**
 * Creates a new coroutine, with body func and args.
 * Return false if func is not valid or creation failed.
 */
COROUTINE_API Coroutine*    CoroutineCreate(int stackSize, CoroutineFn func, void* args);

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
COROUTINE_API bool          CoroutineResume(Coroutine* coroutine);

#ifdef __cplusplus
}
#endif

