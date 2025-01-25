---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: running-unit-tests
title: ChromiumOS unit testing
---

[TOC]

## Resources for writing unit tests

* [Best practices for writing ChromeOS unit tests]
* [Introduction: Why Google C++ Testing Framework?]
* [GoogleTest FAQ]

## Running unit tests

We run unit tests as part of the build process using chromite's
[`cros_run_unit_tests`]. In a nutshell, this script uses the portage method of
building and running unit tests using `FEATURES="test"`. To run all the
ChromiumOS unit tests, you can run `cros_run_unit_tests`. If you want to run
just the unit tests for specific packages, you can use:

```bash
$ cros_sdk cros_run_unit_tests \
           --packages <space-delimited list of portage package names>
```

See `cros_run_unit_tests --help` for more information.

As an example, let's say you just want to run the unit tests for the metrics
package. To do so, you can run:

```bash
$ cros_sdk cros_run_unit_tests --board ${BOARD} --packages "metrics"
```

By default, `cros_run_unit_tests` runs packages in parallel, and shows the
test results only when any of the tests have failed. If you want to see
the test progress and results directly, you can use `-j1`. The full logs
are available in `/build/$BOARD/tmp/portage/logs`.

### Running unit tests with `cros_workon_make`

If you work on a package with `cros_workon`, you can use `cros_workon_make
--test` to run unit tests more quickly.

```bash
$ cros_sdk cros_workon_make --board=${BOARD} ${PACKAGE_NAME} --test
```

Just like `cros_run_unit_tests`, `cros_workon_make` runs the tests in parallel.
If you want to see the test progress directly (e.g. when performing printf
debugging), you can use PLATFORM_PARALLEL_GTEST_TEST="no":

```bash
$ cros_sdk PLATFORM_PARALLEL_GTEST_TEST="no" cros_workon_make \
           --board=${BOARD} ${PACKAGE_NAME} --test
```

### Running a subset of unit tests

To run a subset of tests for a package that uses `gtest`, you can use
GTEST_ARGS= to set a filter with a regex like this:

```bash
# Run the string utilities unit tests for libbrillo.
$ cros_sdk GTEST_ARGS="--gtest_filter=StringUtils.*" cros_run_unit_tests \
           --board ${BOARD} --packages libbrillo
```

You can also use `P2_TEST_FILTER` for a platform2 package:

```bash
$ cros_sdk P2_TEST_FILTER="StringUtils.*" cros_workon_make --board=${BOARD} --test libbrillo
```

For more information, consult the [upstream gtest documentation].

### Running unit tests in gdb

Once your unit tests are built, you need to enter the sysroot (in
`/build/${BOARD}`) with `platform2_test.py`. This script will set up
the environment so that the binaries can be executed correctly (entering the
sysroot, using qemu, etc.).

Note: qemu doesn't support the `ptrace` syscall
used by gdb, so even though the program will run under the debugger,
breakpoints/traps/etc won't work.

To run a shell within the sysroot:

```
/mnt/host/source/src/platform2/common-mk/platform2_test.py \
  --board=${BOARD} /bin/bash
```

From within the shell, you can run your binaries (including unit tests),
and run them with gdb. Within the sysroot, the gdb accessible from `$PATH`
is the correct target gdb. Since you have entered the sysroot, you will
find the build artifacts under
`/var/cache/portage/chromeos-base/${package_name}`.

### Note on CPU architectures

Unit tests typically run code on your host machine which is compiled for the
target ChromeOS system. When the target CPU architecture differs from host
machine (e.g., running ARM unit tests on an x86 host), we run them under QEMU.
When running x86 unit tests on an x86 host, we run them natively. Note that
this can cause problems when the ISA is similar but not identical -- see
https://crbug.com/856686 for more info. In practice, this means that unit tests
built for AMD systems may fail when your host system uses an Intel CPU.

## Adding unit tests

### For platform2 packages

For packages using platform2, see the [biod ebuild] and [biod BUILD.gn] as
a good example of adding your unit test. These steps include:

*   Adding a `*_test.cc` test file (see [example file]).
    *   See `gtest` syntax and the usages from [Google Test] - Chromium's C++
        test harness.

### For non-platform2 packages

See [example CL] for a good example of adding new unit tests. These steps
include:

*   Modifying the package ebuild.
    *   Create a `src_test()` stanza with appropriate `gtest` filters (see
        below), and add `test? ( dev-cpp/gtest:= )` to`DEPEND` in your ebuild.
