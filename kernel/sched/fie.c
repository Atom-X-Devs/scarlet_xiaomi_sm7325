// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024-2025 Sultan Alsawaf <sultan@kerneltoast.com>.
 */

#include <linux/cpufreq.h>
#include <linux/perf_event.h>
#include <linux/reboot.h>
#include <linux/sched/topology.h>
#include <asm/arch_timer.h>
#include <trace/hooks/cpuidle.h>
#include <trace/hooks/sched.h>
#include "sched.h"

/* Minimum sample time in nanoseconds */
#define CPU_MIN_SAMPLE_NS (100 * NSEC_PER_USEC)

/*
 * CNTPCT_EL0 arithmetic helpers to avoid overflowing a u64 when converting
 * between ticks and nanoseconds. This avoids needing mult_frac() in a hot path.
 */
static u64 cntpct_mult __read_mostly;
static u64 cntpct_div __read_mostly;
static u64 cntpct_rate __read_mostly;
static u64 cpu_min_sample_cntpct __read_mostly;

static u64 cntpct_to_ns(u64 cntpct)
{
	return cntpct * cntpct_mult / cntpct_div;
}

static u64 ns_to_cntpct(u64 ns)
{
	return DIV_ROUND_UP_ULL(ns * cntpct_div, cntpct_mult);
}

static void calc_cntpct_arith(void)
{
	int cd;

	/*
	 * Calculate lossless arithmetic to convert between timer ticks and
	 * nanoseconds, extracting all common denominators up through 10.
	 */
	cntpct_rate = arch_timer_get_rate();
	cntpct_mult = NSEC_PER_SEC;
	cntpct_div = cntpct_rate;
	for (cd = 10; cd > 1; cd--) {
		while (!(cntpct_mult % cd) && !(cntpct_div % cd)) {
			cntpct_div /= cd;
			cntpct_mult /= cd;
		}
	}

	/* Compute all nanosecond time intervals in terms of CNTPCT_EL0 ticks */
	cpu_min_sample_cntpct = ns_to_cntpct(CPU_MIN_SAMPLE_NS);
}

/*
 * The generic timer counter (CNTPCT_EL0) is read directly for the lowest
 * possible latency incurred from reading the current time, as well as the
 * greatest precision since we can convert the number of ticks into nanoseconds
 * without sched_clock()'s approximation that aims to do the conversion as
 * quickly as possible at a loss of precision. The preceeding ISB prevents
 * speculative reads of the counter register.
 */
static inline u64 get_cntpct(void)
{
	u64 val;
	isb();
	val = __arch_counter_get_cntpct();
	isb();
	return val;
}

/* The PMU event stats */
struct pmu_stat {
	u64 cpu_cyc;
	u64 cntpct;
};

/*
 * Scale Frequency Data: accumulated CPU cycles and CNTPCT ticks,
 * excluding idle time. The lock protects against concurrent access
 * when a remote runqueue clock update triggers recursion.
 */
struct sfd_data {
	raw_spinlock_t lock;
	u64 cpu_cyc;
	u64 const_cyc;
	bool stale;
};

struct cpu_pmu {
	raw_spinlock_t lock; /* protects cur/prev */
	struct pmu_stat cur;
	struct pmu_stat prev;
	struct sfd_data sfd;
};

static DEFINE_PER_CPU(struct cpu_pmu, cpu_pmu_evs) = {
	.lock = __RAW_SPIN_LOCK_UNLOCKED(cpu_pmu_evs.lock),
	.sfd.lock = __RAW_SPIN_LOCK_UNLOCKED(cpu_pmu_evs.sfd.lock)
};

static DEFINE_STATIC_KEY_FALSE(fie_ready);
static int cpuhp_state;

/* Maximum frequency for each CPU (kHz), populated from cpufreq */
static u32 max_freqs[NR_CPUS] __ro_after_init;

/* Register a perf event for CPU_CYCLES so the PMU is enabled */
enum pmu_events {
	CPU_CYCLES,
	PMU_EVT_MAX
};

static const u32 pmu_evt_id[PMU_EVT_MAX] = {
	[CPU_CYCLES] = ARMV8_PMUV3_PERFCTR_CPU_CYCLES
};

struct cpu_pmu_evt {
	struct perf_event *pev[PMU_EVT_MAX];
};

static DEFINE_PER_CPU(struct cpu_pmu_evt, pevt_pcpu);

static struct perf_event *create_pev(struct perf_event_attr *attr, int cpu)
{
	return perf_event_create_kernel_counter(attr, cpu, NULL, NULL, NULL);
}

