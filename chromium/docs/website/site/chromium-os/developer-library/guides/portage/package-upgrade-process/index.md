---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: package-upgrade-process
title: New & Upgrade Package Process
---

## Purpose

This page documents the process of upgrading a package in our source
to the latest upstream version, or for pulling in a new package that doesn't
yet exist (since that's the same thing as upgrading from version `<none>`).
These instructions are intended for ChromiumOS developers.

The upgrade process documented here uses a script, called
`cros_portage_upgrade`, developed to help automate/simplify this process.
It is not required that you use this script, and if you are confident that
you can update packages in another way you are free to do so. Some of the
FAQ on this page may still be of use to you, in any case.

Note that this document mostly applies to packages in the [portage-stable]
overlay (more on this later). The workflow it describes can be partially
applied to start an uprev for packages in [chromiumos-overlay] but not at
all to `cros-workon`-able packages.

[TOC]

## Quickstart

A quick reminder of the steps to perform in your chroot set up per [ChromiumOS
developer guide]:

```sh
# Enter chroot, using full path if necessary:
cros_sdk

# Set up boards to test on:
setup_board --board=<YOUR-BOARD>

# Start a new git branch in portage-stable repo:
cd /mnt/host/source/src/third_party/portage-stable
repo start <YOUR-NEW-BRANCH>

# Upgrade package locally:
cros_portage_upgrade --upgrade --board=<board>:<board>... some_package1 some_package2 ...
```

1.  Repeat previous step as needed, fixing any upgrade errors that appear.
1.  Test. Yes. Really. You.
1.  Proceed to follow the [contributor's guide].

## Assumptions

The following assumptions are made with these instructions:

1.  You know which package or packages you want to upgrade.
1.  You know how you intend to specifically test the package(s) after the
    upgrade (beyond the basic smoke tests).
1.  You are familiar with the build environment from a developer perspective.
1.  You know how to start branches, amend commits, upload commits, end branches,
    etc. See the [ChromiumOS developer guide].

## Brief Background

Any packages, upstream included, can be marked as stable or unstable
for any architecture. You don't have to know anything more about
this detail unless you need to upgrade a package to an unstable version.
However, this happens somewhat frequently. Perhaps a feature you need is
only available in the absolute latest version of a package, but that version
has not been marked stable for x86, yet. If you do need to upgrade to an
unstable version, check the appendix entry below.

The ChromeOS source has projects that are Gentoo "overlays", all under
`src/third_party`. Three of them are of interest here.

1.  The `src/third_party/portage-stable` overlay has copies of all upstream
    packages that we use unaltered. When we "upgrade" a package,
    what we are really doing is putting a copy of the current upstream package
    into this overlay.
1.  The `src/third_party/chromiumos-overlay` is probably familiar to you. For
    package upgrade purposes, it will only be used in the following scenario:
    If a package is locally patched before the upgrade, then you will probably
    have to port the local patch to the newer version as well. That should be
    done in this overlay.

## The Process

### Plan your upgrade

#### Plan around packages

Figure out which package or packages you want to upgrade, using their full
"category/package" name (e.g. `sys-apps/dbus`). Decide whether you want to
upgrade them to the latest stable or unstable versions. Generally, your
first choice should be to upgrade packages to the latest stable upstream
version.

#### Plan around patches and overlays

Note whether any of the packages you need to upgrade are locally patched.
This means that somebody, possibly you, has made a patch to the package that
we are using right now. It is important to determine what should happen to
that patch after the upgrade. Inspect the git history for the patch, talk
to the committer, inspect the patch, etc.

*   If the package lives in the `portage-stable` overlay, you do not need to
    worry about this as we do not permit patches to be made in there. For all
    other overlays, it's pretty much guaranteed there are custom changes in play.
*   If the patch was done to backport a change from upstream, then simply
    updating the package to the upstream version should be sufficient and the
    patch can be ignored. Be sure to confirm which version the patch was
    backported from.
*   If the patch is still required, then you will have to re-apply the patch
    after the upgrade. Go through the upgrade process, except for the final
    testing and uploading. Then follow the guidelines for
    [re-applying a patch] below.

#### Plan around boards

Decide which boards you want to try the upgrade for. You should include one
board from each supported arch type (e.g. `amd64`, `arm`, `arm64`), unless
the package is only used on one arch for some reason. Typically, there is no
reason to specify more than one board per architecture type, unless you know
of board-specific testing that must be done. If your package or packages
are used on the host, then you want to upgrade for the host (which can be
done at the same time as the boards). Many packages are used on a ChromeOS
board as well as the host (the "host" is the chroot environment). Choose
boards that you can test the upgrades on, or boards that you know the
packages need to be tested on.

### Prepare Environment

You probably want to `repo sync` your entire source tree.

```sh
cd ~/chromiumos && repo sync -j 4
```

Prepare a new branch in the `src/third_party/portage-stable` project.
This is where the upgrades will be staged/committed.

```sh
cd ~/chromiumos/src/third_party/portage-stable && repo start pkg_upgrade .
```

Now, you need to setup all boards that you will need. Note that this is
not required if you're upgrading a host-only package.

```sh
cros_sdk --enter
setup_board --board=amd64-generic
setup_board --board=cherry
```
### Upgrade Locally

For these instructions, let's say you want to upgrade `media-libs/alsa-lib`,
`media-sound/alsa-headers`, and `media-sound/alsa-utils` on boards
`amd64-generic` and `cherry`. You plan to try the stable upstream versions
first. These commands must be run from within your chroot. If you also
need to upgrade on the host (chroot/amd64), use the `--host` option, which
can be used in combination with the `--board` option or without it.

```sh
cros_portage_upgrade --upgrade --board=amd64-generic:cherry media-libs/alsa-lib \
    media-sound/alsa-headers media-sound/alsa-utils
```

If `cros_portage_upgrade` succeeded the first time, then you have no missing
dependencies or other build-related obstacles to foul the upgrade. But you
still need to test the upgrade results! The upgrade should be prepared as a
commit in `src/third_party/portage-stable`. You should inspect/edit the
commit message before uploading it, though.

If `cros_portage_upgrade` didn't succeed in one go, then it should give you
the means to determine why. Typically, this means that one or more of the
upgraded packages could not be built (using `emerge`) after the upgrade.
This may be due to a package dependency that must also be upgraded or a
package mask that must be edited. In any case, you are given the output of
the failing `emerge` command so that you can determine for yourself what is
needed. Make the necessary change (perhaps adding another package to
upgrade at the command line), then run again.

To upgrade to unstable versions, simply add the `--unstable-ok` option and
follow the instructions. Or see the guidelines for [upgrading to unstable
versions] below.

Run `cros_portage_upgrade --help` to see addition options and usage. If
you are having difficulty with this step, please do not hesitate to contact
[the ChromeOS development mailing list] for assistance.

### Clean up Commit

You may also need to clean up the files in your commit. Our convention is
to only check in files that are used, and some upgrades come with extra
files (such as unused patch files, for example). You can usually tell by
inspecting the list of files and the ebuild whether any files are unused.
You can remove these files from your commit with a command like the
following within `portage-stable`:

```sh
git reset HEAD~ <filepath>
rm <filepath>
git commit --amend
```

If you already built your upgraded package, make sure to do it again after
cleaning up extraneous files to verify that you haven't removed something
important!

### Testing

Use common sense, and your specific expertise, to test the upgraded
packages. The changes are active in your source right now, so you can build
your target boards to test. If your package is added to the host (chroot),
you'll need to `emerge` it onto your chroot and test it there as well.

You probably want to run at least the `suite:smoke` tests for each board,
which you can do by following the tips
[here](https://dev.chromium.org/chromium-os/testing)
(Googlers may also use the tips at goto/cros-test). In particular, you
can use [trybot] to determine what effect your upgrade will have on the
greenness of the waterfall.

One common cause of failure is that upstream has introduced new default
[USE flags]. For example, if the upstream ebuild was set to `-foo` before
(which defaults `foo` to off) and now is set to `+foo`, a new dependency or
a new runtime behavior might have been introduced by the change. You would
need to evaluate the change and determine whether the new `USE` flag default
is one that we should adapt to. If, on the other hand, the flag adds a
dependency on something that we don't use in ChromeOS, you might overlay
the `USE` flag default in the `chromiumos-overlay` repo's
`profiles/targets/chromeos/package.use` in a separate commit, first, before
submitting the `portage-stable` upgrade. See below for an explanation of how
`chromiumos-overlay` functions but note that you do not need to copy the
upstream package into `chromiumos-overlay` to override the default `USE`
flags.

Beyond the `suite:smoke` tests, test whatever you think makes sense given
the packages you just upgraded.

### Uploading

When you are satisfied that the upgrade is safe to push, you still need to
edit the commit message that was created for you by `cros_portage_upgrade`.
Add a bug number, test details, and any other details you want.

```sh
cd ~/chromiumos/src/third_party/portage-stable
git commit --amend
```

Upload the usual way:

```sh
cd ~/chromiumos/src/third_party/portage-stable
repo upload .
```

Then go through the usual code review process on Gerrit per the
[contributor's guide].

### Clean up Environment

After your changelist passes review and submitted to the tree, you will
want to get off the branch you created in `src/third_party/portage-stable`.
You know the drill.

```sh
cd ~/chromiumos/src/third_party/portage-stable
repo abandon pkg_upgrade .
```

## Appendices

### Re-applying a patch after upgrade

It is not uncommon for a package to be locally patched in the source now
(typically any upstream package in the `src/third_party/chromiumos-overlay`
overlay). To upgrade that package, the patch will (probably) need to be
applied again to the upgraded version. Let's say you upgraded the `foo/bar`
package to upstream version `1.2.3`, but you need to re-apply a patch to it
that was previously applied to version `1.1.1` (perhaps `1.1.1-r1` is the
active version now with the patch).

#### Perform pristine upgrade

Allow the `cros_portage_upgrade` script to complete the pristine upgrade in
`src/third_party/portage-stable` for the package first. Then copy the entire
directory over to the analogous location under
`src/third_party/chromiumos-overlay`, creating the directory if necessary.

#### Mask your pristine upgrade

You will commit the unaltered package as it is in the first changelist, but
first you must mark it as unstable so that it will not be used. To do so,
add a new file like the following to the
`src/third_party/chromiumos-overlay/profiles/default/linux/package.mask/`
directory:

```
# Copyright <copyright_year> The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file

# TODO(username): Masked for testing!
=foo/bar-1.2.3
```

The above masks the specific package version `foo/bar-1.2.3` for all
targets. (If your intent is to patch the ebuild only for certain platforms,
please speak to the build team as this is generally unnecessary and strongly
discouraged.) Now you are ready to submit the pristine copy of the upstream
package to `chromiumos-overlay`.

#### Modify your pristine upgrade

Make a copy of the ebuild file with a revision suffix. For example:

```sh
cp bar-1.2.3.ebuild bar-1.2.3-r1.ebuild
```

Then re-apply the patch to the new revision ebuild, by copying the epatch
line or lines in the previously patched ebuild (`bar-1.1.1-r1.ebuild`). Be
sure to keep whatever patch files under the `files` directory are required
for the patch, too. You might have to rename them (e.g.
`cp files/bar-1.1.1-something.patch files/bar-1.2.3-something.patch`). Be
careful to apply the patch responsibly, because the source file(s) the patch
alters may have changed since the patch was created. If so, you can create
a new patch or consult the original patch author (if not you).

To inspect the source files involved, both before and after patching, use
the `ebuild` utility. For example, to see the source files for
`foo/bar-1.2.3` before any patching run this inside chroot, you might run
one of the following:

```sh
ebuild `equery which foo/bar-1.2.3` unpack
ebuild-amd64-generic `equery-amd64-generic which foo/bar-1.2.3` unpack
ebuild-cherry `equery-cherry which foo/bar-1.2.3` unpack
```

The above will unpack source files and tell you where they are. You
can inspect them to verify whether the source file(s) the patch touches have
changed between versions. To see what the source files look like after
patching, use the same commands as above but replace `unpack` with
`prepare`. Also, make sure your newly patched version is the version being
picked up by the build system by running `equery which` inside chroot:

```sh
equery which foo/bar
equery-amd64-generic which foo/bar
equery-cherry which foo/bar
```

Make sure your new ebuild is the one being picked up. Your upgrade with
patch is now ready for testing. Return to the testing step of the process
above.

Remember to delete the new entry you made to the `package.mask` file.

Don't forget to discard the upgrade in `portage-stable` that
`cros_portage_upgrade` created for you.

### Handling eclasses

Some packages make use of [eclasses], which are essentially ebuild code
factored out into library files shared across multiple packages. If an
ebuild file includes an inherit statement, then it uses an eclass. You
don't have to check for this before you upgrade. The upgrade script will
attempt to detect when an eclass must be upgraded along with a package
upgrade, but if it is unable to,
[then you must upgrade the eclass yourself](#eclass-upgrade).

### Upgrading to unstable version

If you need to upgrade a package to an unstable version for a compelling
reason, then use the `--unstable-ok` option to the `cros_portage_upgrade`
script and it will try to do the right thing. What it does is edit the
`KEYWORDS` line (or lines) in the ebuild to mark all architectures as
stable. This is the only exception to the rule that packages in
`portage-stable` should be unedited. You should inspect the resulting
ebuild file or files to confirm that the edit worked correctly, and file
a bug on `cros_portage_upgrade` if it did not.

## FAQ

### I ran the upgrade script multiple times, and the runtime dropped drastically after the first time. What gives?

The `cros_portage_upgrade` script needs to have a local checkout of
the upstream origin/gentoo source to use as a reference when it runs. By
default, it creates a clone in a temporary directory and then re-uses
it in later runs (updating it each time). This explains the runtime
symptoms. Messages from the script should say something to this
effect, as well.

Alternatively, you can clone your own copy of upstream origin/gentoo to
point `cros_portage_upgrade` to, using the `--upstream` option. This will
have the same runtime benefits if you run more than once. Do this inside
your chroot:

```sh
cd
git clone https://chromium.googlesource.com/chromiumos/overlays/portage gentoo-portage
cd gentoo-portage/
git checkout origin/gentoo
```

Then add `--upstream=$HOME/gentoo-portage .` to your invocation of
`cros_portage_upgrade`.

### Why doesn't the upgrade script just run on all boards/architectures that matter at once? Why do I have to specify them?

The boards that "matter" vary. The same is true, to a lesser extent,
with architectures. You know how you intend to test the package, and you
may have plans to test it on specific boards for specific reasons. Plus,
`cros_portage_upgrade` requires that `setup_board` be completed for all
specified boards because it evaluates packages and their dependencies within
the context of that board, making use of `emerge-<board>` and
`equery-<board>` utilities heavily. If you already have a reasonable
subset of boards setup, or available to test on, then running on those
boards makes sense.

### Why doesn't the upgrade script run setup_board for me as needed?

This script is intended to live within the build environment we have
today. It lives between the `setup_board` and `build_packages` stages, which
all developers are familiar with. The script is not intended to be a master
script with knowledge of build process order.

### Why doesn't the upgrade script create the branch in src/third_party/portage-stable for me?

This was considered, but discarded in favor of transparency. You need
to know that a branch was created because you must amend the commit message,
upload it, and especially abandon the branch in the end. If you created the
branch in the first place you are more likely to be aware that you need to
abandon the branch to return to your previous state.

### Why can't the upgrade script create a complete commit message so no amend is needed, then do the upload and abandon steps as well?

1.  Only you know what tests you will run to verify the upgrade and those
    should be mentioned in the commit message.
1.  There are many scenarios where the upgrade result is not final and
    requires intervention/repetition by you.
1.  Only you know what bug to associate with this upgrade work.

### How do you install a new package that is not currently installed? I get the "The following packages were not found in current overlays (but they do exist upstream): ..." error.

Make sure you pass `--upgrade` to the script since installing a new
package is the same thing as upgrading it from `<none>`.

### How do you manually upgrade an eclass?

Assuming that you have already run `cros_portage_upgrade` and let it
fail, repeat the following procedure for each eclass in need of upgrade:

1.  In your chroot, look under `/tmp/portage/eclass/` for the upstream
    copy.
1.  Copy said upstream version out of `/tmp/portage/eclass/` into
    `portage-stable/eclass/` in your source tree.
1.  Create a CL and pass it through the CQ.

If this procedure appears incomplete, then please reach out to the build
team for additional guidance.

### How do I deal with incompatible features in upstream ebuilds?

Upstream Gentoo sometimes moves faster than ChromiumOS. For example, they may
bump to new Python versions, EAPI versions, etc., that we don't yet support.
This may cause `cros_portage_upgrade` to skip those ebuilds, and eventually
bail with an error, like:

```
Unable to find "foo/bar" upstream on <arch>.
```

You may be able to fix these issues locally, and try again. For example:

```sh
# Modify the local portage cache, downgrading from EAPI 8 to 7.
sed -i '/EAPI=/s:8:7:' /tmp/portage/foo/bar/bar*.ebuild
# Try again without upgrading the portage cache.
cros_portage_upgrade --local-only [args]
```

[portage-stable]: https://chromium.googlesource.com/chromiumos/overlays/portage-stable/
[chromiumos-overlay]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/
[ChromiumOS developer guide]: /developer_guide.md
[contributor's guide]: /contributing.md
[re-applying a patch]: #Re_applying-a-patch-after-upgrade
[upgrading to unstable versions]: #Upgrading-to-unstable-version
[the ChromeOS development mailing list]: /contact.md
[trybot]: https://www.chromium.org/chromium-os/build/local-trybot-documentation
[USE flags]: ebuild_faq.md
[eclasses]: https://wiki.gentoo.org/wiki/Eclass
