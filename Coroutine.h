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

// Should use shortname for yield
#ifndef COROUTINE_YIELD_SHORTNAME
#define COROUTINE_YIELD_SHORTNAME 0
#endif

/* BEGIN OF EXTERN "C" */
#ifdef __cplusplus
extern "C" {
#endif

/* Define bool if needed */
#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
typedef char bool;
enum
{
    true = 1, false = 0
};
#endif

typedef enum CoroutineStatus
{
    CoroutineStatus_Dead,
    CoroutineStatus_Normal,
    CoroutineStatus_Running,
    CoroutineStatus_Suspended,
} CoroutineStatus;

enum
{
    /* Recommended stack size */
    COROUTINE_STACK_SIZE = 2 * 1024 /* 2KB */
};

typedef void(*CoroutineFn)(void* args);

typedef struct Coroutine Coroutine;

/**
 * Creates a new coroutine, with body func and args.
 * Return NULL if func is not valid or creation failed.
 */
COROUTINE_API Coroutine*        Coroutine_create(int stackSize, CoroutineFn func, void* args);

/**
 * Release coroutine memory usage
 */
COROUTINE_API void              Coroutine_destroy(Coroutine* coroutine);

/**
 *  Returns the status of coroutine.
 *      - CoroutineStatus_Dead: the coroutine has finished its body function, or if it has stopped with an error. 
 *      - CoroutineStatus_Normal: the coroutine is active but not running (that is, it has resumed another coroutine).
 *      - CoroutineStatus_Running: the coroutine is running (that is, it is the coroutine return by Coroutine_running()).
 *      - CoroutineStatus_Suspended: the coroutine is suspended in a call to CoroutineYield, or it has not started running yet.
 */
COROUTINE_API CoroutineStatus   Coroutine_status(Coroutine* coroutine);

/**
 * Returns the running coroutine, or NULL when called by the main coroutine of thread.
 */
COROUTINE_API Coroutine*        Coroutine_running(void);

/**
 * Suspends the execution of the calling coroutine, return to the coroutine of the thread.
 */
COROUTINE_API void              Coroutine_yield(void);

#if COROUTINE_YIELD_SHORTNAME
/**
 * Suspends the execution of the calling coroutine.
 */
COROUTINE_API void              yield(void);
#endif

/**
 * Starts or continues the execution of coroutine.
 * Return true if resume success, false is otherwise.
 */
COROUTINE_API bool              Coroutine_resume(Coroutine* coroutine);


/* END OF EXTERN "C" */
#ifdef __cplusplus
}
#endif

/* END OF FILE, LEAVE A NEWLINE */
