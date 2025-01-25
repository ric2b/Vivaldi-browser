---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: kernel-development
title: Kernel Development
---

This document covers best practices for kernel development in ChromiumOS,
including debugging tips, platform bringup info, committing changes, sending
code upstream, and using upstream repos for testing & development.

*** promo
Note for Googlers: There are additional Google-specific notes and
work-in-progress notes at [go/chromeos-kernel-tips-and-tricks].
***

[TOC]

## Development workflow

### Build and deploy

1. [Ensure the target machine is ready for kernel updates (not usually
   necessary)](#Preparing-the-target)
1. Build your kernel using either method:
   * [Using emerge](#building-with-emerge)
   * [Using cros_workon_make](#building-with-cros_workon_make)
1. [Deploy your kernel](#Deploy-your-kernel)
1. [Recover from a bad installation (if necessary)](#recover-from-a-bad-kernel-update)

#### Preparing the target

First, make sure you're running a dev or test image.  Then ensure that
verity is disabled on the target before running the `update_kernel.sh`
script, or it will complain and abort.  Verity can be disabled using
the command `/usr/share/vboot/bin/make_dev_ssd.sh
--remove_rootfs_verification --partition <partition number>` on the
target followed by a reboot.

#### Preparing to build

If you are making any changes to the kernel sources, and want to build the
kernel with your changes, you must first tell the build system to use your
local sources:
```bash
(chroot) $ cros workon --board ${BOARD} start chromeos-kernel-[x_y]  # first time
```

#### Building with emerge

The `emerge-${BOARD}` command is the standard way of building all packages,
including the kernel.  For instance:

```bash
(chroot) emerge-${BOARD} chromeos-kernel-[x_y]
```
*   After the first time, you will no longer need to check and resolve all
    package dependencies, thus you can run
    ```bash
    (chroot) emerge-${BOARD} chromeos-kernel-[x_y] --nodeps
    ```
    which is faster.
*   `cros_workon_make` is even faster (see below) if you just want to do a build
    test, but since it skips some cleaning steps, and builds in place, it might
    dirty your tree.
*   Add the `--install` flag if you want to deploy the resulting kernel.

#### Building with cros_workon_make

`cros_workon_make` runs an incremental build of the kernel.  However, it
doesn't resolve dependencies, therefore before using it, run `emerge-${BOARD}`
once as described above.  Then run:

```bash
(chroot) $ FEATURES="noclean" cros_workon_make --board=${BOARD} \
    --install chromeos-kernel-[x_y]
```

To enable debug options like lockdep and KASAN, add `USE="debug"` to the
command line above. This is highly recommended because the default build
is optimized for performance rather than debugging purpose. Note that the
debug build bloats the size of kernel image, and the image may not be able
to fit into its partition on some older devices. The debug build also takes
much longer to boot.

You can also enable serial port at the same time by `USE="debug pcserial"`.

*** note
Note that using `cros_workon_make` leaves build artifacts in your
source directory under the `build` directory. When you do a regular
emerge of the kernel (and are `cros-workon`-ed) this will slow things
down because the entire source directory gets copied. So delete the
`build` directory when you're done.
***

#### Deploy your kernel

Update the kernel on the target (if you are sure that it will boot):

```bash
(chroot) $ ~/chromiumos/src/scripts/update_kernel.sh --remote=$DUT
```

*** note
**Note:** [cros deploy] [does not currently support deploying kernel
packages](https://issuetracker.google.com/172217325).
***

##### Deploy your kernel with recovery in mind.

Alternatively flash a known good working image to the device first and then use
`update_kernel.sh` to target the other kernel partition (typically KERN-B when
KERN-A is live) instead of the live kernel partition. Boot the device and then
run `update_kernel.sh` with `--ab_update` option to specify the other kernel
partition.:

```bash
(chroot) ./update_kernel.sh --remote=$DUT --bootonce --ab_update
```

The bootloader will attempt to boot the kernel on the other partition (typically
KERN-B) and kernel modules will be updated to the matching rootfs partition
(matching ROOT-B partition). If the kernel crashes early on then a reboot will
fallback to the A slot kernel and rootfs that is known to be good and
working. If the [boot is considered
successful](https://www.chromium.org/chromium-os/chromiumos-design-docs/boot-design#TOC-Rollback-Protection-After-Update)
the partition is marked with a successful boot and will be used from the next
time.

Be aware, however, that after you reboot again, if your boot was not yet marked as
successful you're back to your old kernel (which can be very confusing... "hey
where did my new feature go??").

See [disk
format](https://dev.chromium.org/chromium-os/chromiumos-design-docs/disk-format)
for more info on partition layouts, as you may need to use a different
partition number depending on how you installed your kernel or which
one you want to replace. The `cgpt` utility can also be used to view a
device's partition layout, as well as to modify the priority of its
kernel partitions. To read a partition's priority, where -i specifies the
partition in question:

```
(DUT) # cgpt show -i 2 /dev/mmcblk0
start size part contents
20480 32768 2 Label: "KERN-A"
Type: ChromeOS kernel
UUID: BCD6FC1E-528F-494B-9B06-FB723EA37672
Attr: priority=1 tries=0 successful=1
```

To switch between kernel primary/backup partition, execute (this can
also be done from a USB stick):

```
(DUT) # cgpt prioritize -P2 -i <partition to be used> /dev/mmcblk0
````

##### Dealing with partition corruption due to bad kernel recovery

One time I really screwed up my system by recovering (after bad kernel
installation) with 'dd if=/dev/sdb of=/dev/sda'. I forgot the '2' after each
drive specification. This overwrote my internal partition table with an exact
copy of the USB stick's partition table, including the GUIDs. When I
subsequently tried to boot USB, the system always seemed to boot off the
internal disk. 'rootdev -s' reported (internal partition) /dev/sda3. After an
hour or so, consultation with Bill showed that I really was booting the kernel
from /dev/sda2, but the kernel found the matching GUID on sda before even
looking at sdb. This was recovered with:

```bash
(DUT) $ a=$(uuidgen)
(DUT) $ cgpt add -i 3 -u $a /dev/sda
```

which generates and installs a new GUID for sda3.

### Recover from a bad kernel update

One issue is often to figure out how to recover if you flash a bad kernel.
Booting from USB and running `chromeos-install` is one solution, but that's
slow. There are a couple approaches that can be useful to recover quickly.

1.   [Always flash a known good working image to the device so that you don't have to do this.](#bootonce)
1. Using `chromeos-install-kernel` to update the kernel:
	*   Always have a good USB stick connected to the device.
	*   (optional) Make sure you use a serial-enabled coreboot firmware.
	*   If the kernel on internal storage does not boot anymore:

	1.  Boot from USB (press Ctrl-U during FW bootup, you may have to do this repeatedly if on a serial console)
	1.  Copy kernel and modules back to internal storage using the `chromeos-install-kernel` script:

		```bash
		(DUT) # chromeos-install-kernel
		(DUT) # reboot
		```

	The system should boot from internal storage again.

1. Using update_kernel to update kernel:

	`update_kernel.sh` can be used to update the kernel in internal storage after
	booting from USB. It will try to copy modules over also but currently has issues
	if rootfs verification is enabled on USB.  If your kernel change doesn't depend
	on the modules on the rootfs, you can go ahead and use --ignore_verity to update
	only the kernel. Example below assume NVME for KERN-A.

	TODO(b/202778198): fix update_kernel.sh to update root fs with modules.

    ```bash
    (chroot) $  ./update_kernel.sh --remote=$DUT --board=$BOARD \
      --device=/dev/nvme0n1p --ignore_verity --partition /dev/nvme0n1p2
    ```

### Kernel configuration

[Kernel
configuration](https://www.chromium.org/chromium-os/developer-library/guides/kernel/kernel-configuration/)
in ChromiumOS has an extra level of indirection from the normal
.config file. So do the instructions - [see this page for more
information](https://www.chromium.org/chromium-os/developer-library/guides/kernel/kernel-configuration/).

See also the [cros-kernel eclass documentation].

#### Inspect kernel config

The built kernel config is available at `/build/$BOARD/boot/config` in
the chroot.

On a running system, the kernel config is not loaded by default (to
save memory), so you'll need to use `modprobe` to load it first:

```bash
(DUT)# modprobe configs; zcat /proc/config.gz
```

#### Kconfig changes

Kconfig changes (changes that affect `chromeos/config`) should be normalized by
running `chromeos/scripts/kernelconfig olddefconfig`

### Modifying the kernel command line

There are several ways to modify the kernel command line. These vary in ease of
use, as well as type of target system (e.g., real hardware running the CrOS
verified boot chain behaves differently than a QEMU VM booting via BIOS).

#### Modify kernel command line via `update_kernel.sh`

The built kernel command line is available at
`~/chromiumos/src/build/images/$BOARD/latest/config.txt` in the chroot. The
`update_kernel.sh` script will use this file for the command line when updating
the device if it exists. Otherwise, it will try to reuse the target device's
args. The `--remote-bootargs` flag can also force reusing the target args.

*** note
**Note:** while `update_kernel.sh` can update the kernel on a system using
legacy BIOS with syslinux (such as `BOARD=amd64-generic` on QEMU), it does not
currently know how to update the command line.
***

#### Modify kernel command line on device

It's possible to modify the command line of an installation on a
target device as well.

For example, to enable the console on a recovery image on USB stick
`/dev/sdb`:

```bash
(DUT) # /usr/share/vboot/bin/make_dev_ssd.sh -i /dev/sdb --partitions 2 --save_config ./foo
(DUT) # vi ./foo
  # add the updated command line, for example: earlycon=uart,mmio32,0xfedc6000,115200,48000000
  # save & exit vi
(DUT) # /usr/share/vboot/bin/make_dev_ssd.sh -i /dev/sdb --partitions 2 --set_config ./foo
(DUT) # /usr/share/vboot/bin/make_dev_ssd.sh -i /dev/sdb --recovery_key
```

This extracts the command line from the kernel partition using vbutil,
allowing you to edit it and write it back.

Instead of `--save_config` and `--set_config`, you can also use `--edit_config`
to edit the config in-place if you want.

*** note
**Note:** this only works on systems using the CrOS vboot boot chain (i.e., all
CrOS hardware). Notably, QEMU VMs do not use CrOS vboot.
***

#### Modify kernel command line in depthcharge

*** note
This option is heavy-handed and difficult, as updating your firmware may brick
your device. Proceed only if you know what you're doing.
***

If you're booting with depthcharge, you can modify the command line thusly:

```bash
(chroot) $ cros-workon-${BOARD} start depthcharge
(chroot) $ vi ~/chromiumos/src/platform/depthcharge/src/board/${board}/board.c
```

Call the `commandline_append()` function containing your command line addition:

```C
#include "boot/commandline.h"

static int board_setup(void)
{
	commandline_append("earlycon=uart,mmio32,0xfedc6000,115200,48000000");
}
```

Rebuild depthcharge, build it into the firmware image, and flash it to your
device.

#### Modify kernel command line on a legacy BIOS

*** note
**Note:** no supported CrOS hardware boots via legacy BIOS. Legacy BIOS is
typically used by QEMU VMs (e.g., [`cros_vm`](/chromium-os/developer-library/guides/containers/cros-vm/)).
***

First, locate the EFI system partition (a.k.a., boot partition). This is often
partition 12, such as `/dev/sda12`. Mount this partition to view and edit the
syslinux boot configuration:

```bash
(DUT) # mkdir /tmp/mnt
(DUT) # mount /dev/sda12 /tmp/mnt
(DUT) # find /tmp/mnt/syslinux
/tmp/mnt/syslinux
/tmp/mnt/syslinux/syslinux.cfg
/tmp/mnt/syslinux/default.cfg
/tmp/mnt/syslinux/usb.A.cfg
/tmp/mnt/syslinux/root.A.cfg
/tmp/mnt/syslinux/root.B.cfg
/tmp/mnt/syslinux/README
/tmp/mnt/syslinux/vmlinuz.A
/tmp/mnt/syslinux/vmlinuz.B
/tmp/mnt/syslinux/ldlinux.sys
/tmp/mnt/syslinux/ldlinux.c32
(DUT) # cat /tmp/mnt/syslinux/README
Partition 12 contains the active bootloader configuration when
booting from a non-ChromeOS BIOS.  EFI BIOSes use /efi/*
and legacy BIOSes use this syslinux configuration.
(DUT) # cat /tmp/mnt/syslinux/default.cfg
DEFAULT chromeos-usb.A
```

Now, edit the relevant boot entry; this is almost always `usb.A.cfg`:

```bash
(DUT) # cat /tmp/mnt/syslinux/usb.A.cfg
label chromeos-usb.A
  menu label chromeos-usb.A
  kernel vmlinuz.A
  append init=/sbin/init rootwait ro noresume loglevel=7 noinitrd console=ttyS0  root=PARTUUID=80BF3EED-EB79-4D99-A837-06C95D8574B9 i915.modeset=1 cros_legacy cros_debug
[...]
```

Edit the `chromeos-usb.A` entry for `append [...]`, save the file, and reboot:

```bash
(DUT) # vi /tmp/mnt/syslinux/usb.A.cfg
(DUT) # umount /tmp/mnt
(DUT) # reboot
```

### Using modules

#### Loading Kernel modules from outside the root filesystem

If you need to load kernel modules from a location other than the root
filesystem, module locking must be disabled. Either a kernel command line
option can be used:

```
lsm.module_locking=0
```

Or, on images with dm-verity disabled (`--noenable_rootfs_verification`), the
restriction can be disabled via the exposed sysctl:

```bash
(DUT) # echo 0 >/proc/sys/kernel/chromiumos/module_locking
```

#### Blocking kernel modules for individual overlays

If you need to block kernel modules for specific overlays, modify the
overlay-<name>/chromeos-base/chromeos-bsp-<name>/chromeos-bsp-<name>-<version>.ebuild
file.

Add the following two lines to the end of the src\_install() function:

```bash
insinto "/etc/modprobe.d"
doins "${FILESDIR}/<blocklist>"
```

The ${FILESDIR} variable points to the files/ directory within the
`chromeos-bsp-<name>/` directory. Within this directory, add your `<blocklist>`
(ex cros-blocklist.conf).

For each kernel module you wish to block, add the following line to
`<blocklist>`:

```
blacklist <module name>
```

You can also use # comments within these files to explain why the kernel module
needs to be blocked.

### Creating changelists (CLs)

#### Which copyright header should I use?

When adding new files to the kernel, please add a regular Google copyright
header to them. In particular this is true for any code that will eventually
find its way upstream (which should include practically everything we do).  The
main reason for this is that there's no concept of "The ChromiumOS Authors"
outside of our project, since it refers to the AUTHORS file that isn't bundled
with the kernel.

Each file type has its own SPDX comment format, [discussed
here](https://www.kernel.org/doc/html/latest/process/license-rules.html#license-identifier-syntax):

C header files:
```c
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * <short description>
 *
 * Copyright 2019 Google LLC.
 */
```

C source files:
```c
// SPDX-License-Identifier: GPL-2.0
/*
 * <short description>
 *
 * Copyright 2019 Google LLC.
 */
```

For reference, old drivers already existing in upstream might still have the
full text format, which would look like below.

```c
/*
 * Copyright 2018 Google LLC.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
```

#### (Compile) test

Make sure that your patch builds fine with allmodconfig:

```bash
mkdir -p ../build/x86-64 ../build/arm64
# Native build (x86-64)
make O=../build/x86-64 allmodconfig
make O=../build/x86-64 all -j50 2>&1|tee ../build/x86-64/build.log
# arm64 build
CROSS_COMPILE=aarch64-cros-linux-gnu- ARCH=arm64 O=../build/arm64 make allmodconfig
CROSS_COMPILE=aarch64-cros-linux-gnu- ARCH=arm64 O=../build/arm64 make -j64 >/dev/null
```

Test build with ChromeOS config:

```bash
cd src/third_party/kernel/v4.19
git checkout linux-next/master
# Checkout config options only
git checkout m/master -- chromeos
# Normal emerge
(chroot) emerge-${BOARD} -av chromeos-kernel-4_19
```

#### Commit messages & summary lines (CHROMIUM, UPSTREAM, FROMLIST, BACKPORT)

See the [Kernel Design
page](https://dev.chromium.org/chromium-os/chromiumos-design-docs/chromium-os-kernel)
for some more details.

For changes which cannot be submitted upstream to the official Linux Kernel
repository, the commit message is important. We use the following conventions:

*   Begin the commit message with **CHROMIUM:**
*   If it is architecture specific, add the architecture. The following are
    samples of supported architectures: **ARM:** or **X86:**
*   If it is machine specific, add machine-identifying information. For
    example, **tegra2:** or **x86-mario:**.
*   Follow the needed tags with the subject for the commit message.
*   Follow the subject line with the body of the commit message. The message
    should not only describe **what**, but also **why**, you have created the
    change. Please include information about the testing that you performed to
    ensure the code is valid.
*   **Signed-off-by** is required.

An example subject line is: **CHROMIUM: ARM: tegra: Add initial support for
aebl**

**If not sure, use git log to check commit messages of earlier commits for the
same file or other files in the same directory.**

Do not include configuration changes (i.e. changes to files within
chromeos/config) with other code changes. See the next section for these.

Files may not be suitable for submission upstream because they have
ChromiumOS-specific information, or may be based on other changes which are
local to the ChromiumOS project. Such changes may not be upstreamed, but the
ChromiumOS project team will continue to maintain the changes.

##### Configuration Changes

When a commit involves configuration changes, make sure that any code changes
are separated out into a different commit. The configuration commit should
contain only changes to files within the chromeos/config directory tree.

The commit message should start with **CHROMIUM: config:**

An example message is: **CHROMIUM: config: enable aebl config**

#### Committing

See the [Contributing Guide](/chromium-os/developer-library/guides/development/contributing/) for details on how to upload your
changes, get them tested & reviewed, and ultimately get them into the tree.

## Debugging

### Finding issues

So the first step is to figure out _what_ are the problems:

*   Look at kernel messages (`dmesg`), on boot, or at any time, and find errors
    or warnings that should not be there.  e.g. what does `dmesg -w` give.
*   Look at `/var/log/messages` (contains kernel logs from `dmesg`, as well as
    logs from most system services).
*   Look at `top` output, to check if certain processes are hogging CPU or
    memory.
*   Build the kernel with `USE=kasan`. [KASan] is a great tool to find memory
    issues in the kernel (use it with the other tests below).
*   `USE=debug` enables many of the individual debug options listed below.
*   `USE=devdebug` enables options that should impose no or little runtime
    overhead.
*   Miscellaneous debug options (see [kernel.eclass] for an up to date list):
    *   Memory-related options:
        *   `USE=kmemleak`
        *   `USE=failslab`. Then configure in `/sys/kernel/debug/failslab` (setting
            `probability` to `10` and `times` to `1000` is a good start).
        *   `USE=dmadebug`
        *   `USE=memory_debug`
    *   `USE=debugobjects`
    *   `USE=ubsan`
    *   `USE=lockdebug`
    *   `FAIL_MMC_REQUEST`
        *   Enable `CONFIG_FAIL_MMC_REQUEST`, `CONFIG_FAULT_INJECTION` and
            `CONFIG_FAULT_INJECTION_DEBUG_FS`, and then configure in
            `/sys/kernel/debug/mmc{n}/fail_mmc_request/`.
    *   `USE=kcsan` to enable [KCSan], only supported on >=5.10 kernels. Very noisy
        for the time being with a lot of false positives, see linked bug.
    *   Others? (Please add here!)

*   Stress tests:
    *   Single iteration suspend test: `powerd_dbus_suspend`
    *   Multi-iteration suspend test: `suspend_stress_test`
    *   Reboot loops, keeping ramoops at each reboot to analyse failures (setup
	    [SSH keys] first):

	    ```bash
		#!/bin/bash

		DUT=$1
		i=0
		while true; do
			while ! scp "root@$DUT:/sys/fs/pstore/console-ramoops-0" "ramoops-$i"; do
			sleep 1
			done
			ssh "root@$DUT" reboot
			sleep 20
			((++i))
		done
		```

		Then run this to extract out the bad ramoops
		```bash
		mkdir bad; ls ramoops-* | xargs -I{} sh -c \
		    'tail -n 1 {} | grep -v "reboot: Restarting system" && cp {} bad/{}'
		```

    *   `restart ui` in a loop.
    *   Run tests (autotests, CTS, etc.)
    *   Stress test `cpufreq` by changing frequency constantly
        *   Other drivers may have similar knobs that one can play with.
    *   Balloons (from [crbug.com/468342], or
        `src/platform/microbenchmarks/mmm_donut.py`
    *   Unbind/rebind drivers (may be nice with `kasan`/`kmemleak`, too):

        ```bash
        cd /usr/local
        find /sys/bus/\*/drivers/\*/\* -type l -maxdepth 0 | grep -v "module$" > list
        sync
        cat list | xargs -I{} sh -c 'echo {}; cd \`dirname {}\`; echo \`basename {}\` > unbind; echo \`basename {}\` > bind; sleep 5'
        # see what crashes, edit list to remove bad drivers, continue
        ```

### Enabling crash collection

Run the following commands on the target. This needs to be done just once after
an install.

```bash
touch /var/lib/crash_sender_paused
touch /home/chronos/"Consent To Send Stats"
chown chronos:chronos /var/lib/crash_sender_paused
chown chronos:chronos /home/chronos/"Consent To Send Stats"
sync; sync; sync
```

The crashes will then appear in /var/spool/crash.

### printk debugging

One advanced debugging technique is to use `printk` and other syslog
output functions to tell you what your code is doing.

*   Add printks in strategic places (`dev_[info/warn/err]` or
    `pr_[info/warn/err]`), reboot, see what happens.
*   `dev_dbg/pr_dbg` in the kernel code can be enabled by setting
    `#define DEBUG` at the top of the source file (before all includes).
*   These are generally not written out to serial so have less effect on
    performance, but are not preserved in ramoops/serial on an OOPs.
*   Adding `dump_stack calls` in places may also be very useful.

Sometimes adding too many `printk`s changes behaviour ([Heisenbug]),
or makes the system unusable, so be careful where you put them
(probably a bad idea to put them on every timer tick for example, or
on each incoming network packet in a wifi driver).  If such
granularity is needed however, there are some options:

*   Consider switching to `ftrace`, see below.
*   Use `ratelimit` to minimize the number of prints. See [example CL].

#### Seeing early debug messages

If you need to see kernel log messages (e.g., over UART) before the full
console driver is running, `earlyprintk` or `earlycon` may help you. Find more
info in the [kernel parameters guide].

Note that unlike with `earlyprintk`, you often don't need any hardware-specific
arguments to use `earlycon` -- you only need to add `earlycon` to the kernel
command line. The kernel can pick up the appropriate console parameters from
either the Device Tree (via `/chosen/stdout-path`) or ACPI (via the SPCR
table).

*** note
**Pitfall:** ChromeOS kernel command lines typically include an empty
`console=` parameter by default, which prevents directing kernel logs to the
default console (earlycon or otherwise). Remove this if you want to direct
kernel logs to your console.
***

Caveats apply: architecture and driver support varies. For example, ACPI/SPCR
earlycon support is
[not fully integrated in ChromeOS](https://issuetracker.google.com/73886662) as
of this writing.

#### Dynamic Debugging (dev_dbg / pr_debug)

Dynamic debugging allows one to enable/disable debugging messages in kernel
code at runtime (e.g., calls to `dev_dbg` or `pr_debug`).

##### Enabling

Using dynamic debugging requires the `CONFIG_DYNAMIC_DEBUG` config option to be
enabled. By default [dynamic debug is disabled on ChromeOS].

Flag `USE=dyndebug` will enable dynamic debug on the ChromeOS kernel ebuilds.

If using `menuconfig`, the following enables it:

```
Kernel hacking
     ---> printk and dmesg options
          ---> [*] Enable dynamic printk() support
```

Once the kernel is compiled with `CONFIG_DYNAMIC_DEBUG`, you can use the
following commands to control the output.

##### Enable all dynamic debugging

```bash
echo "+p" > /sys/kernel/debug/dynamic_debug/control
```

##### Disable all dynamic debugging

```bash
echo "-p" > /sys/kernel/debug/dynamic_debug/control
```

##### Enable dynamic debugging for specific modules

```bash
echo "module cros_ec_spi +p" > /sys/kernel/debug/dynamic_debug/control
echo "module cros_ec_proto +p" > /sys/kernel/debug/dynamic_debug/control
```

##### View all of the individual statements that can be enabled

```bash
cat /sys/kernel/debug/dynamic_debug/control
```

See [Dynamic Debug] for complete details and syntax.

### ftrace debugging

*   [ftrace] allows you to trace events in the kernel (e.g. function calls,
    graphs), without introducing too much overhead. This is especially useful
    to debug timing/performance issues, or for cases when adding printk changes
    the behaviour.

*   It is possible to add custom messages by using `trace_printk`.
*   Example, to trace functions starting with `rt5667` and `mtk_spi`:

    ```bash
    cd /sys/kernel/debug/tracing
    echo "rt5677*" > set_ftrace_filter
    echo "mtk_spi_*" >> set_ftrace_filter
    echo function > current_tracer
    echo 1 > tracing_on
    # Look at the trace
    cat trace
    ```

*   `trace-cmd`, available on test images, provides a nice frontend to the
    tracing infrstructure. With `trace-cmd`, the above becomes:

    ```bash
    # 'record' configures ftrace and writes to trace-cmd.dat (default file).
    trace-cmd record -p function -l 'rt5677*' -l 'mtk_spi_*'
    # Hit Ctrl^C to stop recording
    # 'report' formats trace-cmd.dat, dumps to stdout.
    trace-cmd report
    ```

    See the [trace-cmd man pages] or the [LWN trace-cmd HOWTO] for more info.

Another example:

```bash
cd /sys/kernel/debug/tracing
# Sample output: blk function_graph function nop. These are valid values you can echo into current_tracer
cat available_tracers

# By default this should output 'nop'
cat current_tracer

# function_graph is useful too
echo "function" > current_tracer

# This should output "all functions enabled" by default
cat set_ftrace_filter

# You can also append with "echo *nl80211* >> set_ftrace_filter"
echo *nl80211* > set_ftrace_filter

# Should be the number of functions enabled.
wc -l set_ftrace_filter

# Clear out the tracing pipe of the previous junk. You will need to Ctrl-C kill this after a while
cat trace_pipe > /dev/null

# You should see nothing, now start performing actions that will lead to your module/code being called.
cat trace_pipe
```

Another ftrace article: https://lwn.net/Articles/370423/


Other tricks:

*   It is also possible to start tracing on boot by adding kernel parameters
    (useful to debug early hangs).
*   It is possible to ask the kernel to dump the ftrace buffer to uart on oops,
    this is useful to debug hangs/crashes:

    ```bash
    echo 1 > /proc/sys/kernel/ftrace_dump_on_oops.
    ```

*   Dumping the whole buffer may take an enormous amount of time at serial rate,
    but sometimes it's worth it.

### Getting backtraces with BUG/WARN

BUG/WARN and friends provide nice backtraces. These can be very useful for
figuring out what code path is triggering a hard to reproduce issue.

#### Decoding backtraces
  ```bash
  ~/chromiumos/src/platform/dev/contrib/kernel_decode_stack -b kukui
  ```

Sometimes gdb is more useful (aarch64, update as needed):

  ```
  aarch64-cros-linux-gnu-gdb /build/kukui/usr/lib/debug/boot/vmlinux
  disas /m function
  ```

### Debugging kernel crashes

TODO: This is anecdotal, and may not be an optimal or fully correct solution.
Please verify and remove the TODO.

You have a few options:

1\. Googler-only: Check out go/xstability. Clicking on sample crashes here
go/crash with the filter set for that particular crash. Click on a sample
report. Below the "Report Time" and "Client ID" you should "Files" with a link
to "upload\_file\_kcrash". This has the stack trace towards the end.

TODO: Add more details on this

2\. If you are debugging a local crash on your device, look for the crash in
/var/log/messages (unlikely that it would be saved there) or
/sys/fs/pstore/console-ramoops. You may see some symbols preceded by question
marks in the stack trace, something like the below.

```
<5>[ 25.801932] Call Trace:
<5>[ 25.801947] [<ffffffffc008c064>] ieee80211_amsdu_to_8023s+0xec/0x2df [cfg80211]
<5>[ 25.801968] [<ffffffffc02af0f2>] __iwl7000_ieee80211_sta_ps_transition+0x154a/0x21dc [iwl7000_mac80211]
<5>[ 25.801987] [<ffffffffc03154e4>] ? iwl_mvm_send_lq_cmd+0x8e/0x9c [iwlmvm]
<5>[ 25.802003] [<ffffffffc0324409>] ? iwl_mvm_rs_tx_status+0xf9c/0x1f5cd /4 [iwlmvm]
<5>[ 25.802023] [<ffffffffc02b06f2>] __iwl7000_ieee80211_mark_rx_ba_filtered_frames+0x96e/0xcb0 [iwl7000_mac80211]
<5>[ 25.802041] [<ffffffff9e4ee0f0>] ? kmem_cache_free+0x8a/0xc5
<5>[ 25.802059] [<ffffffffc02b08a1>] __iwl7000_ieee80211_mark_rx_ba_filtered_frames+0xb1d/0xcb0 [iwl7000_mac80211]
<5>[ 25.802080] [<ffffffffc02b0dc6>] __iwl7000_ieee80211_rx_napi+0x392/0x46a [iwl7000_mac80211]
<5>[ 25.802098] [<ffffffffc0316578>] iwl_mvm_rx_rx_mpdu+0x749/0x78b [iwlmvm]
<5>[ 25.802113] [<ffffffffc0310f16>] iwl_mvm_enter_d0i3+0x359/0xe7f [iwlmvm]
<5>[ 25.802128] [<ffffffffc023d504>] iwl_pci_unregister_driver+0xfdb/0x1439 [iwlwifi]
<5>[ 25.802143] [<ffffffffc023e883>] iwl_pcie_irq_handler+0x57d/0x7d1 [iwlwifi]
<5>[ 25.802157] [<ffffffff9e48c255>] ? free_irq+0x8a/0x8a
<5>[ 25.802168] [<ffffffff9e48c272>] irq_thread_fn+0x1d/0x3c
<5>[ 25.802179] [<ffffffff9e48be1a>] irq_thread+0x117/0x21a
<5>[ 25.802191] [<ffffffff9e921dda>] ? __schedule+0x589/0x5d3
<5>[ 25.802202] [<ffffffff9e48b863>] ? kzalloc.constprop.37+0x1c/0x1c
<5>[ 25.802214] [<ffffffff9e48bd03>] ? irq_thread_check_affinity+0x8f/0x8f
<5>[ 25.802227] [<ffffffff9e45183b>] kthread+0xc0/0xc8
<5>[ 25.802238] [<ffffffff9e45177b>] ? __kthread_parkme+0x6b/0x6b
<5>[ 25.802249] [<ffffffff9e92389c>] ret_from_fork+0x7c/0xb0
<5>[ 25.802259] [<ffffffff9e45177b>] ? __kthread_parkme+0x6b/0x6b
```

There are a few ways you can resolve the "? some\_symbol + 0xoffset" format
into a line of source code. For example, you can enter the cros\_sdk
chroot and load up the vmlinux file in gdb.

Be careful to use the gdb binary from the cross-toolchain of the $BOARD you are
debugging on. TODO(crbug.com/995661): ChromiumOS runs 32-bit ARM userspace on
ARM64 boards and there is no good programmatic way of getting the right gdb
tuple in such case, so just use aarch64-cros-linux-gnu-gdb with them for the
time being.

```bash
(cr) user@machine /build/samus $ gdb="$(portageq-$BOARD envvar CHOST)-gdb"
(cr) user@machine /build/samus $ file /build/$BOARD/usr/lib/debug/boot/vmlinux | grep -q aarch64 && gdb="aarch64-cros-linux-gnu-gdb"
(cr) user@machine /build/samus $ ${gdb} /build/$BOARD/usr/lib/debug/boot/vmlinux
```

Next, use the list command to print the code at given address

```bash
Reading symbols from /build/samus/usr/lib/debug/boot/vmlinux...done.
(gdb) list *( iwl_mvm_send_lq_cmd+0x8e)
0x12b5 is in iwl_mvm_send_lq_cmd (/mnt/host/source/src/third_party/kernel/v4.14/drivers/net/wireless/iwl7000/iwlwifi/mvm/utils.c:752).
747  };
748
749  if (WARN_ON(lq->sta_id == IWL_MVM_STATION_COUNT))
750  return -EINVAL;
751
752  return iwl_mvm_send_cmd(mvm, &cmd);
753  }
754
755  /**
756  * iwl_mvm_update_smps - Get a request to change the SMPS mode
(gdb)
```

3\. A slightly more tedious way of getting symbols is to symbolize the whole kernel using objdump -

```bash
cd /build/samus/var/cache/portage/sys-kernel/chromeos-kernel-4_14
# Pick a proper output location - the resulting file is > 2GB in size!
objdump -e vmlinux > /tmp/objdump-output.txt
grep your_kernel_symbol /tmp/objdump-output.txt
```

More information [here](https://wiki.ubuntu.com/Kernel/KernelDebuggingTricks)
and [here](http://www.linuxjournal.com/article/9252).

### Debugging with KGDB/KDB

KGDB is an in-kernel debugger implementation, which allows developers to attach
a local GDB instance on their development machine to debug the kernel on a
remote test machine, using a serial connection. You can find some information
here:

*   https://www.kernel.org/pub/linux/kernel/people/jwessel/kdb/
*   http://elinux.org/Kgdb
*   https://events.static.linuxfound.org/sites/events/files/slides/ELC-E%20Linux%20Awareness.pdf

To use KGDB with ChromiumOS requires two steps for the test machine:

1.  Enable KGDB in the kernel configuration
2.  Set kernel parameters to enable the appropriate debug console

Step 1 can be done by building with `USE="kgdb"`:

```bash
USE="kgdb vtconsole pcserial" emerge-${BOARD} chromeos-kernel-${VER}
```

(pcserial is only needed on x86 builds)

Step 2 can be done by adding `kgdboc=$TTY` to the kernel config.txt, where
`$TTY` depends on the board -- for many systems, this should be `ttyS0`, but
some ARM SoCs use `ttyS2`.

Once you configure the target device, you can break into debug mode with the
`Alt-SysRq-G` shortcut (see [Linux SysRq
docs](https://www.kernel.org/doc/html/latest/admin-guide/sysrq.html)); e.g.:

*   `Alt-VolUp-G` on the DUT keyboard (note: for this to work, you need
    to have `console=ttyS0,115200n8` parameter, but you can also set
    `loglevel=0` if you want to keep the console quiet and avoid associated
    slowdowns)
*   ``<enter> ` Z G`` (brk-g) if using servo ([implemented only for servo
    v2](https://issuetracker.google.com/141000626))
*   `echo g > /proc/sysrq-trigger`
*   `sysrq g` on the EC console

then attach to the target console with your cross-targeted GDB:

```bash
# If running servod, TTY_PORT can be derived from dut-control:
TTY_PORT="$(dut-control cpu_uart_pty | cut -d: -f2)"
# ${CROSS_ARCH} is something like x86_64-cros-linux-gnu-,
# aarch64-cros-linux-gnu-, etc., depending on your target kernel's
# architecture.
(chroot) $ ${CROSS_ARCH}-gdb \
         /build/${BOARD}/usr/lib/debug/boot/vmlinux \
         -ex "target remote ${TTY_PORT}" \
         -ex "set remotebaud 115200"
```

Once attached, you can use standard GDB commands, though report has it that not
everything works well (e.g., stepping and breakpoints) -- YMMV.

Besides basic GDB commands, you can make use of Linux-specific KDB commands via
the `monitor` command. For more info, run this while attached:

```bash
(gdb) monitor help
Command Usage Description
----------------------------------------------------------
[...]
```

#### Debugging modules

You can get a list of modules and addresses in kgdb with `monitor` `lsmod`.
Then you can add symbol files using the base addresses found there:

```bash
add-symbol-file /build/${BOARD}/usr/lib/debug/lib/modules/5.4.109/kernel/drivers/net/wireless/marvell/mwifiex/mwifiex.ko.debug 0xbf077000
add-symbol-file /build/${BOARD}/usr/lib/debug/lib/modules/5.4.109/kernel/drivers/net/wireless/marvell/mwifiex/mwifiex_sdio.ko.debug 0xbf0a0000
```

If you're in kgdb and want to get back to kdb:

```
maintenance packet 3
Ctrl-Z
kill -9 %
```

#### QEMU notes

Debugging a QEMU system (such as one launched via [cros_vm](/chromium-os/developer-library/guides/containers/cros-vm/))
requires a few tweaks.

For one, you need to establish a virtual serial console by adding `-serial pty`
to your QEMU command. For example:

```bash
(chroot) $ cros_vm [...] --start --qemu-args="-serial pty"
```

This will eventually print out a line like:

```
char device redirected to /dev/pts/10 (label serial1)
```

Thus, we use `/dev/pts/10` for our `${TTY_PORT}` when invoking `gdb`'s
`target remote ${TTY_PORT}`.

Then, this new console typically becomes `ttyS1` within the VM, because
`cros_vm` establishes the first serial device to monitor kernel logs. So your
modified kernel command line should include `kgdboc=ttyS1`.

*** note
QEMU also has its own GDB support, which can be easier to set up than KGDB. See
the [QEMU GDB docs](https://qemu-project.gitlab.io/qemu/system/gdb.html).

i.e., `cros_vm [...] --start --qemu-args="-s -S"`, and `${CROSS_ARCH}-gdb [...]
-ex "target remote localhost:1234"`.
***

#### Multiplexing the console

##### Easy method

Use `dut-console` [script](https://chromium.googlesource.com/chromiumos/platform/dev-util/+/HEAD/contrib/dut-console)
with `-k` parameter:

```bash
dut-console -p 9999 -c cpu -k
```

`dut-console` will also print instructions on how to attach `gdb` from inside
the chroot.

##### Detailed method

If you want to use both KGDB and a standard serial console over the same serial
port, you need to run a program like `kdmx` or `agent-proxy` to multiplex your
connection. Both can be found at:

https://kernel.googlesource.com/pub/scm/utils/kernel/kgdb/agent-proxy/

kdmx is probably easier to deal with. If your serial port is at `/dev/pts/80`,
you can start it with:

```bash
agent-proxy/kdmx/kdmx -n -b 115200 -p /dev/pts/80 -s /tmp/kdmx_tty_
```

You can find the ttys to use for console in `/tmp/kdmx_tty_trm` and for gdb in
`/tmp/kdmx_tty_gdb`. Thus connect to the terminal with:

```bash
cu --nostop -l $(cat /tmp/kdmx_tty_trm)
```

and attach gdb with:

```bash
${CROSS_ARCH}-gdb \
	/build/${BOARD}/usr/lib/debug/boot/vmlinux \
	-ex "target remote $(cat /tmp/kdmx_tty_gdb)"
```

If telnet is more your style, use agent-proxy with:

```bash
agent-proxy 127.0.0.1:5510^127.0.0.1:5511 0 /dev/pts/80,115200
```

Then connect to the terminal with:

```bash
telnet localhost 5510
```

and attach gdb with:

```bash
${CROSS_ARCH}-gdb \
         /build/${BOARD}/usr/lib/debug/boot/vmlinux \
         -ex "target remote localhost:5511"
```

#### Errata

*   KDB's `monitor ftdump` calls sleeping allocation functions (as of
    2016-11-17)
*   https://lkml.kernel.org/r/20161117191605.GA21459@google.com

### Bisecting a stable branch merge

To bisect along the upstream stable branch, first identify and test the merge
commit and the chromeos branch.

Merge:
```
commit f5edda0c2aefe22f338c3a00c0aa52161976d4b1
Merge: ce070a331d16 399849e4654e
Author: Guenter Roeck <groeck@chromium.org>
Date:   Wed Jul 1 08:17:19 2020 -0700

    CHROMIUM: Merge 'v4.19.131' into chromeos-4.19

    Merge of v4.19.131 into chromeos-4.19

    Changelog:
    ----------------------------------------------------------------
    Aaron Plattner (1):
          ALSA: hda: Add NVIDIA codec IDs 9a & 9d through a0 to patch table

    Aditya Pakki (1):
          rocker: fix incorrect error handling in dma_rings_init
```

ChromeOS:
```
commit ce070a331d1697048ebfdb9011be299bc77940dc
Author: Benjamin Gordon <bmgordon@chromium.org>
Date:   Thu Mar 26 13:23:28 2020 -0600

    CHROMIUM: LSM: Convert symlink checks to MNT_NOSYMFOLLOW
```

Start a regular `git bisect`, identifying the merge as bad and chromeos branch
as good:
```bash
git bisect
git checkout f5edda0c2aef
# Build and test
git bisect bad
git checkout ce070a331d16
# Build and test
git bisect good
```
Git is smart enough to bisect along the upstream branch, rooted at the common
branch point (the previous upstream merge).

At each bisection point, you need to merge in the chromeos branch. The device
may not boot and function correctly if you do not do this:
```bash
git merge --no-commit ce070a331d16
# Resolve merge conflicts

# Build/deploy/test kernel

# Reset git state and continue bisection
git reset --hard
git bisect [good|bad]
```

## Performance analysis

Use [perf](https://perf.wiki.kernel.org/index.php/Tutorial)!  One easy
perf method is to use `perf top` on the target device to look for hotspots.

Another quick way to profile is to use cycle profiling.  On the DUT, use
```
perf record -a -g -e cycles
```
with an optional time period and/or command if you want to profile a specific
binary.  Then on your workstation, generate the report with something like:
```
perf report -g --symfs /path/to/chroot/build/$BOARD
```
This should resolve symbols and give you a good idea of where cycles were spent
during the `perf record` above.

On specific platforms, other tools may be available.  For example on
Intel, both
[`socwatch`](https://software.intel.com/content/www/us/en/develop/documentation/get-started-with-socwatch/top.html)
and
[VTune](https://software.intel.com/content/www/us/en/develop/tools/vtune-profiler.html)
can be used provided the proper kernel drivers are loaded.

More information on profiling Chrome and (for Googlers) uploading to `pprof`
can be found in [CPU Profiling Chrome](https://chromium.googlesource.com/chromium/src/+/main/docs/profiling.md).

**FIXME: add some stuff on importing perf output into flamegraph tool, etc.**

## Upstream development

### How do I backport an upstream patch?

Let's suppose you've spotted a juicy new commit in Linus's [upstream linux
kernel](http://git.kernel.org/?p=linux/kernel/git/torvalds/linux-2.6.git;a=summary)
tree that you just must have. Instead of creating a new branch and manually
applying the changes, use `git cherry-pick` to do it for you. In addition, the
repository maintainers appreciate it if the cherry-picked commit still contains
the original author and git hash of the original upstream commit.

For "simple" UPSTREAM cherry-picks, one should first try using
[fromupstream.py] script to prepare CLs "automagically". Look
[below](#picking-patches-from-mailing-lists-upstream) for examples.

Otherwise, the follow steps use `git cherry-pick -x` to do most of the work:

```bash
NAME
        git-cherry-pick - Apply the changes introduced by some existing commits

SYNOPSIS
        git cherry-pick [--edit] [-n] [-m parent-number] [-s] [-x] [--ff] <commit>...

DESCRIPTION
	Given one or more existing commits, apply the change each one
	introduces, recording a new commit for each. This requires your working
	tree to be clean (no modifications from the HEAD commit).

OPTIONS
...
    -x
	When recording the commit, append to the original commit message a note
	that indicates which commit this change was cherry-picked from. Append
	the note only for cherry picks without conflicts. Do not use this
	option if you are cherry-picking from your private branch because the
	information is useless to the recipient. If on the other hand you are
	cherry-picking between two publicly visible branches (e.g. backporting
	a fix to a maintenance branch for an older release from a development
        branch), adding this information can be useful.
```

First, add Linus's tree as a remote to the chromium-os kernel tree (assuming the chromium-os root is `~/chromiumos`):

```bash
cd ~/chromiumos/src/third_party/kernel/<version>
git remote add upstream git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
git remote update
```

This will take a little while as git fetches all upstream commits. Luckily, git
is smart and won't refetch commits already in the chromium-os tree.

Once the tree is updated, take a brief look at whats been happening upstream
recently to a particular path (`--oneline` shows short-form upstream hashes and
the brief commit message):

```bash
git log --oneline upstream/master /path/of/interest
```

We can view that juicy commit using its upstream hash:

```bash
git show <upstream_commit_hash>
```

To backport the commit to the chromium-os tree, first start a new branch from
the current Tip of Tree (ToT). Then cherry-pick with `-x` to preserve the
original author and hash, and `-s` to sign-off-by the commit:

```bash
repo sync .
repo start my_upstream_commit .
git cherry-pick -x -s <upstream_commit_hash>
```

Add TEST= and BUG= lines at the bottom of the patch description. Also, remember
to keep the patch subject intact with only an addition of UPSTREAM: or
BACKPORT: as a new prefix. Use UPSTREAM: if you are applying an upstream patch
as-is, or BACKPORT: if you had to change the code to make it run with an older
kernel version.

**NOTE:** Do not make functional changes to backported patches! Downstream
changes in backports should be strictly limited to resolving conflicts. If you
need to make a functional change to a backport (ie: changing a delay, tweaking a
default value, etc), backport the change from upstream as-is and follow up with
a separate patch with CHROMIUM prefix.

Now, the upstream commit is on its own branch, let's upload it to gerrit, like
usual:

```bash
repo upload .
```

This will generate a gerrit change for review.

After review, submit the patch in gerrit like usual.

#### UPSTREAM, BACKPORT, FROMLIST, and you

When backporting patches from Linus's kernel tree, you should tag your patch
with UPSTREAM (or BACKPORT, if modifications were needed). But what about
patches that are "on their way" upstream, but haven't been merged for an
official release yet?

*   **FROMLIST:** use this tag when a patch has been sent to a public mailing
    list for review, but hasn't yet been merged anywhere. Before submitting a
    patch like this, try to address any review comments made in the public
    forum. Please also include a link to the list the patch was obtained from.
    For example:

    ```
    FROMLIST: bibble: a patch to fix everything
    ...
    ... original description verbatim, including any tags,
    ... e.g. Signed-off-by, Reviewed-by, etc.
    ...
    (am from https://lore.kernel.org/patchwork/patch/1060242/)
    ...
    ... any additional downstream information goes here, e.g.
    ... - (also found at A-LINK-BASED-ON-MESSAGE-ID),
    ... - BUG=,
    ... - TEST=,
    ... - Change-Id,
    ... - list of conflicts (e.g. generated by git),
    ... - cherry-picker's Signed-off-by, if not present in original description,
    ... - etc.
    ...
    ```
    *   **NOTE:** If a patch is rejected on the list, and it is still suitable
        for inclusion in the chromium kernel, it must be labeled as
        "CHROMIUM: FROMLIST:". These patches must have a link to the upstream
        discussion and must include the reason why we are diverging from
        upstream.

*   **UPSTREAM:** this tag should be used exclusively for patches that have
    actually landed in Linus' tree, not for cherry-picks from maintainer trees.

*   **BACKPORT:** follow the same rules as UPSTREAM, except that if you have to
    make changes to the patch, you should label it with BACKPORT and document
    what you had to change.

    *   **NOTE:** Do not make functional changes to backported patches!
        See the note in the previous section for guidance on how to handle
        functional changes to upstream patches.

*   **FROMGIT**: use this tag for cherry-picks of patches from maintainer
    trees, which have been applied in preparation for an upcoming release.

    *   Although it is a good reference for "what's going into the next
	release" **never** backport a patch straight from
	[linux-next](https://www.kernel.org/doc/man-pages/linux-next.html).
	Always source either a maintainer tree or a mailing list post.
    *   When including patches from maintainer trees, be specific about your
        source tree and branch. For example, for a patch from the for-next
        branch in the chrome-platform tree:

        ```
        FROMGIT: spi: mediatek: Only do dma for 4-byte aligned buffers
        ...
        (cherry picked from commit 1ce24864bff40e11500a699789412115fdf244bf
         git://git.kernel.org/pub/scm/linux/kernel/git/chrome-platform/linux.git for-next)
        ```

*   **BACKPORT: FROMLIST:** or **BACKPORT: FROMGIT:** follow  the same rules as
    FROMLIST or FROMGIT, except that if you have to make changes to the patch,
    you should also label it with BACKPORT (in addition to FROMLIST/FROMGIT)
    and document what you had to change.

Previous discussions defining this practice:

*   https://groups.google.com/a/chromium.org/forum/#!msg/chromium-os-dev/D56e2JxDhmc/IjgixwEReasJ
*   https://groups.google.com/a/chromium.org/forum/#!msg/chromium-os-dev/\_nY16h27k1s/FuHbWFCABwAJ

### Picking patches from mailing lists / upstream

#### UPSTREAM

```bash
../../../platform/dev/contrib/fromupstream.py -b b:195958998 \
  -t "Deploy reven kernel on RFE type 2 Realtek wireless and test that wireless works" \
  linux://5d6651fe85837b11564a2e2c3c6279c057d078d6
```

#### FROMGIT

```bash
../../../platform/dev/contrib/fromupstream.py -b b:123489157 \
  -t "Deploy kukui kernel with USE=kmemleak, no kmemleak warning in __arm_v7s_alloc_table" \
  'git://git.kernel.org/pub/scm/linux/kernel/git/joro/iommu.git#next/032ebd8548c9d05e8d2bdc7a7ec2fe29454b0ad0'
```

#### FROMLIST

Add project url in `~/.pwclientrc`

```rc
[options]
default=kernel

[kernel]
url=https://patchwork.kernel.org/

[lore]
url=https://lore.kernel.org/patchwork/xmlrpc/
```

Then run:

```bash
../../../platform/dev/contrib/fromupstream.py -b b:132314838 -t "no crash with CONFIG_FAILSLAB" 'pw://10957015'
# or
../../../platform/dev/contrib/fromupstream.py -b b:132314838 -t "no crash with CONFIG_FAILSLAB" 'pw://kernel/10957015'
```

For patches that go only to LKML (for which there is no patchwork), replace the
"pw://" argument with "msgid://". That'll still try to look for a patchwork but
if there is none then it'll just give the lore link.

#### Submitting patch series by gerrit cmd tool

In CrOS chroot (`gerrit deps` prints dependencies from top to bottom, so its
better to use `tac` so that the bottom-most CL is set to ready first):

```bash
# If the CL of interest is HEAD, else substitute the gerrit CL number.
cl=$(git log -1 --format='%(trailers:key=Change-Id,valueonly)')
deps=( $(gerrit --raw deps "${cl}" | tac) )
gerrit label-v "${deps[@]}" 1
gerrit label-cq "${deps[@]}" 2
```

### Downloading patches from upstream

If you want to review or apply a patchset from a mailing list that you are not
subscribed, you can download it from `lore`. Lore is an email archive
maintained by kernel.org.

You can reach lore via their web interface: https://lore.kernel.org .
Simply navigate to the mail thread that you are interested and click on the
`mbox.gz` link to obtain the mailbox file.

You can also use the
[b4](https://b4.docs.kernel.org/en/latest/installing.html) console tool.
Simply run `b4 mbox $Message-id` and you will obtain the mailbox file. You can
also apply the all patches with b4 using `b4 am $Message-id`. b4 will run some
attestations of the patches.

### Review patches from a mailbox file

If you use `mutt`, you can simply run `mutt -f patch.mbox`.

If you prefer to forward the emails to an email account with IMAP access, you
can use `imap-upload`:

1.  Clone the [imap-upload] repo.
1.  `python2.7 ./imap_upload.py patch.mbox --gmail`
1.  Use @chromium.org account.
1.  Find the email in your mailbox, and reply!

### How do I build an upstream kernel?

There are various ways of building mainline Linux, but it can be useful to use
existing ChromeOS tooling to build a non-ChromeOS-flavored kernel. See the
[cros-kernel eclass documentation] for tips on how to use the "fallback"
configuration system to build any (e.g., mainline) kernel tree within the
existing Portage-based flow.

*** note
**Note:** ChromeOS kernels often support hardware that is not yet supported in
an upstream kernel release. Ensuring hardware support for your system is not
covered here.
***

### How do I send a patch upstream?

Changes to parts of the kernel which are not purely ChromeOS-specific should
be upstreamed where possible. This includes just about any part of the kernel:
ARM- and x86-specific changes, driver patches and changes within the main
kernel and mm source. You can start with a code review if you like. Take a look
on the kernel mailing list to get a feel for how people submit and review
patches.

#### 1) Prepare your local repository state

To upstream, create a remote to track upstream.

For example the main kernel:

```bash
git remote add upstream git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
git fetch upstream
git checkout -b send-upstream upstream/master
```

You can then create a commit within this branch. This can be done either by
cherry-picking the commit from another branch and perhaps changing the commit
message:

```bash
git cherry-pick my-change
git commit --amend
# edit the message and save
```

or using git am to turn a patch into a commit:

```bash
git am my-change.patch
```

or manually applying a patch, and then committing:

```bash
patch -p1 < my-change.patch
git add ...
git commit
# create a suitable message
```

#### 2) Check if your patches are correctly formatted

There are two aspects of having correct patches to send upstream: not having
ChromiumOS-specific details, and meeting all the Linux kernel requirements.

##### Remove ChromiumOS-specific Details

Verifying these details is as simple as loading the patch file in your favorite
editor. Edit the file manually to become compliant; this will, of course, have
no affect on the source or commit message stored by git.

*   No CHROMIUM:in the subject line of the patch file.
*   No **BUG=** in the patch file.
*   No **TEST=** in the patch file.
*   No **Change-Id:** in the patch file.
*   **Signed-off-by:** is in the patch file.

Once all of the above is true, you can move on to checking for compliance with
the Linux Kernel guidelines.

##### Check for Compliance with Linux Kernel Requirements

First off, make sure the Kernel builds with patch applied.

For style, the patman tool (see below) will automatically run checkpatch.pl on
your change. If you'd like to run the checkpatch.pl tool manually, here'a an
example workflow:

```bash
git format-patch HEAD~
scripts/checkpatch.pl 0001-my-change.patch
# make improvements
git add ...
git commit --amend
# rinse and repeat
```

#### 3a) Option 1: Send out the patch using b4

Currently, it is recommended to use b4 to send your patch upstream because it is
maintained by the people behind the kernel infra.

You may use the [original documentation](https://b4.docs.kernel.org/en/latest/contributor/prep.html).
Alternatively, [this tutorial](https://people.kernel.org/monsieuricon/sending-a-kernel-patch-with-b4-part-1)
is also helpful.

While following this workflow, keep the following in mind:

* For this workflow, you will have to check out a separate kernel src tree. This is
because you don't want cros hooks to run on the commits you make.

* You will have to modify your `.git/config`, and ensure it has the [user] and [sendemail]
section accurately filled out.

* If you do not want to use a pgp key, or don't have one, make sure you pass in the `--no-sign`
option when you run`b4 send`.

* This workflow gives you the option of using git-sendemail. If you choose to use that instead
of the web endpoint, and it's your first time, make sure to follow instructions
[here](#first_time-email-setup) and to pass in the `--no-sign` option when running `b4 send`.

#### 3b) Option 2: Send out the patch using patman

It is possible to send out patches using `git send-email` manually, but for
most usecases using the `patman` CLI is sufficient and can save a lot of time.

(See the [next section](#first_time-email-setup) for first-time credential setup
for using `patman` and `git send-email`.)

Patman automates patch creation, checking, change list creation, cover letter,
sending to the mailing list, etc. You can find patman in the U-Boot tree
(`src/third_party/u-boot/files/tools/patman`). It usually should be run outside
of the chroot, so you could create an alias, or a symlink to somewhere in your
path:
```bash
alias patman='~/chromiumos/src/third_party/u-boot/files/tools/patman/patman'
# or
ln -s ~/chromiumos/src/third_party/u-boot/files/tools/patman/patman ~/bin
```

To use patman, amend your top commit to have the line:

```
Series-to: LKML <linux-kernel@vger.kernel.org>
Series-cc: (anyone you want to Cc all patches in the series to)
```

Then type:

```bash
patman -n
```

to generate patches, check that they will go to the right place, and send them. Or:

```bash
patman
```

to generate patches and send them.

Various options are available. Particularly useful ones are:

*   \-m - by default patman sends your patches to relevant maintainers. Use this option to turn that off
*   \-t - ignore tags in the subject line which cannot be found
*   \-n - do a dry run

Full documentation is available in the README (patman -h) or
[here](https://source.denx.de/u-boot/u-boot/-/blob/HEAD/tools/patman/patman.rst).
Take a look at the automated change list creation and the alias support also.

##### Troubleshooting
You may get some python `ModuleNotFoundError` errors when running `patman`. This
may be resolved by `pip install pygit2 requests`

#### First-Time Email Setup

If you have never sent email from the command-line, or from `git send-email`, then there is some setup required.

##### 1. **Install `git send-email`**

   * If `git send-email --help` shows an error, you'll need to install it
      * For example, on debian: `apt-get install git-email`

##### 2. **Decide on the email address, password, and mail server to use**

You must configure git's send-email command with the details of how to send
email from your identity. The rest of this section will explain how to set up a
google-mail based account (e.g. an @gmail.com address, @chromium.org, etc).
If you have a different mail server, please contact the system administrator
(or check some help docs related to your email service) for the correct
settings.

**NOTE**: For Googlers, note that [DMARC](http://b/14415867) restrictions
prevent usage of your @google.com email address. Use http://go/chromium-account
to obtain an @chromium.org address.

##### 3. Set up email credentials

For google-mail-based addresses, it's recommended to use an "App
Password" for convenience when storing your real password on disk is
undesirable (which should be most cases). Follow [these
instructions](https://support.google.com/accounts/answer/185833) to obtain an
App Password, and use it as the `smtppass` value in the next section.

**Edit your `~/.gitconfig`**

Open up your `~/.gitconfig` file to include the following stanza:

```
[sendemail]
  smtpserver = smtp.gmail.com
  smtpserverport = 587
  smtpencryption = tls
  smtpuser = YOUR_EMAIL_ADDRESS
  smtppass = PASSWORD
  confirm = always
```

Remember to swap in the `YOUR_EMAIL_ADDRESS` with your full email address, and
`PASSWORD` with your password (or App Password).

#### Optional: Automating the Compliance Checks

To use the following script, you will need to have created a _patch_ file using
`git format-patch`. Also note that you will have to recreate the patch file, and
re-check your patch file each time you check in code to your source tree.

This script might be useful also, as it checks a series of patches, checks for
ChromeOS-specific commit tags and prints a summary at the end. Put it in your
path and run it from anywhere.

```bash
#! /bin/sh
# Pass a list of patchfiles to check for compliance

KERNEL=./scripts/
OUT=$(tempfile)
while (( "$#" )); do
	ERRCP=
	ERR=
	"${KERNEL}/checkpatch.pl" $1 || ERRCP=1
	grep BUG= $1 && ERR="$ERR BUG"
	grep TEST= $1 && ERR="$ERR TEST"
	grep "Change-Id" $1 && ERR="$ERR Change-Id"
	grep "Review URL" $1 && ERR="$ERR Review URL"
	if [ -n "${ERR}" ]; then
		echo "Bad $1 ($ERR)" >>$OUT
	else
		echo "OK $1" >>$OUT
	fi
	shift
done
cat $OUT
```

[cros-kernel eclass documentation]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/eclass/cros-kernel/README.md
[fromupstream.py]: https://chromium.googlesource.com/chromiumos/platform/dev-util/+/HEAD/contrib/fromupstream.py
[cros deploy]: /chromium-os/developer-library/reference/tools/cros-deploy/
[kernel parameters guide]: https://www.kernel.org/doc/html/latest/admin-guide/kernel-parameters.html
[Dynamic Debug]: https://www.kernel.org/doc/html/v4.19/admin-guide/dynamic-debug-howto.html
[dynamic debug is disabled on ChromeOS]: https://crbug.com/188825
[crbug.com/468342]: https://crbug.com/468342
[example CL]: https://crrev.com/c/1325821
[ftrace]: https://www.kernel.org/doc/Documentation/trace/ftrace.txt
[go/chromeos-kernel-tips-and-tricks]: https://goto.google.com/chromeos-kernel-tips-and-tricks
[Heisenbug]: https://en.wikipedia.org/wiki/Heisenbug
[imap-upload]: https://github.com/rgladwell/imap-upload
[KASan]: https://www.kernel.org/doc/html/v4.14/dev-tools/kasan.html
[UPSTREAM, BACKPORT and FROMLIST]: /chromium-os/developer-library/guides/kernel/kernel-development/#UPSTREAM_BACKPORT_FROMLIST_and-you
[SSH keys]: /chromium-os/developer-library/guides/development/developer-guide/#Set-up-SSH-connection-between-chroot-and-DUT
[trace-cmd man pages]: https://man7.org/linux/man-pages/man1/trace-cmd.1.html
[LWN trace-cmd HOWTO]: https://lwn.net/Articles/410200/
[kernel.eclass]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/eclass/cros-kernel.eclass
[KCSan]: http://issuetracker.google.com/issues/182965087
