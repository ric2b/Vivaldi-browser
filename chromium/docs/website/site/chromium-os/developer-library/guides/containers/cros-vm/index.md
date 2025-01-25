---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: cros-vm
title: ChromeOS VM for Chromium developers
---

This workflow allows developers with a Chromium checkout using [Simple Chrome]
to download and launch a ChromeOS VM on their workstations, update the VM with
locally built Chrome, and run various tests.

[TOC]

## Prerequisites

1.  [depot_tools installed]
2.  [Linux Chromium checkout]
3.  [Virtualization enabled](#virtualization-check)
4.  [Simple Chrome] set up

### Virtualization check
To check if kvm is already enabled:
```bash
(shell) if [[ -e /dev/kvm ]] && grep '^flags' /proc/cpuinfo | grep -qE 'vmx|svm'; then
echo 'KVM is working'; else echo 'KVM not working'; fi
```

If KVM is not working it must be enabled, which usually requires ensuring that
your system firmware (BIOS) has virtualization features enabled and KVM is
enabled in your kernel.

**Googlers**: check the [Virtualization enabled] doc for instructions.

## Typography conventions

| Label         | Paths, files, and commands                             |
|---------------|--------------------------------------------------------|
|  (shell)      | on your build machine, outside the sdk/chroot          |
|  (sdk)        | inside the `chrome-sdk` [Simple Chrome] shell          |
|  (chroot)     | inside the `cros_sdk` [chroot]                         |
|  (vm)         | inside the VM ssh session                              |


## Pick which board to use

If you're just getting started, choose one of these:
*   **betty** - recommended for Googlers, enables ARC and has API keys to allow
    adding users. There are variants for each Android version, but just "betty"
    will work.
*   **amd64-generic-vm** - recommended for open source contributors

Some boards do not generate VM images. Also, using `cros vm` for non-x86 boards
is currently not supported.

The rest of these instructions assume you set an environment variable with your
board. If you always use the same board you can just type it into each command.
```bash
(shell) .../chrome/src $ export BOARD=betty
```

## Download the VM

cd to your Chromium repository:
```bash
(shell) $ cd chrome/src
```

Ensure your `../.gclient` file looks something like this:
```
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
      # Downloads the SDK and VM image:
      "cros_boards_with_qemu_images": "betty",  # or "amd64-generic-vm"
    },

  },
]
target_os=['chromeos']
```
Googlers may wish to add the following to "custom_vars":
```
      "checkout_src_internal": True,
```

Then run gclient sync to download the SDK and VM image:
```
(shell) .../chrome/src $ gclient sync
```
This will create an out_betty/Release directory, which you'll use later.

## Launch a ChromeOS VM

```bash
(shell) .../chrome/src $ cros vm --start --board=$BOARD
```
To avoid having to type your password everytime you launch a VM, add yourself
to the kvm group: `sudo usermod -a -G kvm $USER`

### Viewing the VM
To view the VM in a window, you can launch `vncviewer`:
```bash
(shell) vncviewer localhost:5900 &
```

To install `vncviewer`, which is a virtual package provided by `tigervnc-viewer`:
```bash
(shell) sudo apt-get install tigervnc-viewer
```

See the [ChromiumOS developer guide](/chromium-os/developer-library/guides/development/developer-guide/#vm) for additional
options.

### Stop the VM

```bash
(shell) .../chrome/src $ cros vm --stop
```

## Remotely run a smoke test in the VM

```bash
(shell) .../chrome/src $ cros vm --cmd -- /usr/local/autotest/bin/vm_sanity.py
```
The command output in the VM will be output to the console after the command
completes. Other commands run within an ssh session can also run with `--cmd`.

## SSH into the VM

```bash
(shell) .../chrome/src $ cros shell localhost:9222
```

### Run a local smoke test in the VM

```
(vm) localhost ~ # /usr/local/autotest/bin/vm_sanity.py
```

## Run Telemetry Tests

To run telemetry functional or performance tests:
```bash
(shell) .../chrome/src $ third_party/catapult/telemetry/bin/run_tests \
--browser=cros-chrome --remote=localhost --remote-ssh-port=9222 [test]
(shell) .../chrome/src $ tools/perf/run_tests \
--browser=cros-chrome --remote=localhost --remote-ssh-port=9222 [test]
```

Alternatively, to run these tests in local mode instead of remote mode, SSH into
the VM as above, then invoke `run_tests`:
```bash
(vm) localhost ~ # python \
/usr/local/telemetry/src/third_party/catapult/telemetry/bin/run_tests [test]
(vm) localhost ~ # python /usr/local/telemetry/src/tools/perf/run_tests [test]
```

## Update Chrome in the VM

### Build Chrome

For testing local Chrome changes on ChromeOS, use the [Simple Chrome] shell-less
flow to build Chrome. Ensure your .gclient file looks like the one at the start
of this page.

Syncing your source will now download the correct SDK toolchain for tip-of-trunk
chrome and create the out_$BOARD/Release directory for you.
```bash
(shell) .../chrome/src $ git rebase-update
(shell) .../chrome/src $ gclient sync -D  # deletes unused dependencies
(shell) .../chrome/src $ ls out_$BOARD/Release
```

You may wish to edit your out_$BOARD/Release/args.gn to something like:
```
import("//build/args/chromeos/betty.gni")  # or amd64-generic-vm.gni
# Place any additional args or overrides below:
dcheck_always_on = true
is_debug = false

# For Googlers:
is_chrome_branded = true
use_remoteexec = true
```

Then use gn to generate your ninja file and build:
```bash
(shell) .../chrome/src $ gn gen out_$BOARD/Release/
(shell) .../chrome/src $ autoninja -C out_$BOARD/Release/ \
chromiumos_preflight
```

### Launch the VM

```bash
(shell) .../chrome/src $ cros vm --start --board=$BOARD
```

### Deploy your Chrome to the VM

```bash
(shell) .../chrome/src $ ./third_party/chromite/bin/deploy_chrome \
--build-dir=out_$BOARD/Release/ \
--device=localhost:9222
```

## Run a Tast test in the VM

Tast tests are typically executed from within a ChromeOS [chroot]:
```bash
(chroot) $ tast run -build=false localhost:9222 login.Chrome
```

You can also run Tast tests directly in the VM:
```bash
(shell) .../chrome/src $ cros shell localhost:9222
(vm) localhost ~ # local_test_runner example.Pass
```

See the [Tast: Running Tests] document for more information.

## Run an ARC test in the VM

Download the betty VM:
```bash
(sdk) .../chrome/src $ cros chrome-sdk --board=betty --download-vm
```

Run an ARC test:
```bash
(vm) localhost ~ # local_test_runner arc.Boot
```

Run a different ARC test from within your [chroot]:
```bash
(chroot) $ tast run -build=false localhost:9222 arc.Downloads
```

## Run a Chrome GTest binary in the VM

The following will create a wrapper script at `out_$BOARD/Release/bin/` that
can be used to launch a VM, push the test dependencies, and run the GTest. See
the [chromeos-amd64-generic-rel] builder on Chromium's main waterfall for the
list of GTests currently running in VMs (eg: `base_unittests`,
`ozone_unittests`).
```bash
(sdk) .../chrome/src $ autoninja -C out_$BOARD/Release/ $TEST
(sdk) .../chrome/src $ ./out_$BOARD/Release/bin/run_$TEST --use-vm
```

## Run a [GPU test] in the VM

The following will run GPU tests that matches \<glob-file-pattern\>.
```bash
(sdk) .../chrome/src $ content/test/gpu/run_gpu_integration_test.py \
webgl_conformance --show-stdout --browser=cros-chrome --passthrough -v \
--extra-browser-args='--js-flags=--expose-gc --force_high_performance_gpu' \
--read-abbreviated-json-results-from=content/test/data/gpu/webgl_conformance_tests_output.json \
--remote=127.0.0.1 --remote-ssh-port=9222 --test-filter=<glob-file-pattern>
```

## Login and navigate to a webpage in the VM

```bash
(vm) localhost ~ # /usr/local/autotest/bin/autologin.py \
--url "http://www.google.com/chromebook"
```

## Launch a VM built by a waterfall bot

Select a [full] or [release] canary builder of interest. Note that not all
of these bots build an image compatible with QEMU, so you'll likely want some
flavor of [amd64-generic] or [betty]. Pick one of their builds, click on
artifacts, and download `chromiumos_test_image.tar.xz` to `~/Downloads/`

Unzip:
```bash
(shell) $ tar xvf ~/Downloads/chromiumos_test_image.tar.xz
```

Launch a VM:
```bash
(shell) .../chrome/src $ cros vm --start \
--image-path  ~/Downloads/chromiumos_test_image.bin
```

## Launch a locally built VM from within the chroot

Follow instructions to [build ChromiumOS] and a VM image. In the [chroot]:
```bash
(outside) $ export BOARD=betty   # or amd64-generic (not amd64-generic-vm)
(outside) $ cros build-packages --board=$BOARD
(outside) $ cros build-image --no-enable-rootfs-verification test --board=$BOARD
```

You can specify the image path, and if you leave it out, the latest built image
will be used:
```bash
(chroot) $ cros vm --start --board $BOARD --image-path \
../build/images/$BOARD/latest/chromiumos_test_image.bin
(chroot) $ cros vm --start --board $BOARD
```

You can also launch the VM from anywhere within your chromeos source tree:
```bash
(shell) .../chromeos $ chromite/bin/cros vm --start --board $BOARD
```

## `cros_run_test`

`cros_run_test` runs various tests in a VM. It can use an existing VM or
launch a new one.

### In Simple Chrome

Enter the [Simple Chrome] SDK environment. Note that if you're using the
recommended shell-less flow this will overwrite your args.gn (but with something
reasonable).
```bash
(shell) .../chrome/src $ cros chrome-sdk --board=$BOARD
```

To launch a VM and run a smoke test:
```bash
(sdk) .../chrome/src $ cros_run_test
```

To build chrome, deploy chrome, or both, prior to running tests:
```bash
(sdk) .../chrome/src $ cros_run_test --build --deploy --build-dir \
out_$BOARD/Release
```

To run a Tast test:
```bash
(sdk) .../chrome/src $ cros_run_test --tast login.Chrome
```

To build and run an arbitrary test (e.g. `base_unittests`):
```bash
(sdk) .../chrome/src $ cros_run_test --build --chrome-test -- \
out_$BOARD/Release/base_unittests
```

### In the chroot

These examples require a locally-built VM. See
[Launch a locally built VM from within the chroot].

To run an individual Tast test from within the chroot:
```bash
(chroot) $ cros_run_test --board $BOARD --tast login.Chrome
```

To run all Tast tests matched by an [attribute expression]:
```bash
(chroot) $ mkdir /tmp/results
(chroot) $ cros_run_test --board $BOARD --results-dir=/tmp/results \
--tast '("group:mainline" && !informational)'
```

See [go/tast-infra] (Googler-only) for more information about which Tast tests
are run by different builders.

This doc is at [go/cros-vm].

#### Tips

To lookup release versions for a particular board (e.g. betty):
```bash
(shell) .../chrome/src $ gsutil ls gs://chromeos-image-archive/betty-release/
```

[depot_tools installed]: https://www.chromium.org/developers/how-tos/install-depot-tools
[go/cros-qemu]: https://storage.cloud.google.com/achuith-cloud.google.com.a.appspot.com/qemu.tar.gz
[Linux Chromium checkout]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/linux/build_instructions.md
[Virtualization enabled]: https://g3doc.corp.google.com/tools/android/g3doc/development/crow/enable_kvm.md
[Simple Chrome]: /chromium-os/developer-library/guides/development/simple-chrome-workflow/
[chroot]: /chromium-os/developer-library/guides/development/developer-guide/
[Tast: Running Tests]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/running_tests.md
[full]: https://cros-goldeneye.corp.google.com/chromeos/legoland/builderSummary?builderGroups=full
[release]: https://cros-goldeneye.corp.google.com/chromeos/legoland/builderSummary?builderGroups=release
[amd64-generic]: https://cros-goldeneye.corp.google.com/chromeos/legoland/builderHistory?buildConfig=amd64-generic-full&buildBranch=master
[betty]: https://cros-goldeneye.corp.google.com/chromeos/legoland/builderHistory?buildConfig=betty-release&buildBranch=master
[build ChromiumOS]: /chromium-os/developer-library/guides/development/developer-guide/
[attribute expression]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/test_attributes.md
[Launch a locally built VM from within the chroot]: #Launch-a-locally-built-VM-from-within-the-chroot
[go/tast-infra]: https://chrome-internal.googlesource.com/chromeos/chromeos-admin/+/HEAD/doc/tast_integration.md
[go/cros-vm]: /chromium-os/developer-library/guides/containers/cros-vm/
[chromeos-amd64-generic-rel]: https://ci.chromium.org/p/chromium/builders/luci.chromium.ci/chromeos-amd64-generic-rel
[GPU test]: https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/gpu/gpu_testing.md#Running-the-GPU-Tests-Locally
