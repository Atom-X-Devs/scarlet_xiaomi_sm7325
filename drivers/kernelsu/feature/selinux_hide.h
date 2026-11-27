// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 \xx
 *
 * This file is a downstream extension and NOT affiliated, endorsed by,
 * or maintained by the official KernelSU developers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

// selinux_hide's list, hand-rolled hazard pointers via C11 atomics

#ifndef __KSU_H_SELINUX_HIDE
#define __KSU_H_SELINUX_HIDE

void ksu_selinux_hide_init();
void ksu_selinux_hide_exit();

static int sepol_expected_argc(u32 cmd);

struct ksu_hide_entry {
	u16 len;
	char name[];
};

static __always_inline size_t ksu_hide_entry_step(size_t name_len)
{
	return ALIGN(sizeof(struct ksu_hide_entry) + name_len + 1, sizeof(u16));
}

// flex array
struct ksu_hide_buf {
	size_t len;
	char data[];
} __attribute__((aligned(sizeof(size_t))));

// types
// [struct ksu_hide_entry (u16 len, name\0, align pad)] ...
static struct ksu_hide_buf *ksu_hide_type_list __read_mostly = nullptr;

// rules
// [src ksu_hide_entry][tgt ksu_hide_entry] ...
static struct ksu_hide_buf *ksu_hide_rule_list __read_mostly = nullptr;

static DEFINE_MUTEX(selinux_hide_list_mutex);

static struct ksu_hide_buf *__percpu *ksu_selinux_hide_hazptr_slot __read_mostly = nullptr;

static inline void ksu_selinux_hide_alloc_hazptr_slot(void)
{
	ksu_selinux_hide_hazptr_slot = alloc_percpu(struct ksu_hide_buf *);

	// asshole assert nofail.
	if (!ksu_selinux_hide_hazptr_slot) {
		__builtin_trap();
		__builtin_unreachable();
	}

	unsigned int cpu;
	for_each_possible_cpu(cpu) {
		struct ksu_hide_buf **slot = per_cpu_ptr(ksu_selinux_hide_hazptr_slot, cpu);
		pr_info("selinux_hide: hazptr_slot: 0x%lx cpu: %u \n", (uintptr_t)slot, cpu );
	}
}

// NOTE: can ret null on uninitialized state
static inline struct ksu_hide_buf *ksu_selinux_hide_get_buf(struct ksu_hide_buf **g_buf)
{
	// reader has to pin its own slot
	preempt_disable();
	struct ksu_hide_buf **slot = this_cpu_ptr(ksu_selinux_hide_hazptr_slot);
	struct ksu_hide_buf *buf;

check_buf:
	buf = __atomic_load_n(g_buf, __ATOMIC_ACQUIRE);
	__atomic_store_n(slot, buf, __ATOMIC_RELAXED);
	smp_mb();

	if (buf != __atomic_load_n(g_buf, __ATOMIC_ACQUIRE))
		goto check_buf;

	return buf;
}

static inline void ksu_selinux_hide_put_buf(struct ksu_hide_buf **unused)
{
	(void)unused;
	struct ksu_hide_buf **slot = this_cpu_ptr(ksu_selinux_hide_hazptr_slot);
	__atomic_store_n(slot, NULL, __ATOMIC_RELEASE);
	
	preempt_enable();
}

static inline void ksu_selinux_hide_hazptr_free(struct ksu_hide_buf *old_ptr)
{
	if (!old_ptr)
		return;

	smp_mb();

	// acquire it on ALL cpus!
	// only free it once ALL slots say that their slot no longer contains old ptr
	unsigned int cpu;
	for_each_possible_cpu(cpu) {
		struct ksu_hide_buf **slot = per_cpu_ptr(ksu_selinux_hide_hazptr_slot, cpu);
		while (__atomic_load_n(slot, __ATOMIC_ACQUIRE) == old_ptr)
			cpu_relax();
	}

	kfree(old_ptr);
}

