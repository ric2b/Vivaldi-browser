---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: recreating-a-snapshot-or-buildspec
title: Recreating a Snapshot or Buildspec
---


This document describes how a developer can get their chromiumos repo into the
state described by a snapshot or buildspec.

[TOC]

## What are snapshots and buildspecs?

Snapshots and buildspecs are both special manifests in which every project is
pinned to an explicit git revision. They allow exact recreations of the state of
a builder, but only if you know how to do the recreation. This document
describes the steps needed to do that recreation.

The difference is where they come from. Snapshots are created periodically by
annealing. Buildspecs are the definition of a particular ChromeOS version, and
are created when a new version is made.

NOTE: Googlers can read more about snapshots and buildspecs at
[go/chromeos-manifests#buildspecs](http://go/chromeos-manifests#buildspecs).

Often, a build breakage or failing test can be traced to a particular ChromeOS
snapshot or version. For instance, "This test passed in snapshot 54673 but
started failing in snapshot 54674."

If you cannot determine the root cause, you may need to recreate the successful
build (snapshot 54673 in this example) and then apply individual changes to
the successful build until the problem appears.

## Recreating a snapshot buildspec

### Ensure you have no local modifications

Ensure there are no branches checked out:
``` sh
$ repo branch
```
The output should not have any branches with a "*" in the first column. If there
are, go into each directory and run
``` sh
$ git checkout cros/main
```
(or cros-internal/main for internal repos)


Check for changes that haven't been added to git:
``` sh
$ repo diff
```
Make sure the output is empty.

Depending on exactly what you need to accomplish, you may also wish to make
sure you are not `cros workon`ing anything:
``` sh
$ cros workon --board=${BOARD} list
# for each package listed:
$ cros workon --board=${BOARD} stop ${PACKAGE}
```
(This is especially important if you are trying to recreate a bug in
build-packages' dependency calculations; `cros workon start` ensures a package
will always be emerged even if its dependencies didn't change.)

Next, you will need to know if you looking for a snapshot or a buildspec.

### If you want a snapshot
#### Find the snapshot you are looking for

Snapshots are found by going to the snapshot branch:
https://chromium.googlesource.com/chromiumos/manifest/+/refs/heads/snapshot for
public and
https://chrome-internal.googlesource.com/chromeos/manifest-internal/+/refs/heads/snapshot
for internal. Each commit has the snapshot number at the top of the commit
message.

Find the commit hash for that commit. (Hint: hit the \[log\] link to bring up a
list of all the snapshots in that branch.)

For example, for public snapshot 54673, you would look at
https://chromium.googlesource.com/chromiumos/manifest/+/9619dc893f4dfdc27a936c79cd1f891ebef65d75
and see the commit hash was 9619dc893f4dfdc27a936c79cd1f891ebef65d75.

#### Switch repo to the snapshot

From the root chromiumos directory (normally ~/chromiumos), run
``` sh
$ repo init -u https://chromium.googlesource.com/chromiumos/manifest -b <commit hash>
```
for a public snapshot/buildspec or
``` sh
$ repo init -u https://chrome-internal.googlesource.com/chromeos/manifest-internal  -b <commit hash>
```
for a private snapshot/buildspec.

Regardless of which variant of `repo init` you used, immediately run
``` sh
$ repo sync -j <number>
```

After this completes, all the git logs in all the git repos should show that the
latest commit is tagged "m/\<manifest commit hash\>"; for instance, after going
to public snapshot 54673 (with a manifest commit hash of
9619dc893f4dfdc27a936c79cd1f891ebef65d75), `git log` in src/platform2 will show:

```
commit 4bce3873f6a8bc88df4182ae642442b4e39a51f4 (HEAD, m/9619dc893f4dfdc27a936c79cd1f891ebef65d75)
Author: Betul Soysal <betuls@google.com>
Date:   Wed Aug 16 01:22:50 2023 +0000
```

### If you want a buildspec
#### Find the buildspec you are looking for

To re-initialize the repo to a buildspec, you will need the "gsutil URI" (a
URI starting with `gs://`) that leads to the buildspec XML file.

##### For everyone

If you already know the milestone and version, you should be able to just
construct the URI manually as
* `gs://chromiumos-manifest-versions/buildspecs/<milestone>/<version>.xml` for
  the public manifest and
* `gs://chromeos-manifest-versions/buildspecs/<milestone>/<version>.xml` for
  the internal manifest. (Of course, only Googlers will be able to access this
  URI.)

For example, version 15550.0.0 was in milestone 117, so the gsutil URI for
the public buildspec is
`gs://chromiumos-manifest-versions/buildspecs/117/15550.0.0.xml`.

If you are building a release buildspec, pass `--no-usepkg` to `cros
build-packages` to build from source.

##### For Googlers

If you want to browse the list of all the available buildspecs, Googlers can
refer to http://go/chromeos-manifests#buildspec-storage for the location of the
internal websites that list them. Find the XML file corresponding to the version
you want to recreate then click on the ⋮ (three vertical dots) menu on the
righthand side and select "Copy gsutil URI".

Alternatively, you can use the `gsutil` tool with the above URIs.

##### For Non-Googlers

It is only possible to browse the buildspecs for the public manifest using the
`gsutil.py` tool that comes with depot_tools; there is no browser-based access.

If you are external to Google and you need to browse the list of buildspecs,
start with
``` sh
$ gsutil.py ls gs://chromiumos-manifest-versions
```
and work your way downwards until you find the XML file you want.

#### Switch repo to the buildspec

From the root chromiumos directory (normally ~/chromiumos), run
``` sh
$ repo init --standalone-manifest <gsutil URI>
```

For example, for 15550.0.0 in the public manifest, this would look like
``` sh
$ repo init --standalone-manifest gs://chromiumos-manifest-versions/buildspecs/117/15550.0.0.xml
```

Then run
``` sh
$ repo sync -j <number>
```

## Getting back to normal

Once you are done with your investigations, run
``` sh
$ repo init -u https://chromium.googlesource.com/chromiumos/manifest -b HEAD
```
if you are external or
``` sh
$ repo init -u https://chrome-internal.googlesource.com/chromeos/manifest-internal -b HEAD
```
if you are a Googler. Once that completes, run
``` sh
$ repo sync -j <number>
```

`git log` in one of the git repositories should now show the latest commit
tagged with "m/main" and "cros/main".

Remember to `cros workon` any projects you stopped working on above.
