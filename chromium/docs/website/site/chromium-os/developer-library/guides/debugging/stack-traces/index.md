---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: stack-traces
title: How to get a stack trace at runtime
---

While debugging it's often useful to print stack traces at runtime. This
document describes how to do so.

[TOC]

## Typography conventions


| Label         | Paths, files, and commands                            |
|---------------|-------------------------------------------------------|
|  (shell)      | outside the chroot and SDK shell on your workstation  |
|  (sdk)        | inside the `chrome-sdk` SDK shell on your workstation |
|  (device)     | in your [VM](./cros_vm.md) or ChromeOS device        |
|  (cros)       | inside the `cros_sdk` ChromeOS chroot                |

## Chromium

### How to build and deploy a Chrome that can print runtime stack traces

#### Make sure is_official_build is false.

Unless you explicitly specify `--official` to chrome-sdk, this should be
false. It seems that recently (2022), enabling is_official_build stops
runtime stack traces from working in all scenarios.

#### Making sure `exclude_unwind_tables` is false or frame pointer unwinding works

NOTE: It seems that forcing `exclude_unwind_tables=false` no longer
works if is_official_build is true, as of 2022-ish. For now,
make sure is_official_build is false (see the above section).

You can skip this section if you are not building an official build, i.e.
not passing either `--internal` or `--official` to chrome-sdk, and not
explicitly setting `is_official_build=true` in your gn arguments.

You may also skip this section if frame pointer unwinding works on your
platform. Currently all platforms support frame pointer unwinding,
except ARM devices that use thumb instructions. See [compiler.gni] for
more information.

However, if you are building an official build then `exclude_unwind_tables`
will be true by default. To add flags to gn args, you can either [add them via]
`--gn-extra-args`:

```sh
(shell) cros chrome-sdk --board=eve --gn-extra-args="exclude_unwind_tables=false"
```

Or, you can run the following command, which will let you edit
the arguments directly. N.B. your path to your build directory may be
different, and re-running `cros chrome-sdk` will overwrite these.

```sh
(sdk) gn args out_${SDK_BOARD}/Release
```

Then append `exclude_unwind_tables=false`.

[add them via]: simple_chrome_workflow.md#cros-chrome_sdk-options
[compiler.gni]: https://source.chromium.org/chromium/chromium/src/+/main:build/config/compiler/compiler.gni

#### Deploying Chrome

After rebuilding (see example command below), make sure to pass the
`--nostrip` or `--strip-flags=-S` flags to `deploy_chrome`. The `--nostrip`
flag will usually result in a much bigger binary, so if you only need stack
traces at runtime, prefer `--strip-flags=-S`.

```sh
# Rebuild example
(sdk) autoninja -C out_${SDK_BOARD}/Release chrome nacl_helper
# Deploy chrome example
(sdk) deploy_chrome --build-dir=out_${SDK_BOARD}/Release --device=DUT --mount --strip-flags=-S
```

Note: The effects of the `--mount` option will not survive a reboot. If you
reboot, re-run deploy_chrome.

Note: `--mount` may be optional if the rootfs of your device is large enough for your build
configuration.

### How to use base::StackTrace

Next, include the following header in the file from which you want runtime
stack traces:

```c
#include "base/debug/stack_trace.h"
```

Then, add this code to where you want the stack trace.

```c
LOG(ERROR) << base::debug::StackTrace();
```

This code will output to the standard Chrome log, which is accessible via:

```sh
(device) tail -F /var/log/chrome/chrome
```

See the [chrome os logging] document for further info.

[chrome os logging]: https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/chrome_os_logging.md

It's also possible to log the stack trace to stderr with the following code:

```c
base::debug::StackTrace().Print();
```

However, this prints to stderr rather than the standard log, so it won't
show up in `/var/log/chrome/chrome`.

Be aware that taking a runtime stack trace is an expensive operation, so if
you put it in a hot codepath it can make Chrome's performance very bad.
In timing dependent code (e.g. race conditions, graphics related stuff),
it can even change the result.