static noinline void ksu_add_shit_to_list(u32 cmd, const char *args[])
{
	if (!args || !args[0])
		return;

	guarded_mutex_lock(&selinux_hide_list_mutex);

	int argc = sepol_expected_argc(cmd);

	if (cmd == KSU_SEPOLICY_CMD_TYPE || cmd == KSU_SEPOLICY_CMD_TYPE_ATTR || cmd == KSU_SEPOLICY_CMD_TYPE_STATE || cmd == KSU_SEPOLICY_CMD_ATTR) {
		const char *name = args[0];
		size_t name_len = strlen(name);
		if (!name_len || name_len >= U16_MAX)
			return;

		size_t needed_len = ksu_hide_entry_step(name_len);

		if (!ksu_hide_type_list)
			goto skip_type_dup_check;

		// anti duplicate
		size_t offset = 0;
		while (ksu_hide_type_list->len > offset) {
			const struct ksu_hide_entry *e = (const struct ksu_hide_entry *)(ksu_hide_type_list->data + offset);
			if (e->len == name_len && !memcmp(e->name, name, name_len))
				return;

			offset += ksu_hide_entry_step(e->len);
		}

	skip_type_dup_check:;
		size_t old_len = (ksu_hide_type_list) ? ksu_hide_type_list->len : 0;
		size_t new_total_len = old_len + needed_len;

		struct ksu_hide_buf *new_ptr = kzalloc(sizeof(*new_ptr) + new_total_len, GFP_KERNEL | __GFP_NOFAIL);
		new_ptr->len = new_total_len;

		if (ksu_hide_type_list && old_len > 0)
			memcpy(new_ptr->data, ksu_hide_type_list->data, old_len);

		struct ksu_hide_entry *w = (struct ksu_hide_entry *)(new_ptr->data + old_len);
		w->len = (u16)name_len;
		memcpy(w->name, name, name_len + 1);

		struct ksu_hide_buf *old_ptr = __atomic_exchange_n(&ksu_hide_type_list, new_ptr, __ATOMIC_ACQ_REL);
		ksu_selinux_hide_hazptr_free(old_ptr);

		pr_info("selinux_hide: tracking type: %s\n", w->name);
	} else if (argc >= 2) {
		if (!args[1])
			return;

		const char *src = args[0];
		const char *tgt = args[1];
		size_t src_len = strlen(src);
		size_t tgt_len = strlen(tgt);
		if (!src_len || !tgt_len || src_len >= U16_MAX || tgt_len >= U16_MAX)
			return;

		size_t src_step = ksu_hide_entry_step(src_len);
		size_t tgt_step = ksu_hide_entry_step(tgt_len);
		size_t needed_len = src_step + tgt_step;

		if (!ksu_hide_rule_list)
			goto skip_rule_dup_check;

		// anti duplicate
		size_t offset = 0;
		while (ksu_hide_rule_list->len > offset) {
			const struct ksu_hide_entry *src_e = (const struct ksu_hide_entry *)(ksu_hide_rule_list->data + offset);
			size_t cur_src_step = ksu_hide_entry_step(src_e->len);
			const struct ksu_hide_entry *tgt_e = (const struct ksu_hide_entry *)(ksu_hide_rule_list->data + offset + cur_src_step);
			size_t cur_tgt_step = ksu_hide_entry_step(tgt_e->len);

			if (src_e->len == src_len && tgt_e->len == tgt_len &&
			    !memcmp(src_e->name, src, src_len) &&
			    !memcmp(tgt_e->name, tgt, tgt_len))
				return;

			offset += cur_src_step + cur_tgt_step;
		}

	skip_rule_dup_check:;
		size_t old_len = (ksu_hide_rule_list) ? ksu_hide_rule_list->len : 0;
		size_t new_total_len = old_len + needed_len;

		struct ksu_hide_buf *new_ptr = kzalloc(sizeof(*new_ptr) + new_total_len, GFP_KERNEL | __GFP_NOFAIL);
		new_ptr->len = new_total_len;

		if (ksu_hide_rule_list && old_len > 0)
			memcpy(new_ptr->data, ksu_hide_rule_list->data, old_len);

		struct ksu_hide_entry *w_src = (struct ksu_hide_entry *)(new_ptr->data + old_len);
		w_src->len = (u16)src_len;
		memcpy(w_src->name, src, src_len + 1);

		struct ksu_hide_entry *w_tgt = (struct ksu_hide_entry *)(new_ptr->data + old_len + src_step);
		w_tgt->len = (u16)tgt_len;
		memcpy(w_tgt->name, tgt, tgt_len + 1);

		struct ksu_hide_buf *old_ptr = __atomic_exchange_n(&ksu_hide_rule_list, new_ptr, __ATOMIC_ACQ_REL);
		ksu_selinux_hide_hazptr_free(old_ptr);

		pr_info("selinux_hide: tracking rule: %s %s\n", w_src->name, w_tgt->name);
	}

	return;
}