static void release_perf_events(int cpu)
{
	struct cpu_pmu_evt *cpev = &per_cpu(pevt_pcpu, cpu);
	int i;

	for (i = 0; i < PMU_EVT_MAX; i++) {
		if (IS_ERR(cpev->pev[i]))
			break;

		perf_event_release_kernel(cpev->pev[i]);
	}
}

static int create_perf_events(int cpu)
{
	struct cpu_pmu_evt *cpev = &per_cpu(pevt_pcpu, cpu);
	struct perf_event_attr attr = {
		.type = PERF_TYPE_RAW,
		.size = sizeof(attr),
		.pinned = 1,
		/*
		 * Request a long counter (i.e., 64-bit instead of 32-bit) by
		 * setting bit 0 in config1. See armv8pmu_event_is_64bit().
		 */
		.config1 = 0x1
	};
	int i;

	for (i = 0; i < PMU_EVT_MAX; i++) {
		attr.config = pmu_evt_id[i];
		cpev->pev[i] = create_pev(&attr, cpu);
		if (WARN_ON(IS_ERR(cpev->pev[i])))
			goto release_pevs;
	}

	return 0;

release_pevs:
	release_perf_events(cpu);
	return PTR_ERR(cpev->pev[i]);
}

/*
 * Read the CPU cycle counter. If AMU is directly accessible from EL0, we use it.
 * Otherwise fall back to the perf event for PMU.
 */
static u64 read_cpu_cycles(void)
{
#ifdef SYS_AMEVCNTR0_CORE_EL0
	return read_sysreg_s(SYS_AMEVCNTR0_CORE_EL0);
#else
	struct cpu_pmu_evt *cpev = this_cpu_ptr(&pevt_pcpu);
	struct perf_event *event = cpev->pev[CPU_CYCLES];

	event->pmu->read(event);
	return local64_read(&event->count);
#endif
}

static void fie_read_counters(struct pmu_stat *stat)
{
	stat->cntpct = get_cntpct();
	stat->cpu_cyc = read_cpu_cycles();
}

/* The sfd helpers must be called with sfd->lock held */
static void reset_sfd_data(struct sfd_data *sfd)
{
	sfd->cpu_cyc = sfd->const_cyc = 0;
	sfd->stale = false;
}

static void add_sfd_data(struct sfd_data *sfd, u64 delta_cyc, u64 delta_cntpct)
{
	/*
	 * Check the delta since the last reading and ditch any stale readings
	 * if this sample window is sufficiently large.
	 */
	if (sfd->stale && delta_cntpct >= cpu_min_sample_cntpct)
		reset_sfd_data(sfd);

	/* Accumulate data for calculating the CPU's frequency */
	sfd->cpu_cyc += delta_cyc;
	sfd->const_cyc += delta_cntpct;
}

static void update_freq_scale(int cpu, struct rq *rq, bool local_cpu)
{
	struct cpu_pmu *pmu = &per_cpu(cpu_pmu_evs, cpu);
	struct sfd_data *sfd = &pmu->sfd;
	struct pmu_stat cur, prev;
	u64 delta_cyc, delta_cntpct;

	if (local_cpu) {
		fie_read_counters(&cur);
		raw_spin_lock(&pmu->lock);
		prev = pmu->cur;
		pmu->cur = cur;
		raw_spin_unlock(&pmu->lock);
	}

	/*
	 * Don't race with remote CPUs which may update the current CPU's
	 * runqueue clock and thus access sfd in parallel, and vice versa.
	 */
	raw_spin_lock(&sfd->lock);
	if (local_cpu) {
		delta_cyc = cur.cpu_cyc - prev.cpu_cyc;
		delta_cntpct = cur.cntpct - prev.cntpct;
		add_sfd_data(sfd, delta_cyc, delta_cntpct);
	}

	/*
	 * Set the CPU frequency scale measured via counters if enough data is
	 * present for the runqueue that's getting its clock updated (and thus
	 * about to use the frequency scale). This excludes idle time because
	 * although the cycle counter stops incrementing while the CPU idles,
	 * the system timer doesn't.
	 */
	if (rq->cpu == cpu) {
		if (sfd->const_cyc >= cpu_min_sample_cntpct) {
			u64 freq, max_freq = max_freqs[cpu];
			u64 ns = cntpct_to_ns(sfd->const_cyc);

			/* Report the measured frequency and reset the stats */
			freq = min(max_freq, USEC_PER_SEC * sfd->cpu_cyc / ns);
			per_cpu(arch_freq_scale, cpu) =
				SCHED_CAPACITY_SCALE * freq / max_freq;
			reset_sfd_data(sfd);
		} else if (sfd->const_cyc) {
			/*
			 * Track that the sfd statistics now contain stale data,
			 * since the frequency measurement won't perfectly
			 * correlate to the runqueue clock update window
			 * anymore. Keeping stale data for a previous window
			 * technically perpetuates this inaccuracy, but it is
			 * better than being unable to update the CPU frequency
			 * scale due to not having accumulated enough data. The
			 * stale data won't be used if the next window is long
			 * enough to compute the CPU's frequency.
			 */
			sfd->stale = true;
		}
	}
	raw_spin_unlock(&sfd->lock);

	/*
	 * Update the frequency scale data for the remote CPU when the updated
	 * runqueue doesn't belong to this CPU. This recursion is bounded.
	 */
	if (rq->cpu != cpu)
		update_freq_scale(rq->cpu, rq, false);
}