Alternatively, you can save the stack trace (before printing it) and print it
out at a later time:

```c
auto stack = base::debug::StackTrace();
LOG(ERROR) << stack;
```

For reference, getting the stack trace itself takes on the order of
milliseconds. Symbolizing the stack trace takes on the order of hundreds
of milliseconds.

### Printing stack traces from the GPU process

Printing a stack trace at runtime requires access to /proc/self/maps among
other files. The GPU process does not have access to this, so stack traces
will show unsymbolized. To symbolize them, add the following flag to your
`/etc/chrome_dev.conf`:

```sh
--disable-gpu-sandbox
```

To know if you are in the GPU process or not, add a `LOG(ERROR)` to the code
you are inspecting, and get the process ID. For example, it's 8689 in the below
example:

```[8689:8689:0612/144957.826867:ERROR:window_state.cc(866)]```

Then get the chrome invocation for that PID:

```sh
(device) cat /proc/8689/cmdline
```

This should give something like this:

```sh
/opt/google/chrome/chrome --type=gpu-process <...>
```

In particular, `--type=gpu-process` tells you it is the GPU process.

### Printing stack traces from a renderer process

Similar to the GPU process, it requires permissions. You can identify the
process by its invocation having `--type=renderer`. To get runtime stack traces
while in a renderer process by adding the following flag to your
`/etc/chrome_dev.conf`:

```sh
--no-sandbox
```

### Debugging runtime stack traces not being symbolized

Sometimes stack traces are printed without being symbolized ("<unknown>").
If this happens, there are a few things to check:

1\. Check `exclude_unwind_tables` is false or frame-pointer unwinding works.

2\. Check the process you are printing from isn't sandboxed.

Add the sandbox disabling options mentioned in the document to rule this out.
Processes need to be able to access files in `/proc/self/` to inspect their
memory maps.

3\. Check that the binary on the DUT (device-under-test) hasn't been stripped.

Make sure deploy_chrome is run with `--nostrip` or `--strip-flags=-S`. You can
compare the size between the binary on your machine and the DUT to check
further.

4\. Check that you are running the correct chrome binary on the DUT.

Re-running `deploy_chrome` with the `--mount` option can rule this out.

5\. Check that the appropriate symbol_level gn argument is set.

Not setting `symbol_level` should be sufficient to avoid this issue.
However, you can rule this being a problem out by explicitly specifying
`symbol_level=1`.

See the below table to see which configurations will let you print stack
traces if you are unsure. However, building with symbol_level=-1 (default) and
exclude_unwind_tables=false should let you print runtime stack traces.

Stripping with `--strip-flags=-S` vs `--nostrip` produces a much smaller
binary if you are building with symbol_level=1 or symbol_level=2. For stack
trace purposes, symbol_level=1 should be enough.

If frame pointer unwinding is supported, you may be able to print
an unsymbolized stack trace. Unfortunately it is non-trivial to symbolize
the trace afterwards (with only the trace information). It would require
the memory map information, at least.

| symbol_level | exclude_unwind_tables | deploy_chrome  |  size  | runtime traces? |
|--------------|-----------------------|----------------|--------|-----------------|
| 2            | false                 | nostrip        | 5.3 GB | yes             |
| 2            | false                 | strip-flags=-S | 282 MB | yes             |
| 2            | false                 | <none>         | 197 MB | no              |
| 2            | true                  | nostrip        | 5.3 GB | yes             |
| 2            | true                  | strip-flags=-S | 262 MB | yes             |
| 2            | true                  | <none>         | 176 MB | no              |
| 0            | true                  | nostrip        | 263 MB | x86/x64 only    |
| 0            | true                  | strip-flags=-S | 262 MB | x86/x64 only    |
| 0            | true                  | <none>         | 176 MB | no              |

Data collected on hatch, 2021/05, ~M92 with GN flags like:

```sh
% gn gen out_hatch/Release --args="is_debug=false is_chrome_branded=true symbol_level=2 exclude_unwind_tables=false"
```

