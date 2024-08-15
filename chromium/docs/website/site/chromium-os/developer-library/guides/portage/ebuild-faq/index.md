---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: ebuild-faq
title: Ebuild FAQ
---

This document aims to answer common questions about building packages used in
developing ChromiumOS.

The CrOS SDK chroot environment uses the [Portage] package management system.
Portage, from the Gentoo Linux distribution, consists of two main components:
the tree of ebuild overlays and `emerge`. The ebuild system is a tree of bash
scripts (ebuilds) that contain build instructions responsible for building and
installing packages. Emerge is the user interface to interacting with ebuilds.
For questions about overlays, see the [Overlay FAQ].

Portage via the CrOS SDK chroot enables the creation of different
cross-compilation environments which are used to build system images for
Chromebooks.

Good Gentoo Documentation:

* [Working with Gentoo]
* [Gentoo Development Guide]


[TOC]

## How does the cross compiling setup work?

The chroot is used for cross-compiling. Portage, the chroot’s package
management system, is designed to compile packages from source and install the
resulting binaries into a root directory. Unlike Debian, where build rules are
found only in source code, Portage keeps build instructions in dedicated files
called ebuilds.

In our chroot, Portage installs packages into multiple directories:

* `/` for the SDK/chroot itself
* `/build/<board_name>` (aka sysroots) for a target board

