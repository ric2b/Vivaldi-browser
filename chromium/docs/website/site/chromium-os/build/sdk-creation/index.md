---
breadcrumbs:
- - /chromium-os
  - ChromiumOS
- - /chromium-os/build
  - ChromiumOS Build
page_name: sdk-creation
title: ChromiumOS SDK Creation
---

[TOC]

## Introduction

The ChromiumOS project has an SDK that provides a standalone environment for
building the target system. When you boil it down, it's simply a Gentoo/Linux
chroot with a lot of build scripts to simplify and automate the overall build
process. We use a chroot as it ends up being much more portable -- we don't have
to worry what distro you've decided to run and whether you've got all the right
packages (and are generally up-to-date). Most things run inside of the chroot,
and we fully control that environment, so ChromiumOS developers have a lot less
to worry about. It does mean that you need root on the system, but
unfortunately, that cannot be avoided.

This document will cover how the SDK is actually created. It assumes you have a
full ChromiumOS checkout already.

## Prebuilt Flow

When you run `cros_sdk` (found in
[chromite/bin/](https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/scripts/cros_sdk.py))
for the first time, it automatically downloads the last known good sdk version
and unpacks it into the chroot/ directory (in the root of the ChromiumOS source
checkout).

That version information is stored in
[src/third_party/chromiumos-overlay/chromeos/binhost/host/sdk_version.conf](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/chromeos/binhost/host/sdk_version.conf):

```
SDK_LATEST_VERSION="2022.03.17.105954"
```

