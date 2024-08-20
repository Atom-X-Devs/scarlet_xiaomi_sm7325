.. SPDX-License-Identifier: GPL-2.0

=============
Scheduler QoS
=============

1. Introduction
===============

Different workloads have different scheduling requirements to operate
optimally. The same applies to tasks within the same workload.

To enable smarter usage of system resources and to cater for the conflicting
demands of various tasks, Scheduler QoS provides a mechanism to provide more
information about those demands so that scheduler can do best-effort to
honour them.

  @sched_qos_type	what QoS hint to apply
  @sched_qos_value	value of the QoS hint
  @sched_qos_cookie	magic cookie to tag a group of tasks for which the QoS
			applies. If 0, the hint will apply globally system
			wide. If not 0, the hint will be relative to tasks that
			has the same cookie value only.

QoS hints are set once and not inherited by children by design. The
rationale is that each task has its individual characteristics and it is
encouraged to describe each of these separately. Also since system resources
are finite, there's a limit to what can be done to honour these requests
before reaching a tipping point where there are too many requests for
a particular QoS that is impossible to service for all of them at once and
some will start to lose out. For example if 10 tasks require better wake
up latencies on a 4 CPUs SMP system, then if they all wake up at once, only
4 can perceive the hint honoured and the rest will have to wait. Inheritance
can lead these 10 to become a 100 or a 1000 more easily, and then the QoS
hint will lose its meaning and effectiveness rapidly. The chances of 10
tasks waking up at the same time is lower than a 100 and lower than a 1000.

To set multiple QoS hints, a syscall is required for each. This is a
trade-off to reduce the churn on extending the interface as the hope for
this to evolve as workloads and hardware get more sophisticated and the
need for extension will arise; and when this happen the task should be
simpler to add the kernel extension and allow userspace to use readily by
setting the newly added flag without having to update the whole of
sched_attr.