/*
 * Called from update_rq_clock(), just before update_rq_clock_task(). This way,
 * the CPU's frequency scale info has a chance to get updated just before it is
 * used by update_rq_clock_pelt() for computing load.
 */
void fie_update_rq_clock(struct rq *rq)
{
	int cpu = raw_smp_processor_id();

	/* Don't race with reboot or probe, since this isn't a vendor hook */
	if (!static_branch_unlikely(&fie_ready))
		return;

	/* Don't race with CPU hotplug for this CPU or the runqueue's CPU */
	if (unlikely(!cpu_active(cpu) || !cpu_active(rq->cpu)))
		return;

	/*
	 * Update the local CPU's frequency scale info, even if the runqueue in
	 * question doesn't belong to the current CPU. This way, any runqueue
	 * clock updates for remote CPUs will have fresh counter data, for when
	 * the current CPU's runqueue is the one being updated remotely.
	 *
	 * This also handles updating the frequency scale info for the remote
	 * CPU if the runqueue is indeed remote.
	 *
	 * Although the measured CPU frequency is ignored by PELT for the idle
	 * task, measurements are still allowed inside the idle task so that IRQ
	 * load average can still be tracked accurately for interrupts which
	 * fire while the idle task runs. There is otherwise no point to
	 * measuring CPU frequency within the idle task. PELT only cares about
	 * precisely tracking non-idle tasks' runtime, which it does in terms of
	 * time a task consumed relative to CPU frequency, so that the scheduler
	 * can accurately calculate the load of each actual task.
	 */
	update_freq_scale(cpu, rq, true);
}

/*
 * In this standalone FIE driver, the frequency scale is updated exclusively
 * from fie_update_rq_clock(), so this callback does nothing.
 */
static void fie_tick(void) {}

static struct scale_freq_data fie_sfd = {
	.source = SCALE_FREQ_SOURCE_ARCH,
	.set_freq_scale = fie_tick
};

static void fie_idle_enter(void *data, int *state, struct cpuidle_device *dev)
{
	int cpu = raw_smp_processor_id();
	struct cpu_pmu *pmu = &per_cpu(cpu_pmu_evs, cpu);
	struct pmu_stat cur, prev;

	/* Don't race with reboot */
	if (!static_branch_unlikely(&fie_ready))
		return;

	/* Don't race with CPU hotplug */
	if (unlikely(!cpu_active(cpu)))
		return;

	/* Update the current counters one last time before idling */
	fie_read_counters(&cur);
	raw_spin_lock(&pmu->lock);
	prev = pmu->cur;
	pmu->cur = cur;
	raw_spin_unlock(&pmu->lock);

	/* Accumulate data for calculating the CPU's frequency */
	raw_spin_lock(&pmu->sfd.lock);
	add_sfd_data(&pmu->sfd, cur.cpu_cyc - prev.cpu_cyc,
		     cur.cntpct - prev.cntpct);
	raw_spin_unlock(&pmu->sfd.lock);
}

static void fie_idle_exit(void *data, int state, struct cpuidle_device *dev)
{
	int cpu = raw_smp_processor_id();
	struct cpu_pmu *pmu = &per_cpu(cpu_pmu_evs, cpu);
	struct pmu_stat cur;

	/* Don't race with reboot */
	if (!static_branch_unlikely(&fie_ready))
		return;

	/* Don't race with CPU hotplug */
	if (unlikely(!cpu_active(cpu)))
		return;

	/*
	 * Reset the baseline without accumulating idle time.
	 * CNTPCT kept running while the CPU was idle, but CPU cycles
	 * were gated; starting a fresh baseline discards that skew.
	 */
	fie_read_counters(&cur);

	raw_spin_lock(&pmu->lock);
	pmu->cur = cur;
	raw_spin_unlock(&pmu->lock);
}