static __always_inline bool ksu_sel_isspace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static __always_inline bool ksu_memeq_ct(const char *a, const char *b, size_t len)
{
	unsigned char diff = 0;
	for (size_t i = 0; i < len; i++)
		diff |= (unsigned char)(a[i] ^ b[i]);
	return diff == 0;
}

static __always_inline size_t ksu_extract_context_type(const char *str, size_t limit, size_t *type_len)
{
	*type_len = 0;
	if (!str || !limit)
		return 0;

	size_t i = 0;
	while (i < limit && str[i] != '\0' && !ksu_sel_isspace(str[i]) && str[i] != ':')
		i++;
	if (i >= limit || str[i] != ':')
		return 0;
	i++;

	while (i < limit && str[i] != '\0' && !ksu_sel_isspace(str[i]) && str[i] != ':')
		i++;
	if (i >= limit || str[i] != ':')
		return 0;
	i++;

	size_t start = i;
	while (i < limit && str[i] != '\0' && !ksu_sel_isspace(str[i]) && str[i] != ':')
		i++;

	*type_len = i - start;
	return (*type_len > 0) ? start : 0;
}

static __always_inline bool ksu_parse_access_tokens(const char *str, size_t limit,
						    size_t *t1_off, size_t *t1_len,
						    size_t *t2_off, size_t *t2_len)
{
	*t1_off = 0;
	*t1_len = 0;
	*t2_off = 0;
	*t2_len = 0;
	if (!str || !limit)
		return false;

	size_t i = 0;
	while (i < limit && str[i] != '\0' && ksu_sel_isspace(str[i]))
		i++;

	size_t tok1_start = i;
	while (i < limit && str[i] != '\0' && !ksu_sel_isspace(str[i]))
		i++;
	size_t tok1_end = i;

	while (i < limit && str[i] != '\0' && ksu_sel_isspace(str[i]))
		i++;

	size_t tok2_start = i;
	while (i < limit && str[i] != '\0' && !ksu_sel_isspace(str[i]))
		i++;
	size_t tok2_end = i;

	if (tok1_end > tok1_start) {
		size_t len = 0;
		size_t rel = ksu_extract_context_type(str + tok1_start, tok1_end - tok1_start, &len);
		if (len > 0) {
			*t1_off = tok1_start + rel;
			*t1_len = len;
		}
	}

	if (tok2_end > tok2_start) {
		size_t len = 0;
		size_t rel = ksu_extract_context_type(str + tok2_start, tok2_end - tok2_start, &len);
		if (len > 0) {
			*t2_off = tok2_start + rel;
			*t2_len = len;
		}
	}

	return (*t1_len > 0) || (*t2_len > 0);
}

static __always_inline bool ksu_type_is_hidden(const char *type_ptr, size_t type_len)
{
	if (!type_ptr || !type_len)
		return false;

	bool matched = false;
	struct ksu_hide_buf *type_buf __cleanup(ksu_selinux_hide_put_buf) = ksu_selinux_hide_get_buf(&ksu_hide_type_list);
	if (unlikely(!type_buf))
		return false;

	size_t offset = 0;
	while (type_buf->len > offset) {
		const struct ksu_hide_entry *e = (const struct ksu_hide_entry *)(type_buf->data + offset);
		if (e->len == type_len)
			matched |= ksu_memeq_ct(type_ptr, e->name, type_len);
		offset += ksu_hide_entry_step(e->len);
	}

	return matched;
}