To cross-compile, the SDK requires special tools which are installed via the
`virtual/target-chromium-os-sdk` package. When additional tools are needed,
this package can be upgraded and re-merged (see
[Adding a Package to the SDK](https://www.chromium.org/chromium-os/build/add-sdk-package)).
The `build_packages` script ensures
that re-merging will add required tools automatically. Notice that since we are
updating the chroot/host, we use the standard `emerge` command that installs
packages at the root and uses the host's configuration:

```bash
sudo emerge -a virtual/target-chromium-os-sdk
```

Each target board requires a distinct sysroot. First, we ensure the host has
the necessary toolchain installed and then we set up sysroot wrappers for the
toolchain binaries that set the sysroot location during compilation. We use the
toolchain headers and libraries to bootstrap the board target sysroot in
`/build/<board_name>`. When we set up a sysroot for a board, we select an
overlay corresponding to that board and link it in to the sysroot. This allows
us to customize or add packages & configuration on a per-board basis. The board
overlay can also include a `make.conf` that overrides some default settings
such as compiler `CFLAGS`.

When cross-compiling, you also need packages that correspond to the target,
such as header files and libraries. All build-time dependencies for a target
are installed in to that target's sysroot. These are only used for compilation
and will not be placed into images that are built via `build_image`.

Once a board is set up, we can start building packages for the board target. We
use the board target specific version of emerge to do this.

## How does the image build work?

Building a full bootable image is a multi-step process. For a given target, the
first step is to build binary packages for all packages needed by ChromiumOS
using `build_packages`. If you look into the details of `build_packages`, you
will see that it works by installing the `virtual/target-os` package into a
target. For example, to prepare to build an x86-generic image:

```bash
emerge-amd64-generic -a virtual/target-os
```

We've set a portage option to build binary packages as a side-effect of building
anything from source. If you build all of the packages needed by the
`virtual/target-os` ebuild then we will have a binary package for each of these.

Once `build_packages` finishes, we'll run build_image. If we look into the
details of `build_image`, you will see that it works by setting up a
loopback-mounted ext3 file system, prepping it for booting, and then installing
`virtual/target-os` into it with an option to force using binary packages only.
That way we only install the packages and their run-time dependencies and shed
all of the build dependencies:

```bash
emerge-<board_name> --root="$ROOT_FS_DIR" --root-deps=rdeps --usepkgonly
virtual/target-os
```

We use the `--root-deps=rdeps` option to only install run-time dependencies into
the root file system. This discards the build-only dependencies and gives us a
clean file system.

*** note
**SIDE NOTE**: It turns out that for most developers, the above is a bit of a
lie. Most developers don't actually build all binary packages from source when
they do a build. By default, the build system "cheats" and downloads a binary
package from the `BINHOST` if it can find one. See
[What does build_packages actually do](#what-does-build-packages-do).
***

## Where do the important files live?

Here are a few important directories:

Type                                                  | Directory
----------------------------------------------------- | ---------
main set of ebuilds                                   | `src/third_party/portage-stable/`
ChromiumOS portage overlay                           | `src/third_party/chromiumos-overlay/`
ChromiumOS ebuilds                                   | `src/third_party/chromiumos-overlay/chromeos-base/`
target profile (per-package unmask, `USE` flags, etc) | `src/third_party/chromiumos-overlay/profiles/targets/chromeos/`
host and per-target configs                           | `src/third_party/chromiumos-overlay/chromeos/config/`
crossdev autoconf configs (in chroot)                 | `/usr/share/crossdev/include/site/`
board sysroot (in chroot)                             | `/build/${BOARD}`

## What are `USE` flags and why do I care?

One of the interesting aspects of the Portage build system is parameterized
dependencies and build options. Each package can declare a list of `USE` flags
that can be turned on and off. Turning them on and off can affect the set of
packages that they depend on. For example, if a package supports creating
optional perl bindings, then it can declare a perl `USE` flag. If `-perl` is
specified then the package can omit creation of the bindings and drop its
dependency on perl.

## What are DEPEND, BDEPEND, and RDEPEND?

> **Note**: `BDEPEND` was introduced in `EAPI=7`, so ebuilds using older
> versions must be updated before it is available.

> **Note**: All `BDEPEND` packages must be explicitly enumerated in the
> `virtual/target-sdk` package.  Using `BDEPEND` alone is not sufficient.  See
> [How does the cross compiling setup work?](#how-does-the-cross-compiling-setup-work)
> for more info.

The different dependencies fall into two categories; build-time and run-time.
`DEPEND` and `BDEPEND` are build-time dependencies.
If I have a build-time dependency on a package, it means that the package
needs to be installed before I can build.
`RDEPEND` is a run-time dependency.
If I have a run-time dependency on a package, it means I need that package in
order to run, but do not need it to build.

The `DEPEND` and `BDEPEND` build-time dependencies are most distinct when
cross-compiling (we are always cross-compiling when running a ChromiumOS
device build).
We use `CBUILD` and `CHOST` to differentiate the targets.

*   `CBUILD` packages are executed at build-time, and are not installed to the
    system being built.
*   `CHOST` packages are installed to the system being built, and cannot be
    executed.

There are implications for what is and isn't installed when building from
binpkgs (a.k.a. prebuilts).
Build-time dependencies ARE NOT installed for a package that uses a prebuilt.
Run-time dependencies ARE installed for a package that uses a prebuilt.

These dependency types are also what is used to determine what actually makes it
on an image.
Build-time dependencies ARE NOT included in an image.
Run-time dependencies ARE included in an image.

To put it all together:

*   `BDEPEND`
    *   A `CBUILD` build-time dependency.
    *   `BDEPEND="foo/bar"`: I need to run `foo/bar` to be able to build, and
        any artifacts I need from `foo/bar` will be in my binpkg, so I won't
        need it if I'm installed from a binpkg.
    *   If I need to run a Python script to configure myself before I build,
        but I otherwise do not use Python, then I `BDEPEND` on Python so that I
        can run my configuration script.
    *   `BDEPEND` is new in `EAPI=7`
*   `DEPEND`
    *   A `CHOST` build-time dependency.
    *   `DEPEND="foo/bar"`: I need `foo/bar` to be installed before I can build,
        and any artifacts I need from `foo/bar` will be in my binpkg, so I won't
        need it if I am installed from a binpkg.
    *   A good example of this is when we need a header file or static library
        that is installed by another package.
*   `RDEPEND`
    *   A `CHOST` run-time dependency.
    *   `RDEPEND="foo/bar"`: I need `foo/bar` to be able to run, but not to
        build, and I do need it to be installed when I'm installed from a
        binpkg.
    *   We need the dependent package to be present at run-time, like a shared
        library, or another program that we call.

As a more concrete example, if you're building an ARM board:

* `CHOST` = ARM (the device)
* `CBUILD` = amd64 (the SDK)

And from the perspective of a package that is only run on devices:

*   `DEPEND` = I need this package to be built for the ARM device, so I can use
    it to build myself for the ARM device.
*   `BDEPEND` = I need this package to be built for the SDK, so I can run it to
    build myself for the ARM device.
*   `RDEPEND` = I need to run this package on the ARM device, but can build
    myself without it.

Packages CAN be in any/all `[BR]DEPEND` at the same time.
It is especially common that we have both a `DEPEND` and an `RDEPEND` on a
package.
A good example of this is when we depend on a `.so` file.
We need the shared library's header file at build time and its `.so` file at
runtime.

### Examples

The following examples use `A --DEPEND-> B` to mean A `DEPEND`s on B (i.e.
package B is in package A's `DEPEND` field in its ebuild), and
`A --DEPEND--> B --DEPEND--> C` to mean A `DEPEND`s on B and B `DEPEND`s on C.

#### Example 1: Which RDEPENDs end up in an image?

* A --DEPEND-> B --RDEPEND-> C
* A --RDEPEND-> D --RDEPEND-> E --BDEPEND-> F -> RDEPEND G

`build_packages A && build_image`:

| Location | Packages |
| --- | --- |
| SDK | F G |
| Sysroot | A B C D E |
| Image | A D E |

C does NOT make it to the **Image** because it's an `RDEPEND` of B, which is
only a `DEPEND` of A, i.e. the `RDEPEND` graph of A does not include B, so
cannot include C.

D DOES make it to the **Image** because it's a direct `RDEPEND` of A, so would
naturally be in the `RDEPEND` graph of A.

E DOES make it to the **Image** because it's an `RDEPEND` of D, which is an
`RDEPEND` of A, i.e. the `RDEPEND` graph of A DOES transitively include E.

G does NOT make it to the **Image** because it's an `RDEPEND` of F, which is
only a `BDEPEND` of E, i.e. the `RDEPEND` graph of A does not include F, so
cannot include G.

G does NOT make it to the **Sysroot** because it's an `RDEPEND` of F, which is
a `BDEPEND` of E, so since F is only ever needed in the **SDK**, so is G.

#### Example 2: When is run-time?

* B --RDEPEND-> C
* A --BDEPEND-> B
* A --RDEPEND-> D --DEPEND-> B
* A --RDEPEND-> C

`build_packages A && build_image`:

| Location | Packages |
| --- | --- |
| SDK (amd64) | B C |
| Sysroot (arm) | A B C D |
| Image (arm) | A C D |

B is installed to the **SDK** because A `BDEPENDS` on B, so B must be compiled
for and installed to the **SDK**. Since B `RDEPEND`s on C, C must also be built
for and installed to the **SDK** so that it's available to amd64 B when it's run
as a part of A's build steps.

B is installed to the **Sysroot** because D `DEPEND`s on B. Since B `RDEPEND`s
on C, C must also be built for and installed to the **Sysroot** so it's
available to ARM B when it's included as a part of D's build steps.

C is installed to the **Image** because A `RDEPEND`s on C, so C must be on the
**Image** so that it's available to A when A actually runs on the device.

Since B was only ever in `DEPEND`s, it's no longer needed and does not get
installed in the **Image**.

## How do I see and trim dependencies?

The easiest way to inspect dependencies is to use the `--emptytree` and
`--pretend` options for `emerge`. These options tell you what packages would be
installed if you were installing from scratch and you didn't have any of its
dependencies yet. The `--verbose` option will also show the set of `USE` flags
that can be turned on and off for the packages to hopefully trim dependencies:

```bash
emerge-amd64-generic --pretend --emptytree --verbose vim

These are the packages that would be merged, in order:

Calculating dependencies... done!
[ebuild   R   ] sys-apps/sed-4.2 to /build/x86-mario/ USE="-acl -nls -static" 862 kB
[ebuild   R   ] sys-libs/ncurses-5.7-r3 to /build/x86-mario/ USE="cxx minimal unicode -ada -debug -doc -gpm -profile -trace" 2,388 kB
[ebuild   R   ] app-editors/vim-core-7.2.303 to /build/x86-mario/ USE="-acl -bash-completion -livecd -nls" 9,475 kBS
```

To get an idea of the dependency tree and why some packages would be built do
`parallel_emerge --pretend --emptytree --tree` like so:

```bash
parallel_emerge --pretend --emptytree --tree --board=x86-generic vim

app-editors/vim-7.2.303: (merge) needs
    app-editors/vim-core-7.2.303
    sys-apps/sed-4.2
    sys-libs/ncurses-5.7-r3
app-editors/vim-core-7.2.303: (merge) needs
    sys-apps/sed-4.2
    sys-libs/ncurses-5.7-r3
app-vim/gentoo-syntax-20090720: (merge) needs
    app-editors/vim-7.2.303
sys-apps/sed-4.2: (merge) needs
    no dependencies
sys-libs/ncurses-5.7-r3: (merge) needs
    no dependencies
```

## How do I search for a package that I might want to install?

If you wanted to look for busybox, you can:

```bash
emerge --search busybox
```

## How do I see a list of all packages installed in a root?

```bash
equery-<board_name> list '*'
```

## How do I find out the on-disk package size?

For this, we are primarily interested in getting the size of the package and its
runtime dependencies.

```bash
# ignores files in package that match <file mask regex>.
qsize -i<file mask regex> package
```

Example:

```bash
# Calculate size of package excluding debug symbols and development header files.
qsize -i '/usr/lib/debug/' -i '/usr/include/' shill
```

```bash
# For specific boards, first build the package for the board and then measure it.
# (does the same thing as `ROOT=/board/<board> qsize <rest of command>`)
qsize-<board> -i '/usr/lib/debug/' -i '/usr/include/' shill
```

For an accurate estimate, the above requires manually excluding files that would
normally be removed due to the `INSTALL_MASK` (defined in
src/scripts/common.sh). You can also use the more direct method of replicating
what `build_image` does by installing the package into an empty root.

Example: Say you want to find out how much space `update_engine` and its runtime
dependencies would take on an empty image.

```bash
export INSTALL_MASK="<DEFAULT_INSTALL_MASK from src/scripts/common.sh>"
mkdir /tmp/foo
emerge-<board> --root=/tmp/foo --root-deps=rdeps --usepkgonly update_engine
du -sh /tmp/foo
```

## How do I figure out which package a file belongs to?

```bash
equery-<board-name> belongs libglog.so.0
equery belongs /bin/bash
```

## How do I write my own ebuild?

An ebuild file is the recipe to build a package. Since it is written in shell
script it is very flexible, but there is a recommended format. Start with one of
our existing chromium ebuilds as an example. The ebuild should be named
`project-version.ebuild` and it should live in a directory under an appropriate
category. For `chromiumos` packages we've been using the `chromeos-base`
category. The man page can be extremely helpful when writing an ebuild:

```bash
man 5 ebuild
```

The board setup process creates an `ebuild-<board_name>` wrapper that you can
use within the chroot just as you do with `emerge-<boardname>`

```bash
ebuild-arm-generic openssh-5.2_p1-r3.ebuild compile
```

If you are writing a new package for that does not exist upstream then you will
want to upload the files for the package to the localmirror. See the
[download mirror](#download-mirror) section.

## How do I store transient artifacts in an ebuild?

Most builds won't need this, but some builds need to store artifacts that are
not related to the functionality of the package and don't need to be installed
on users' machines. For example, you may wish to collect data about the resource
usage of your build. You can store such artifacts in the directory named by the
environment variable `CROS_ARTIFACTS_TMP_DIR`. This directory is created before
the `clean` phase of a build. In the `postinst` phase, all artifacts in this
directory will be moved to
`"${SYSROOT}/var/lib/chromeos/package-artifacts/${CATEGORY}/${PF}"`.

## How do I check that my package is present on the download mirror?

When we download packages for use in ChromiumOS, we only download packages that
are present on the upstream gentoo mirror or on our localmirror. If your package
is not present on either download mirror, then you need to
[upload it manually to localmirror][localmirror].

Once you have uploaded your package to the mirror, you can test that it's
working by running the following commands:

```bash
rm -f /var/lib/portage/distfiles-target/*
emerge-arm-generic -F app-laptop/laptop-mode-tools

>>> Fetching (1 of 1) app-laptop/laptop-mode-tools-1.57-r2 from chromiumos for /build/x86-generic/
>>> Downloading 'http://commondatastorage.googleapis.com/chromeos-mirror/gentoo/distfiles/laptop-mode-tools_1.57.tar.gz'
  % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                 Dload  Upload   Total   Spent    Left  Speed
  0     0    0     0    0     0      0      0 --:--:-- --:--:-- --:--:--     0
curl: (22) The requested URL returned error: 404
No digest file available and download failed.

>>> Downloading 'http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/laptop-mode-tools_1.57.tar.gz'
  % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                 Dload  Upload   Total   Spent    Left  Speed
100   97k  100   97k    0     0   565k      0 --:--:-- --:--:-- --:--:--  647k
```

## How do I uprev an ebuild?

An ebuild file is the recipe to build a package. When building a package,
portage searches all active overlays, and selects the highest version ebuild
that is unmasked for the given architecture. Once it determines which ebuild to
build, it compares its version number against the most recently emerged version
(if any) to determine if the package must be upgraded or downgraded.

When you are working on a `cros-workon` package (one with a 9999 ebuild), you
only need to modify the 9999 ebuild - the Commit Queue will auto-uprev the
version of the ebuild by copying the 9999 ebuild to an ebuild with the new
bumped revision. Or if your `cros-workon` ebuild sets
`CROS_WORKON_MANUAL_UPREV=1`, you can uprev manually using the
`cros_mark_as_stable` script.

But, when you are working on a "non `cros-workon`" package (i.e a package for
whose ebuild doesn't `inherit cros-workon`, chromiumos doesn't host a git tree,
and autouprev isn't supported) you must manually increase the version number in
the ebuild filename.

When upgrading to a new version of the package (i.e. grabbing a newer version
from upstream), then increase the ebuild version number (`foo-M.m.ebuild`).

When changing the ebuild contents (i.e. applying a custom patch or fixing
cross-compile issues), then increase the ebuild revision number
(`foo-M.m-rN.ebuild`).

When uprevving ebuilds, just rename the symlink (by convention, named as
`*-r#.ebuild`), or create a new one, if it doesn't exist.

```bash
$ ls -l
chromeos-bsp-board-0.0.1.ebuild
chromeos-bsp-board-0.0.1-r1.ebuild -> chromeos-bsp-board-0.0.1.ebuild
```

```bash
$ git mv chromeos-bsp-board-0.0.1-r1.ebuild chromeos-bsp-board-0.0.1-r2.ebuild
```

If updating packages versions (say from PV=0.0.1 to PV=0.1.0 above), rename the
base file (i.e., `chromeos-bsp-board-0.0.1.ebuild`).

### Using chromeos-version.sh

For `cros-workon` packages, a `chromeos-version.sh` script can be used to tell
the Commit Queue which version to uprev the stable ebuild version to. The script
should be placed under the `files/` directory of the ebuild and looks like the
following:

```bash
#!/bin/sh
# Copyright <copyright_year> The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This echo statement sets the package base version (without its -r value).
# If it is necessary to add a new blocker or version dependency on this ebuild
# at the same time as revving the ebuild to a known version value, editing this
# version can be useful.

echo 0.0.2
```

Here, `0.0.2` is the version you want to set the package to. If you need to set
a blocker, it would look like the following:

```bash
RDEPEND="!<chromeos-base/pkgname-0.0.2"
```

The version can also be generated from the package sources, whose directory is
passed as parameter to the script, e.g.:
```bash
exec sed -e 's/devel/pre/g' -e 's/-/_/g' $1/VERSION
```

The Commit Queue will automatically add and increment `-rN` part of the version
as required.

## How do I build a debug package?

There are a few things that are important to get a debug package:

*   Make sure that you are compiling with the right compiler flags. Typically
    this can be done by setting `CFLAGS` before you invoke emerge.
*   Make sure that the binaries are not stripped, and the build files are not
    deleted. You can do that by setting the right `FEATURES`.
*   Tell portage to enable debug options for the package, with `USE`.

Putting these together:

```bash
USE="cros-debug" CFLAGS="-g -O0" FEATURES="nostrip noclean -splitdebug" \
    emerge-amd64-generic -a <package-name>
```

*   *NOTE*: the `cros-debug` `USE` flag is specific to first-party
    ChromiumOS packages. Its main purpose is to enable debug
    assertions.
*   You may also want to check if your target `USE`s the bare `debug`
    flag, especially in third-party packages (like
    [CUPS][cups-ebuild-use-debug]). However, take note that `debug`
    isn't as well-defined as `cros-debug`; third-party ebuilds can do
    any number of things, like increasing log levels to high verbosity.
*   By default, we separate the debug symbols from the binary and store it in
    `/build/<board name>/usr/lib/debug` in the chroot. You have to set
    `FEATURES="-splitdebug nostrip"` to not strip the binary.
*   prebuilts binaries produced by the builder do not contain the separate
    symbols. To install the debug symbols for all packages, you can run
    `cros_install_debug_syms --board=$BOARD --all` in the chroot or always run
    `build_packages` with `--withdebugsymbols`

## How do I use the dev server?

See
[Using the dev server][devserver].

## How do I install a single package (using gmerge and the dev server)?

See the
[How to build a single package and install it without doing a full update][devserver-single-package]
section of the dev server page.

## How do I install a single package (without gmerge)?

Mount the image from file or USB stick using:

```bash
./mount_gpt_image.sh
```

Emerge into the mounted system:

```bash
emerge-amd64-generic -k --root=/tmp/m package-name
```

## The package I want to install has been "masked". How do I fix that?

When importing a package with `emerge-<board_name>`, you may get an error
message about "masked packages". For instance, the response to

```bash
emerge-amd64-generic flashrom
```

may contain the following:

```bash
!!! All ebuilds that could satisfy "sys-apps/flashrom" have been masked.

!!! One of the following masked packages is required to complete your request:

-   sys-apps/flashrom-0.9.0 (masked by: ~x86 keyword)
```

To unmask a package, edit the `KEYWORDS` field of the ebuild directly and add
`"amd64 arm x86"`. If you see existing entries like `"~amd64"`, simply remove
the tilde (`~`).

## How do I build a package without it deleting the build directory (eg, to see a kernel `.config` file)?

Prepend `FEATURES=noclean` to `emerge`:

```bash
FEATURES="noclean" emerge-amd64-generic -a kernel
```

```bash
ls -l
/build/x86-generic/tmp/portage/chromeos-base/kernel-0.0.1-r2/work/kernel-0.0.1/.config
```

## How do I modify a portage package?

Within ChromiumOS the portage repository is considered read-only. This means
that if the portage package has a problem then we must create an overlay for it
in **chromiumos-overlay**. A common reason for needing to do this is that the
package will not cross compile (for ARM or x86). Here is how to do that:

**Note:** You must run `emerge-${BOARD}` inside the chroot, but the `repo` and
`git` commands should be done outside the chroot.

1.  Try emerging the package to see if it builds. We assume that it doesn't (or
    perhaps builds, but doesn't work properly) which is why you are here.

    ```bash
    (cros-chroot) $ emerge-arm-generic nfs-utils

     * IMPORTANT: 1 news items need reading for repository 'gentoo'.
     * Use eselect news to read news items.

    Calculating dependencies... done!

    >>> Emerging (1 of 1) net-fs/nfs-utils-1.1.4-r1 for /build/arm-generic/
     * nfs-utils-1.1.4.tar.bz2 RMD160 SHA1 SHA256 size ;-) ...               [ ok ]
     * CPV:  net-fs/nfs-utils-1.1.4-r1
     * REPO: gentoo
     * USE:  arm elibc_glibc kernel_linux tcpd userland_GNU
    >>> Unpacking source...
    >>> Unpacking nfs-utils-1.1.4.tar.bz2 to /build/arm-generic/tmp/portage/net-fs/nfs-utils-1.1.4-r1/work
     * Applying nfs-utils-1.1.4-rpcgen-ioctl.patch ...                        [ ok ]
     * Applying nfs-utils-1.1.4-ascii-man.patch ...                           [ ok ]
     * Applying nfs-utils-1.1.4-mtab-sym.patch ...                            [ ok ]
     * Applying nfs-utils-1.1.4-no-exec.patch ...                             [ ok ]
    >>> Source unpacked in /build/arm-generic/tmp/portage/net-fs/nfs-utils-1.1.4-r1/work
    >>> Compiling source in /build/arm-generic/tmp/portage/net-fs/nfs-utils-1.1.4-r1/work/nfs-utils-1.1.4 ...
     * econf: updating nfs-utils-1.1.4/config.guess with /usr/share/gnuconfig/config.guess
     * econf: updating nfs-utils-1.1.4/config.sub with /usr/share/gnuconfig/config.sub
    ./configure --prefix=/usr --build=x86_64-pc-linux-gnu --host=armv7a-cros-linux-gnueabi --mandir=/usr/share/man --infodir=/usr/share/info --datadir=/usr/share --sysconfdir=/etc --localstatedir=/var/lib --mandir=/usr/share/man --with-statedir=/var/lib/nfs --disable-rquotad --enable-nfsv3 --enable-secure-statd --with-tcp-wrappers --enable-nfsv4 --disable-gss
    configure: loading site script /usr/share/config.site

    ...

    Making all in locktest
    make[2]: Entering directory `/build/arm-generic/tmp/portage/net-fs/nfs-utils-1.1.4-r1/work/nfs-utils-1.1.4/tools/locktest'
    gcc -DHAVE_CONFIG_H -I. -I../../support/include  -D_GNU_SOURCE -D_GNU_SOURCE  -O2 -pipe -I/build/arm-generic/usr/include/ -I/build/arm-generic/include/ -ggdb -march=armv7-a -mtune=cortex-a8 -mfpu=vfpv3-d16 -mfloat-abi=softfp -MT testlk-testlk.o -MD -MP -MF .deps/testlk-testlk.Tpo -c -o testlk-testlk.o `test -f 'testlk.c' || echo './'`testlk.c
    cc1: error: unrecognized command line option "-mfpu=vfpv3-d16"
    cc1: error: unrecognized command line option "-mfloat-abi=softfp"
    testlk.c:1: error: bad value (armv7-a) for -march= switch
    testlk.c:1: error: bad value (cortex-a8) for -mtune= switch
    make[2]: *** [testlk-testlk.o] Error 1
    make[2]: Leaving directory `/build/arm-generic/tmp/portage/net-fs/nfs-utils-1.1.4-r1/work/nfs-utils-1.1.4/tools/locktest'
    make[1]: *** [all-recursive] Error 1
    make[1]: Leaving directory `/build/arm-generic/tmp/portage/net-fs/nfs-utils-1.1.4-r1/work/nfs-utils-1.1.4/tools'
    make: *** [all-recursive] Error 1
     * ERROR: net-fs/nfs-utils-1.1.4-r1 failed:
     *   Failed to compile
     *
     * Call stack:
     *     ebuild.sh, line  54:  Called src_compile
     *   environment, line 2648:  Called die
     * The specific snippet of code:
     *       emake || die "Failed to compile"
     *
     * If you need support, post the output of 'emerge --info =net-fs/nfs-utils-1.1.4-r1',
     * the complete build log and the output of 'emerge -pqv =net-fs/nfs-utils-1.1.4-r1'.
     * The complete build log is located at '/build/arm-generic/tmp/portage/net-fs/nfs-utils-1.1.4-r1/temp/build.log'.
     * The ebuild environment file is located at '/build/arm-generic/tmp/portage/net-fs/nfs-utils-1.1.4-r1/temp/environment'.
     * S: '/build/arm-generic/tmp/portage/net-fs/nfs-utils-1.1.4-r1/work/nfs-utils-1.1.4'

    >>> Failed to emerge net-fs/nfs-utils-1.1.4-r1 for /build/arm-generic/, Log file:
    ```

1.  Make sure you have the latest version in portage from gentoo. We don't want
    old versions since it makes for more work when we update

    ```bash
    $ cd ...src/third_party/portage-stable
    $ git checkout remotes/cros/gentoo
    $ ls net-fs/nfs-utils/
    ChangeLog     nfs-utils-1.1.4-r1.ebuild  nfs-utils-1.2.0.ebuild
    Manifest      nfs-utils-1.1.5.ebuild     nfs-utils-1.2.1.ebuild
    files         nfs-utils-1.1.6-r1.ebuild
    metadata.xml  nfs-utils-1.1.6.ebuild

    ```

1.  Use `repo start` to start your modification to chromiumos-overlay

    ```bash
    cd ...src/third_party/chromiumos-overlay/
    repo start add-nfs-utils-to-overlay .
    ```

1.  Copy this package into the corresponding place in chromiumos-overlay

    ```bash
    mkdir net-fs
    cp -r ../portage-stable/net-fs/nfs-utils net-fs/.

    # we want to use the latest ebuild, so remove the others
    cd net-fs/nfs-utils
    rm nfs-utils-1.1* nfs-utils-1.2.0*
    ```

1.  Upload this change to the review server and go through the process to get it
    committed

    ```bash
    git commit
    # Imported nfs-utils from portage
    #
    # TEST=emerge, although it doesn't actually build
    git cl upload
    # ... wait for review
    # git push
    ```

1.  Use `repo start` to start your modification to your new overlay package

    ```bash
    cd ...src/third_party/chromiumos-overlay/
    repo start fix-nfs-utils .
    ```

1.  Add in a patch and modify the ebuild to use it

    ```bash
    # find or create a patch, put it in the files directory

    # modify the ebuild, e.g. a cross-compile.patch

    src_prepare() {
        epatch "${FILESDIR}"/${PN}-1.1.4-mtab-sym.patch
        epatch "${FILESDIR}"/${PN}-1.1.4-no-exec.patch

        # here is the new one
        epatch "${FILESDIR}"/${PN}-1.1.4-cross-compile.patch
    }
    ```

1.  Check that it builds OK now

    ```bash
    (cros-chroot) $ emerge-arm_generic nfs-utils
    ```

1.  Upload this change to the review server and go through the process to get it
    committed

    ```bash
    git commit
    # Modifications to nfs-utils to make it build
    #
    # TEST=emerge-arm_generic

    git cl upload
    # ... wait for review
    # git push
    ```

1.  Relax you are done

## How do I handle file collisions?

Sometimes when using `build_package`s or `setup_board` you might get an error
like this:

```bash
gtk-doc-am-1.13-r2: >>> Installing (1 of 1) dev-util/gtk-doc-am-1.13-r2
gtk-doc-am-1.13-r2: * This package will overwrite one or more files that may
belong to other
gtk-doc-am-1.13-r2: * packages (see list below).
gtk-doc-am-1.13-r2: *
gtk-doc-am-1.13-r2: * Detected file collision(s):
gtk-doc-am-1.13-r2: *
gtk-doc-am-1.13-r2: * /usr/bin/gtkdoc-rebase
gtk-doc-am-1.13-r2: *
gtk-doc-am-1.13-r2: * Searching all installed packages for file collisions...
gtk-doc-am-1.13-r2: *
gtk-doc-am-1.13-r2: * Press Ctrl-C to Stop
gtk-doc-am-1.13-r2: *
gtk-doc-am-1.13-r2: * dev-util/gtk-doc-1.11
gtk-doc-am-1.13-r2: * /usr/bin/gtkdoc-rebase
gtk-doc-am-1.13-r2: *
gtk-doc-am-1.13-r2: * Package 'dev-util/gtk-doc-am-1.13-r2' NOT merged due to
file
gtk-doc-am-1.13-r2: * collisions. If necessary, refer to your elog messages for
the whole
gtk-doc-am-1.13-r2: * content of the above message.
```

This tends to happen when files move between packages, or if the upstream Gentoo
packages in your chroot become inconsistent.

### Moving files between packages

Sometimes you have one package (let's call it "pkgsrc" for short) providing a
file but want to change things so that a different package ("pkgdst") now
provides that file. If you were to try to upgrade "pkgdst" and have it install
the file but the old "pkgsrc" still installs that file, we get into a bad state.

To solve this, we leverage package blockers. At the simplest level, this is an
ebuild dependency syntax that allows you to say "do not allow pkg A to be
installed whilst pkg B is installed". But if you combine the blocker syntax with
versions, portage is smart enough to detect that it needs to handle these
blocking packages specially and do an automatic upgrade for you.

Let's use a real world example. The package `sys-fs/e2fsprogs-libs` used to
install the libuuid library. But the upstream developers decided to move it to
the `sys-apps/util-linux package`. Specifically, the `e2fsprogs-libs-1.41.7`
release was the last one to include the library while `util-linux-2.16` was the
first one to include it.

So in the `util-linux-2.16` ebuild, we declare that at runtime, it cannot be
installed simultaneously with `e2fsprogs-libs-1.41.7` or older. Since
`util-linux` does not need `e2fsprogs-libs` to build libuuid, we don't have to
declare the blocker in DEPEND.

```bash
# It's helpful to future readers if you document why the blocker is needed. For
# example:
# File /x/y/z moved from e2fsprogs-libs.
RDEPEND="!<=sys-libs/e2fsprogs-libs-1.41.7

    ...other runtime dependency stuff...
```

Since `e2fsprogs-libs` doesn't actually need libuuid itself in order to build,
there is no dependency in that ebuild on a newer util-linux version. However,
let's assume it did. That would mean in the `e2fsprogs-libs-1.41.8` ebuild, we
would write:

```bash
RDEPEND=">=sys-apps/util-linux-2.16
    ...."
DEPEND=">=sys-apps/util-linux-2.16
    ...."
```

This is enough information for portage to be able to automatically resolve this
blocker for you. It will make sure that when upgrading `util-linux` and
`e2fsprogs-libs`, it will first upgrade `util-linux`, then ignore file
collisions that it hits with `e2fsprogs-libs` (since `util-linux` is taking
ownership of those files), then upgrade `e2fsprogs-libs`. If there was a bug and
`e2fsprogs-libs` still tried to install libuuid, portage would throw an error
because `util-linux` now owns those files.

See also the
[Gentoo devmanual][gentoo-blockers] for more information about blockers.

#### Handling 9999 ebuilds

If the file you are moving comes from a `cros-workon` package (where you only
modify the 9999 ebuild), the uprev is handled automatically. This means there
isn't anything to safely block against.  Instead, you should add/update
`chromeos-version.sh` and bump the version.  See the
[`chromeos-version.sh` description](#using-chromeos_version_sh) for more
details.

### Testing of upstream packages

For example, you had tried to `build_packages` or `emerge` after pulling in some
new/updated upstream packages.

To recover from mixing of upstream packages, you can:

*   rebuild your chroot (this can be very painful)
*   track down and unmerge the offending packages individually

To find out which package provides the offending file and unmerge it, try:

```bash
(cros) equery belongs /usr/bin/gtkdoc-rebase
*   Searching for /usr/bin/gtkdoc-rebase ...
dev-util/gtk-doc-am-1.18 (/usr/bin/gtkdoc-rebase)

(cros) emerge --unmerge gtkdoc
```

To deal with moving files between packages, you'll want to utilize blockers.

## What does "build_packages" actually do?

```bash
emerge-arm-generic virtual/target-os
```

Depending on the options you give, other packages may be emerged also (note that
many of these options are the default):

*   `--withdev` brings in `virtual/target-os-dev` which is some extra stuff
    useful for developers like python, the gmerge utility and some X11
    utilities.
*   `--withfactory` brings in `chromeos-base/chromeos-factoryinstall` sets
    things up for the factory (production)
*   `--withtest` brings in `virtual/target-os-test` which includes test
    infrastructure like gzip, tar, and ssh server and rsync
*   `--withautotest` brings in `chromeos-base/autotest-all` which brings in the
    python autotests. These are used to test that the built image functions
    correctly.

Each of the packages is defined by an 'ebuild' which defines what needs to be
done to build that package. The ebuild includes the package's dependencies -
both build dependencies (things needed to build the package on the host) and
run-time dependencies (things needed at run-time on the target to run any
installed software). You can find most of the ebuild packages in the
`src/third_party/chromiumos-overlay directory`. For example,
`src/third_party/chromiumos-overlay/virtual/target-os/target-os-1.ebuild` is the
base ebuild containing all the packages in ChromeOS. This ebuild lists all
other top-level ChromiumOS packages as explicit dependencies. Thus, all of
ChromiumOS is built when building this one package.

A small but important detail is that normally `build_packages` calls
`parallel_emerge`, a script which builds a number of packages in parallel,
taking advantage of multi-core machines. This is why you will see a display
showing how many packages are left to build, and how many are currently in
progress.

If you are using the minilayout then you will not have downloaded all the source
for ChromeOS. So when an ebuild is emerged it may need to download some source.
For example, see the ebuild in:

```bash
.../src/third_party/chromiumos-overlay/app-arch/
```

It has some `SRC_URI` lines describing where the source package can be
downloaded from. The file used in case of tar may be something like:

```
http://gsdview.appspot.com/chromeos-mirror/gentoo/distfiles/tar-1.23.tar.bz2
```

This file is cached for you after the first download, in, for example,
`chroot/var/lib/portage/distfiles-target/tar-1.23.tar.bz2`. It contains a
tarball of the source code. The next time you emerge, portage will untar it from
this location. You may also be interested to know that emerge may apply some
patches to the source before it starts building - you can find these in a
`files` subdirectory of `app-arch/`, in the case of tar. If you look at the
ebuild you can even see where it patches these in, using `epatch`.

If you want to try this out, type:

```bash
(cros) emerge-arm_generic app-arch/tar
```

You should see it download some source, build it and install it into your target
build in `chroot/build/arm-generic`.

So does `build_packages` build absolutely everything from source? Wouldn't that
take forever? Actually, no. It cheats. There are ChromiumOS build servers
constantly building binary packages for many common platforms. These pre-built
packages are available and normally these are used instead. There is a
`--usepkg` (default true) option for `build_packages` that adds some flags to
the emerge command to make it use binary packages. To see what happens in this
case, type:

```bash
emerge-amd64-generic --getbinpkg --usepkg --with-bdeps y app-arch/tar
```

This time you will probably see it download something like:

```
http://gsdview.appspot.com/chromeos-prebuilt/board/x86-generic/master-02.12.10.182555/packages/app-arch/tar-1.23-r4.tbz2
```

and then simply install the package as before. The C compiler will not be
touched. Again the file is cached, this time in something like
`chroot/build/arm-generic/packages/app-arch/tar-1.23-r4.tbz2`. Next time you run
the `emerge --usepkg` it will not need to download anything.

## What is a virtual package and how do they work?

A virtual package is used in portage when any of several different packages can
perform the same function. A classic example is "`virtual/editor`". There are
dozens of editors out there (including the most fabulous one of them all:
~vim~~ ~~emacs~~ ~~nano~~ ed), all of which work just fine. It's a matter of
preference which one to install. If we want to write our own package that
depends on an editor being installed, we don't really care which editor is
picked. In order to specify this, we specify a runtime dependency on
`virtual/editor`.

### Depending on a virtual package

Depending on a virtual package is pretty easy: just add a dependency (either
`DEPEND` or `RDEPEND`) on the virtual, like:

```bash
DEPEND="virtual/editor"
```

### Adding a virtual

Depending on a virtual is very easy, but what about if you want to add a new
virtual? First: create an ebuild inside the virtual category with a `-0` version
like `mything-0.ebuild`. This virtual doesn't do too much except to specify
dependencies on all of the possible implementations of the virtual.

It's easiest to look at two samples.

First, we'll look at [virtual/editor]. The important things to notice are that
this ebuild doesn't do anything itself--it just specifies dependencies.
Specifically, it says that any of the editors listed are OK by specifying this
as the `RDEPEND` (note the `||` at the beginning):

```bash
RDEPEND="|| (
    app-editors/nano
    app-editors/neatvi
    app-editors/qemacs
    app-editors/vim
)"
```

In this case the ebuild doesn't provide any particular way to choose which
editor is chosen--it just specifies all of the options. As long as one of those
options is installed, `virtual/editor` will be happy. If none of those are
installed and you try to build `virtual/editor` (or you depend on it), portage
will pick one of the editors and install it (by trying them in order).

A second example is [ChromiumOS's virtual/linux-sources ebuild]. You can see
the important bits here:

```bash
IUSE="-kernel_next"

RDEPEND="
kernel_next? ( sys-kernel/chromeos-kernel-next )
!kernel_next? ( sys-kernel/chromeos-kernel )
"
```

You can see that the kernel ebuild specifies a way to choose what implementation
via `USE` flags: you can use this to choose between `kernel` and `kernel-next`.

### Virtuals and central management

You should definitely pay attention to the fact that virtuals are
centrally-managed. Said another way: if you need to add another implementation
of a virtual (like that nifty new `vimacs` editor you wrote), you need to go to
the virtual itself and add it to the list.

There is one common (at least in ChromiumOS) case where it's not be possible to
have things centrally managed. This happens when you've got an implementation of
a virtual that's in your private overlay. We'll imagine a virtual called
`virtual/chromeos-firmware`. There might be several public implementations of
chromeos-firmware, like:

```bash
sys-boot/chromeos-firmware-seaboard
sys-boot/chromeos-firmware-mario
sys-boot/chromeos-firmware-alex
```

The `virtual/chromeos-firmware` ebuild would list all of those as options.
...but what happens when you've got a new project called `sisyphus` and you've
got a private overlay for it (because you don't want to make all the details
public yet). How do you make this work?

You can do this by overriding the virtual in your private overlay. Your overlay
will have a _copy_ of the `virtual/chromeos-firmware ebuild`. However, in your
copy the `RDEPEND` will just be `sys-boot/chromeos-firmware-sisyphus`. Now you
can put the `chromeos-firmware-sisyphus` ebuild in your overlay and you're all
set!

#### Overriding virtual policies

When portage searches for packages to install, it merges all of the packages in
all the overlays. If the same ebuild (`category/package-name`) is found in
multiple places, it comes down to comparing versions such that the highest
version wins (and if the same version is found, then overlays will "whiteout"
other overlays based on search order). But that's a lot to keep straight and is
fairly fragile. If the ebuild in the main tree is revbumped to a version higher
than is in an overlay, then bad things could start happening.

The policy we have in place for versioning of our virtuals is as follows:

overlay                               | version (pv)
------------------------------------- | ------------
chromiumos-overlay                    | 1
chromeos-overlay                      | 1.3
project-`<project name>`              | 1.5
project-`<project name>`-private      | 1.7
chipset-`<chip>`                      | 1.8
chipset-`<chip>`-private              | 1.8.5
baseboard-`<baseboard>`               | 1.9
baseboard-`<baseboard>`-private       | 1.9.5
overlay-`<board>`                     | 2
overlay-variant-`<board>`             | 2.5
overlay-`<board>`-private             | 3
overlay-variant-`<board>`-private     | 3.5
overlay-`<board>`-`<special>`-private | 4

### Old-style virtuals

The above description is for new-style virtuals. There's also old-style
virtuals. You shouldn't ever add one of those, but you might still run into
them.

Declaring an old-style virtual is easy, but inflexible. In the `profiles/`
subdir, you will find files named virtuals. It is a simple text file with one
virtual per line:

```bash
# Add virtual packages for this profile
virtual/chromeos-bsp chromeos-base/chromeos-bsp-null
virtual/chromeos-bsp-dev chromeos-base/chromeos-bsp-dev-null
```

The first element is the name of the virtual while the second element is the
package that provides that virtual. This file format has no other
options/extensions available to it.

## When does a dependency cause a rebuild?

Portage's dependency system can be tricky to understand, especially when it
comes to what will cause a package to be rebuilt.

Important: package manager's dependencies are NOT like make dependencies. If
package B depends on package A, it does NOT mean B is re-built when A is
updated. It only means when B is pulled in, A is also pulled in.

Be sure you understand [the different dependency types](#dependency-types)
before proceeding.

To think about how everything works, we'll pretend we have three ebuilds:

*   `staticlib.ebuild` - Provides `staticlib.h` and `staticlib.a`
*   `dynamiclib.ebuild` - Provides `dynamiclib.h` and `dynamiclib.so`
*   `milliondollarapp.ebuild` - Uses `staticlib` (`DEPEND`) and `dynamiclib`
    (both `DEPEND` and `RDEPEND`)

OK, so what happens when we make changes and rebuild things. Here is what
happens:

*   If we change / build `staticlib`, only `staticlib` will be build.
*   If we change / build `dynamiclib`, `dynamiclib` will be built and then
    `milliondollarapp` (if is is currently installed).
*   If we've never built `staticlib` or `dynamiclib` and then we change / build
    `milliondollarapp`, we'll first build `staticlib` and `dynamiclib` and then
    `milliondollarapp`.
*   If we've already built `staticlib` and `dynamiclib` and then we change /
    build `milliondollarapp`, we'll just build `milliondollarapp`.
    *   This is true _even if_ we've changed (but not built) `staticlib` or
        `dynamiclib`.

The above is a little weird, but can be understood by remembering that portage
is primarily concerned with speed and correctness. ...but portage is not super
concerned with making sure you have the latest version of every last dependency
(as long as you have some version it is happy). Specifically:

*   If a newer version of a static library is available / installed, that
    doesn't mean you need to rebuild everything that uses that static library.
    They all worked fine with the old version of the static library and will
    keep working just fine.
*   If a newer version of a dynamic library is available (but not installed),
    there's no particular reason to install it if the old one worked fine.
*   If you _install_ a newer version of a dynamic library you'd better rebuild
    all apps that use it. It's possible that the newer version included a
    matching header/shared object change (like changing function call from
    taking an int32 to an int64) and that the only correct thing to do is to
    rebuild.
    *   Note that the same argument _can't_ be made if we only `RDEPEND` on
        `dynamiclib`. In that case a build of `dynamiclib` _wouldn't_ cause a
        rebuild of `milliondollarapp`.

Note that the above examples use a static library and dynamic library as an
example. Hopefully it makes sense that we can map other problems to the same
concepts. For instance, if we had an ebuild that took a whole bunch of other
build outputs and created a `.zip` file out of them, that would be just like the
static library case.

### ChromeOS-specific notes

People working on ChromeOS probably find portage's philosophy (that getting the
newest version isn't important) a bit frustrating. If an engineer syncs down new
source code and tries to build a new boot image, the engineer would hope that
the image has all of the newest versions of all of the packages.

We'll take `chromeos-bootimage` as an example. This build wants to take the
binary output of several other ebuilds (the BCT, the firmware, the flattened
device tree, etc) and concatenate them together (with some extra processing) to
make a single binary image. Technically, `chromeos-bootimage` should only
`DEPEND` on all of the other components. However, that means that if a new
version of the BCT is checked in and then we do a build, portage will not decide
to re-build `chromeos-bootimage`. Ick.

As a hack, ChromeOS packages often say that they `DEPEND` and `RDEPEND` in
cases like `chromeos-bootimage`. Now if you build all packages, portage will be
sure to rebuild `chromeos-bootimage` if the BCT ever changes. This is an awkward
way to do things but is the current workaround. The fabled `ABIDEPEND` feature
of portage (doesn't exist yet) is supposed to fix this and make it so we don't
need the hack anymore.

## Where to look for more information

The [Gentoo Development Guide] provides plenty of detailed information about
[Portage], the [Gentoo] build system. This [package management specification]
is a good writeup describing Gentoo ebuild system.

Instructions for building ChromiumOS can be found
[here][chromium-os-dev-guide].

[Portage]: https://wiki.gentoo.org/wiki/Portage
[Overlay FAQ]: overlay_faq.md
[Working with Gentoo]: https://www.gentoo.org/doc/en/handbook/handbook-x86.xml?part=2
[Gentoo Development Guide]: https://devmanual.gentoo.org/
[Portage]: https://wiki.gentoo.org/wiki/Portage
[Gentoo]: https://www.gentoo.org/
[localmirror]: ../archive_mirrors.md
[devserver]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/docs/devserver.md
[devserver-single-package]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/docs/devserver.md#TOC-How-to-build-a-single-package-and-i
[gentoo-blockers]: https://devmanual.gentoo.org/general-concepts/dependencies/#blockers
[virtual/editor]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/virtual/editor/editor-1.ebuild
[ChromiumOS's virtual/linux-sources ebuild]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/virtual/linux-sources/
[package management specification]: https://projects.gentoo.org/pms/8/pms.html
[chromium-os-dev-guide]: ../developer_guide.md
[cups-ebuild-use-debug]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/932089ace19442cbd1d72a9304226ab604984ac4/net-print/cups/cups-9999.ebuild#28
