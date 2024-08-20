/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_SCHED_TYPES_H
#define _UAPI_LINUX_SCHED_TYPES_H

#include <linux/types.h>

struct sched_param {
	int sched_priority;
};

#define SCHED_ATTR_SIZE_VER0	48	/* sizeof first published struct */
#define SCHED_ATTR_SIZE_VER1	56	/* add: util_{min,max} */

/*
 * Extended scheduling parameters data structure.
 *
 * This is needed because the original struct sched_param can not be
 * altered without introducing ABI issues with legacy applications
 * (e.g., in sched_getparam()).
 *
 * However, the possibility of specifying more than just a priority for
 * the tasks may be useful for a wide variety of application fields, e.g.,
 * multimedia, streaming, automation and control, and many others.
 *
 * This variant (sched_attr) allows to define additional attributes to
 * improve the scheduler knowledge about task requirements.
 *
 * Scheduling Class Attributes
 * ===========================
 *
 * A subset of sched_attr attributes specifies the
 * scheduling policy and relative POSIX attributes:
 *
 *  @size		size of the structure, for fwd/bwd compat.
 *
 *  @sched_policy	task's scheduling policy
 *  @sched_nice		task's nice value      (SCHED_NORMAL/BATCH)
 *  @sched_priority	task's static priority (SCHED_FIFO/RR)
 *
 * Certain more advanced scheduling features can be controlled by a
 * predefined set of flags via the attribute:
 *
 *  @sched_flags	for customizing the scheduler behaviour
 *
 * Sporadic Time-Constrained Task Attributes
 * =========================================
 *
 * A subset of sched_attr attributes allows to describe a so-called
 * sporadic time-constrained task.
 *
 * In such a model a task is specified by:
 *  - the activation period or minimum instance inter-arrival time;
 *  - the maximum (or average, depending on the actual scheduling
 *    discipline) computation time of all instances, a.k.a. runtime;
 *  - the deadline (relative to the actual activation time) of each
 *    instance.
 * Very briefly, a periodic (sporadic) task asks for the execution of
 * some specific computation --which is typically called an instance--
 * (at most) every period. Moreover, each instance typically lasts no more
 * than the runtime and must be completed by time instant t equal to
 * the instance activation time + the deadline.
 *
 * This is reflected by the following fields of the sched_attr structure:
 *
 *  @sched_deadline	representative of the task's deadline
 *  @sched_runtime	representative of the task's runtime
 *  @sched_period	representative of the task's period
 *
 * Given this task model, there are a multiplicity of scheduling algorithms
 * and policies, that can be used to ensure all the tasks will make their
 * timing constraints.
 *
 * As of now, the SCHED_DEADLINE policy (sched_dl scheduling class) is the
 * only user of this new interface. More information about the algorithm
 * available in the scheduling class file or in Documentation/.
 *
 * Task Utilization Attributes
 * ===========================
 *
 * A subset of sched_attr attributes allows to specify the utilization
 * expected for a task. These attributes allow to inform the scheduler about
 * the utilization boundaries within which it should schedule the task. These
 * boundaries are valuable hints to support scheduler decisions on both task
 * placement and frequency selection.
 *
 *  @sched_util_min	represents the minimum utilization
 *  @sched_util_max	represents the maximum utilization
 *
 * Utilization is a value in the range [0..SCHED_CAPACITY_SCALE]. It
 * represents the percentage of CPU time used by a task when running at the
 * maximum frequency on the highest capacity CPU of the system. For example, a
 * 20% utilization task is a task running for 2ms every 10ms at maximum
 * frequency.
 *
 * A task with a min utilization value bigger than 0 is more likely scheduled
 * on a CPU with a capacity big enough to fit the specified value.
 * A task with a max utilization value smaller than 1024 is more likely
 * scheduled on a CPU with no more capacity than the specified value.
 *
 * A task utilization boundary can be reset by setting the attribute to -1.
 *
 * Scheduler QoS
 * =============
 *
 * Different workloads have different scheduling requirements to operate
 * optimally. The same applies to tasks within the same workload.
 *
 * To enable smarter usage of system resources and to cater for the conflicting
 * demands of various tasks, Scheduler QoS provides a mechanism to provide more
 * information about those demands so that scheduler can do best-effort to
 * honour them.
 *
 *  @sched_qos_type	what QoS hint to apply
 *  @sched_qos_value	value of the QoS hint
 *  @sched_qos_cookie	magic cookie to tag a group of tasks for which the QoS
 *			applies. If 0, the hint will apply globally system
 *			wide. If not 0, the hint will be relative to tasks that
 *			has the same cookie value only.
 *
 * QoS hints are set once and not inherited by children by design. The
 * rationale is that each task has its individual characteristics and it is
 * encouraged to describe each of these separately. Also since system resources
 * are finite, there's a limit to what can be done to honour these requests
 * before reaching a tipping point where there are too many requests for
 * a particular QoS that is impossible to service for all of them at once and
 * some will start to lose out. For example if 10 tasks require better wake
 * up latencies on a 4 CPUs SMP system, then if they all wake up at once, only
 * 4 can perceive the hint honoured and the rest will have to wait. Inheritance
 * can lead these 10 to become a 100 or a 1000 more easily, and then the QoS
 * hint will lose its meaning and effectiveness rapidly. The chances of 10
 * tasks waking up at the same time is lower than a 100 and lower than a 1000.
 *
 * To set multiple QoS hints, a syscall is required for each. This is a
 * trade-off to reduce the churn on extending the interface as the hope for
 * this to evolve as workloads and hardware get more sophisticated and the
 * need for extension will arise; and when this happen the task should be
 * simpler to add the kernel extension and allow userspace to use readily by
 * setting the newly added flag without having to update the whole of
 * sched_attr.
 *
 * Details about the available QoS hints can be found in:
 * Documentation/scheduler/sched-qos.rst
 */
struct sched_attr {
	__u32 size;

	__u32 sched_policy;
	__u64 sched_flags;

	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;

	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;

	/* SCHED_DEADLINE */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;

	/* Utilization hints */
	__u32 sched_util_min;
	__u32 sched_util_max;

	__u32 sched_qos_type;
	__s64 sched_qos_value;
	__u32 sched_qos_cookie;

};

#endif /* _UAPI_LINUX_SCHED_TYPES_H */
