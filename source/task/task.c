#include "task/task.h"

#include "internal.h"
#include "core/core.h"
#include "core/log.h"
#include "core/malloc.h"
#include <string.h>

static struct task_ctx * g_Ctx;

static int task_thread_routine(void * param)
{
	struct task_thread * tt = param;
	boolean shutdown = FALSE;
	while(!shutdown) {
		struct task_descriptor * task = dequeue_task(tt->in_queue);
		if (!task)
			task = dequeue_task(tt->in_queue_low_priority);
		if (task) {
			task->result = task->work_cb(task->data);
			if (task->post_cb)
				add_task_to_pool(tt->done, task, FALSE);
		}
		lock_mutex(tt->shutdown_mutex);
		shutdown = *tt->shutdown_signal;
		unlock_mutex(tt->shutdown_mutex);
		if (!task)
			sleep(1);
	}
	INFO("Terminating task thread..");
	return 0;
}

static boolean init_thread_pool(struct task_ctx * ctx)
{
	uint32_t i;
	size_t sz = ctx->thread_count * sizeof(struct task_thread);
	ctx->thread_pool = alloc(sz);
	memset(ctx->thread_pool, 0, sz);
	for (i = 0; i < ctx->thread_count; i++) {
		struct task_thread * tt = &ctx->thread_pool[i];
		tt->in_queue = &ctx->in_queue;
		tt->in_queue_low_priority = &ctx->in_queue_low_priority;
		tt->done = &ctx->done;
		tt->shutdown_signal = &ctx->shutdown_signal;
		tt->shutdown_mutex = ctx->shutdown_mutex;
		tt->handle = create_thread(task_thread_routine, tt);
		if (!tt->handle) {
			ERROR("Failed to create task thread.");
			return FALSE;
		}
	}
	return TRUE;
}

/*
 * Non-deterministic, only useful in certain situations.
 */
static boolean is_queue_empty(struct task_pool * pool)
{
	struct task_descriptor * task = NULL;
	lock_mutex(pool->mutex);
	task = pool->list;
	unlock_mutex(pool->mutex);
	return (task == NULL);
}

static struct task_descriptor * dequeue_task(
	struct task_pool * pool)
{
	struct task_descriptor * task = NULL;
	lock_mutex(pool->mutex);
	task = pool->list;
	if (task)
		pool->list = task->next;
	unlock_mutex(pool->mutex);
	return task;
}

static void complete_task(
	struct task_ctx * ctx,
	struct task_descriptor * task)
{
	struct task_descriptor * cur;
	struct task_descriptor * last = NULL;
	task->next = NULL;
	lock_mutex(ctx->done.mutex);
	cur = ctx->done.list;
	while (cur) {
		last = cur;
		cur = cur->next;
	}
	if (!last)
		ctx->done.list = task;
	else
		last->next = task;
	unlock_mutex(ctx->done.mutex);
}

static void add_task_to_pool(
	struct task_pool * pool,
	struct task_descriptor * task,
	boolean is_list)
{
	struct task_descriptor * cur;
	struct task_descriptor * last = NULL;
	if (!is_list)
		task->next = NULL;
	lock_mutex(pool->mutex);
	cur = pool->list;
	while (cur) {
		last = cur;
		cur = cur->next;
	}
	if (!last)
		pool->list = task;
	else
		last->next = task;
	unlock_mutex(pool->mutex);
}

static void consume_queue(
	struct task_ctx * ctx,
	struct task_pool * pool)
{
	while (TRUE) {
		struct task_descriptor * task = dequeue_task(pool);
		if (!task)
			break;
		task->result = task->work_cb(task->data);
		if (task->post_cb)
			add_task_to_pool(&ctx->done, task, FALSE);
	}
	task_do_post_cb();
}

boolean task_startup()
{
	struct task_ctx * ctx = alloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));
	ctx->in_queue.mutex = create_mutex();
	ctx->in_queue_low_priority.mutex = create_mutex();
	ctx->done.mutex = create_mutex();
	ctx->shutdown_mutex = create_mutex();
	ctx->thread_count = get_cpu_core_count();
	if (ctx->thread_count)
		ctx->thread_count--;
	if (ctx->thread_count && !init_thread_pool(ctx))
		return FALSE;
	g_Ctx = ctx;
	return TRUE;
}

void task_shutdown()
{
	uint32_t i;
	lock_mutex(g_Ctx->shutdown_mutex);
	g_Ctx->shutdown_signal = TRUE;
	unlock_mutex(g_Ctx->shutdown_mutex);
	for (i = 0; i < g_Ctx->thread_count; i++)
		wait_thread(g_Ctx->thread_pool[i].handle);
}

void task_add(struct task_descriptor * task, boolean low_priority)
{
	if (low_priority) {
		add_task_to_pool(&g_Ctx->in_queue_low_priority, task,
			FALSE);
	}
	else {
		add_task_to_pool(&g_Ctx->in_queue, task, FALSE);
	}
}

void task_add_list(
	struct task_descriptor * task,
	boolean low_priority)
{
	if (low_priority) {
		add_task_to_pool(&g_Ctx->in_queue_low_priority, task,
			TRUE);
	}
	else {
		add_task_to_pool(&g_Ctx->in_queue, task, TRUE);
	}
}

void task_wait()
{
	struct task_ctx * ctx = g_Ctx;
	while (!is_queue_empty(&ctx->in_queue))
		consume_queue(ctx, &ctx->in_queue);
}

void task_wait_all()
{
	struct task_ctx * ctx = g_Ctx;
	while (!is_queue_empty(&ctx->in_queue) ||
		!is_queue_empty(&ctx->in_queue_low_priority) ||
		!is_queue_empty(&ctx->done)) {
		consume_queue(ctx, &ctx->in_queue);
		consume_queue(ctx, &ctx->in_queue_low_priority);
	}
}

void task_do_post_cb()
{
	struct task_ctx * ctx = g_Ctx;
	while (TRUE) {
		struct task_descriptor * task = dequeue_task(&ctx->done);
		if (!task)
			break;
		task->post_cb(task, task->data, task->result);
	}
}
