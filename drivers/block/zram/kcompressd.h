/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _KCOMPRESSD_H_
#define _KCOMPRESSD_H_

#include <linux/rwsem.h>
#include <linux/kfifo.h>
#include <linux/atomic.h>

typedef void (*compress_callback)(void *mem, struct bio *bio);

struct kcompress {
	struct task_struct *kcompressd;
	wait_queue_head_t kcompressd_wait;
	struct kfifo write_fifo;
	atomic_t running;
};

int kcompressd_enabled(void);
int schedule_bio_write(void *mem, struct bio *bio, compress_callback cb);
#endif