x86/x64 symbols available with symbol_level=0 ([source]).

[source]: https://source.chromium.org/chromium/chromium/src/+/HEAD:build/config/compiler/compiler.gni;l=222;drc=97cb4f90bf93714f139f5d0b8702256499a42075

From this table, we can tell the unwind tables take up ~21MB (197MB -
176MB), and the symbol tables take up ~86MB (262MB - 176MB).

### Example stack trace output

```c
#0 0x5788e671fe19 base::debug::CollectStackTrace()
#1 0x5788e66f2eb3 base::debug::StackTrace::StackTrace()
#2 0x5788e62bb264 content::ContentMainRunnerImpl::RunServiceManager()
#3 0x5788e62baca1 content::ContentMainRunnerImpl::Run()
#4 0x5788e62dc7b1 service_manager::Main()
#5 0x5788e62b916e content::ContentMain()
#6 0x5788e4127b65 ChromeMain
#7 0x7c58b7c8dad4 __libc_start_main
#8 0x5788e41279fa _start
```

# Symbolizing minidumps with tast symbolize

When Chrome or other programs, such as system daemons, crash on ChromeOS, they
save their memory in minidump format. This is where `tast symbolize` comes in
handy. You can obtain a stack trace from a minidump, without having to
reproduce the crash. It works for release builds and locally built binaries.
Crashes generated by Chrome and CrOS CQs are **not supported**. To symbolize
a minidump file use the following command inside the ChromeOS chroot:

```c
(cros) tast symbolize <crash.dmp>
```

Check [ChromiumOS Crash Reporting] to see where you can find the minidumps
on your test device. ``tast symbolize`` downloads symbols if the crashed binary
was produced by ChromeOS builders or generates them for local builds done
in ChromeOS chroot (this is different to simple chrome workflow discussed
above). Should you encounter any problems, try using
`tast --verbose symbolize <crash.dmp>` to see diagnostic information.

## How it works

This section describes how tast symbolize works. If you just want to get a stack
trace, you can stop reading here.

`tast symbolize` is typically run without any extra arguments and obtains all
necessary information from the minidump itself. The details depend on the origin
of the minidump:
 - Chrome crashes, which are handled by [Crashpad], generate minidumps
   containing `chromeos-board` and `chromeos-builder-path` annotations.
   `tast symbolize` uses these annotations to download or generate symbols.
 - Other binaries are handled by a similar tool, [Breakpad], which copies
  `/etc/lsb-release` into the minidump. `tast symbolize` extracts board name
  and builder path from this file.

You can inspect contents of a minidump using `minidump_dump` tool from Breakpad,
which is available in ChromeOS chroot. Googlers can also install
[prebuilt binaries] to use it outside chroot.

Under the hood, `tast symbolize` runs minidump_stackwalk twice.
 - First, it runs minidump_stackwalk to collect names and module IDs of
   the binaries referenced by a minidump. If everything can be symbolized, for
   example when cached symbols are available, then this is the final output.
 - Next, it obtains missing symbols. If the builder path is not empty, then it
   downloads them from `gs://chromeos-image-archive`. Otherwise, it uses
   dump_syms to generate symbols files from split debug files in build root
   (in the local chroot). Symbols are cached in `/tmp/breakpad_symbols`.
 - Finally, it runs minidump_stackwalk the second time and prints the output.

# Other resources

 - [Symbol Life Cycle](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/crash-reporter/docs/symbols.md)

## Googler only:

 - [go/cros-stack-traces](http://go/cros-stack-traces) - information about
   getting stack traces on DUTs

[ChromiumOS Crash Reporting]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/crash-reporter/docs/design.md
[Crashpad]: https://chromium.googlesource.com/crashpad/crashpad
[Breakpad]: https://chromium.googlesource.com/breakpad/breakpad
[prebuilt binaries]: ./simple_chrome_workflow.md#Googlers_Prebuilt-minidump_stackwalk-and-other-Breakpad-tools
