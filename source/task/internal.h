#ifndef _TASK_INTERNAL_H_
#define _TASK_INTERNAL_H_

#include "core/macros.h"
#include "core/types.h"
#include "core/os.h"
#include "task/task.h"

#define MODULE_NAME "task"

BEGIN_DECLS

struct task_pool {
	struct task_descriptor * list;
	mutex_t mutex;
};

struct task_thread {
	thread_handle handle;
	struct task_pool * in_queue;
	struct task_pool * in_queue_low_priority;
	struct task_pool * done;
	boolean * shutdown_signal;
	mutex_t shutdown_mutex;
};

struct task_ctx {
	struct task_thread * thread_pool;
	struct task_pool in_queue;
	struct task_pool in_queue_low_priority;
	struct task_pool done;
	uint32_t thread_count;
	boolean shutdown_signal;
	mutex_t shutdown_mutex;
};

struct task_descriptor * dequeue_task(struct task_pool * pool);

void add_task_to_pool(
	struct task_pool * pool,
	struct task_descriptor * task,
	boolean is_list);

END_DECLS

#endif /* _TASK_INTERNAL_H_ */