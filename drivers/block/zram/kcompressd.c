// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/freezer.h>
#include <linux/kernel.h>
#include <linux/psi.h>
#include <linux/kfifo.h>
#include <linux/swap.h>
#include <linux/delay.h>

#include "kcompressd.h"

#define INIT_QUEUE_SIZE		4096
#define DEFAULT_NR_KCOMPRESSD	4

static atomic_t enable_kcompressd;
static unsigned int nr_kcompressd;
static unsigned int queue_size_per_kcompressd;
static struct kcompress *kcompress;

enum run_state {
	KCOMPRESSD_NOT_STARTED = 0,
	KCOMPRESSD_RUNNING,
	KCOMPRESSD_SLEEPING,
};

struct kcompressd_para {
	wait_queue_head_t *kcompressd_wait;
	struct kfifo *write_fifo;
	atomic_t *running;
};

static struct kcompressd_para *kcompressd_para;
static BLOCKING_NOTIFIER_HEAD(kcompressd_notifier_list);

struct write_work {
	void *mem;
	struct bio *bio;
	compress_callback cb;
};

int kcompressd_enabled(void)
{
	return likely(atomic_read(&enable_kcompressd));
}
EXPORT_SYMBOL(kcompressd_enabled);

static void kcompressd_try_to_sleep(struct kcompressd_para *p)
{
	DEFINE_WAIT(wait);

	if (!kfifo_is_empty(p->write_fifo))
		return;

	if (freezing(current) || kthread_should_stop())
		return;

	atomic_set(p->running, KCOMPRESSD_SLEEPING);
	prepare_to_wait(p->kcompressd_wait, &wait, TASK_INTERRUPTIBLE);

	/*
	 * After a short sleep, check if it was a premature sleep. If not, then
	 * go fully to sleep until explicitly woken up.
	 */
	if (!kthread_should_stop() && kfifo_is_empty(p->write_fifo))
		schedule();

	finish_wait(p->kcompressd_wait, &wait);
	atomic_set(p->running, KCOMPRESSD_RUNNING);
}

static int kcompressd(void *para)
{
	struct task_struct *tsk = current;
	struct kcompressd_para *p = (struct kcompressd_para *)para;

	tsk->flags |= PF_MEMALLOC | PF_KSWAPD;
	set_freezable();

	while (!kthread_should_stop()) {
		bool ret;

		kcompressd_try_to_sleep(p);
		ret = try_to_freeze();
		if (kthread_should_stop())
			break;

		if (ret)
			continue;

		while (!kfifo_is_empty(p->write_fifo)) {
			struct write_work entry;

			if (sizeof(struct write_work) == kfifo_out(p->write_fifo,
						&entry, sizeof(struct write_work))) {
				entry.cb(entry.mem, entry.bio);
				bio_put(entry.bio);
			}
		}

	}

	tsk->flags &= ~(PF_MEMALLOC | PF_KSWAPD);
	atomic_set(p->running, KCOMPRESSD_NOT_STARTED);
	return 0;
}

static int init_write_queue(void)
{
	int i;
	unsigned int queue_len = queue_size_per_kcompressd * sizeof(struct write_work);

	for (i = 0; i < nr_kcompressd; i++) {
		if (kfifo_alloc(&kcompress[i].write_fifo,
					queue_len, GFP_KERNEL)) {
			pr_err("Failed to alloc kfifo %d\n", i);
			return -ENOMEM;
		}
	}
	return 0;
}

static void clean_bio_queue(int idx)
{
	struct write_work entry;

	while (sizeof(struct write_work) == kfifo_out(&kcompress[idx].write_fifo,
				&entry, sizeof(struct write_work))) {
		bio_put(entry.bio);
		entry.cb(entry.mem, entry.bio);
	}
	kfifo_free(&kcompress[idx].write_fifo);
}

static int kcompress_update(void)
{
	int i;
	int ret;

	kcompress = kvmalloc_array(nr_kcompressd, sizeof(struct kcompress), GFP_KERNEL);
	if (!kcompress)
		return -ENOMEM;

	kcompressd_para = kvmalloc_array(nr_kcompressd, sizeof(struct kcompressd_para), GFP_KERNEL);
	if (!kcompressd_para)
		return -ENOMEM;

	ret = init_write_queue();
	if (ret) {
		pr_err("Initialization of writing to FIFOs failed!!\n");
		return ret;
	}

	for (i = 0; i < nr_kcompressd; i++) {
		init_waitqueue_head(&kcompress[i].kcompressd_wait);
		kcompressd_para[i].kcompressd_wait = &kcompress[i].kcompressd_wait;
		kcompressd_para[i].write_fifo = &kcompress[i].write_fifo;
		kcompressd_para[i].running = &kcompress[i].running;
	}

	return 0;
}

