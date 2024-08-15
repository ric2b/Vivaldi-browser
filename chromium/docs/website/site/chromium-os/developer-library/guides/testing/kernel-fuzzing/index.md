---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: kernel-fuzzing
title: Kernel fuzzing in ChromeOS
---

ChromeOS kernel fuzzing uses [syzkaller](https://github.com/google/syzkaller/).
Syzkaller fuzzes core kernel components in VMs, and device drivers on
DUTs in the lab.

## How often is the kernel fuzzed?

The core ChromeOS kernel is fuzzed continuously in VMs.

Device drivers are fuzzed in the lab for 30 minutes per board type,
about two times per day. More information about how syzkaller is
deployed in the lab can be found [here](http://go/syzkaller-ctp-deployment).

## How can I fuzz the core ChromeOS kernel?

The instructions
[here](https://github.com/google/syzkaller/blob/HEAD/docs/linux/setup_ubuntu-host_qemu-vm_x86-64-kernel.md)
can be used to fuzz the core components of ChromeOS kernels 4.4 and newer.
You can avoid having to re-create the rootfs by simply using [this
rootfs](https://storage.googleapis.com/syzkaller/wheezy.img) and its
corresponding
[key](https://storage.googleapis.com/syzkaller/wheezy.img.key).

## How can I fuzz device drivers on a DUT?

You can run syzkaller against a DUT you have locally, or a leased device
from the lab. Instructions for flashing the DUT with a debug kernel and
fuzzing it can be found [here](http://go/syzkaller-leased-dut).

## How can I write fuzz targets for device drivers?

A guide on writing fuzz targets for syzkaller can be found
[here](http://go/writing-ctp-syz-desc).

## Where can the fuzz results be viewed?

**NOTE** Links below are internal-links.
The results of VM-fuzzing can be found [here](https://buganizer.corp.google.com/hotlists/733621)
and the results of device driver fuzzing can be found [here](https://buganizer.corp.google.com/hotlists/2610283).