*   Adding a [testrunner].
    *   Look at the [canonical example].
*   Adding a `*_test.cc` test file (see [example file]).
    *   See `gtest` syntax and the usages from [Google Test] - Chromium's C++
        test harness.

Regarding `src_test()` stanza, it is fine to have them build in the
`src_compile()` stage as well. See also [Portage documentation] on `src_test()`.

## Non-native architectures (e.g. QEMU)

Platform2 supports running unittests via QEMU for non-native architectures (low
level details can be found in [this doc]). The good news is that the process is
the same as described above! Simply use `cros_run_unit_tests` for your board and
specify the packages you want to run.

### Caveats

Sometimes QEMU is not able to support your unittest due to using functionality
not yet implemented. If you see errors like `qemu: unknown syscall: xxx`, you
will need to filter that test (see below), and you should [file a bug] so we can
update QEMU.

Since QEMU only works when run inside the chroot, only ebuilds that use the
`platform.eclass` are supported currently.

## Filtering tests

Sometimes a test is flaky or requires special consideration before it'll
work. In general it is not recommended to disable flaky/broken tests in
ebuilds. Instead add `DISABLED_` to the prefix of the test name in the source,
hence eliminating to touch ebuilds at all. Look at [this example CL]. Otherwise,
see below for disabling tests in ebuilds.

For packages using `platform.eclass` use the existing gtest filtering logic by
simply passing it as an argument to `platform_test`:

```bash
platform_pkg_test() {
    local gtest_filter=""
    gtest_filter+="DiskManagerTest.*"

    platform_test "run" "${OUT}/some_testrunner" "0" "${gtest_filter}"
}
```

For other packages, export the relevant gtest variables directly in `src_test()`
function.

## Disabling tests for non-native architectures

For packages using `platform.eclass` update the global knob in your ebuild:

```bash
PLATFORM_NATIVE_TEST="yes"
```

For other packages, write your `src_test()` like so:

```bash
src_test() {
    if ! use x86 && ! use amd64; then
        ewarn "Skipping unittests for non-native arches"
        return
    fi
    ... existing test logic ...
}
```

## Running unit tests as "root"

For platform2 users, unittests run as non-root by default. If a unittest really
needs root access, they can do so. Here is [an example CL] showing how to. It is
recommended to mark such tests with `RunAsRoot` in their names.

## Special Considerations

By default, `cros_run_unit_tests` will only use `FEATURES="test"` on packages
that:

*   are in the dependency tree of `virtual/target-os` (packages only installed
    in SDK are ignored).
*   have a known `src_test()` function; i.e. ebuilds that:
    *   explicitly declare `src_test()`, or
    *   explicitly declare `platform_pkg_test()` & inherit the `platform`
        eclass, or
    *   inherit the `cros-common.mk` eclass, or
    *   inherit the `cros-go` eclass, or
    *   inherit the `tast-bundle` eclass.

[Best practices for writing ChromeOS unit tests]: /chromium-os/developer-library/guides/testing/unit-tests/
[Introduction: Why Google C++ Testing Framework?]: https://github.com/google/googletest/blob/HEAD/docs/primer.md
[GoogleTest FAQ]: https://github.com/google/googletest/blob/HEAD/docs/faq.md
[`cros_run_unit_tests`]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/scripts/cros_run_unit_tests.py
[platform2 testing section]: /chromium-os/developer-library/guides/development/platform2-primer/#running-unit-tests
[biod ebuild]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/chromeos-base/biod/biod-9999.ebuild
[biod BUILD.gn]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/biod/BUILD.gn
[Google Test]: https://github.com/google/googletest
[Portage documentation]: https://devmanual.gentoo.org/ebuild-writing/functions/src_test/index.html
[this doc]: /chromium-os/developer-library/guides/testing/qemu-unit-tests-design/
[file a bug]: https://crbug.com/new
[example CL]: https://crrev.com/c/583938/
[example file]: https://crrev.com/c/583578/7/src/manifest_unittest.cc
[canonical example]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/common-mk/testrunner.cc
[testrunner]: https://chromium-review.googlesource.com/c/583578/7/src/testrunner.cc
[this example CL]: https://crrev.com/c/1760792
[an example CL]: https://crrev.com/c/1716195
[upstream gtest documentation]: https://github.com/google/googletest/blob/HEAD/docs/advanced.md#running-a-subset-of-the-tests
