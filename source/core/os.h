#ifndef _OS_H_
#define _OS_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

typedef void * dll_handle;

typedef void * thread_handle;
typedef int(*thread_routine_t)(void * param);

typedef void * mutex_t;

typedef void * timer_t;

/*
 * Creates a new mutex object.
 */
mutex_t create_mutex();

/*
 * Locks mutex.
 */
void lock_mutex(mutex_t m);

/*
 * Unlocks mutex.
 */
void unlock_mutex(mutex_t m);

/*
 * Destroys mutex.
 */
void destroy_mutex(mutex_t m);

/*
 * Attempts to load a dynamically-linked library.
 * If the function succeeds, returns the handle value.
 * If the function fails, returns NULL.
 */
dll_handle load_library(const char * path);

/*
 * Attempts to free the loaded dll.
 * If the function succeeds, returns TRUE.
 * If the function fails, returns FALSE.
 */
boolean free_library(dll_handle handle);

/*
 * Attempts to retrieve a symbol address from dll handle.
 * If the function succeeeds, returns symbol address.
 * If the function fails, returns NULL.
 */
void * get_sym_addr(dll_handle handle, const char * sym);

typedef void * window;

window create_window(
	const char * title,
	uint32_t width,
	uint32_t height);

boolean poll_window_events(window window);

void destroy_window(window window);

void * get_native_window_handle(window window);

boolean is_window_open(window window);

/*
 * Creates a new thread.
 * If thread was successfully created, returns thread handle,
 * otherwise, returns NULL.
 */
thread_handle create_thread(
	thread_routine_t routine,
	void * param);

/*
 * Returns when thread is terminated.
 * If thread was already terminated, returns immediately.
 */
void wait_thread(thread_handle handle);

/*
 * Returns current thread id.
 */
uint64_t get_current_thread_id();

/*
 * Returns number of cpu cores in the system.
 */
uint32_t get_cpu_core_count();

/*
 * Suspends the execution of the current thread 
 * for at least until the time-out interval elapses.
 */
void sleep(uint32_t ms);

/*
 * If successful, returns a high-resolution timer object.
 * Returns NULL otherwise.
 */
timer_t create_timer();

/*
 * Returns the number of nanoseconds since the 
 * timer was created or this function was called.
 *
 * If `reset` is set to TRUE, timer will be reset.
 */
uint64_t timer_delta(timer_t timer, boolean reset);

uint64_t timer_delta_no_reset(timer_t timer);

END_DECLS

#endif /* _OS_H_ */