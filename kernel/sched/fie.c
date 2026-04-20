// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Sultan Alsawaf <sultan@kerneltoast.com>.
 */

#include <linux/cpufreq.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/of_platform.h>
#include <linux/perf_event.h>
#include <linux/reboot.h>
#include <linux/units.h>
#include <asm/arch_timer.h>
#include <trace/hooks/cpuidle.h>
#include <trace/hooks/sched.h>
#include <sched.h>
#include <pelt.h>

/* Minimum sample time in nanoseconds */
#define CPU_MIN_SAMPLE_NS (100 * NSEC_PER_USEC)

/* Max frequencies for SM8550 (kHz) */
static const u64 max_freqs[] = {
	2016000, 2016000, 2016000,           /* Cores 0-2 (Silver/LITTLE) */
	2803200, 2803200,                    /* Cores 3-4 (Gold/Big) */
	2803200, 2803200,                    /* Cores 5-6 (Gold+/Big) */
	3187200                              /* Core 7 (Prime) */
};

/*
 * CNTPCT_EL0 arithmetic to convert between ticks and nanoseconds without
 * overflow, mirroring the approach in tensor_aio.
 */
static u64 cntpct_mult __read_mostly;
static u64 cntpct_div  __read_mostly;
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

	cntpct_rate = arch_timer_get_rate();
	cntpct_mult = NSEC_PER_SEC;
	cntpct_div  = cntpct_rate;
	for (cd = 10; cd > 1; cd--) {
		while (!(cntpct_mult % cd) && !(cntpct_div % cd)) {
			cntpct_div  /= cd;
			cntpct_mult /= cd;
		}
	}

	cpu_min_sample_cntpct = ns_to_cntpct(CPU_MIN_SAMPLE_NS);
}

/* Read CNTPCT_EL0 with ISB before and after for precision */
static inline u64 get_cntpct(void)
{
	u64 val;
	isb();
	val = __arch_counter_get_cntpct();
	isb();
	return val;
}

/* PMU event statistics */
struct pmu_stat {
	u64 cpu_cyc;
	u64 cntpct;
};

/*
 * Scale Frequency Data: accumulates CPU cycles and constant-rate ticks
 * (CNTPCT), excluding idle time via the cpuidle hooks.
 */
struct sfd_data {
	u64  cpu_cyc;
	u64  const_cyc; /* CNTPCT ticks excluding idle */
	bool stale;
};

struct cpu_pmu {
	raw_spinlock_t lock;
	struct pmu_stat cur;
	struct pmu_stat prev;
	struct sfd_data sfd;
};

static DEFINE_PER_CPU(struct cpu_pmu, cpu_pmu_evs) = {
	.lock = __RAW_SPIN_LOCK_UNLOCKED(cpu_pmu_evs.lock)
};

static bool in_reboot __read_mostly;
static int  cpuhp_state;

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
		.type   = PERF_TYPE_RAW,
		.size   = sizeof(attr),
		.pinned = 1
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

/* --- sfd helpers (mirrors tensor_aio's reset_sfd_data / add_sfd_data) --- */

static void reset_sfd_data(struct sfd_data *sfd)
{
	sfd->cpu_cyc = sfd->const_cyc = 0;
	sfd->stale = false;
}

static void add_sfd_data(struct sfd_data *sfd, u64 delta_cyc, u64 delta_cntpct)
{
	/*
	 * If the data is stale but this window is large enough, discard the
	 * stale portion and start fresh.
	 */
	if (sfd->stale && delta_cntpct >= cpu_min_sample_cntpct)
		reset_sfd_data(sfd);

	sfd->cpu_cyc   += delta_cyc;
	sfd->const_cyc += delta_cntpct;
}

/* --- core FIE update --- */

static void update_freq_scale(bool tick)
{
	int cpu = raw_smp_processor_id();
	struct cpu_pmu *pmu = &per_cpu(cpu_pmu_evs, cpu);
	struct sfd_data *sfd = &pmu->sfd;
	struct pmu_stat cur, prev;
	u64 freq, max_freq, ns;

	/* Read both counters together to avoid skew */
	cur.cntpct = get_cntpct();
	cur.cpu_cyc = read_cpu_cycles();

	raw_spin_lock(&pmu->lock);
	prev = pmu->cur;
	pmu->cur = cur;
	raw_spin_unlock(&pmu->lock);

	/* Accumulate active time (idle time excluded by hooks) */
	if ((cur.cntpct - prev.cntpct) >= cpu_min_sample_cntpct) {
		add_sfd_data(sfd,
			     cur.cpu_cyc - prev.cpu_cyc,
			     cur.cntpct  - prev.cntpct);
	}

	if (sfd->const_cyc >= cpu_min_sample_cntpct) {
		max_freq = max_freqs[cpu];
		ns = cntpct_to_ns(sfd->const_cyc);
		freq = min(max_freq, USEC_PER_SEC * sfd->cpu_cyc / ns);
		per_cpu(arch_freq_scale, cpu) =
				SCHED_CAPACITY_SCALE * freq / max_freq;
		reset_sfd_data(sfd);
	} else if (tick) {
		if (sfd->const_cyc)
			sfd->stale = true;
		else
			reset_sfd_data(sfd);
	}
}