static void stop_all_kcompressd_thread(void)
{
	int i;

	for (i = 0; i < nr_kcompressd; i++) {
		kthread_stop(kcompress[i].kcompressd);
		kcompress[i].kcompressd = NULL;
		clean_bio_queue(i);
	}
}

static int do_nr_kcompressd_handler(const char *val,
		const struct kernel_param *kp)
{
	int ret;

	atomic_set(&enable_kcompressd, false);

	stop_all_kcompressd_thread();

	ret = param_set_int(val, kp);
	if (!ret) {
		pr_err("Invalid number of kcompressd.\n");
		return -EINVAL;
	}

	ret = init_write_queue();
	if (ret) {
		pr_err("Initialization of writing to FIFOs failed!!\n");
		return ret;
	}

	atomic_set(&enable_kcompressd, true);

	return 0;
}

static const struct kernel_param_ops param_ops_change_nr_kcompressd = {
	.set = &do_nr_kcompressd_handler,
	.get = &param_get_uint,
	.free = NULL,
};

module_param_cb(nr_kcompressd, &param_ops_change_nr_kcompressd,
		&nr_kcompressd, 0644);
MODULE_PARM_DESC(nr_kcompressd, "Number of pre-created daemon for page compression");

static int do_queue_size_per_kcompressd_handler(const char *val,
		const struct kernel_param *kp)
{
	int ret;

	atomic_set(&enable_kcompressd, false);

	stop_all_kcompressd_thread();

	ret = param_set_int(val, kp);
	if (!ret) {
		pr_err("Invalid queue size for kcompressd.\n");
		return -EINVAL;
	}

	ret = init_write_queue();
	if (ret) {
		pr_err("Initialization of writing to FIFOs failed!!\n");
		return ret;
	}

	pr_info("Queue size for kcompressd was changed: %d\n", queue_size_per_kcompressd);

	atomic_set(&enable_kcompressd, true);
	return 0;
}

static const struct kernel_param_ops param_ops_change_queue_size_per_kcompressd = {
	.set = &do_queue_size_per_kcompressd_handler,
	.get = &param_get_uint,
	.free = NULL,
};

module_param_cb(queue_size_per_kcompressd, &param_ops_change_queue_size_per_kcompressd,
		&queue_size_per_kcompressd, 0644);
MODULE_PARM_DESC(queue_size_per_kcompressd,
		"Size of queue for kcompressd");

int schedule_bio_write(void *mem, struct bio *bio, compress_callback cb)
{
	int i;
	bool submit_success = false;
	size_t sz_work = sizeof(struct write_work);

	struct write_work entry = {
		.mem = mem,
		.bio = bio,
		.cb = cb
	};

	if (unlikely(!atomic_read(&enable_kcompressd)))
		return -EBUSY;

	if (!nr_kcompressd || !current_is_kswapd())
		return -EBUSY;

	bio_get(bio);

	for (i = 0; i < nr_kcompressd; i++) {
		submit_success =
			(kfifo_avail(&kcompress[i].write_fifo) >= sz_work) &&
			(sz_work == kfifo_in(&kcompress[i].write_fifo, &entry, sz_work));

		if (submit_success) {
			switch (atomic_read(&kcompress[i].running)) {
			case KCOMPRESSD_NOT_STARTED:
				atomic_set(&kcompress[i].running, KCOMPRESSD_RUNNING);
				kcompress[i].kcompressd = kthread_run(kcompressd,
						&kcompressd_para[i], "kcompressd:%d", i);
				if (IS_ERR(kcompress[i].kcompressd)) {
					atomic_set(&kcompress[i].running, KCOMPRESSD_NOT_STARTED);
					pr_warn("Failed to start kcompressd:%d\n", i);
					clean_bio_queue(i);
				}
				break;
			case KCOMPRESSD_RUNNING:
				break;
			case KCOMPRESSD_SLEEPING:
				wake_up_interruptible(&kcompress[i].kcompressd_wait);
				break;
			}
			return 0;
		}
	}

	bio_put(bio);
	return -EBUSY;
}
EXPORT_SYMBOL(schedule_bio_write);

static int __init kcompressd_init(void)
{
	int ret;

	nr_kcompressd = DEFAULT_NR_KCOMPRESSD;
	queue_size_per_kcompressd = INIT_QUEUE_SIZE;

	ret = kcompress_update();
	if (ret) {
		pr_err("Init kcompressd failed!\n");
		return ret;
	}

	atomic_set(&enable_kcompressd, true);
	blocking_notifier_call_chain(&kcompressd_notifier_list, 0, NULL);
	return 0;
}

static void __exit kcompressd_exit(void)
{
	atomic_set(&enable_kcompressd, false);
	stop_all_kcompressd_thread();

	kvfree(kcompress);
	kvfree(kcompressd_para);
}

module_init(kcompressd_init);
module_exit(kcompressd_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Qun-Wei Lin <qun-wei.lin@mediatek.com>");
MODULE_DESCRIPTION("Separate the page compression from the memory reclaiming");
