---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: platform2-primer
title: Getting started with platform2
---

[TOC]

## What is platform2?

`platform2` is an effort to make the build of platform packages faster and
simpler; it uses [GN] and ninja to build everything.

The common build system includes:
* protobuf and dbus adaptors generation
* incremental builds
* pkg-config support
* dependencies extraction (dependencies are extracted from your GN files and
  be used to generate pkg-config files)

## The platform2 philosophy

Given a package `${package_name}`:

1.  The code should live in `src/platform2/${package_name}`.
1.  The code should be built from a single GN file
    `src/platform2/${package_name}/BUILD.gn`.
1.  Each package should have their own ebuild. The logic to build `platform2`
    packages is contained in `platform.eclass`.

## How to write an ebuild for a platform2 package

Packages in `platform2` should have very lean ebuilds. Most of the build
configuration should happen behind the scene (`platform.eclass`) or in the
common GN logic. In most cases, your ebuild will only contain dependencies,
an install section, and a list of tests to run.

Note: as for all `cros_workon` packages, you only need to create the `-9999`
version of the ebuild (`${package_name}-9999.ebuild`). The CQ will
automatically create the stable version of the ebuild
(`${package_name}-x.y.z-r${revision_number}.ebuild`).

### Example ebuild

``` sh
# Copyright <copyright_year> The ChromiumOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI="7"

CROS_WORKON_INCREMENTAL_BUILD=1
CROS_WORKON_LOCALNAME="platform2"
CROS_WORKON_PROJECT="chromiumos/platform2"
CROS_WORKON_DESTDIR="${S}/platform2"
CROS_WORKON_SUBTREE="common-mk package .gn"

PLATFORM_SUBDIR="package" # name of the directory in src/platform2

inherit cros-workon platform

DESCRIPTION="description"
HOMEPAGE="https://www.chromium.org/"

LICENSE="BSD-Google"
SLOT="0/0"
KEYWORDS="~*"

RDEPEND="
"

DEPEND="
  ${RDEPEND}
"

# NB: src_install should be omitted and all install handled by GN.
# See the install section of https://www.chromium.org/chromium-os/developer-library/reference/build/chromeos-gn/#how-to-install-files

# NB: src_test should be omitted and all tests handled by GN.
# See the test section of https://www.chromium.org/chromium-os/developer-library/guides/testing/running-unit-tests/#for-platform2-packages
```

## Adding generators

`platform2` already has pre-made generators for `dbus-xml2cpp` and `protoc`,
but if you need to add a new generator, here's how.

1.  Create a new `.gni` file under `src/platform2/common-mk/` named
    appropriately to describe your generator.
1.  Copy the contents of an existing one, such as `dbus_glib.gni`
1.  Modify the rule specified to call your tool with the arguments
1.  Update the inputs/outputs lists to reflect the files your tool will
    generate. (Ensure your tool is in `virtual/target-sdk` if necessary.)

You can now include this generator in a target for a project you're porting
to `platform2`, by writing a target in your project that uses the template
you just wrote.

## Adding a new USE flag conditional

It is possible to have conditional GN rules that depend on the value
of a `USE` flag at the time of build. This is useful, for example, if
certain targets must only be built if a certain feature (controlled by a
`USE` flag) is enabled.

See the instructions in [GN in ChromeOS](/chromium-os/developer-library/reference/build/chromeos-gn/#how-to-check-use-flags-in-gn).

## Rapid development

`cros_workon_make` will use the ebuild to build a package, but
will build incrementally and skip as many steps as possible from the normal
emerge process (not checking dependencies, not installing, etc.). However,
please use this command with caution because **it allows ebuilds to directly
modify the source repository.** For example, it may delete some files.

For extra speed, you can also use `--noreconf` to skip the configure stage
(creating the ninja file from the GN files):

```
cros_workon_make --board=${BOARD} --noreconf shill
```

You can inspect the produced artifacts and test binaries in
`/build/${BOARD}/var/cache/portage/chromeos-base/shill`.

## Running unit tests

See [ChromiumOS Unit Testing].

## Further reading

* [GN in ChromeOS] further describes using GN
* [GN] on Google Git
* (internal) Introduction to GN tech talk [GN video]/[GN slides]

[ChromiumOS Unit Testing]: /chromium-os/developer-library/guides/testing/running-unit-tests/
[GN in ChromeOS]: /chromium-os/developer-library/reference/build/chromeos-gn/
[GN]: https://gn.googlesource.com/gn/
[GN video]: https://goto.google.com/gn-intro-tech-talk
[GN slides]: https://goto.google.com/gn-intro-slides
