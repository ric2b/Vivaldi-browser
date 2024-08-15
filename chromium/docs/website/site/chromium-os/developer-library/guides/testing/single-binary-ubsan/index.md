---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: single-binary-ubsan
title: Running a single binary with UBSAN
---

[UBSAN](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html) is a
runtime undefined-behavior sanitizer. When a program is built with UBSAN, it
will print an error message and fail out if the program does things which are
not allowed in C++, such as overflowing a signed integer or exiting a non-void
function without returning a value.

[TOC]

## What this guide is

Currently, it is not possible to boot a copy of ChromiumOS using UBSAN.
However, it is possible to build individual binaries using UBSAN and then run
them on a machine which is otherwise running a normal developer image. This can
be useful for checking for bugs in an individual program.

This guide describes how to run a single UBSAN-instrumented binary on a test
device.

These instructions should also work for running a single binary with the other
sanitizers. Just change the USE flag to asan, tsan, etc instead of ubsan.

### Restrictions and Caveats

Since the UBSAN build is already broken (does not compile), the build is not
checked by CQ, and may break further over time. These directions may break at
any time. They worked in March of 2021.

I only tried this on a single board (cyan, which is an X86 architecture). Other
boards, other architectures may not work.

I only tried this with anomaly_detector and crash_reporter. Other binaries may
not work if their libraries will not compile under UBSAN.

I have not tried this with the Chromium binary. This guide is about the
ChromiumOS-specific binaries.


## Steps

`(chroot) $` indicates a command that should be run inside the cros_sdk chroot.
 `localhost #` indicates a command that should be run in a root shell on the DUT
 (test device).

### Set USE UBSAN

Choose a chroot shell in which you will do all the other compiles below. You
must do all compiling in this shell.

```bash
(chroot) $ USE=ubsan; export USE
```

Once you do this, do not attempt any other (non-UBSAN) compiles in this shell!
Mixing UBSAN and non-UBSAN compiles in the same build will not end well. I
usually recolor the xterm I set `USE=ubsan` in so I remember to not use it for
anything else.

### Recompile libraries using UBSAN

In the same shell,

```bash
(outside) $ cros build-packages --board=${BOARD} --cleanbuild --autosetgov \
               --skip-chroot-upgrade
```

This will eventually fail with a compile error, but it will build the libraries
first.

### Recompile binary using UBSAN

In the same shell, `emerge` the binary you want. For instance, to compile
anomaly_detector, which is in the crash-reporter ebuild file:

```bash
(chroot) $ emerge-${BOARD} crash-reporter
```

This should compile correctly.

### Determine which libraries you need

Get the list of libraries that your binary needs using `lddtree`. For
anomaly_detector, this would look like:

```bash
(chroot) $ lddtree /build/${BOARD}/usr/bin/anomaly_detector
```

For anomaly_detector, this might print out:

```
/build/cyan/usr/bin/anomaly_detector (interpreter => /lib64/ld-linux-x86-64.so.2)
    libbrillo-core.so => /usr/lib64/libbrillo-core.so
        libbase-dl.so => /usr/lib64/libbase-dl.so
        libcrypto.so.1.1 => /usr/lib64/libcrypto.so.1.1
            libz.so.1 => /lib64/libz.so.1
    libbase-core.so => /usr/lib64/libbase-core.so
        libdouble-conversion.so.3 => /usr/lib64/libdouble-conversion.so.3
    (etc)
```

The names and paths on the right side are the libraries we want.

### Transfer binary and libaries

If you haven't yet, make sure your test device has a writable drive:

```bash
localhost # /usr/libexec/debugd/helpers/dev_features_rootfs_verification
localhost # reboot
```

Make a directory where your binary and its libraries will live. (You can't
replace the non-UBSAN libraries with the UBSAN libraries; it will crash other
parts of the system. So you need a separate location to keep them.)

```bash
localhost # mkdir /etc/ubsan_test
```

Now, `scp` the UBSAN binaries over. You'll want to transfer the binary over,
plus all the libraries we discovered above. For each library, copy-and-paste
the name+path, minus the initial /, to get the relative path from the build
directory. So for anomaly_detector, given the lddtree output above, we might do:

```bash
(chroot) $ cd /build/${BOARD}
(chroot) $ scp usr/bin/anomaly_detector usr/lib64/libbrillo-core.so \
                usr/lib64/libbase-dl.so usr/lib64/libcrypto.so.1.1 \
                lib64/libz.so.1 usr/lib64/libbase-core.so \
                usr/lib64/libdouble-conversion.so.3 \
                root@my_crbook:/etc/ubsan_test
```

Note: If there are any libraries that you can't find, just skip them. The on-
device test will default to the non-UBSAN version of any libraries you don't
copy over.

### Sub-programs

Any programs launched by the UBSAN-instrumented binary also need to be compiled
using UBSAN, or they will fail when trying to link to the UBSAN-instrumented
libraries.

As an example, anomaly_detector launches crash_reporter. So I needed to compile
crash_reporter using UBSAN, check what libraries it uses via `lddtree`, and
copy the crash_reporter binary and all of its libraries to /etc/ubsan_test. I
also needed to change anomaly_detector's code to launch
/etc/ubsan_test/crash_reporter instead of /sbin/crash_reporter.

### Run on the test device

Set LD_LIBRARY_PATH to point to the test directory. Tell UBSAN to log to
stderr.

For example

```bash
localhost # cd /etc/ubsan_test
localhost # LD_LIBRARY_PATH=/etc/ubsan_test UBSAN_OPTIONS="log_path=stderr" \
            ./anomaly_detector
```

If you don't set `UBSAN_OPTIONS="log_path=stderr"`, the logs will appear in
/var/log/asan (yes, even for UBSAN)

### Clean up

Remember to close the shell that has `USE=ubsan`. Delete the build directory
to force all the libraries to recompile without UBSAN.

```bash
(outside) $ cros build-packages --board=${BOARD} --cleanbuild --autosetgov
```

Good hunting, and may all your bugs be shallow!