/* --- scheduler / cpuidle hooks --- */

static void tensor_aio_tick(void)
{
	if (unlikely(in_reboot))
		return;
	update_freq_scale(true);
}

static struct scale_freq_data tensor_aio_sfd = {
	.source        = SCALE_FREQ_SOURCE_ARCH,
	.set_freq_scale = tensor_aio_tick
};

static void tensor_aio_ttwu(void *data, struct task_struct *p)
{
	int cpu = raw_smp_processor_id();

	if (unlikely(in_reboot || !cpu_active(cpu)))
		return;

	update_freq_scale(false);
}

static void tensor_aio_idle_enter(void *data, int *state,
				  struct cpuidle_device *dev)
{
	int cpu = raw_smp_processor_id();
	struct cpu_pmu *pmu = &per_cpu(cpu_pmu_evs, cpu);
	struct pmu_stat cur, prev;

	if (unlikely(in_reboot || !cpu_active(cpu)))
		return;

	cur.cntpct = get_cntpct();
	cur.cpu_cyc = read_cpu_cycles();

	raw_spin_lock(&pmu->lock);
	prev = pmu->cur;
	pmu->cur = cur;
	raw_spin_unlock(&pmu->lock);

	add_sfd_data(&pmu->sfd,
		     cur.cpu_cyc - prev.cpu_cyc,
		     cur.cntpct  - prev.cntpct);
}

static void tensor_aio_idle_exit(void *data, int state,
				 struct cpuidle_device *dev)
{
	int cpu = raw_smp_processor_id();
	struct cpu_pmu *pmu = &per_cpu(cpu_pmu_evs, cpu);
	struct pmu_stat cur;

	if (unlikely(in_reboot || !cpu_active(cpu))) {
		reset_sfd_data(&pmu->sfd);
		return;
	}

	/*
	 * Update cur without accumulating to sfd so the idle period
	 * (CNTPCT running, CPU cycles gated) is excluded.
	 */
	cur.cntpct = get_cntpct();
	cur.cpu_cyc = read_cpu_cycles();

	raw_spin_lock(&pmu->lock);
	pmu->cur = cur;
	raw_spin_unlock(&pmu->lock);
}

/* --- CPU hotplug --- */

static int memperf_cpuhp_up(unsigned int cpu)
{
	struct cpu_pmu *pmu = &per_cpu(cpu_pmu_evs, cpu);
	int ret;

	ret = create_perf_events(cpu);
	if (ret)
		return ret;

	raw_spin_lock(&pmu->lock);
	pmu->cur.cntpct  = get_cntpct();
	pmu->cur.cpu_cyc = read_cpu_cycles();
	pmu->prev = pmu->cur;
	raw_spin_unlock(&pmu->lock);

	reset_sfd_data(&pmu->sfd);
	topology_set_scale_freq_source(&tensor_aio_sfd, cpumask_of(cpu));
	return 0;
}

static int memperf_cpuhp_down(unsigned int cpu)
{
	topology_clear_scale_freq_source(SCALE_FREQ_SOURCE_ARCH,
					 cpumask_of(cpu));
	release_perf_events(cpu);
	return 0;
}

/* --- Reboot notifier --- */

static int fie_reboot(struct notifier_block *notifier, unsigned long val,
		      void *cmd)
{
	/*
	 * Disable all hooks and clear scale_freq source before kvm_reboot()
	 * to prevent any PMU access after system quiesce.
	 */
	in_reboot = true;
	topology_clear_scale_freq_source(SCALE_FREQ_SOURCE_ARCH,
					 cpu_possible_mask);
	kick_all_cpus_sync();
	cpuhp_remove_state_nocalls(cpuhp_state);
	return NOTIFY_OK;
}

static struct notifier_block fie_reboot_nb = {
	.notifier_call = fie_reboot,
	.priority = INT_MAX,
};

/* --- Initialisation --- */

static int __init fie_monitoring_init(void)
{
	/* Precompute CNTPCT ↔ ns arithmetic */
	calc_cntpct_arith();

	/* Clear any existing arch callback */
	topology_clear_scale_freq_source(SCALE_FREQ_SOURCE_ARCH,
					 cpu_possible_mask);

	/* Register CPU hotplug callbacks */
	cpuhp_state = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "fie",
					memperf_cpuhp_up, memperf_cpuhp_down);
	BUG_ON(cpuhp_state <= 0);

	/* Register cpuidle and scheduler hooks */
	BUG_ON(register_trace_android_vh_cpu_idle_enter(tensor_aio_idle_enter, NULL));
	BUG_ON(register_trace_android_vh_cpu_idle_exit(tensor_aio_idle_exit,  NULL));
	BUG_ON(register_trace_android_rvh_try_to_wake_up(tensor_aio_ttwu,     NULL));

	/* Register reboot notifier */
	register_reboot_notifier(&fie_reboot_nb);

	pr_info("FIE: frequency invariance initialised\n");
	return 0;
}
late_initcall(fie_monitoring_init);
