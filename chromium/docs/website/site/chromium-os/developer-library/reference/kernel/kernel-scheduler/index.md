---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: kernel-scheduler
title: Kernel scheduler in ChromeOS
---

The kernel scheduler tries its best to achieve good performance while saving
energy. However its decisions are not perfect as it does not have much
knowledge of the executing workload. To assist the kernel, userspace provides
several hints and this document describes several of them. The document will
also describe the kernel scheduler's behavior in general.

[TOC]

## How does Chrome browser classify threads?

Chrome classifies threads as normal, urgent and non-urgent. Currently, display,
compositing and low-latency real time audio threads in Chrome are marked urgent.
Background, utility and resource efficient threads are marked as non-urgent.
All other threads are classified as normal.

Along with this, renderers are categorized into `foreground` and `background`
groups. When tabs are switched, the Chrome browser processes moves the entire
renderer process back and forth between foreground and background groups. This
can be evidenced by glancing at `/proc/<pid>/cgroup` of the renderer processes.

## Scheduler tuning mechanisms in ChromeOS

The following mechanisms are used on ChromeOS for tuning the scheduler:

### Cpuset

1. Chrome’s cpuset in ChromeOS

   Chrome uses cpuset to control the placement of urgent and non-urgent threads.
   On big.LITTLE systems, non-urgent threads are placed on the little cores, with
   urgent threads able to run on all CPUs.

   Currently big.LITTLE systems are detected via reading cpu capacities or max
   frequencies in sysfs, and they have two or more capacity / frequency values. A
   mask of the little CPUs is constructed and written into the non-urgent cpuset.
   It's implemented in session manager.

2. Future work

   Note that we currently do not use cpusets for background kernel threads such as
   RCU. It is not clear yet if limiting critical kernel threads to the little CPUs
   might cause performance issues.

   Further, VMs other than ArcVM are not in the root CGroup and do not distinguish
   between I/O bound and CPU bound threads. This is bound to change in the future.

### Thread priority

The Chrome browser assigns different nice values for normal, urgent and non-urgent threads.
This lets the scheduler give appropriate amount of CPU time to threads, with
urgent threads getting the highest amount of CPU, followed by urgent and then non-urgent.
non-urgent threads being background tasks are expected to disrupt other tasks as less
as possible. Refer to the Chrome code for more details about these values.

### Util clamp

The scheduler's built-in estimation of a task’s utilization is based on
execution time and as such can cause sub-optimal frequency selection or CPU
placement decisions.

For example, a task with a small utilization might be very important but will
be considered by the scheduler as a small task that can be consolidated on to a
little CPU with other small tasks, and/or be run at a low CPU frequency.
To achieve good performance, the utilization value can be clamped to a minimum.

In Chrome, the `sched_setattr(2)` system call is used to set the utilization
clamp. Since these threads are in CGroups, those CGroups must be appropriately
configured for the `sched_setattr(2)` call to take effect. This CGroups value
is specified in the board-specific `model.yaml` file. For example, for Trogdor
it is located at
`overlay-trogdor/chromeos-base/chromeos-config-bsp/files/model.yaml` in the
ChromeOS sources. The `model.yaml` value is used in upstart’s scripts to set
the uclamp.min value of Chrome’s foreground and UI CPU controller CGroups. If
there is no value in `model.yaml`, the default of 20% is used.

NOTE: The uclamp.min of the CGroup sets the upper limit of `uclamp.min`.
So the final clamp of the task is calculated by the kernel as:
```
effective_clamp = min(cgroup_clamp, task_clamp)
```
You can read the clamp value from the task's `/proc/<pid>/sched` file.

For Urgent Chrome threads, uclamp.min is set to a default of 20% and can be
configured via Chrome’s command line. Currently there is no platform/board way
of setting the per-task value other than passing parameters on Chrome's command
line.

### CGroup CPU controller

1. Chrome CGroups

   Depending on whether a chrome renderer is visible, it is place in either a
   `foreground` or `background` CPU control cgroup. CPU shares are assigned to
   these groups to weight the foreground group more.

   In the future, renderers will be moved to their own dedicated CGroup in the
   root of the CGroup hierarchy to improve fairness.

2. VM CGroups

   Android on ChromeOS run inside a VM called ArcVM. ArcVM’s I/O threads and ArcVM’s
   vCPU threads are separated into 2 different CGroups. Both these CGroups are housed
   under the root CGroup to improve fairness due to the difference between CPU usage
   of I/O bound and CPU bound threads.

   In the future, other VMs will also be moved to the root group. For now those other
   VM threads are under `/sys/fs/cgroup/vms/<vm name>/`
