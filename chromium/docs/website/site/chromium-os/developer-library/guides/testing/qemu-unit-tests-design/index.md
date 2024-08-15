---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: qemu-unit-tests-design
title: Running unittests via QEMU
---

This doc covers technical details for how we run unittests for non-native
architectures. It is geared towards people who want a deeper understanding of
the overall system. If you simply want to execute your tests, see the
[ChromiumOS Unit Testing] document instead.

[TOC]

## Introduction

ChromiumOS has a number of interesting features:

*   cross-compiling of everything (no native execution)
*   tests, both unit (API) and integration
*   support for a number of architectures (many of which are labeled as
    "embedded")

All of these taken together lead to an interesting state: we have a large code
base and want to test it, but how can we do it without resorting to real
hardware? Unit tests can be compiled and run using the SDK and gives us
confidence in the API, but we're still not testing the code that'll run on the
system (which has a number of implications). It also means we can't run fuller
(integration) tests.

The answers to our needs is [QEMU]. It allows us to run Linux programs compiled
for a different architecture as if it were native code. It does mean we rely on
QEMU's emulation of an architecture matching real hardware, but that's OK. If we
find discrepancies, we can fix QEMU. In practice, the project is pretty stable.

There is also another semi-minor point that might occur to the more astute
reader. What if the unittest wants to run another program itself? Say something
as innocent as using the system() helper function? Won't that execute the host's
/bin/sh and perhaps try to run target programs too? Indeed it will, but our
system below accounts for it transparently! We certainly want to avoid having to
modify code under test in ways that only get executed when testing.

Note that we are not doing full system emulation here. In that mode, we start
off by booting a full kernel, and then userspace, and then running programs.
Here we are just executing the unittests directly, and QEMU takes care of
converting system calls from the target to the host and vice versa.

## Technical Details

The overall process boils down to:

*   set up a full sysroot of libraries/programs
*   set up qemu inside of the chroot
*   register a handler for the target ELF
*   set up bind mounts for important file systems
*   make sure the unittests and necessary files are available inside the sysroot
*   chroot into the sysroot and use qemu to run the unittest

We also have to keep in mind that our testsuite might be running in parallel
with other testsuites and that a testsuite may have multiple unittests which
also might be running in parallel. So we have to be careful to not mess with
resources that are shared among the whole system otherwise things will be racy
and randomly fail. Flaky tests are terrible.

Note: All the setup described below is handled by chromite's
[qemu python module] now. The shell code below is more for reference.

### Sysroot Setup

As part of the ChromiumOS build system, we always have a full sysroot available
for a target board (created by build\_packages). This is usually found in the
`/board/$BOARD/` path inside of the sdk. We need this anyways in order to
compile packages for the target.

The sysroot is not generally directly bootable, but that doesn't matter to us.
We just need to be able to chroot into it.

## QEMU Setup

We ship a copy of QEMU inside the chroot already, and we make sure it is
statically linked. This lets us easily copy it into a sysroot and run it inside
of a chroot without having to copy any other files.

```bash
arch="arm"
name="qemu-${arch}"
qemu_src_path="/usr/bin/${name}"
qemu_sysroot_path="/build/${BOARD}/build/bin/${name}"

# Make sure the files are the same. This let's us easily update qemu in the
# chroot without having to worry about keeping all the sysroots up-to-date.
if [[ $(stat -c %i "${qemu_src_path}" 2>/dev/null) != $(stat -c %i "${qemu_sysroot_path}" 2>/dev/null) ]]; then
  # Do the update atomically. We'll hardlink to a unique path, and then use `mv`
  # a rename() for us. Since it's on the same device, we know it'll be atomic.
  ln -fT "${qemu_src_path}" "${qemu_sysroot_path}.$$"
  mv -fT "${qemu_sysroot_path}.$$" "${qemu_sysroot_path}"
fi
```

### Registering a Binary Format Handler

The Linux kernel provides a nifty pseudo filesystems called [`binfmt_misc`].
This allows you to directly execute files that are normally not natively
executable. For example, you could configure the system so that whenever you run
a Windows exe program, it'll automatically invoke [Wine] for you. It simplifies
the overall process. In our case, we'll register handlers so that QEMU is
automatically executed whenever we try to run one of these target ELFs.

#### Mount

First we mount the filesystem if it is not yet mounted:

```bash
$ mount -t binfmt_misc binfmt_misc /proc/sys/fs/binfmt_misc
```

If this errors out with `EBUSY`, we should check to see if it has been mounted
already. This way parallel testsuites can safely execute this.

#### Register

All handlers are registered by name (and the name is arbitrary). You can check
by name by looking at `/proc/sys/fs/binfmt_misc/<name>`. If it already exists,
we need to make sure it points to our interpreter. If it doesn't, we have to
unregister & re-register it. This phase is a little racy, but should rarely (if
ever) happen, so we'll just ignore the issue here.

```bash
# This magic string registers the ARM ELF format and tells it to use qemu to run.
arch="arm"
name="qemu-${arch}"
qemu_path="/build/bin/${name}"
qemu_magic=":${arch}:M::\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x28\x00:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:${qemu_path}:"

binfmt_path="/proc/sys/fs/binfmt_misc/${name}"

if [[ -e ${binfmt_path} ]]; then
  # Make sure the existing registration matches.
  if [[ $(awk '$1 == "interpreter" {print $NF}' "${binfmt_path}") != "${qemu_path}" ]]; then
    # Unregister it since there's a mismatch (maybe due to upgrade?).
    echo -1 | sudo tee "${binfmt_path}"
  fi
fi

if [[ -e ${binfmt_path} ]]; then
  # Register it since it doesn't yet exist.
  # We should ignore errors EBUSY at this point in case we run in parallel.
  echo "${qemu_magic}" | sudo tee "/proc/sys/fs/binfmt_misc/register"
fi
```

### Bind Mounts

Linux programs often want some basic filesystems available in order to run. We
bind mount them here to simplify things.

Normally the process would be to do these in a setup script that'd tail into
running the unittests. That way we can use the `unshare` utility to run the
unittest in its own mount namespace which allows us to create as many mounts as
we want without impacting other processes (like unittests running in parallel).

```bash
#!/bin/sh
# Make sure they've told us where to set things up.
if [ -z "${SYSROOT}" ]; then
  echo "You must set \$SYSROOT first"
  exit 1
fi

# Re-exec ourselves as root in a new mount namespace.
[ -z "${_UNSHARE}" ] && exec sudo unshare -m -- "$0" "$@"

# Do all the bind mounts we need.
mount -n --bind /proc "${SYSROOT}"/proc
mount -n --bind /dev "${SYSROOT}"/dev
mount -n --bind /dev/pts "${SYSROOT}"/dev/pts
mount -n --bind /sys "${SYSROOT}"/sys

# Run the unittest now.
exec chroot "${SYSROOT}" -- "$@"
```

### Source Tree Setup

Since the ChromiumOS build system builds all packages inside of the sysroot
already (typically `/build/$BOARD/tmp/portage/...`), we already have the files
available to us. We simply need to use paths relative to inside the sysroot.

### Chroot & Execute

Using the script from above, we can now run unittests inside the sysroot. It'll
take care of the sysroots and stuff for us.

```bash
$ ./helper-script.sh /tmp/portage/.../unittest
```

[ChromiumOS Unit Testing]: /chromium-os/developer-library/guides/testing/running-unit-tests/
[QEMU]: https://qemu.org/
[qemu python module]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/lib/qemu.py
[`binfmt_misc`]: https://www.kernel.org/doc/html/latest/admin-guide/binfmt-misc.html
[Wine]: https://www.winehq.org
