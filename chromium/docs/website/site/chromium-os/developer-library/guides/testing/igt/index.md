---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: igt
title: Running IGT tests on ChromeOS
---

[IGT GPU Tools](https://github.com/freedesktop/xorg-intel-gpu-tools) is a collection
of tools for development and testing of the DRM drivers that includes low-level
tools and tests specifically for development and testing of the DRM Drivers.

In ChromeOS, we support both [KMS tests](https://chromium.googlesource.com/chromiumos/platform/tast-tests/+/HEAD/src/chromiumos/tast/local/bundles/cros/graphics/igt_kms.go)
and [Chamelium tests](https://chromium.googlesource.com/chromiumos/platform/tast-tests/+/HEAD/src/chromiumos/tast/local/bundles/cros/graphics/igt_chamelium.go)

IGT can run in 3 different ways. They all accomplish the same thing (running the
tests) but each can be used for different context (production vs fast development).

This document assumes you have a Chromebook that you can ssh to, and are able to
`emerge` and `deploy` packages to your DUT. If not, please start with this [Developer
Guide](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/developer_guide.md).
For Chamelium tests, it also assumes that you have a working reachable Chamelium device.

The document uses the ChromeOS [typography convention](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/developer_guide.md#typography-conventions).

[TOC]

## Important References
Source code and everything in between to run, develop and test IGT.

### ChromiumOS Internal
- [Local Source](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/igt-gpu-tools/)
- [ChromiumOS Git](https://chromium.googlesource.com/chromiumos/third_party/igt-gpu-tools/+/refs/heads/master)

### Google lab links
- [Everything IGT](http://goto.google.com/igtlab)
- [IGT KMS](http://goto.google.com/igtlab-kms)
- [IGT Chamelium](http://goto.google.com/igtlab-chamelium)
- [IGT for Intel Devices](http://goto.google.com/igtlab-intel)
- [IGT for AMD Devices](http://goto.google.com/igtlab-amd)
- [IGT for Qcom Devices](http://goto.google.com/igtlab-qcom)
- [IGT for MTK Devices](http://goto.google.com/igtlab-mtk)


### Upstream
- [Source Code](https://gitlab.freedesktop.org/drm/igt-gpu-tools)
- [API References](https://drm.pages.freedesktop.org/igt-gpu-tools/)
- [Mailing List](https://patchwork.freedesktop.org/project/igt/series/?ordering=-last_updated)
---------------------------------------

## Tast Test
IGT can run as a [tast test](https://source.chromium.org/chromiumos/chromiumos/platform/tast/) on
Chromebooks. The advantage of this is that you run everything from your development machine and get
a test result at the end without fiddling with the DUT. This is the method used in the testing labs
so your tast result would match the lab result.

### How to run an IGT Tast
0. **Get the Test Suite name**
_This is the name of your test func in your Go file. Look for `Func: SUITE_NAME`. e.g._
- `graphics.IgtKms*` runs all IGT tests
- `graphics.DisplayHwValidation*` runs the HW requirements
- `graphics.DisplayQualityValidation*` runs the basic SW requirements
- `graphics.IgtAmdgpu*` runs all AMD specific tests
- `graphics.IgtChamelium*` runs all IGT Chamelium Tests

1. **Get the test name** e.g.
- `graphics.<SUITE_NAME>*` runs all IGT tests
- `graphics.Igt*` runs all IGT tests
- `graphics.Display*` runs all Display Validation tests
- `graphics.IgtKms.plame_*` runs all KMS tests starting with plane_
- `graphics.IgtKms.kms_plane`/`graphics.IgtChamelium.kms_chamelium_hpd` runs a specific test

2. **To run IGT KMS**
- `(outside)$ cros_sdk tast run <DUT IP> <TEST NAME>`
- e.g. `(outside)$ cros_sdk tast run dutontoilet graphics.IgtKms.kms_plane_*`

3. **To run IGT Chamelium**
- `(outside)$ cros_sdk tast run -var=graphics.chameleon_ip=<CHAMELEON IP> <DUT IP> <TEST NAME>`
- e.g. `(outside)$ cros_sdk tast run -var=graphics.chameleon_ip=10.128.17.143 localhost:33465  graphics.IgtChamelium.kms_chamelium_hpd_*`
- *Chamelium IGT tests are divided by subtest in Tast*

*drop the `cros_sdk` if you're already `(inside)` chroot*

### There are only 3 possible results:
1.  **SKIP**  *(if the tast is running on an unsupported configuration)*
2.  **FAIL**
3.  **PASS**  *(which includes skips from within the test if a condition is not met, typically from an `igt-require()` call)*

### References:

 - [KMS IGT Tast source code](https://chromium.googlesource.com/chromiumos/platform/tast-tests/+/HEAD/src/chromiumos/tast/local/bundles/cros/graphics/igt_kms.go)
  - [Chamelium IGT Tast source code](https://chromium.googlesource.com/chromiumos/platform/tast-tests/+/HEAD/src/chromiumos/tast/local/bundles/cros/graphics/igt_chamelium.go)
 - [IGT Tast dependencies](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/src/chromiumos/tast/internal/crosbundle/software_defs.go#109) (filters out what runs in the lab)
 - [List of all supported dependencies](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/test_dependencies.md)

## Individual tests

You can build the tests and deploy them (all together or individually) and run them
locally on your DUT.
This is used more for development as you get to see the tests running in action and see all
the logs (verbose if needed). Also useful to run a test on an unsupported device that's not part of
tast dependencies such as `kms_hdr` on `kohaku`.

You can deploy individual tests or altogether (explained below), but building them
goes through the same process.

### Build/Compile IGT tests
`(outside)$ cros_sdk cros-workon-${BOARD} start igt-gpu-tools`*(you only need this one time)*

`(outside)$ cros_sdk emerge-${BOARD} igt-gpu-tools`

### Deploy all tests to your DUT
`(outside)$ cros_sdk cros deploy <DUT IP> igt-gpu-tools --root /usr/local`

*You MUST add `--root /usr/local` to end up in the right location.*

### Deploy an individual test to your DUT
Find the output file and scp it. it would look like this:

`(outside)$ scp /sda/cros/chroot/build/${BOARD}/usr/libexec/igt-gpu-tools/<test name> <DUT IP>:///usr/local/libexec/igt-gpu-tools`

*`<test name>` is the IGT test name such as `kms_vrr`, `kms_chamelium_frame`,...*

### Run a single test on your DUT
`(outside)$ ssh <DUT IP>`

`(device)$ cd /usr/local/libexec/igt-gpu-tools`

`(device)$ stop ui`

`(device)$ ./<test name>` *(i.e. `./kms_plane`)*

### Configure the device to run a chamelium test
For a chamelium test, all the steps above are valid, but there is a one-time thing (per DUT image) that needs to be
done to tell your DUT how to find its associated chamelium.
*The full documentation is found [here](https://gitlab.freedesktop.org/drm/igt-gpu-tools/-/blob/15562e123ee6ed88163c7da2ef330dfe9bbd16a5/docs/chamelium.txt#L78)*

0. ssh to your DUT (you would be accessing through root@)
1. Remove rootfs verification if you haven't already `(device)$ sudo /usr/share/vboot/bin/make_dev_ssd.sh --remove_rootfs_verification -f`
2. Create a new file on your DUT `~/.igtc`
3. Add the following in `.igtc` (Modify it as needed: especially the `URL`(leave the port number as `9992`) and `ChameliumPortID`)
```
[Common]
# The path to dump frames that fail comparison checks
FrameDumpPath=/tmp

[DUT]
SuspendResumeDelay=15

# IP address of the Chamelium
# The ONLY mandatory field (the rest of the file can be blank yet IGT will be usable)
# Replace the URL below by yours
[Chamelium]
URL=http://192.168.0.36:9992

# The ID of the chamelium port <> DUT port mapping
# DUT port can be found using `modetest -c`
# Replace the IDs below by yours
# It's an optional field. When no set, Chamelium will perform autodiscovery.
[Chamelium:DP-4]
ChameliumPortID=0

```
**The above steps can be generated automatically to you using [generate_igtrc.sh](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/dev/contrib/chameleon/generate_igtrc.sh)**

---------------------------------------
 # Running Chameleon Tast tests (NOT IGT)
 *This section is not directly related to IGT, but it's useful for testing Chameleon using Chrome*

 ## Tast vs IGT
 - IGT runs straight on the kernel, it takes over the user space, so it's great to test the driver.
 - Tast runs on top of Chrome, so it's great to test the whole stack, or Chrome specific functionalities.
 - All the Chameleon Tast tests are found with the [graphics tests](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/graphics/) and start with `chameleon_`.
 ## Running a Chameleon Tast test
- `(outside)$ cros_sdk tast run -var=graphics.chameleon_ip=<CHAMELEON IP> -checktestdeps=false <DUT IP> <TEST NAME>`
- e.g. `(outside)$ cros_sdk tast run -var=graphics.chameleon_ip=10.128.17.143 -checktestdeps=false localhost:46169 graphics.ChameleonSmoke`
---------------------------------------

## Troubleshooting
1. ### My changes aren't being reflected when I run the test
	a. The build could be not picking up your changes.
    Make sure you do `(outside)$ cros_sdk cros-workon-${BOARD} start igt-gpu-tools`

	b. You could be deploying to the wrong place.
    Make sure you append `--root /usr/local` to `(outside)$ cros_sdk cros deploy <DUT IP> igt-gpu-tools`

2. ### Deploying to DUT is failing: Missing deps
	a. You could be missing some dependencies. Try doing:
```
(outside)$ cros_sdk emerge-${BOARD} sys-libs/llvm-libunwind  && cros_sdk cros deploy <DUT IP> sys-libs/llvm-libunwind
(outside)$ cros_sdk emerge-${BOARD} x11-libs/libpciaccess  && cros_sdk cros deploy <DUT IP> x11-libs/libpciaccess
(outside)$ cros_sdk emerge-${BOARD} dev-libs/xmlrpc-c  && cros_sdk cros deploy <DUT IP> dev-libs/xmlrpc-c
(outside)$ cros_sdk emerge-${BOARD} sci-libs/gsl  && cros_sdk cros deploy <DUT IP> sci-libs/gsl
```

	b. If you get an error that looks like

```
WARNING: One or more repositories have missing repo_name entries:
        /usr/local/tmp/cros-deploy/tmp.pQJ4RK7iu1/profiles/repo_name
NOTE: Each repo_name entry should be a plain text file containing a
unique name for the repository on the first line.
!!! CONFIG_PROTECT is empty for '/usr/local/'
!!! Problem with sandbox binary. Disabling...
!!! Problem with sandbox binary. Disabling...
!!! Problem resolving dependencies for /usr/local/tmp/cros-deploy/tmp.pQJ4RK7iu1/packages/x11-apps/igt-gpu-tools-99.1.25.tbz2

emerge: there are no binary packages to satisfy "dev-libs/json-c" for /usr/local/.

(dependency required by "x11-apps/igt-gpu-tools-99.1.25::chromeos-intel-internal-override" [binary])
(dependency required by "/usr/local/tmp/cros-deploy/tmp.pQJ4RK7iu1/packages/x11-apps/igt-gpu-tools-99.1.25.tbz2" [argument])
Calculating dependencies
*** emerging by path is broken and may not always work!!!
... done!
cmd=['ssh', '-oConnectTimeout=30', '-oConnectionAttempts=4', '-oNumberOfPasswordPrompts=0', '-oProtocol=2', '-oServerAliveInterval=10', '-oServerAliveCountMax=3', '-o
StrictHostKeyChecking=no', '-oUserKnownHostsFile=/dev/null', '-oIdentitiesOnly=yes', '-i', '/tmp/ssh-tmp5vt8a_td/testing_rsa', 'root@10.23.15.12', '--', 'FEATURES=-sa
ndbox', 'PKGDIR=/usr/local/tmp/cros-deploy/tmp.pQJ4RK7iu1/packages', 'PORTAGE_CONFIGROOT=/usr/local', 'PORTAGE_TMPDIR=/usr/local/tmp/cros-deploy/tmp.pQJ4RK7iu1/portag
e-tmp', 'PORTDIR=/usr/local/tmp/cros-deploy/tmp.pQJ4RK7iu1', "CONFIG_PROTECT='-*'", 'PATH=/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin', 'emerge', '--
usepkg', '--ignore-built-slot-operator-deps=y', '--root', '/usr/local', '/usr/local/tmp/cros-deploy/tmp.pQJ4RK7iu1/packages/x11-apps/igt-gpu-tools-99.1.25.tbz2'], ext
ra env={'LC_MESSAGES': 'C'}
```

You will see above a line explaining the missing dependency:
**`emerge: there are no binary packages to satisfy "dev-libs/json-c"`**.

You need to emerge and deploy the missing package to your DUT (only need to do it one time). i.e.
```
(outside)$ cros_sdk emerge-${BOARD} dev-libs/json-c && cros_sdk cros deploy <DUT IP> dev-libs/json-c
```

3. ### Running Tasts on DUT is failing: DUT software features
If your DUT is missing software features and your error message looks like:
```
2021-05-07T17:43:11.013594Z Building local_test_runner, cros, remote_test_runner, cros
2021-05-07T17:43:11.821523Z Built in 808ms
2021-05-07T17:43:11.821585Z Pushing executables to target
2021-05-07T17:43:12.045178Z Pushed executables in 224ms (sent 0 B)
2021-05-07T17:43:12.870143Z Failed to run tests: failed to get DUT software features: can't check test deps; no software features reported by DUT
```
For debugging, it is legitimate to override tast with this option (`-checktestdeps=false`) as your stateful partition might be in a bad state. To fix, do
```
(outside)$ cros_sdk tast run -checktestdeps=false <DUT_IP> graphics.IGT.kms_*
```

4. ### [Google-only] Building to Skylab DUT is failing: context deadline exceeded
if you can't connect to your *Skylab* DUT and your error looks like
```
2021-05-07T17:39:07.908035Z Connecting to skylab.cros
2021-05-07T17:39:17.908447Z Failed to connect to skylab.cros: context deadline exceeded
```
Make sure you have the right [ProxyCommand](https://g3doc.corp.google.com/company/teams/cloudtop/faq.md?cl=head#ssh) in place and this [workaround](https://groups.google.com/a/google.com/g/chromeos-chatty-eng/c/opC-HyMTe8E/m/UkIi7diBCAAJ) to enable skylab from chroot.

5. ### If the test isn't starting on the DUT: `Can't become DRM master`
if you see this error on your DUT
```
Test requirement: __igt_device_set_master(fd) == 0
Can't become DRM master, please check if no other DRM client is running.
```
Make sure the UI is stopped on your DUT by doing `(device)$ stop ui`

6. ### If the test isn't starting on the DUT: `version not found`
if your error looks like
```
./kms_dp_aux_dev: /lib64/libc.so.6: version `GLIBC_2.30' not found (required by /usr/local/lib64/libigt.so.0)
```
Likely due to an uprev of the libraries, resulting in a mismatch between ChromeOS and userspace.
Getting the most recent ChromeOS image on your DUT should fix the problem.

---------------------------------------