static __always_inline bool ksu_rule_is_hidden(const char *src_type, size_t src_len,
					       const char *tgt_type, size_t tgt_len)
{
	if (!src_type || !src_len || !tgt_type || !tgt_len)
		return false;

	bool matched = false;
	struct ksu_hide_buf *rule_buf __cleanup(ksu_selinux_hide_put_buf) = ksu_selinux_hide_get_buf(&ksu_hide_rule_list);
	if (unlikely(!rule_buf))
		return false;

	size_t offset = 0;
	while (rule_buf->len > offset) {
		const struct ksu_hide_entry *src_e = (const struct ksu_hide_entry *)(rule_buf->data + offset);
		size_t src_step = ksu_hide_entry_step(src_e->len);
		const struct ksu_hide_entry *tgt_e = (const struct ksu_hide_entry *)(rule_buf->data + offset + src_step);
		size_t tgt_step = ksu_hide_entry_step(tgt_e->len);

		if (src_e->len == src_len && tgt_e->len == tgt_len) {
			bool src_hit = ksu_memeq_ct(src_type, src_e->name, src_len);
			bool tgt_hit = ksu_memeq_ct(tgt_type, tgt_e->name, tgt_len);
			matched |= (bool)((unsigned int)src_hit & (unsigned int)tgt_hit);
		}
		offset += src_step + tgt_step;
	}

	return matched;
}

static __always_inline void ksu_corrupt_type_char(char *type_ptr, bool do_corrupt)
{
	char orig = type_ptr[0];
	char repl = (orig != '6') ? '6' : '7';
	type_ptr[0] = do_corrupt ? repl : orig;
}

static __always_inline void replace_after_second_colon(char *str, size_t bound)
{
	size_t type_len = 0;
	size_t type_off = ksu_extract_context_type(str, bound, &type_len);
	if (type_len > 0)
		ksu_corrupt_type_char(str + type_off, true);
}

static __always_inline bool ksu_sanitize_single_context(char *str, size_t limit)
{
	size_t type_len = 0;
	size_t type_off = ksu_extract_context_type(str, limit, &type_len);
	if (!type_len)
		return false;

	bool hidden = ksu_type_is_hidden(str + type_off, type_len);
	ksu_corrupt_type_char(str + type_off, hidden);
	return hidden;
}

static __always_inline bool ksu_sanitize_access_buf(char *str, size_t limit)
{
	size_t t1_off = 0, t1_len = 0, t2_off = 0, t2_len = 0;
	if (!ksu_parse_access_tokens(str, limit, &t1_off, &t1_len, &t2_off, &t2_len))
		return false;

	bool hit1 = ksu_type_is_hidden(str + t1_off, t1_len);
	bool hit2 = ksu_type_is_hidden(str + t2_off, t2_len);
	bool hit_rule = ksu_rule_is_hidden(str + t1_off, t1_len, str + t2_off, t2_len);

	if (hit1)
		ksu_corrupt_type_char(str + t1_off, true);
	if (hit2 || hit_rule)
		ksu_corrupt_type_char(str + t2_off, true);

	return hit1 || hit2 || hit_rule;
}

static __always_inline bool ksu_should_destroy_context(const char *str, size_t limit, const bool types_only)
{
	if (!str || !limit)
		return false;

	if (types_only) {
		size_t type_len = 0;
		size_t type_off = ksu_extract_context_type(str, limit, &type_len);
		if (!type_len)
			return false;
		return ksu_type_is_hidden(str + type_off, type_len);
	}

	size_t t1_off = 0, t1_len = 0, t2_off = 0, t2_len = 0;
	if (!ksu_parse_access_tokens(str, limit, &t1_off, &t1_len, &t2_off, &t2_len))
		return false;

	if (ksu_type_is_hidden(str + t1_off, t1_len) ||
	    ksu_type_is_hidden(str + t2_off, t2_len))
		return true;

	return ksu_rule_is_hidden(str + t1_off, t1_len, str + t2_off, t2_len);
}

static noinline void do_useless_alloc(size_t sz)
{
	volatile uint32_t random = get_random_u32();
	volatile uint8_t *s = (volatile uint8_t *)&random;
	volatile int n = ((s[0] ?: 64) + (s[1] ?: 32) + (s[2] ?: 16) + (s[3] ?: 8)) / 2;
	volatile char *stack = __builtin_alloca(n);

	if (sz > 256)
		sz = 256;

	volatile char *heap __zoffstack(sz * 4 + n);
	memset_explicit(heap, s[0], sz * 4 + n);
	memset_explicit(stack, s[1], n);
}

#endif
