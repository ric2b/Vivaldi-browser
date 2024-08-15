---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: cros-tools
title: CrOS Tools
---

This aims to be an reference page to help find CrOS tools and their documentation.

## Tools

### Alphabetical List

#### build_image

`build_image` is the old name for [`cros build-image`](#cros-build-image).
Users should invoke that tool instead.

#### build_packages

`build_packages` is the old name for
[`cros build-packages`](#cros-build-packages).  Users should invoke that tool
instead.

#### cros build-image

`cros build-image` handles building ChromiumOS and ChromeOS images that can then
be flashed to a device or USB stick.

#### cros build-packages

`cros build-packages` compiles all of the packages for a given board into a
[sysroot][build-glossary].  Packages can also be listed that should be built.

#### cros deploy

`cros deploy` is used to install a single package to a [DUT][glossary-acronyms] (Device Under Test).
See the [`cros deploy` documentation](/chromium-os/developer-library/reference/tools/cros-deploy/) for more information.

#### cros flash

`cros flash` is used to flash an image to a [DUT][glossary-acronyms] or a USB drive.
See the [tool's documentation](/chromium-os/developer-library/reference/tools/cros-flash/) for more information.

#### cros_run_unit_tests

This script runs the src_test phase of the ebuilds for the packages it tests.
It allows giving a [sysroot][build-glossary], build target (board), or a list of packages.

#### cros_sdk

The cros_sdk script lives in the [chromite] repo.
The implementation can be found [here](cros_sdk_src), while its executable can be found in [Chromite's bin directory](chromite_bin).
cros_sdk is used to manage the SDK/chroot.
This is one of the first scripts that needs to be run in most cases, to create the chroot.

Some of the most common usages are:
* Create SDK:
    * `cros_sdk --create`
* Delete SDK:
    * `cros_sdk --delete`
* Replace SDK:
    * `cros_sdk --replace`


#### cros_workon_make

Location: [platform/dev-util][cros-workon-make-src]

This script is a bash script that wraps the core operations for working on a specific workon package.

* Build Package:
    * `cros_workon_make --board=<board> <package>`
* Test Package:
    * `cros_workon_make --test --board=<board> <package>`
* Install Package:
    * `cros_workon_make --install --board=<board> <package>`

#### security_test_image

Repo: [chromite][chromite_bin]

This script uses a set of baselines to run security tests for a built image.
For most use cases this currently generally requires an internal checkout.
The tests it runs are also currently very strongly tied to the concept of the image being for a Chromebook.
Use cases of ChromeOS/ChromiumOS for other uses usually fail these tests.

### By Entity/Action

#### AP Firmware

* Build: TODO
* Test: TODO
* Install: TODO

#### [Chroot: See SDK](#sdk)

#### EC Firmware

* Build: TODO
* Test: TODO
* Install: TODO

#### Image

* [Build](#cros-build-image)
* [Test](#security_test_image)
* [Install](#cros-flash)

#### Kernel

* Build: TODO
* Test: TODO
* Install: TODO

#### Package

Note: This is distinct from the [Packages](#Packages) section.
This section focuses on operations that are relevant to a single package.
The [Packages](#Packages) section focuses on operations relevant to sets of packages (most often a full sysroot).
There is overlap in these sections, and we hope one day to be able to collapse them into a single section.

* Build
    * [`cros build-packages`](#cros-build-packages)
    * [cros_workon_make](#cros_workon_make)
* Test
    * [cros_run_unit_tests](#cros_run_unit_tests)
    * [cros_workon_make](#cros_workon_make)
* Install
    * [cros deploy](#cros-deploy)
    * [cros_workon_make](#cros_workon_make)

#### Packages

* [Build](#cros-build-packages)
* [Test](#cros_run_unit_tests)
* Install: N/A
    * The installation of full sets of packages is currently done by creating and flashing an image.

#### SDK

* [Build](#cros_sdk)
* Test: None currently exist
* Install: N/A

#### Sysroot
**Handled automatically for most users!**

**Only use these if you're sure you really need to manually do so.**

The current test/install phases of the sysroot are very limited and not distinct from the build phase.

 * Build/Test/Install: setup_board


[chromite_bin]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/bin/
[cros_sdk_src]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/scripts/cros_sdk.py
[crosutils]: https://chromium.googlesource.com/chromiumos/platform/crosutils/
[cros-workon-make-src]: https://chromium.googlesource.com/chromiumos/platform/dev-util/+/HEAD/host/cros_workon_make
[build-glossary]: /chromium-os/developer-library/glossary/#cros-build
[glossary-acronyms]: /chromium-os/developer-library/glossary/#acronyms
