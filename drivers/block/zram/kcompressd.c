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
#include <linux/cgroup-defs.h>
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
	bool fifo_freed;
};

static struct kcompressd_para *kcompressd_para;
static DECLARE_RWSEM(kcompressd_config_sem);

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

static int kcompressd(void *para)
{
	struct task_struct *tsk = current;
	struct kcompressd_para *p = (struct kcompressd_para *)para;

	tsk->flags |= PF_MEMALLOC | PF_KSWAPD;

	while (!kthread_should_stop()) {
		if (kfifo_is_empty(p->write_fifo)) {
			atomic_set(p->running, KCOMPRESSD_SLEEPING);
			wait_event_interruptible(*p->kcompressd_wait,
						 !kfifo_is_empty(p->write_fifo) ||
						 kthread_should_stop());
			atomic_set(p->running, KCOMPRESSD_RUNNING);
		}

		if (kthread_should_stop())
			break;

		while (!kfifo_is_empty(p->write_fifo)) {
			struct write_work entry;

			if (sizeof(struct write_work) == kfifo_out(p->write_fifo,
						&entry, sizeof(struct write_work))) {
				entry.cb(entry.mem, entry.bio);
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
			while (i-- > 0)
				kfifo_free(&kcompress[i].write_fifo);
			return -ENOMEM;
		}
	}
	return 0;
}

static void drain_bio_queue(int idx)
{
	struct write_work entry;

	while (sizeof(struct write_work) == kfifo_out(&kcompress[idx].write_fifo,
				&entry, sizeof(struct write_work))) {
		entry.cb(entry.mem, entry.bio);
	}
}

static void clean_bio_queue(int idx)
{
	if (kcompressd_para[idx].fifo_freed)
		return;

	drain_bio_queue(idx);
	kfifo_free(&kcompress[idx].write_fifo);
	kcompressd_para[idx].fifo_freed = true;
}

static void stop_all_kcompressd_thread(void);

static int kcompress_update(void)
{
	int i;
	int ret;

	kcompress = kvmalloc_array(nr_kcompressd, sizeof(struct kcompress), GFP_KERNEL);
	if (!kcompress)
		return -ENOMEM;

	kcompressd_para = kvmalloc_array(nr_kcompressd, sizeof(struct kcompressd_para), GFP_KERNEL);
	if (!kcompressd_para) {
		kvfree(kcompress);
		kcompress = NULL;
		return -ENOMEM;
	}

	ret = init_write_queue();
	if (ret) {
		pr_err("Initialization of writing to FIFOs failed!!\n");
		kvfree(kcompressd_para);
		kcompressd_para = NULL;
		kvfree(kcompress);
		kcompress = NULL;
		return ret;
	}

	for (i = 0; i < nr_kcompressd; i++) {
		init_waitqueue_head(&kcompress[i].kcompressd_wait);
		spin_lock_init(&kcompress[i].fifo_lock);
		kcompressd_para[i].kcompressd_wait = &kcompress[i].kcompressd_wait;
		kcompressd_para[i].write_fifo = &kcompress[i].write_fifo;
		kcompressd_para[i].running = &kcompress[i].running;
		kcompressd_para[i].fifo_freed = false;

		atomic_set(&kcompress[i].running, KCOMPRESSD_SLEEPING);
		kcompress[i].kcompressd = kthread_run(kcompressd,
						&kcompressd_para[i], "kcompressd:%d", i);
		if (IS_ERR(kcompress[i].kcompressd)) {
			pr_err("Failed to create kcompressd:%d\n", i);
			kcompress[i].kcompressd = NULL;
			stop_all_kcompressd_thread();
			kvfree(kcompressd_para);
			kcompressd_para = NULL;
			kvfree(kcompress);
			kcompress = NULL;
			return -ENOMEM;
		}
	}

	return 0;
}

static void stop_all_kcompressd_thread(void)
{
	int i;

	if (!kcompress)
		return;

	for (i = 0; i < nr_kcompressd; i++) {
		if (!IS_ERR_OR_NULL(kcompress[i].kcompressd)) {
			kthread_stop(kcompress[i].kcompressd);
			kcompress[i].kcompressd = NULL;
		}
		clean_bio_queue(i);
	}
}

static int kcompress_reconfig(unsigned int *param, unsigned int new_val)
{
	int ret = 0;

	down_write(&kcompressd_config_sem);

	if (new_val == *param)
		goto out;

	atomic_set(&enable_kcompressd, false);
	stop_all_kcompressd_thread();

	kvfree(kcompress);
	kvfree(kcompressd_para);
	kcompress = NULL;
	kcompressd_para = NULL;

	*param = new_val;

	ret = kcompress_update();
	if (ret)
		pr_err("Reconfiguration failed for value %u\n", new_val);
	else
		atomic_set(&enable_kcompressd, true);

out:
	up_write(&kcompressd_config_sem);
	return ret;
}

static int do_nr_kcompressd_handler(const char *val,
		const struct kernel_param *kp)
{
	unsigned int new_nr;
	int ret;

	ret = kstrtouint(val, 10, &new_nr);
	if (ret < 0) {
		pr_err("Invalid number of kcompressd.\n");
		return ret;
	}

	if (new_nr == 0)
		return -EINVAL;

	ret = kcompress_reconfig(&nr_kcompressd, new_nr);
	if (!ret)
		pr_info("Number of threads for kcompressd was changed: %u\n", new_nr);

	return ret;
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
	unsigned int new_sz;
	int ret;

	ret = kstrtouint(val, 10, &new_sz);
	if (ret < 0) {
		pr_err("Invalid queue size for kcompressd.\n");
		return ret;
	}

	if (new_sz == 0)
		return -EINVAL;

	ret = kcompress_reconfig(&queue_size_per_kcompressd, new_sz);
	if (!ret)
		pr_info("Queue size for kcompressd was changed: %u\n", new_sz);

	return ret;
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

static atomic_t next_kcompressd_idx = ATOMIC_INIT(0);

int schedule_bio_write(void *mem, struct bio *bio, compress_callback cb)
{
	unsigned int i, start_idx, idx, local_nr;
	int ret = -EBUSY;
	bool submit_success = false;
	size_t sz_work = sizeof(struct write_work);

	struct write_work entry = {
		.mem = mem,
		.bio = bio,
		.cb = cb
	};

	if (!current_is_kswapd())
		return -EBUSY;

	down_read(&kcompressd_config_sem);

	if (unlikely(!atomic_read(&enable_kcompressd)))
		goto out;

	local_nr = READ_ONCE(nr_kcompressd);
	if (!local_nr)
		goto out;

	start_idx = (unsigned int)atomic_fetch_inc(&next_kcompressd_idx) % local_nr;

	for (i = 0; i < local_nr; i++) {
		idx = (start_idx + i) % local_nr;
		spin_lock(&kcompress[idx].fifo_lock);
		submit_success =
			(kfifo_avail(&kcompress[idx].write_fifo) >= sz_work) &&
			(sz_work == kfifo_in(&kcompress[idx].write_fifo, &entry, sz_work));
		spin_unlock(&kcompress[idx].fifo_lock);

		if (submit_success) {
			wake_up_interruptible(&kcompress[idx].kcompressd_wait);
			ret = 0;
			goto out;
		}
	}

out:
	up_read(&kcompressd_config_sem);
	return ret;
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
