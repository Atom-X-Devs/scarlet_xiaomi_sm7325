/* SPDX-License-Identifier: GPL-2.0 */

/*
 * SCHED_DEADLINE tasks has negative priorities, reflecting
 * the fact that any of them has higher prio than RT and
 * NORMAL/BATCH tasks.
 */

#define MAX_DL_PRIO		0

static inline int dl_prio(int prio)
{
	if (unlikely(prio < MAX_DL_PRIO))
		return 1;
	return 0;
}

static inline int dl_task(struct task_struct *p)
{
	return dl_prio(p->prio);
}

static inline bool dl_time_before(u64 a, u64 b)
{
	return (s64)(a - b) < 0;
}

#ifdef CONFIG_SMP

struct root_domain;
extern void dl_add_task_root_domain(struct task_struct *p);
extern void dl_clear_root_domain(struct root_domain *rd);
/*
 * Return whether moving DL task @p to @new_mask requires moving DL
 * bandwidth accounting between root domains. This helper is specific to
 * DL bandwidth move accounting semantics and is shared by
 * cpuset_can_attach() and set_cpus_allowed_dl() so both paths use the
 * same source root-domain test.
 */
extern bool dl_task_needs_bw_move(struct task_struct *p,
				  const struct cpumask *new_mask);

#endif /* CONFIG_SMP */
