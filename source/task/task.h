#ifndef _TASK_H_
#define _TASK_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

typedef boolean(*task_work_t)(void * data);

typedef void(*task_post_t)(
	struct task_descriptor * task,
	void * data,
	boolean result);

struct task_descriptor {
	task_work_t work_cb;
	task_post_t post_cb;
	void * data;
	boolean result;
	struct task_descriptor * next;
};

boolean task_startup();

void task_shutdown();

/*
 * Queue-up a new task.
 */
void task_add(struct task_descriptor * task, boolean low_priority);

/*
 * Queue-up a task chain.
 *
 * This function assumes that the `next` field of 
 * `struct task_descriptor` is arranged correctly.
 */
void task_add_list(
	struct task_descriptor * task,
	boolean low_priority);

/*
 * If task queue (only regular) is empty, returns immediately.
 *
 * Otherwise, consumes tasks in queue until it is empty.
 */
void task_wait();

/*
 * If task queue (regular and low-priority) is empty, 
 * returns immediately.
 *
 * Otherwise, consumes tasks in queue until it is empty.
 */
void task_wait_all();

/*
 * Triggers post task callbacks.
 */
void task_do_post_cb();

END_DECLS

#endif /* _TASK_H_ */