static int fie_cpuhp_up(unsigned int cpu)
{
	struct cpu_pmu *pmu = &per_cpu(cpu_pmu_evs, cpu);
	int ret;

	ret = create_perf_events(cpu);
	if (ret)
		return ret;

	/*
	 * Update and reset the statistics for this CPU as it comes online. No
	 * need to take any locks since `cpu_active(cpu) == false` (except in
	 * fie_monitoring_init()), so no shared data can be accessed concurrently
	 * with the hotplug handler. Disabling IRQs when reading the PMU
	 * statistics is needed to prevent interrupts from firing during the
	 * measurement and thus skewing the data.
	 */
	local_irq_disable();
	fie_read_counters(&pmu->cur);
	local_irq_enable();
	pmu->prev = pmu->cur;
	reset_sfd_data(&pmu->sfd);

	/* Install fie_tick() */
	topology_set_scale_freq_source(&fie_sfd, cpumask_of(cpu));
	return 0;
}

static int fie_cpuhp_down(unsigned int cpu)
{
	/* Stop fie_tick() from running on this CPU anymore */
	topology_clear_scale_freq_source(SCALE_FREQ_SOURCE_ARCH,
					 cpumask_of(cpu));
	release_perf_events(cpu);
	return 0;
}

static void fie_shutdown(void)
{
	/*
	 * Kill fie_tick() on all CPUs and disable `fie_ready` to prevent
	 * further PMU register access after this. PMU registers must not be
	 * accessed after kvm_reboot() finishes; attempting to do so will fault.
	 *
	 * This also needs to kick all CPUs to ensure that the scheduler and
	 * cpuidle hooks aren't running anymore. This works because the hooks
	 * themselves are always called from IRQs-disabled context, so when the
	 * IPI kick goes through it means that all in-flight IRQs-disabled
	 * contexts are done executing. Thus, once kick_all_cpus_sync() returns,
	 * it is guaranteed that all hooks which may read PMU registers will
	 * observe `fie_ready == false`.
	 */
	static_branch_disable(&fie_ready);
	topology_clear_scale_freq_source(SCALE_FREQ_SOURCE_ARCH,
					 cpu_possible_mask);
	kick_all_cpus_sync();
	cpuhp_remove_state_nocalls(cpuhp_state);
	unregister_trace_android_vh_cpu_idle_enter(fie_idle_enter, NULL);
	unregister_trace_android_vh_cpu_idle_exit(fie_idle_exit, NULL);
}

static int fie_reboot(struct notifier_block *nb, unsigned long val, void *cmd)
{
	fie_shutdown();
	return NOTIFY_OK;
}

/* Use the highest priority in order to run before kvm_reboot() */
static struct notifier_block fie_reboot_nb = {
	.notifier_call = fie_reboot,
	.priority = INT_MAX,
};

static int get_max_frequencies(void)
{
	int cpu;

	for_each_possible_cpu (cpu) {
		struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);

		if (!policy)
			return -ENODEV;
		max_freqs[cpu] = policy->cpuinfo.max_freq;
		cpufreq_cpu_put(policy);
	}
	return 0;
}

static int __init fie_monitoring_init(void)
{
	int ret;

	ret = get_max_frequencies();
	if (ret)
		return ret;

	calc_cntpct_arith();

	/*
	 * Delete the arch's scale_freq_data callback to get rid of the
	 * duplicated work by the arch's callback, since we read the same
	 * values. A new scale_freq_data callback is installed in
	 * fie_cpuhp_up().
	 */
	topology_clear_scale_freq_source(SCALE_FREQ_SOURCE_ARCH,
					 cpu_possible_mask);

	/* Register the CPU hotplug notifier with calls to all online CPUs */
	cpuhp_state = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "fie",
					fie_cpuhp_up, fie_cpuhp_down);
	if (cpuhp_state <= 0)
		return -EINVAL;

	/* Register cpuidle hooks */
	ret = register_trace_android_vh_cpu_idle_enter(fie_idle_enter, NULL);
	if (ret)
		goto err_cpuhp;

	ret = register_trace_android_vh_cpu_idle_exit(fie_idle_exit, NULL);
	if (ret)
		goto err_idle_enter;

	/* Register reboot notifier */
	register_reboot_notifier(&fie_reboot_nb);

	/* Begin updating CPU scheduler statistics from update_rq_clock() */
	static_branch_enable(&fie_ready);

	pr_info("FIE: Frequency invariance engine initialized\n");
	return 0;

err_idle_enter:
	unregister_trace_android_vh_cpu_idle_enter(fie_idle_enter, NULL);
err_cpuhp:
	cpuhp_remove_state_nocalls(cpuhp_state);
	return ret;
}
late_initcall(fie_monitoring_init);
