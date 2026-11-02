/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mm

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_MM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_MM_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_RESTRICTED_HOOK(android_rvh_bitmap_find_best_next_area,
		TP_PROTO(unsigned long *bitmap,
			unsigned long bitmap_maxno,
			unsigned long start,
			unsigned int bitmap_count,
			unsigned long mask,
			unsigned long offset,
			unsigned long *bitmap_no,
			bool status),
		TP_ARGS(bitmap, bitmap_maxno, start, bitmap_count, mask,
			offset, bitmap_no, status), 1);

DECLARE_HOOK(android_vh_cma_alloc_retry,
	TP_PROTO(char *name, int *retry),
	TP_ARGS(name, retry));

DECLARE_HOOK(android_vh_filemap_fault_end,
	TP_PROTO(struct file *file, pgoff_t pgoff),
	TP_ARGS(file, pgoff));

DECLARE_HOOK(android_vh_filemap_fault_start,
	TP_PROTO(struct file *file, pgoff_t pgoff),
	TP_ARGS(file, pgoff));

DECLARE_HOOK(android_vh_get_page_wmark,
	TP_PROTO(unsigned int alloc_flags, unsigned long *page_wmark),
	TP_ARGS(alloc_flags, page_wmark));

DECLARE_HOOK(android_vh_page_add_new_anon_rmap,
	TP_PROTO(struct page *page, struct vm_area_struct *vma,
		unsigned long address),
	TP_ARGS(page, vma, address));

DECLARE_HOOK(android_vh_page_cache_readahead_end,
	TP_PROTO(struct file *file, pgoff_t pgoff),
	TP_ARGS(file, pgoff));

DECLARE_HOOK(android_vh_page_cache_readahead_start,
	TP_PROTO(struct file *file, pgoff_t pgoff,
		unsigned int size, bool sync),
	TP_ARGS(file, pgoff, size, sync));

DECLARE_HOOK(android_vh_tune_mmap_readaround,
	TP_PROTO(unsigned int ra_pages, pgoff_t pgoff,
		pgoff_t *start, unsigned int *size, unsigned int *async_size),
	TP_ARGS(ra_pages, pgoff, start, size, async_size));

#endif /* _TRACE_HOOK_MM_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
