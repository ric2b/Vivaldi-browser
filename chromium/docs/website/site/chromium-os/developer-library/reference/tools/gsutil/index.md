---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: cros-commit-pipeline
title: gsuti
---

`gsutil` is a command line tool for working with Google Storage (GS) buckets.
We use it heavily in ChromiumOS to mirror our files around the world.

See the official [gsutil Tool] documentation for more details.

This document is a practical guide for our developers working with GS.

[TOC]

## Installing gsutil

The easiest way to run `gsutil` on your workstation is to use the copy that is
part of the CrOS checkout in [chromite].
Simply run `~/chromiumos/chromite/scripts/gsutil` to get an up-to-date version.
You can symlink that into your personal `$PATH` to simplify things.

Otherwise, feel free to follow the official [Install gsutil] documentation.

### Configure authentication (.boto)

> NB: If you previously used `gsutil config` to create your ~/.boto, those
> credentials should remain valid, so you can copy them around from existing
> installs.  However, Google has turned down support for using `gsutil` to
> acquire new credentials.

If you don't have a `~/.boto` file set up yet for your account, use `gcloud`
from the Google Cloud SDK.

For Googlers, you can install it with `sudo apt-get install google-cloud-sdk`.
For everyone else, please follow the
[Installation instructions](https://cloud.google.com/sdk/docs/install#installation_instructions).

Use the `auth login` subcommand to obtain credentials.  It will print out
directions for you to follow.

```bash
(outside) $ gcloud auth login
```

When it finishes, credentials are copied into a subdirectory that you will need
to copy out of.

```bash
# NB: If this fails, replace "*" with your e-mail address.
(outside) $ cp ~/.config/gcloud/legacy_credentials/*/.boto ~/.boto
```

> **Note**: The gcloud tool suggests to set `PROJECT_ID`, but for our purposes,
> there's no need to set it to anything.

## Various GS buckets

There are a variety of GS buckets that show up in CrOS.
We try to cover everything that CrOS developers might see.

As a general rule, buckets that start with `gs://chromeos-` are not browseable
by the public, although some files might be readable for tools to access.

Buckets that start with `gs://chromiumos-` are fully readable by the public.
Do not host any internal files on such buckets.

Note: We omit buckets that are already documented in [Archive Mirrors].

### Build/Release buckets

These buckets are used by our builders and release process.

`cros flash` and `cros chrome-sdk` will pull artifacts from these buckets.

*   `gs://chromeos-image-archive/` (all internal unsigned artifacts): All CrOS
    builders write their outputs/artifacts to this bucket.  No signed images
    are stored here.
    *   `$BOT_CONFIG_NAME/$CROS_VERSION/`: Each bot config gets a unique
        directory, and each build has a unique version.
*   `gs://chromiumos-image-archive/` (all public unsigned artifacts): Like the
    `chromeos-image-archive` bucket above, but for public builders.
*   `gs://chromeos-releases/` (only signed artifacts): After CrOS builders
    upload to `gs://chromeos-image-archive/`, they copy some of the images to
    this bucket for signing.  The signer then downloads those, signs them, and
    then uploads new (now signed) files.
    *   `$CHANNEL-channel/$BOARD/$CROS_VERSION/`: Each channel is independent,
        as is the board and version.
*   `gs://chromeos-releases-public/`: Used for hosting OS updates.  Only used
    by release engineers.

### Public artifacts

These buckets are automatically updated by builders for public consumption.

*   `gs://chromiumos-sdk/`: The ChromeOS SDK itself and standalone toolchains.
    The SDK tarballs live in the top directory while toolchains live in dated
    subdirectories.
*   `gs://chromeos-dev-installer/`: Prebuilts for the `dev_install` command
    for quickly installing developer packages on release images in dev mode.
*   `gs://chromeos-prebuilt/`: Various prebuilt (e.g. binpkgs) for quickly
    building & updating boards and the SDK.

### Testing buckets

These aren't used for anything other than testing.
Code should not rely upon these for anything other than testing.

*   `gs://chromeos-releases-test/`: Used for release/paygen/signing network
    based testing.  Content is not guaranteed to have a long life.  Attempts to
    get official/mp key signed artifacts here will be rejected.
*   `gs://chromeos-throw-away-bucket/`: Writable by anyone in Google.  Useful
    for network based unittests without worrying about breaking things.  Content
    is not guaranteed to have a long life.

### Android (ARC++) buckets

These are used by developers working on ARC++ (Android in CrOS).

*   `gs://android-build-chromeos/`: Candidate prebuilt Android images generated
    continuously by the Android infrastructure.  The CrOS infrastructure, after
    verifying them via the Android PFQ (and `cros_mark_android_as_stable`), will
    copy the files over to `gs://chromeos-arc-images/` for releases.
    Note: Files in here have a limited lifespan and cannot be relied upon for
    long term storage.
*   `gs://chromeos-arc-images/`: Prebuilt Android images referenced by official
    ARC++ ebuilds for use in CrOS releases.

### VM/container (Crostini) buckets

These buckets are used for [Containers/VM/Crostini](./containers_and_vms.md).

*   `gs://chrome-component-termina/`: Termina VM images for signing. VM images
    uploaded here will be signed, then made available in the Omaha dashboard
    for release.
*   `gs://termina-component-testing/`: Termina VM images for testing. This
    bucket contains both the live and staging VM images.
*   `gs://cros-containers/`: LXD container images.
*   `gs://cros-containers-staging/`: Staging LXD container images. Used only
    in testing.
*   `gs://cros-packages/`: apt package repo for container guests.
*   `gs://cros-packages-staging/`: Staging apt package repo for container
    guests. Used only in testing.

### ClusterFuzz buckets

See the [fuzzing documentation](testing/fuzzing.md#using-clusterfuzz) for details
on the buckets used by ClusterFuzz.

### Old/dead buckets

These buckets are no longer used by current systems.

*   `gs://chromeos-bcs/`: Precursor to `gs://chromeos-binaries/` when BCS
    (Binary Component Server) was a ssh/ftp box rather than GS bucket.  Used by
    developers and partners to host random (usually firmware) files.

## googlestorage_acl.txt file format

The `googlestorage_acl.txt` file is used to set permissions for files associated
with a particular overlay/board/project.
It is a series of arguments that are passed to the `gsutil acl ch` command.
Comments start with a `#` character and extend to the end of the line.

```
# For information about this file, see
# https://chromium.googlesource.com/chromiumos/docs/+/HEAD/gsutil.md#googlestorage_acl.txt

# Give owners of the chromeos.int.bot@gmail.com project full control.
-g 00b4903a97fb6344be6306829e053825e18a04ab0cc5513e9585a2b8c9634c80:FULL_CONTROL

# Give viewers of the chromeos.int.bot@gmail.com project read access.
-g 00b4903a97ce95daf4ef249a9c21dddd83fdfb7126720b7c3440483b6229a03c:READ

# Give all Googlers read access.
-g google.com:READ
```

## FAQ

### How do you set up prebuilts for a new private overlay?

Add a [`googlestorage_acl.txt`](#googlestorage_acl.txt) file in the root of the
private overlay.

### How do I create a new GS bucket?

Creating a new GS bucket is not normally something we do.
You should double check to see if any of the existing buckets satisfy your use
case, and if not, be prepared to provide extensive justification.

If you're confident you need a new bucket, please [file a bug][bugs] using the
`Infra>Client>ChromeOS` component.


[Archive Mirrors]: archive_mirrors.md
[bugs]: reporting_bugs.md
[chromite]: https://chromium.googlesource.com/chromiumos/chromite/
[gsutil Tool]: https://cloud.google.com/storage/docs/gsutil
[Install gsutil]: https://cloud.google.com/storage/docs/gsutil_install
