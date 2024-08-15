---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: fuzzing
title: Fuzz testing in ChromeOS
---

Fuzzing is a testing technique that feeds auto-generated inputs to a piece of
target code in an attempt to crash the code. It's one of the most effective tools we
have for finding [security] and [non-security bugs] (also see
[go/fuzzing-success](http://go/fuzzing-success)). You can learn more about the
benefits of fuzzing at [go/why-fuzz](http://go/why-fuzz).

This guide introduces ChromeOS developers to fuzz testing.
It assumes basic familiarity with the ChromeOS development environment. If
you're looking for information on fuzz testing code in other projects, like
Android or Google3, see [go/fuzzing](http://go/fuzzing).

[TOC]

## How does fuzz testing work in ChromeOS?

Fuzzing takes a set of initial inputs called a *seed corpus* and randomly
mutates it to try to crash the code under test. ChromeOS fuzz testing is
*coverage-guided*, which means that when a new input increases the amount of
code covered by the testing, that input gets added to the corpus.

In ChromeOS, you can fuzz a piece of code (e.g., an API) by creating a program
called a *fuzz target*. The target gets linked with a fuzzing engine
([libFuzzer]) and picked up by [ClusterFuzz], Google's cross-platform fuzzing
infrastructure.

libFuzzer uses your fuzz target to produce an executable (via Clang) called
the *fuzzer*, which calls the code you want to test. ClusterFuzz then runs the
fuzzer and makes sure you can act on the results; it picks up new fuzz
targets, fuzzes them, reports bugs, and even verifies fixes.


## Quickstart

This section describes the basic steps to writing a fuzzer in ChromeOS and
getting ClusterFuzz to pick it up. For more details on each step, see
[Detailed instructions](#Detailed-instructions).

*** note
**Note**: If you're working on a platform package (one that is in the
`platform` or `platform2` source directory and whose ebuild inherits from the
`platform` eclass), skip to
[Build a fuzzer for a platform package](#platform).
***

Quickstart steps:

1.  In your package, write a new test program with a name that ends in `_fuzzer`
    (it's a good idea to prepend your package name to your fuzzer as well, e.g.
    `cryptohome_<descriptive-name>_fuzzer`. In the program, define the function
    `LLVMFuzzerTestOneInput` with the following signature:

    ```cpp
    #include <cstddef>
    #include <cstdint>

    extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
        <your test code goes here>
        return 0;
    }
    ```

    *** note
    **Note**: Make sure `LLVMFuzzerTestOneInput` calls the function you
    want to fuzz.
    ***

2.  Update the build system for your package to build your `*_fuzzer` binary.

3.  Update your package's ebuild file:

    *   Add `fuzzer` to the `IUSE` flags list.
    *   Build your new fuzzer (conditioned on `use fuzzer`), with the appropriate
        flags:
        1.  [Inherit cros-fuzzer and cros-sanitizers eclasses]
        2.  [Set up flags: call sanitizers-setup-env in src_configure]
        3.  [USE flags: fuzzer]
    *   Install your binary in `/usr/libexec/fuzzers/`
    *   If applicable, build the libraries your fuzzer depends on with the
        appropriate `-fsanitize` flags.

4.  Build and test your new fuzzer locally, then commit your changes.

5.  Add the package dependency to the `chromium-os-fuzzers` ebuild, then commit
    the change.

That's it! The continuously running [fuzzer builder] on the ChromeOS waterfall
automatically detects your new fuzzer, builds it, and uploads it to ClusterFuzz,
which will start running it. For more on what you can do with ClusterFuzz, see
[Using ClusterFuzz](#Using-ClusterFuzz).

[**Return to Top**](#top)

## Detailed instructions

This section goes over the [Quickstart](#Quickstart) steps in more detail:

*   [Build a fuzzer for a platform package](#platform)
*   [Build a fuzzer for a non-platform package](#non-platform)
*   [Write a fuzzer](#Write-a-fuzzer)
*   [What if my code doesn't simply consume a chunk of bytes?](#bytes)
    *   [FuzzedDataProvider](#FuzzedDataProvider)
    *   [Libprotobuf-mutator](#lib)
*   [Get help modifying ebuild files](#Get-help-modifying-ebuild-files)

[**Return to Top**](#top)

### Build a fuzzer for a platform package

This section describes how to build a fuzzer for a platform package (one that is
in the `platform` or `platform2` source directory and whose ebuild inherits from
the `platform` eclass). If you're not working on a platform package, see
[Build a fuzzer for any other package](#non-platform).

1.  In your package, write a new test program. Start with a stub fuzzer:

    ```cpp
    // Copyright <copyright_year> The ChromiumOS Authors
    // Use of this source code is governed by a BSD-style license that can be
    // found in the LICENSE file.

    #include <cstddef>
    #include <cstdint>

    class Environment {
     public:
      Environment() {
        // Set-up code.
      }
    };

    extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
      static Environment env;
      // Fuzzing code. Empty for now.
      return 0;
    }
    ```

    *** note
    **Note**: It might seem counterintuitive to build a fuzzer before it's
    written, but the process of actually writing the fuzzer will be easier with
    a fast compile-test cycle. To set this up, we need to be able to compile the
    fuzzer.
    ***

2.  Update the build system for your package to build your `*_fuzzer` binary.

    *   For packages that are built with GN files, update your GN file to
        build the fuzzer binary:

        ```
        # Update group("all") if necessary with the following
        group("all") {
            # ...
            if (use.fuzzer) {
                deps += [
                    # ...
                    ":your_fuzzer",
                ]
            }
        }

        # ...

        if (use.fuzzer) {
          executable("your_fuzzer") {
            configs += [
              "//common-mk/common_fuzzer",
              ":target_defaults",
            ]
            sources = [
              "your_fuzzer.cc",
            ]
            deps = [
              "libprotobuf-mutator", # For fuzzing with Protobufs (optional)
               # any other library dependencies in your package
            ]
          }
        }
        ```

        See the [midis GN file] for a complete example.

    *   If your package is not built with a GN file, then you will need to
        update your `Makefile` (or whatever build system you use), in such a way
        that your fuzzer binary gets built when a special flag or argument is
        passed to the normal build command.

        If testing this by hand (without the ebuild file changes in the next
        step), you will need to manually pass the compiler flags
        `-fsanitize=address -fsanitize=fuzzer-no-link` and the linker flags
        `-fsanitize=address -fsanitize=fuzzer` to your build. You will not need
        to pass these flags manually once you have updated the ebuild file.

3.  Update your package's ebuild file:

    1.  Update the ebuild file to build the new binary when the fuzzer `USE` flag
        is used. **For platform packages built with GN files, you should
        skip this step and go directly to the next step for installing the
        fuzzer binary.**

        *  Find the `src_compile()` function in your ebuild file. If there
            isn't one, add one:

            ```bash
            src_compile() {
            }
            ```

        *  Add the call to actually build your fuzzer.

            Find the line in your `src_compile` function that actually builds
            your package (the command will probably look like `emake` or `make`
            or `cmake`). This is the command that is meant by *original build
            command* below. Copy the original build command and add whatever
            flags or arguments you need in order to make it build just your
            fuzzer binary (see step 2 above). Replace the original build command
            in the `src_compile` function with a conditional statement similar
            to the one below, so that when `USE="fuzzer"` is used to build the
            package, it will build your fuzzer binary, otherwise it will build
            the package normally.

            ```bash
            if use fuzzer ; then
                 <modified build command>
            else
                 <original build command>
            fi
            ```

    2.  Install your binary in `/usr/libexec/fuzzers/`

        In your ebuild file, find the `src_install()` function. Add a
        statement to install your fuzzer:

        ```bash
        local fuzzer_component_id="<your buganizer componentid>"
        platform_fuzzer_install "${S}"/OWNERS "${OUT}"/<your_fuzzer> \
            --comp "${fuzzer_component_id}"
        ```

        If you don't have a Buganizer componentid yet, you can omit the
        `--comp <...>` flag, and it will be reported in [Machine-found-bugs]
        by default. However, relying on the default should be the exception,
        not the rule.

4.  Build and test your new fuzzer locally.

    To build your new fuzzer, once you have updated the ebuild file, it should
    be sufficient to build it with `USE="asan fuzzer"`.

    *** promo
    **Tip**: Fuzzing using the undefined behavior sanitizer (ubsan) is also
    supported. To use ubsan, simply replace `asan` with `ubsan` in the commands
    below.
    ***

    *** note
    **Note**: If your package depends on chromeos-chrome, the
    `cros build-packages` command below can take a very long time.  You may want
    to to adjust the package dependencies to not depend on `chromeos-chrome`
    with `USE=fuzzer`.
    ***

    ```bash
    (outside) $ cros workon --board ${BOARD} start <your_package>
    # Run cros build-packages to build the package and its dependencies.
    (outside) $ USE="asan fuzzer" cros build-packages --board=${BOARD} \
                    --skip-chroot-upgrade <your_package>
    # If you make more changes to your fuzzer or build, you can rebuild the package with:
    (chroot) $ USE="asan fuzzer" emerge-${BOARD} <your_package>
    ```

    For more details on `cros-workon` packages, see the [Developer Guide][cros-workon].

    You should verify that your fuzzer was built and that it was installed in
    `/build/${BOARD}/usr/libexec/fuzzers/` (make sure the `.owners` file was
    installed there as well).

    *** note
    **Warning**: The .owners file installed with the fuzzer must contain
    actual email addresses, not an include-link to another OWNERS file. If
    the OWNERS file in the fuzzer's directory is just an include-link, you
    should install the target OWNERS file instead, like
    `platform_fuzzer_install "${S}"/../target/OWNERS "${OUT}"/<your_fuzzer>`
    ***

    To run your fuzzer locally, run this command to prepare the environment
    and get a shell ready for fuzzing:

    ```bash
    (chroot) $ cros_fuzz --board=${BOARD} shell
    ```

    Then run your fuzzer:

    ```bash
    (board chroot) # /usr/libexec/fuzzers/<your_fuzzer>
    ```

    *** note
    **Note**: The fuzzer will run forever (or until it finds a bug), so
    you will want to halt it manually (using Ctrl-C) after a couple of minutes.
    ***

    You can read more about the `cros_fuzz` script in the
    [Using cros_fuzz](#script-cros-fuzz) section. You should also verify that
    your package still builds correctly without `USE="fuzzer"`.

    Once you are happy with your new fuzzer, commit your changes.

5.  Add the package dependency to the `chromium-os-fuzzers` ebuild. Inside your
    chroot:

    Edit `~/chromiumos/src/third_party/chromiumos-overlay/virtual/chromium-os-fuzzers/chromium-os-fuzzers-1.ebuild`.
    In that file, find the `RDEPEND` list and add your package/fuzzer (you can
    look at the other packages there, to see how it's done). Don't forget to
    [uprev the ebuild] symlink. Commit the changes and upload them for review.

6.  Optional: Verify that the `amd64-generic-fuzzer` builder is happy with your
    changes.

    Submit a tryjob *outside of the chroot*:
    ```bash
    (outside) $ cros tryjob -g 'CL1 CL2' amd64-generic-fuzzer-tryjob
    ```
    Replace *CL1* and *CL2* with the actual CL numbers.

    You should verify that your package is picked up by the builder by looking
    at the `BuildPackages` stage logs.

    The builder builds the full system with AddressSanitizer and libFuzzer
    instrumentation. If you do not want a particular library pulled in by your
    changes to be instrumented, you can add a call to `filter_sanitizers` in
    the library's ebuild file.

[**Return to Top**](#top) > [**Return to Detailed
instructions**](#Detailed-instructions)

### Build a fuzzer for a non-platform package

This section describes how to build a fuzzer for a non-platform package. If
you're working on a platform package, (one that is in the `platform` or
`platform2` source directory and whose ebuild inherits from the `platform`
eclass), see
[Build a fuzzer for a platform package](#platform).

1.  In your package, write a new test program. Start with a stub fuzzer:

    ```cpp
    // Copyright <copyright_year> The ChromiumOS Authors
    // Use of this source code is governed by a BSD-style license that can be
    // found in the LICENSE file.

    #include <cstddef>
    #include <cstdint>

    class Environment {
     public:
      Environment() {
        // Set-up code.
      }
    };

    extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
      static Environment env;
      // Fuzzing code. Empty for now.
      return 0;
    }
    ```

    *** note
    **Note**: It might seem counterintuitive to build a fuzzer before it's
    written, but the process of actually writing the fuzzer will be easier with
    a fast compile-test cycle. To set this up, we need to be able to compile the
    fuzzer.
    ***

2.  Update the build system for your package to build your `*_fuzzer` binary.

    The exact instructions here are going to vary widely, depending on your
    package and the build system in your package (Make, Ninja, SCons, etc.).
    In general, you must be able to invoke your normal build
    command, passing a special flag or argument or environment variable, so
    that it will build your fuzzer binary and only your fuzzer binary. This
    will involve updating GN files or Makefiles or whatever other build files
    your package uses.

    If testing this by hand (without the ebuild file changes in step 3 below),
    you will need to manually pass the compiler flags
    `-fsanitize=address -fsanitize=fuzzer-no-link` and the linker flags
    `-fsanitize=address -fsanitize=fuzzer` to your build. You will not need
    to pass these flags manually once you have updated the ebuild file.

3.  Update your package's ebuild file:

    1.  Add `fuzzer` to the `IUSE` flags list.

        In all probability your package ebuild already contains an IUSE
        definition. Look for a line starting with `IUSE="..."`, and add `fuzzer`
        to the list. If your file does not already contain such a line, add
        one near the top:

        ```bash
        IUSE="fuzzer"
        ```

        See the [puffin ebuild] for a good example.

    2.  Update the ebuild file to build the new binary when the fuzzer USE flag
        is used:

        *  Find the `inherit` line in your ebuild (near the top of the
            file). Make sure that `cros-fuzzer and cros-sanitizers` are in the
            inherit list. If your file does not have a line that starts with
            `inherit`, add one near the top (after the `EAPI` line and before
            the `KEYWORDS` line):

            ```bash
            inherit cros-fuzzer cros-sanitizers
            ```

        *  Find the `src_configure()` function in your ebuild file. If
            there isn't one, add one:

            ```bash
            src_configure() {
            }
            ```

        *  Add calls `sanitizers-setup-env`, near the top of
            `src_configure`, to set the appropriate compiler/linker flags:

            ```bash
            src_configure() {
                sanitizers-setup-env
                ...
            }
            ```

        *  Find the line in your `src_compile` function that actually builds
            your package (the command will probably look like `emake` or
            `make` or `cmake`). This is the command that is meant by *original
            build command* below. Copy the original build command and add
            whatever flags or arguments you need in order to make it build
            just your fuzzer binary (see step 1 above). Replace the original
            build command in the `src_compile` function with a conditional
            statement similar to the one below, so that when `USE="fuzzer"` is
            used to build the package, it will build your fuzzer binary,
            otherwise it will build the package normally.

            ```bash
            if use fuzzer ; then
                <modified build command>
            else
                <original build command>
            fi
            ```

    3.  Install your binary in `/usr/libexec/fuzzers/`

        In your ebuild file, find the `src_install()` function. Add a
        `fuzzer_install` statement to install your fuzzer:

        ```bash
        fuzzer_install "${S}/OWNERS" <your_fuzzer>
        ```

        (The owners part above is so that ClusterFuzz knows to whom to assign
        the bugs generated by this fuzzer.)

        *** note
        **Warning**: The .owners file installed with the fuzzer must contain
        actual email addresses, not an include-link to another OWNERS file. If
        the OWNERS file in the fuzzer's directory is just an include-link, you
        should install the target OWNERS file instead, like
        `fuzzer_install "${S}/../target/OWNERS" <your_fuzzer>`
        ***

4.  Build and test your new fuzzer locally.

    To build your new fuzzer, once you have updated the ebuild file, it should
    be sufficient to build it with `USE="asan fuzzer"`:

    **Note**: Fuzzing using undefined behavior sanitizer (ubsan) is also
    supported. To use ubsan, simply replace `asan` with `ubsan` in the
    commands below.

    ```bash
    # Run cros build-packages to build the package and its dependencies.
    (outside) $ USE="asan fuzzer" cros build-packages --board=${BOARD} \
                    --skip-chroot-upgrade <your_package>
    # If you make more changes to your fuzzer or build, you can rebuild the package by:
    (chroot) $ USE="asan fuzzer" emerge-${BOARD} <your_package>
    ```

    You should verify that your fuzzer was built and that it was installed in
    `/build/${BOARD}/usr/libexec/fuzzers/` (make sure the owners file was
    installed there as well). To run your fuzzer locally, run this command to
    prepare the environment and get a shell ready for fuzzing:

    ```bash
    (chroot) $ cros_fuzz --board=${BOARD} shell
    ```

    Then run your fuzzer:

    ```bash
    (board chroot) # /usr/libexec/fuzzers/<your_fuzzer>
    ```

    *** note
    **Note**: The fuzzer will run forever (or until it finds a bug), so
    you should halt it manually (using Ctrl-C) after a couple of minutes.
    ***

    You can read more about the `cros_fuzz` script in the
    [Using cros_fuzz](#script-cros-fuzz) section. You should also verify that
    your package still builds correctly without `USE="fuzzer"`.

    Once you are happy with your new fuzzer, commit your changes.

5.  Add the package dependency to the `chromium-os-fuzzers` ebuild. Inside your
    chroot:

    Edit `~/chromiumos/src/third_party/chromiumos-overlay/virtual/chromium-os-fuzzers/chromium-os-fuzzers-9999.ebuild`.
    In that file, find the `RDEPEND` list and add your package/fuzzer with
    the fuzzer USE flag (you can look at the other packages there, to see how
    it's done). Commit the changes and upload them for review.

6.  Optional: Verify that the `amd64-generic-fuzzer` builder is happy with your
    changes.

    Submit a tryjob *outside of the chroot* as:
    ```bash
    (outside) $ cros tryjob -g 'CL1 CL2' amd64-generic-fuzzer-tryjob
    ```
    Replace *CL1* and *CL2* with the actual CL numbers.

    You should verify that your package is picked up by the builder by looking
    at the `BuildPackages` stage logs.

    The builder builds the full system with AddressSanitizer and libFuzzer
    instrumentation. If you do not want a particular library pulled in by your
    changes to be instrumented, you can add a call to `filter_sanitizers` in the
    library's ebuild file.

[**Return to Top**](#top) > [**Return to Detailed
instructions**](#Detailed-instructions)

### Write a fuzzer

Now that you can build and execute your fuzzer, you can start working on getting
it to actually test things. If the function you want to test takes a chunk of
bytes and a length, then you're done: that's what the fuzzing scaffolding will
provide in the `LLVMFuzzerTestOneInput` function. Just pass that to your
function:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Call your function here.
  return 0;
}
```

Some things to keep in mind:

*   The fuzz target will be executed many times with different inputs in the
    same process.
*   It must tolerate any kind of input (empty, huge, malformed, etc). You can
    write your fuzz target to simply return 0 on certain types of malformed
    input. See the [GURL fuzzer] for such an example.
*   It must not `exit()` on any input.
*   It may use threads but ideally all threads should be joined at the end of
    the function.
*   It must be as deterministic as possible. Non-determinism (e.g. random
    decisions not based on the input bytes) will make fuzzing inefficient.
*   It must be fast. Avoid >= cubic complexity, logging, high memory
    consumption.
*   Ideally, it should not modify any global state (although that's not strict).

In particular, if you're fuzzing code that's using the logging primitives from
`<base/logging.h>`, you should disable logging:

```cpp
#include "base/logging.h"

class Environment {
 public:
  Environment() {
    logging::SetMinLogLevel(logging::LOGGING_FATAL);  // <- DISABLE LOGGING.
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  // Fuzzing code.
  return 0;
}
```

[**Return to Top**](#top) > [**Return to Detailed
instructions**](#Detailed-instructions)


### What if my code doesn't simply consume a chunk of bytes?

Odds are that the code that you want to test *doesn't* just take a chunk of
bytes and a length. You might want to fuzz an API where functions take integers,
strings, or protocol buffers. There might be useful state to set up before
hitting parsing code, or you might want to test a state machine where some calls
need to happen in a certain order to avoid erroring out of functions early.

Don't despair! This is actually very common. Most fuzzers end up extracting some
structure out of the random data received in order to better exercise the code
under test. Below are two different tools for handling this case. One for
non-protos and another for protocol buffers.

[**Return to Top**](#top) > [**Return to Detailed
instructions**](#Detailed-instructions)

#### [FuzzedDataProvider]

FuzzedDataProvider is a class that can generate many things, such as integers
or strings from the fuzz target input data.

Consider the interface of the `permission_broker` [firewall implementation]:

```cpp
bool AddAcceptRules(ProtocolEnum protocol,
                    uint16_t port,
                    const std::string& interface);
bool DeleteAcceptRules(ProtocolEnum protocol,
                       uint16_t port,
                       const std::string& interface);
```
It's not really straightforward to feed a chunk of bytes to this API. To address
this, we can use code provided by [FuzzedDataProvider]. In order to use it, add
`#include <fuzzer/FuzzedDataProvider.h>` to your fuzz target source file. To
learn more about `FuzzedDataProvider`, check out the
[google/fuzzing documentation page] on it.

`FuzzedDataProvider` will consume fuzzing input and allow extracting structure
out of it:

```cpp
class FuzzedDataProvider {
public:
  ...
  std::string ConsumeBytesAsString(size_t num_bytes);

  std::string ConsumeRemainingBytesAsString();

  std::string ConsumeRandomLengthString(size_t max_length);

  template <typename T> T ConsumeIntegralInRange(T min, T max);

  template <typename T> T ConsumeIntegral();

  template <typename T> T ConsumeFloatingPointInRange(T min, T max)

  template <typename T> T ConsumeFloatingPoint();

  bool ConsumeBool();
  ...
};
```

Using this API, we can obtain integers and strings to pass to the above API, and
also use booleans to decide how to call the API:

```cpp
#include <fuzzer/FuzzedDataProvider.h>
...
...
  uint8_t num_ports = data_provider.ConsumeIntegral<uint8_t>();
  for (size_t i = 0; i < num_ports; i++) {
    bool is_tcp = data_provider.ConsumeBool();
    uint16_t port = data_provider.ConsumeIntegral<uint16_t>();

    if (!is_tcp && port == 0) {
      // Did we run out of data? Consume another bool to check.
      if (!data_provider.ConsumeBool())
        break;
      }
    }
...
...
    if (do_add) {
      fake_firewall.AddAcceptRules(is_tcp ? permission_broker::kProtocolTcp
                                          : permission_broker::kProtocolUdp,
                                   port, "iface");
    } else {
      fake_firewall.DeleteAcceptRules(is_tcp ? permission_broker::kProtocolTcp
                                             : permission_broker::kProtocolUdp,
                                      port, "iface");
    }
...
```

The fuzzer is using its input to decide what to do and which functions to call.
As long as the `FuzzedDataProvider` object is initialized with the same input,
its functions will return the same values and the results will be deterministic.

Note that in this particular case we could have chosen to also pass a fuzzed
string for the `interface` argument. However, the check for that argument
happens very early in the code, so fuzzing that argument ends up being
counterproductive because it prevents the rest of the code from being reached.
When writing your fuzzer, take this into account. If random input is unlikely to
make it past an initial check consider using a boolean value to decide whether
or not to test that argument, or forgo testing that argument altogether.

Running the fuzzer locally will continuously print a measure of coverage that
can be used (in relative terms) to understand whether skipping an argument
allows the fuzzer to reach more code:

`#3268 NEW    cov: 218`

The `cov` value will increase (unsurprisingly) with increased coverage. In the
current example, avoiding random strings for the `interface` argument
significantly increased coverage because both API functions were no longer
erroring out early.

Note that it's not recommended to use `FuzzedDataProvider` unless you actually
need to split the fuzz input. If you need to convert the fuzz input into a
vector or string object, for example, simply initialize that object by passing
`const uint8_t* data, size_t size` to its constructor.

[**Return to Top**](#top) > [**Return to Detailed
instructions**](#Detailed-instructions)

#### [Libprotobuf-mutator]

For cases where your code accepts a protocol buffer as input, there is a library
[libprotobuf-mutator] that generates mutated protocol buffers for fuzz targets.
First identify the function(s) you want to call in your fuzz target. If there
is only one input proto, writing a fuzz target is really easy and you can
skip the next paragraph.

Whenever you need more than one input proto or want to invoke a function
multiple times, you can make a new proto message that includes all the input
protos. Repeated fields can be used in cases where more than one action needs to
be performed multiple times to get good code coverage from a fuzz target.

```cpp
// Copyright <copyright_year> The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libprotobuf-mutator/src/libfuzzer/libfuzzer_macro.h>

#include "my_code.h"
#include "my_proto.pb.h"

DEFINE_PROTO_FUZZER(const my_package::MyProto& input) {
  my_code::MyFunction(input);
}
```

Building this fuzz target requires linking against [libprotobuf-mutator]. You
can add `'libprotobuf-mutator'` under the `deps` section in the GN buildfile
for platform projects ([GN Example](#platform)).

Lastly, for both platform and non-platform projects
`fuzzer? ( dev-libs/libprotobuf-mutator )` should be added to the dependency
list in the ebuild. This makes sure the `dev-libs/libprotobuf-mutator` package
is installed on the target and the headers are available at compile time.

For additional tips you can take a look at this chromium document for writing a
[grammar-based-fuzzer] with libprotobuf-mutator.

[**Return to Top**](#top) > [**Return to Detailed
instructions**](#Detailed-instructions)

### Get help modifying ebuild files

Some ebuild files are more complex or confusing than others. There are
several links in the [References](#References) section of this document that
can help you with understanding/editing your ebuild file. If you are still
having difficulties editing your ebuild file and need more help, please file a
bug in the [Chromium issue tracker], assign it to the
*Tools>ChromeOS-Toolchain* component, and send an email to
[chromeos-fuzzing@google.com]. We will try to help you figure this out.

[**Return to Top**](#top) > [**Return to Detailed
instructions**](#Detailed-instructions)

### Using ClusterFuzz

As already mentioned, ClusterFuzz will pick up any fuzzer written using the
above steps, run the fuzzer, and file bugs for any crashes found. ClusterFuzz
runs fuzzers as soon as the [fuzzer builder] completes a build and uploads it
to the Google Cloud Storage bucket: `gs://chromeos-fuzzing-artifacts/`.

ClusterFuzz has many features such as statistics reporting that you may find
useful. Below are links to some of the more important ones:

*   [Fuzzer statistics] - Statistics from fuzzer runs, updated daily. Ignore the
    columns `edge_cov`, `func_cov`, and `cov_report` as these are not supported
    for ChromeOS. Graphs of stats can viewed by changing the "Group by" drop
    down to "Time" and specifying the fuzzer you are interested in, rather than
    "libFuzzer".
*   [Crash statistics] - Statistics on recent crashes.
*   [Fuzzer logs] - Cloud storage bucket containing logs output by your fuzzer
    each time ClusterFuzz runs it. This is usually a good place to debug issues
    with your fuzzer. The URI for this bucket is:
    `gs://chromeos-libfuzzer-logs/`.
*   [Fuzzer corpus] - Cloud storage bucket containing test cases produced by the
    fuzzer that libFuzzer has deemed "interesting" (meaning it causes unique
    program behavior). The URI for this bucket is:
    `gs://chromeos-corpus/libfuzzer/`.

Note that you may need to authenticate to access the corpus and logs buckets
using `gsutil`. See the documentation on [Configuring Authentication] for how to
do this.

[**Return to Top**](#top) > [**Return to Detailed
instructions**](#Detailed-instructions)

## Using cros_fuzz

`cros_fuzz` is a script we have provided to use within the ChromeOS SDK chroot.
Its purpose is to make fuzzer development easier by automating important tasks.
These include:

*   [Preparing the environment for fuzzing]
*   [Getting a coverage report for your fuzzer]
*   [Reproducing Crashes from ClusterFuzz]

`cros_fuzz` is used like so:

   ```bash
   (chroot) $ cros_fuzz --board=${BOARD} <command> <command arguments>
   ```

You can get detailed help information on each command by using the command
argument: `--help`. We explain the more important commands in each subsection.

*** promo
**Tip**: Set your `.default_board` to avoid the need to specify `--board` every
time you run `cros_fuzz`

**Tip**: `cros_fuzz` will print every shell command it runs if you set the
log-level to debug ("--log-level debug").
***

[**Return to Top**](#top)

### Preparing the environment for fuzzing

This section describes the `shell`, `cleanup`, `setup` and commands, in that
order. To run a fuzzer, you should not just chroot into the board where it was
built and run it since libFuzzer needs certain special files that are present on
most systems but not on the board. You should also not run the fuzzer
outside of the board because that environment will contain devices and other
things that will not be available on ClusterFuzz. Instead you should run the
`shell` command from `cros_fuzz`. `shell` will prepare the board for fuzzing and
then chroot into the board giving you a shell. It is simple to use:

   ```bash
   (chroot) $ cros_fuzz --board=${BOARD} shell
   (board chroot) # /usr/libexec/fuzzers/<your_fuzzer>
   ```

The changes made by the `shell` command to the board can be mostly undone
using the `cleanup` command.

Internally, the `shell` command does the same thing as the `setup` command and
then gives you a shell with the `ASAN_OPTIONS` env variable set to
`log_path=stderr`, which is needed to view stack traces when the fuzzer finds a
bug. You can use the `setup` command to run your fuzzer using this sequence of
these bash commands:

   ```bash
   (chroot) $ cros_fuzz --board=${BOARD} setup
   (chroot) $ sudo chroot /build/${BOARD}
   (board chroot) # ASAN_OPTIONS="log_path=stderr" /usr/libexec/fuzzers/<your_fuzzer>
   ```

[**Return to Top**](#top) > [**Return to Using cros_fuzz**](#script-cros-fuzz)

### Getting a coverage report for your fuzzer

This section describes the `coverage` command. Coverage reports are a great way
to tell what code your fuzzer is actually testing. `cros_fuzz` provides an easy
way to get a coverage report using the `coverage` command.

Before we explain how to use `coverage`, there are two things you should
understand about the coverage report process.

*   First is the importance of a corpus in generating a coverage report. As
    explained in the [How does fuzz testing work?](#how) section, a testcase is
    added to the corpus if it increases the fuzzer's coverage of the target
    code. Thus, if everything works correctly, one can use a fuzzer's corpus
    from fuzzing to quickly find the code covered by the fuzzer.

*   The second thing to understand is how we generate the report. The report is
    generated by [clang's source based coverage]. This is not the same
    instrumentation libFuzzer uses to get coverage data, but we use it
    because the reports it generates are much better than the ones libFuzzer
    generates. Because we need a new kind of instrumentation to generate the
    report we must do a new kind of build.

The `coverage` command on the most simple level: runs a fuzzer, collects
coverage info from the run, and then generates an HTML coverage report for you
to view. It can also automate many things you would want to do in this process
including doing a build with the source based coverage instrumentation and using
a fuzzer's corpus from disk or from ClusterFuzz (if it is already run on
ClusterFuzz).

These are a list of the options the `coverage` command accepts, and an
explanation of each of them.

*   `--fuzzer <your_fuzzer>`: The name of the fuzzer to generate a coverage
    report for. This is the only mandatory option.

*   `--package <your_package>`: The name of the fuzzer's package. Not mandatory,
    but you probably should use this unless you know what you are doing. If this
    option is used, a coverage build of the package will be done. A coverage
    build uses the `USE` flags `fuzzer asan coverage`. Unless the current build
    of the fuzzer on the board is a coverage build, the `coverage` command will
    fail.

*   `--corpus <path_to_corpus>`: The path to a corpus we should use when
    generating the coverage report. If this option is used `cros_fuzz` will copy
    the corpus into the board (if needed) and the fuzzer will run every
    testcase in the corpus once and will not actually do any fuzzing. This
    option is mutually exclusive with `--download`.

*   `--download`: Whether to download the corpus from ClusterFuzz. This will
    fail if the fuzzer has not yet been run on ClusterFuzz. It will also fail if
    you are not logged into `gsutil` using your `@google.com` account (it will
    instruct you on how to do this if that happens). If the download succeeds,
    then pretty much the same thing happens if the `--corpus` command were used,
    the fuzzer will run every testcase in the corpus but not try to find any new
    testcases (fuzzing). This option is mutually exclusive with `--corpus`.

*  `--fuzz-args <libfuzzer_options>`: libFuzzer options to pass to the fuzzer.
    Each separated by a space. Note that using this option will probably
    confuse the script's argument parser unless you use it like this:
    `--fuzz-args="-max_total_time=60"`

Once this command has completed, it will print a `file://` URI of an HTML
coverage report that you can view in your browser.
If neither the `--corpus` or `--download` options is used, there needs to be
some kind of arbitrary limit imposed on fuzzing so it doesn't continue forever.
If you don't specify one, the script will specify a timeout of 30 seconds. You
can specify a limit by passing one of the `-max_total_time` or `-runs` libFuzzer
options in `--fuzz-args`.

Please note that in order to get the report with the detailed information about
line by line coverage in source files, you would need to build the packages with
the related libraries with `FEATURES="noclean"`.

If you want access to some of the data used by this command, it is probably
stored in the directory: `/build/${BOARD}/tmp/fuzz/` but we make no guarantees
about this.

[**Return to Top**](#top) > [**Return to Using cros_fuzz**](#script-cros-fuzz)

### Reproducing crashes from ClusterFuzz

This section explains how to reproduce bugs found by ClusterFuzz. No knowledge
of fuzzing is assumed and it summarizes info from elsewhere in this document.

To reproduce crashes you should use the `reproduce` command from `cros_fuzz`,
here's a guide on how to use it to reproduce crashes reported by ClusterFuzz.

1.  Download the reproducer testcase from the link on the bug report, and copy
    it to the board.

    ```bash
    (outside) $ cp ~/Downloads/<testcase_name> /path/to/chromiumos-checkout/chroot/build/${BOARD}/tmp/
    ```

2.  Identify the package, build type, and the name of the fuzzer. The name of
    the fuzzer will be on the "Fuzzer:" line. It will begin with
    "libFuzzer_chromeos_" but this prefix is added by ClusterFuzz and isn't
    actually part of the binary name. You can determine the build type from the
    "Sanitizer" line in the bug report. It will be one of asan/ubsan/msan.

3.  Run the `reproduce` command of `cros_fuzz` from within the ChromeOS SDK
    chroot like so:

    ```bash
    (chroot) $ cros_fuzz --board=${BOARD} reproduce --fuzzer <your_fuzzer> --testcase /path/to/testcase/<testcase_name> \
              --package <package_of_fuzzer> --build-type <build_type>
    ```

You should see a stack trace after running the `reproduce` command.

**Note**: For reproducing msan crashes, [a full build](#third-party-crashes) of
all packages with instrumentation is needed.

For more advanced uses, we will explain the details for the `reproduce` command.
Below is an explanation of what the options to `reproduce` mean, note that no
option is mandatory unless explicitly specified.

*  `--testcase`: The path to the testcase to run the fuzzer on. Mandatory. Note
   that the path must be in the chroot, but does not need to be on the board, it
   will be copied if necessary.

*  `--fuzzer`: The fuzzer to run the testcase on. Mandatory.

*   `--package`: The package of the fuzzer to build. If this option is provided
    then one must also provide the `--build-type` option.

*   `--build-type`: The type of build we want to do. This can either be `asan`,
    `msan`, `ubsan`, or `coverage`. Note that `coverage` is not actually used on
    ClusterFuzz. This option may only be used if the `--package` option is used.

Note that once the build needed is in a root it doesn't need to be done again,
so it isn't necessary to use `--package` unless another build clobbered the one
we need if we have already done the build we need.

To summarize: the reproduce command does a build of the package (if needed),
copies the testcase into the board, prepares the board for running the fuzzer,
and then runs the fuzzer on the testcase.

[**Return to Top**](#top) > [**Return to Using cros_fuzz**](#script-cros-fuzz)

## Improving fuzzer effectiveness

This section describes optional techniques for improving the effectiveness of
your fuzzer.

*   [Adding a seed corpus](#Adding-a-seed-corpus)
*   [Adding a dictionary](#Adding-a-dictionary)
*   [Adding an options file](#Adding-an-options-file)

*** note
**Note**: These instructions primarily explain how to make improvements
used by ClusterFuzz. Local use is explained at the end of each section.
***

[**Return to Top**](#top)

### Adding a seed corpus

A seed corpus is a set of interesting test cases (input files) that are
meaningful for your fuzz target and provide a good starting point for libFuzzer
to mutate. For example, if you are fuzzing a PNG parser, a good seed corpus
would be a small number of small PNG files from the parser's test suite.
A seed corpus is helpful in this case since it is much easier for libFuzzer
to produce interesting PNGs if it starts from valid ones than if it starts
from nothing (where it would need to learn to produce PNG-like inputs over
many thousands of executions).

A small file that has the same coverage as a large file is better since it is
more likely that the fuzzer will mutate important bytes rather than unimportant
ones (eg: comments in a C file). Real inputs generally lead to better coverage
than inputs generated by fuzzing from scratch.

Your seed corpus should contain no redundant (ie: causing no unique program
behavior) test cases.
You can minimize the size of your seed corpus (but not individual testcases) by
running these commands:

```bash
(board chroot) $ mkdir MINIMIZED_SEED_CORPUS
(board chroot) $ ./<your_fuzzer> -merge MINIMIZED_SEED_CORPUS <path_to_your_fuzzers_corpus>
```

Once you have decided what files you will include in your seed corpus, you can
upload it to the target's corpus (after the fuzzer has already run for the first
time, which will probably happen within a day of committing it) by running this
command:

```bash
# Verify that your fuzzer's seed corpus directory exists.
(outside) $ gsutil ls -l gs://chromeos-corpus/libfuzzer/chromeos_<your_fuzzer_name>
# Upload new seed corpus files.
(outside) $ gsutil -m cp <path_to_your_fuzzers_corpus>/* gs://chromeos-corpus/libfuzzer/chromeos_<your_fuzzer_name>
```

You can upload newer corpus files to this location any time. Files uploaded to
this directory will get pruned when no longer useful.
Your ChromeOS SDK chroot comes with a copy of `gsutil`, or you can install it
on your host by following the [instructions to set up the `gsutil` tool]. Note
that `gsutil` must be connected to your `@google.com` account to have access to
the corpus.

To use a corpus in local fuzzing, pass the directory to your fuzzer, like so:

```bash
(board chroot) $ ./<your_fuzzer> <corpus_directory>
```

[**Return to Top**](#top) > [**Return to Improving fuzzer
effectiveness**](#Improving-fuzzer-effectiveness)

### Adding a dictionary

A dictionary is a file containing tokens that are useful for fuzzing a
particular format. For example if we are fuzzing a C compiler, useful tokens
would be things like `void`, `int`, `if`, `break` etc. Dictionaries are passed
to libFuzzer using the `-dict=$DICTIONARY_FILE` argument.
In a dictionary, each token should be quoted and appear on a line by itself.
Here is an example from the libFuzzer documentation:

```
# Lines starting with '#' and empty lines are ignored.

# Adds "blah" (w/o quotes) to the dictionary.
"blah"
# Use \\ for backslash and \" for quotes.
"\"ac\\dc\""
# Use \xAB for hex values
"\xF7\xF8"
```

Once you have decided the content of your dictionary, add it to a file called
`<your_fuzzer>.dict` and then edit your ebuild to install the dictionary, like
so:

```bash
platform_fuzzer_install "${S}"/OWNERS "${OUT}"/<your_fuzzer> \
    --dict "${S}"/path/to/<your_fuzzer>.dict
```

The ebuild for `config_parser_fuzzer` can be used as an
[example for installing a dictionary].

There are many [dictionaries in the Chromium code base], you may be able to
reuse one if your fuzzer's format is also fuzzed in Chrome.

To use a dictionary in local fuzzing, use the `-dict=` option, like so:

```bash
(board chroot) $ ./<your_fuzzer> -dict=/path/to/your/dictionary
```

[Return to Top](#top) > [**Return to Improving fuzzer
effectiveness**](#Improving-fuzzer-effectiveness)

### Adding an options file

You can set options that will be passed to libFuzzer. You can read about
libFuzzer options in the [libFuzzer docs]. The options below will probably
be most useful to you.

*    `-ascii_only` which instructs libFuzzer to only feed ascii characters to
     your target.
*    `-max_len` which instructs libFuzzer not to feed inputs larger than a
     certain size to your target.

Note that your fuzzer can not depend on options being passed to it, because
ClusterFuzz sometimes intentionally omits options. This means that if your
target cannot accept inputs beyond a certain length, you need to handle this in
the target function (`LLVMFuzzerTestOneInput`), rather than depending on
`max_len`, which is an optimization. Your options file should use this format:

```
[libfuzzer]
ascii_only = 1
max_len = 1024
```


Your options file should be named: `<your_fuzzer>.options`
You can install the options file by editing your ebuild to use `--options` like
so:

```bash
platform_fuzzer_install "${S}"/OWNERS "${OUT}"/<your_fuzzer> \
    --options "${S}"/path/to/<your_fuzzer>.options
```

Note that you do not need to change an options file when adding a dictionary.
ClusterFuzz automatically passes the dictionary, if named correctly, to
libFuzzer.

[**Return to Top**](#top) > [**Return to Improving fuzzer
effectiveness**](#Improving-fuzzer-effectiveness)

## FAQ

This section provides answers to common questions:
* [Will my fuzzer get past checks or conditional statements?](#conditional-statements)
* [How do I reproduce issues found in a third party library?](#third-party-crashes)
* [How do I run gdb on a fuzzer?](#gdb)
* [How do I suppress errors reported by a fuzzer?](#suppress)

[Return to Top](#top)

### Will my fuzzer get past checks or conditional statements?

It depends on the check, but the answer is probably yes. Most checks (such as
string comparisons against a magic string) are easy for the fuzzer to get past.
Other less common operations, such as verification of a checksum, hash, or
signature are quite hard for the fuzzer to get past. You should first verify
(for example, by adding print statements) that the fuzzer is not passing a
certain check before worrying about it. If the checks are not among the
difficult ones listed above or similar to them, adding [a dictionary] or [a seed
corpus] can help. It is only recommend to try the options below if these
suggestions does not work and you are confident that the fuzzer is having
trouble passing some check.

*   The first option is to `#ifdef` the check so that it isn't done in fuzzing
    builds. This generally works in cases where it is easy to craft an input by
    hand that causes the targeted code, with the check, to behave the same as a
    fuzzer-generated input without the check. A good example of this are the
    checksums on each chunk section in a PNG. libFuzzer isn't smart enough to
    write the correct checksum each time it mutates the chunk, but correcting
    the chunk by hand is trivial and a chunk with the correct causes the same
    behavior (except for the check itself) in the program with or without the
    check.

*   The second option is to process the input from libFuzzer in
    `LLVMFuzzerTestOneInput` to make it acceptable to your targeted code. For
    example, if your target code accepts input with the following format:

    ```
    [md5sum(Body)][Body]
    ```

    You can write `LLVMFuzzerTestOneInput` so that it passes the hash check:

    ```c++
    extern "C" int LLVMFuzzerTestOneInput(uint8t_t* data, size_t size) {
        std::string processed_data = md5sum(data, size) + std::string(data);
        TargetedFunction(processed_data.data(), processed_data.size());
        return 0;
    }
    ```

*   The options above may not be enough for a complicated format. There are
    other tools such as libprotobuf-mutator that allow you to specify a format
    for libFuzzer to mutate, which you then convert into raw bytes.

[**Return to Top**](#top) > [**Return to FAQ**](#FAQ)

### How do I reproduce issues found in a third party library?

The fuzzing builders instrument most of the packages with sanitizer flags which
can sometimes find errors in third party libraries.

To reproduce the errors found in a third party package, the easiest way is to
build the packages with sanitizers flags just like the builders.

```bash
# Run setup_board with the fuzzer profile.
# The example below uses "--profile=fuzzer" which selects asan flags.
# For msan or ubsan, use "--profile=msan-fuzzer" or "--profile=ubsan-fuzzer".
(chroot) $ setup_board --board=amd64-generic --profile=fuzzer
# Run cros build-packages to build the package and its dependencies.
# Note that `--nousepkg` must be passed to avoid using prebuilts.
(outside) $ cros build-packages --board=amd64-generic --skip-chroot-upgrade \
                --no-usepkg <your_package>
```

Once the package and its dependencies have been built,
[cros_fuzz](#script-cros-fuzz) can be used
to reproduce the issue using the downloaded testcase.

[**Return to Top**](#top) > [**Return to FAQ**](#FAQ)

### How do I run gdb on a fuzzer?

If you want to to debug a fuzzer using gdb, you can do it through gdbserver.
[cros_fuzz](#script-cros-fuzz) automatically
copies `/usr/bin/gdbserver` to the board if present. `gdbserver` is provided by
the chroot package `sys-devel/gdb` and can be installed as:

```bash
(chroot) $ sudo emerge sys-devel/gdb
```

To debug a fuzzer binary using `gdbserver`, two separate cros_sdk chroot shells
are needed.

Shell 1:
Start gdbserver on port 8888 with the binary.
```bash
(chroot) $ cros_fuzz --board=<board> shell
(chroot) $ gdbserver :8888 /usr/libexec/fuzzers/<fuzzer>
```

Shell 2:
Run gdb on the binary and attach to the gdbserver session.
```bash
(chroot) $ gdb /build/<board>/usr/libexec/fuzzers/<fuzzer>
  (gdb) target remote :8888
  (gdb) set sysroot /build/<board>
  (gdb) set breakpoints, debug as usual.
```

*** note
Both [LeakSanitizer][lsan] and gdb use [`ptrace(2)`][ptrace-manual] -
but `ptrace` cannot be used concurrently by two different entities. If
examining a leaky binary under gdb, have LeakSanitizer cede `ptrace` to
gdb by invoking `gdbserver` with the environment variable
`ASAN_OPTIONS+=":detect_leaks=0"`.
***

[**Return to Top**](#top) > [**Return to FAQ**](#FAQ)

### How do I suppress errors reported by a fuzzer?

If you want to suppress some errors reported by fuzzer that are not interesting
or not actionable, those errors can be suppressed by:

*   (*Preferred*) Using clang's no_sanitize attribute:
    [Clang's no_sanitize attributes] can be used to suppress the specific error
    (e.g. [skia error suppression]). This is strongly preferred for cases where
    source code can be modified.
    Note: Packages using libchrome can also use the
    [macros provided in libchrome].

*   Using a blocklist file:
    If modifying source code is not an option, then a blocklist file can be
    used to specify compile time suppressions. It requires that the package
    [inherits cros-sanitizers eclass] and [calls sanitizers-setup-env] in
    the src_configure stage. Packages that [inherit platform eclass] do not
    need to add this step as the [platform eclass] takes care of calling
    `sanitizers-setup-env`. The blocklist file can be added in the any of the
    following locations:
    1. `files` directory of the ebuild.
    2. The source directory root of the package i.e. the location pointed by
      `${S}` variable in the package ebuild.

    The blocklist file should have one of the following names:

    * sanitizer_blocklist.txt:  A common blocklist for all sanitizer types.
    * asan_blocklist.txt: Address sanitizer (asan) specific.
    * msan_blocklist.txt: Memory sanitizer (msan) specific.
    * ubsan_blocklist.txt: Undefined behavior sanitizer (ubsan) specific.

    See the [libchrome blocklist] as an example of suppressing ubsan errors.
    The syntax of the blocklist file is explained in more details at
    [clang's sanitizer special case list page].

[**Return to Top**](#top) > [**Return to FAQ**](#FAQ)

## Getting help

You can send an email to [chromeos-fuzzing@google.com] if you get stuck, or to
ask questions.

[**Return to Top**](#top)

## References

* [Setting up Fuzzing for ChromeOS](https://docs.google.com/document/d/1tvY4YV6q5RPVGK8hivkSKLPbNufebZ6BBozT0LqbGRA/edit?usp=sharing)

* [Continuous in-process fuzzing for ChromeOS targets](https://docs.google.com/document/d/1sd1IejWzcbQgF7soVVKlqTMk3HZfvPGR7QYP5T6qBYU/edit#heading=h.5irk4csrpu0y)

* [libFuzzer - a library for coverage-guided fuzz testing.](https://llvm.org/docs/LibFuzzer.html)

* [Fault injection through unexpected input data (aka Fuzz Testing)](https://goto.google.com/fuzzing)

* [Getting Started with libFuzzer in Chromium](https://chromium.googlesource.com/chromium/src/+/HEAD/testing/libfuzzer/getting_started.md)

* [Gentoo Cheat Sheet](https://wiki.gentoo.org/wiki/Gentoo_Cheat_Sheet)

* [Gentoo Ebuild Variables Guide](https://devmanual.gentoo.org/ebuild-writing/variables/)

* [Gentoo Ebuild Install Functions Reference](https://devmanual.gentoo.org/function-reference/install-functions/)

* [Gentoo Ebuild Writing Developers Manual](https://devmanual.gentoo.org/ebuild-writing/index.html)

[**Return to Top**](#top)

## Useful go links

* [go/ClusterFuzz](https://sites.google.com/corp/google.com/clusterfuzz/home)

* [go/libfuzzer-Chrome](https://chromium.googlesource.com/chromium/src/+/HEAD/testing/libfuzzer/README.md)

* [go/fuzzit](http://go/fuzzit)

* [go/fuzzing](https://g3doc.corp.google.com/security/fuzzing/g3doc/index.md?cl=head)

* [go/whyfuzz](https://docs.google.com/document/d/1jNDjMBrXyCalNDQsHGmXP-Xc2M1_mmE4eptdOBE0tfA/edit#heading=h.6icp69vxzbq)

[**Return to Top**](#top)

[libFuzzer]: https://llvm.org/docs/LibFuzzer.html
[ClusterFuzz]: https://sites.google.com/corp/google.com/clusterfuzz/home
[libFuzzer and ClusterFuzz]: https://chromium.googlesource.com/chromium/src/+/HEAD/testing/libfuzzer/README.md
[security]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=reporter:clusterfuzz@chromium.org%20-status:duplicate%20-status:wontfix%20type=bug-security
[non-security bugs]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=reporter%3Aclusterfuzz%40chromium.org+-status%3Aduplicate+-status%3Awontfix+-type%3Dbug-security&sort=modified
[Inherit cros-fuzzer and cros-sanitizers eclasses]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/645c52be0d4388eb8200f8ef07cc60875dcc5b10/media-libs/virglrenderer/virglrenderer-9999.ebuild#6
[Set up flags: call sanitizers-setup-env in src_configure]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/645c52be0d4388eb8200f8ef07cc60875dcc5b10/media-libs/virglrenderer/virglrenderer-9999.ebuild#47
[USE flags: fuzzer]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/07580574deb4409eec940ed582d6139b26094c07/dev-util/puffin/puffin-9999.ebuild#29
[Using ClusterFuzz]: #using-clusterfuzz
[bsdiff fuzzer]: https://android.googlesource.com/platform/external/bsdiff/+/HEAD/bspatch_fuzzer.cc
[puffin_fuzzer]: https://android.googlesource.com/platform/external/puffin/+/HEAD/src/fuzzer.cc
[GURL fuzzer]: https://chromium.googlesource.com/chromium/src/+/HEAD/url/gurl_fuzzer.cc
[midis GN file]: https://chromium.googlesource.com/chromiumos/platform2/+/547320548b8bd6cd1b4d78dfdeefdc720882e1dc/midis/BUILD.gn#83
[puffin ebuild]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/dev-util/puffin/puffin-9999.ebuild
[Chromium issue tracker]: https://crbug.com
[fuzzer builder]: https://cros-goldeneye.corp.google.com/chromeos/legoland/builderHistory?buildConfig=amd64-generic-fuzzer&buildBranch=master
[chromeos-fuzzing@google.com]: mailto:chromeos-fuzzing@google.com
[firewall implementation]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/patchpanel/firewall.h#30
[FuzzedDataProvider]: https://github.com/llvm/llvm-project/blob/d2af368aee56abf77f4a6ca3fd57ebdb697c48f2/compiler-rt/include/fuzzer/FuzzedDataProvider.h
[google/fuzzing documentation page]: https://github.com/google/fuzzing/blob/HEAD/docs/split-inputs.md#fuzzed-data-provider
[libprotobuf-mutator]: https://github.com/google/libprotobuf-mutator
[firewall_fuzzer]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/patchpanel/firewall_fuzzer.cc
[Fuzzer statistics]: https://clusterfuzz.com/fuzzer-stats/by-fuzzer/fuzzer/libFuzzer/job/libfuzzer_asan_chromeos
[Crash statistics]: https://clusterfuzz.com/crash-stats?block=day&days=7&end=423713&fuzzer=libFuzzer&group=platform&job=libfuzzer_asan_chromeos&number=count&sort=total_count
[Fuzzer logs]: https://console.cloud.google.com/storage/browser/chromeos-libfuzzer-logs/
[Fuzzer corpus]: https://console.cloud.google.com/storage/browser/chromeos-corpus/libfuzzer
[example for installing a dictionary]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/d1b2628a2008712af9752f79bc0972b222a56a05/chromeos-base/kerberos/kerberos-9999.ebuild#63
[dictionaries in the Chromium code base]: https://cs.chromium.org/search/?q=file:.*dict$&sq=package:chromium&type=cs
[libFuzzer docs]: https://llvm.org/docs/LibFuzzer.html#options
[a dictionary]: #adding-a-dictionary
[a seed corpus]: #adding-a-seed-corpus
[issue 853017]: https://crbug.com/853017
[instructions to set up the `gsutil` tool]: https://cloud.google.com/storage/docs/quickstart-gsutil
[Using cros_fuzz]: #using-cros_fuzz
[Preparing the environment for fuzzing]: #preparing-the-environment-for-fuzzing
[Reproducing crashes from ClusterFuzz]: #reproducing-crashes-from-clusterfuzz
[Getting a coverage report for your fuzzer]: #getting-a-coverage-report-for-your-fuzzer
[What is fuzz testing?]: #what-is-fuzz-testing
[clang's source based coverage]: https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
[Configuring Authentication]: /chromium-os/developer-library/reference/tools/gsutil/#setup
[cros-workon]: /chromium-os/developer-library/guides/development/developer-guide/#Making-changes-to-packages-whose-source-code-is-checked-into-ChromiumOS-git-repositories
[Clang's no_sanitize attributes]: https://clang.llvm.org/docs/AttributeReference.html#no-sanitize
[macros provided in libchrome]: https://chromium.googlesource.com/chromiumos/platform/libchrome/+/5ca6b5581735fdb7a46249d4eb587aff936434f5/base/compiler_specific.h#168
[skia error suppression]: https://skia.googlesource.com/skia.git/+/d6f3f18d51ec612d38019ce6cb3021050c6b5a84/include/private/SkFloatingPoint.h#157
[inherits cros-sanitizers eclass]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/645c52be0d4388eb8200f8ef07cc60875dcc5b10/media-libs/virglrenderer/virglrenderer-9999.ebuild#6
[calls sanitizers-setup-env]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/c85a04a77eb8895a4b8ef4ee3619aa539748a590/media-libs/virglrenderer/virglrenderer-0.7.0_p20190401.ebuild#52
[inherit platform eclass]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/c85a04a77eb8895a4b8ef4ee3619aa539748a590/chromeos-base/p2p/p2p-0.0.1-r3038.ebuild#17
[platform eclass]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/c85a04a77eb8895a4b8ef4ee3619aa539748a590/eclass/platform.eclass#183
[libchrome blocklist]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/8c39d9715e86a6a62cc327bf2aefe4a18b430a02/chromeos-base/libchrome/files/ubsan_blocklist.txt
[clang's sanitizer special case list page]: https://clang.llvm.org/docs/SanitizerSpecialCaseList.html
[grammar-based-fuzzer]: https://chromium.googlesource.com/chromium/src/+/HEAD/testing/libfuzzer/libprotobuf-mutator.md
[uprev the ebuild]: /chromium-os/developer-library/guides/portage/ebuild-faq/#TOC-How-do-I-uprev-an-ebuild-
[lsan]: https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer
[ptrace-manual]: https://man7.org/linux/man-pages/man2/ptrace.2.html
[Machine-found-bugs]: https://buganizer.corp.google.com/issues?q=componentid:1099326