This is used to look up the tarball in the
[chromiumos-sdk](https://storage.googleapis.com/chromiumos-sdk/) Google Storage
bucket. So with the information above, we know to fetch the file:

<https://storage.googleapis.com/chromiumos-sdk/cros-sdk-2022.03.17.105954.tar.xz>

With the `--update` flag enabled, `cros_sdk` will ensure your chroot was built
from the file in `sdk_version.conf`, and otherwise re-builds your chroot from
that version.  This behavior is soon to become the default.

## Bootstrap Flow

The question might arise: How is the prebuilt SDK tarball created in the first
place? There is a pipeline of builders (a.k.a. the "SDK builders") that builds a
new version of the SDK tarball, tests it, and updates the SDK version file so
that developers and other builders will use the new tarball. The SDK builder
pipeline runs twice per day and takes ~4 hours for a successful run.

For a bootstrap starting point, to avoid the case where the SDK itself may have
been broken by a commit, the builder uses a pinned known "good" version from
which to build the next SDK version. This version is manually moved forward as
needed.

We download a copy of the SDK tarball to bootstrap with and setup the chroot
environment. This is just using the standard `cros_sdk` command with its
`--bootstrap` option.

As with the latest SDK version, the bootstrap version of the SDK to use is
stored in
[sdk_version.conf](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/chromeos/binhost/host/sdk_version.conf):

```
BOOTSTRAP_FROZEN_VERSION="2020.11.09.170314
```

This is used to look up the tarball in the chromiumos-sdk Google Storage bucket.
So with the information above, `cros_sdk` knows to fetch the file:

<https://storage.googleapis.com/chromiumos-sdk/cros-sdk-2020.11.09.170314.tar.xz>

The SDK builder pipeline also creates binpkgs (prebuilt versions of software
packages) for all toolchain packages. These are uprevved simultaneously with the
SDK: so toolchains binpkgs will be uprevved if and only if a new SDK is
uprevved.

If the overall SDK generation fails, or if the newly built SDK cannot build each
target ChromeOS architecture, then the pinned SDK version is not updated.

### Overview

SDKs are built, uprevved, and tested by a pipeline of builders. ("Builders" are
automated jobs in the LUCI continuous integration framework.) The overall flow
looks like:

1.  The "SDK Builder" (`chromeos/infra/build-chromiumos-sdk`) creates a new SDK
    tarball and toolchain binpkgs.
1.  The "SDK Uprevver" (`chromeos/pupr/chromiumos-sdk-pupr-generator`) creates
    CLs that will update the source-controlled SDK version file and binpkg files
    to point to the new SDK. It sends those CLs into the commit queue for
    testing.
1.  The commit queue launches the "SDK Verifiers":
    `chromeos/cq/chromiumos-sdk-uprev-*-cq`, where `*` is each target
    architecture that the SDK needs to be able to build. These verifiers each
    use the new SDK and binpkgs to build images from source on one of our target
    architectures. If these tests pass, then the uprev CLs will be merged.
1.  As soon as the uprev CLs are merged, the "Remote Latest File Syncer"
    (`chromeos/infra/sync-remote-latest-sdk-file`) will copy the new SDK version
    into the latest `cros-sdk-latest.conf` file on Google Storage.

Read on for more information about the different builders.

### SDK Builder

The SDK builder does the following:

*   Downloads the bootstrap version of the SDK and sets up the chroot
    environment.
*   Builds the SDK board (amd64-host) by entering the chroot and running
    `./build_sdk_board`.
*   Builds and installs the cross-compiler toolchains, as well as standalone copies of the cross-compilers, by entering the chroot and running `cros_setup_toolchains`.
*   Packages the freshly built SDK into a tarball, and uploads it to Google
    Storage.
*   Uploads amd64-host binpkgs and toolchain binpkgs to Google Storage.
*   Launches the SDK Uprevver, which will create CLs so that the newly built
    SDK and prebuilts will be used.

The builder runs the `build_sdk` recipe, defined at
[`infra/recipes/build_sdk.py`][build_sdk.py]. It is configured at
[`infra/config/misc_builders/build_sdk.star`][build_sdk.star]. You can view
the latest runs of that builder [on MILO][milo-build-chromiumos-sdk].

In addition to the main SDK builder, there are also a few varieties of this
builder:
*   [build-chromiumos-sdk-llvm-next][milo-llvm-next] builds the SDK with the
    `llvm-next` USE flag. This is purely informational; SDKs and binpkgs created
    by this builder will never be uprevved.
*   [staging-build-chromiumos-sdk][milo-staging-build-chromiumos-sdk] uses the
    latest, unverified version of Recipes to run the build. This is useful for
    checking on the builder's status. It uploads its artifacts to different
    Google Storage buckets so that they are not accidentally confused with real
    artifacts. It creates uprev CLs, but they are never merged.

[build_sdk.py]: https://chromium.googlesource.com/chromiumos/infra/recipes/+/HEAD/recipes/build_sdk.py "Link to the SDK builder's source code."
[build_sdk.star]: https://chrome-internal.googlesource.com/chromeos/infra/config/+/HEAD/misc_builders/build_sdk.star "Link to the SDK builder's configuration."
[milo-build-chromiumos-sdk]: https://luci-milo.appspot.com/ui/p/chromeos/builders/infra/build-chromiumos-sdk "Link to the latest runs of build-chromiumos-sdk."
[milo-llvm-next]: https://luci-milo.appspot.com/ui/p/chromeos/builders/infra/build-chromiumos-sdk-llvm-next "Link to the latest runs of build-chromiumos-sdk-llvm-next."
[milo-staging-build-chromiumos-sdk]: https://luci-milo.appspot.com/ui/p/chromeos/builders/staging/staging-build-chromiumos-sdk "Link to the latest runs of staging-build-chromiumos-sdk."

### SDK Uprevver

The SDK uprevver uses the PUpr framework (**P**arallel **U**prevs). This
tool was originally developed to uprev ebuilds in the ChromiumOS tree, but has
been extended to create SDK uprev CLs.

The builder writes CLs to uprev
`chromiumos/overlays/chromiumos-overlay/chromeos/binhost/host/sdk_version.conf`,
`chromiumos/overlays/chromiumos-overlay/chromeos/config/make.conf.amd64-host`,
and
`chromiumos/overlays/board-overlays/overlay-amd64-host/prebuilt.conf` so that
they point to the newly built SDK and binhosts. It uploads those CLs to Gerrit,
and it marks CQ+2.

### SDK Verifiers

When the uprevved files run through the CQ, a few builders are started:

*   `chromeos/cq/chromiumos-sdk-uprev-amd64-generic-cq`
*   `chromeos/cq/chromiumos-sdk-uprev-arm-generic-cq`
*   `chromeos/cq/chromiumos-sdk-uprev-arm64-generic-cq`

Each of these builders will cherry-pick in the uprev CLs, download the newly
built SDK, and try to build a ChromeOS image from source.

If all of the verifiers pass, then the uprev CLs will be merged.

### Remote Latest File Syncer

There is a
[Gitiles poller](https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md#luci.gitiles-poller)
that polls for changes to `sdk_version.conf` on `main`. Whenever that file is
modified, the poller automatically triggers the builder
`chromeos/infra/sync-remote-latest-sdk-file`.

That file simply reads the new SDK version, and updates the Google Storage file
`gs://chromiumos-sdk/cros-sdk-latest.conf`. Some workflows within Google3 read
that file to figure out which SDK to download.

### Reverting an SDK uprev

If the SDK uprev lands but causes issues, the correct action for the on-caller
is to revert the uprev CLs.  Please be aware of the `Cq-Depend`s during the
revert.

## Running the SDK builder as a developer

Suppose as a developer you have changes (e.g. in
[chromite/lib](https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/lib/))
that you wish to test with the SDK builder. To run the entire chromiumos-sdk
builder process described above, use `cros try`:

```
cros try chromiumos_sdk -g <cl_1>,<cl_2>
```

where `<cl_1> .. <cl_n>` are CLs to be run against, formatted as
`crrev.com/c/1234578`. This will run the staging builder, and thus will not
actually merge an uprev, unless you use `--production`. It will work from the
`main` branch unless you use `--branch`.

It's likely that just building the SDK board locally would be sufficent for most
cases. To do that, from `~/chromiumos/chromite/shell` run:

```
./build_sdk_board
```
