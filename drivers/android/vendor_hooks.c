// SPDX-License-Identifier: GPL-2.0-only
/* vendor_hook.c
 *
 * Android Vendor Hook Support
 *
 * Copyright 2020 Google LLC
 */
#include <linux/iova.h>
#include <linux/pm_qos.h>

#define CREATE_TRACE_POINTS
#include <trace/hooks/vendor_hooks.h>
#include <linux/tracepoint.h>

/* keep-sorted start */
#include <trace/hooks/avc.h>
#include <trace/hooks/bug.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/cpufreq.h>
#include <trace/hooks/cpuidle.h>
#include <trace/hooks/cpuidle_psci.h>
#include <trace/hooks/debug.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/epoch.h>
#include <trace/hooks/fault.h>
#include <trace/hooks/fpsimd.h>
#include <trace/hooks/gic.h>
#include <trace/hooks/gic_v3.h>
#include <trace/hooks/iommu.h>
#include <trace/hooks/logbuf.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/mpam.h>
#include <trace/hooks/net.h>
#include <trace/hooks/pm_domain.h>
#include <trace/hooks/power.h>
#include <trace/hooks/printk.h>
#include <trace/hooks/psi.h>
#include <trace/hooks/reboot.h>
#include <trace/hooks/remoteproc.h>
#include <trace/hooks/selinux.h>
#include <trace/hooks/signal.h>
#include <trace/hooks/sys.h>
#include <trace/hooks/syscall_check.h>
#include <trace/hooks/sysrqcrash.h>
#include <trace/hooks/timer.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/traps.h>
#include <trace/hooks/ufshcd.h>
#include <trace/hooks/vmscan.h>
/* keep-sorted end */

/*
 * Export tracepoints that act as a bare tracehook (ie: have no trace event
 * associated with them) to allow external modules to probe them.
 */

/* keep-sorted start */
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_arm64_serror_panic);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_bitmap_find_best_next_area);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_cpu_capacity_show);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_cpu_cgroup_attach);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_cpu_cgroup_online);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_cpufreq_transition);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_die_kernel_fault);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_do_el1_bti);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_do_el1_fpac);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_do_el1_undef);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_do_sp_pc_abort);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_hw_protection_shutdown);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_iommu_alloc_insert_iova);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_iommu_dma_info_to_prot);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_iommu_iovad_init_alloc_algo);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_iommu_limit_align_shift);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_iommu_setup_dma_ops);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_logbuf);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_panic_unhandled);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_report_bug);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_selinux_avc_insert);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_selinux_avc_lookup);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_selinux_avc_node_delete);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_selinux_avc_node_replace);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_selinux_is_initialized);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_set_balance_anon_file_reclaim);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_show_max_freq);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_ufs_reprogram_all_keys);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_allow_domain_state);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cgroup_attach);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_check_file_open);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_check_mmap_file);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cma_alloc_retry);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpu_idle_enter);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpu_idle_exit);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpufreq_fast_switch);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpufreq_resolve_freq);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpufreq_target);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpuidle_psci_enter);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_cpuidle_psci_exit);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_do_send_sig_info);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_freq_qos_add_request);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_freq_qos_remove_request);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_freq_qos_update_request);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_get_page_wmark);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_gic_set_affinity);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_iommu_iovad_alloc_iova);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_iommu_iovad_free_iova);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ipi_stop);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_is_fpsimd_save);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_logbuf);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_logbuf_pr_cont);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_mpam_set);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_page_add_new_anon_rmap);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_printk_caller);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_printk_caller_id);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_printk_ext_header);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_printk_hotplug);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_psi_event);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_psi_group);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ptype_head);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rproc_recovery);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rproc_recovery_set);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_sched_show_task);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_set_tsk_need_resched_lazy);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_show_resume_epoch_val);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_show_suspend_epoch_val);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_syscall_prctl_finished);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_sysrq_crash);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_timer_calc_index);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_check_int_errors);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_compl_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_dev_cmd_completion);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_eh_timed_out);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_fill_prdt);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_link_startup);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_prepare_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_send_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_send_tm_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_send_uic_command);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_transfer_rsp_status);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_update_sysfs);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_ufs_wait_for_uic_cmd);
/* keep-sorted end */
