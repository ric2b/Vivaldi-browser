---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: archive-mirrors
title: ChromiumOS Archive Mirrors
---

The ChromiumOS project maintains a number of mirrors of archives.
We do this to provide stability to our build system: if an upstream project
deleted or modified a tarball on their website, we don't want that breaking
all of our builders, especially for older branches.
Unfortunately, this happens all the time, so ChromiumOS has to isolate itself.

[TOC]

## Policies

No package may be added to any overlay if the artifacts (tarballs, etc...) are
not cached on one of ChromiumOS's existing mirrors.
In other words, all files must be copied to a Google Storage (GS) bucket that
we maintain.

Which mirror, and whether developers have to copy files manually, is where it
gets a bit tricky.

As a general guideline, ebuilds in public overlays may only use public mirrors
and must never refer to private overlays.
Ebuilds in private overlays may use both public and private mirrors.

Once an ebuild has been merged into the tree, any files it refers to in
`SRC_URI` must never be changed.
Even if you found a bug in the archive and want to fix it, you cannot modify
the file in place.
Instead, you must create a new file using a new name and update the ebuild to
refer to that new artifact.
Any modifications to files that the system is already using will break the
build immediately.

Filenames on mirrors must be globally unique.
That means you must never use generic names like `files.tar.xz`.
Even if the full source URI is unique (e.g. `https://foo/file.tar` and
`https://foo/file.tar`), the full URI is stripped off when mirroring.
See the [Custom Naming] section for more details.

The base part of the name (e.g. `files`) must be fairly unique to the package
or project you're working on, and must include version information at all times.
So `some-project-0.0.1.tar.xz` is OK assuming `some-project` isn't a generic
name all by itself.
See the [Custom Naming] section for more details.

When putting version information into the file, the revision field must never
be included.
See the [Custom Naming] section for more details.

Binaries must not be saved directly in ebuild repos, and rarely should they be
saved directly in other repos (although there are a few exceptions like the
`linux-firmware` repo).
Once a file is added to the git history, it's in there forever, so even if it's
deleted in a subsequent commit, it still lives on in the `.git/` directory.
Over time, these files add up (even if they're "small" <10k) and slow down the
build system for developers and bots.
Also note, just because a file has been encoded in a plain text format (e.g.
hex or base64 encoded) does not make it not a "binary".
It's still a standalone blob that is copied from one place to another.
See the [Custom Artifacts] section for more details.

## Mirror overview

We have a number of possible GS buckets we may utilize depending on where the
artifact is coming from and whether it should be publicly accessible.

### Public mirrors

These mirrors are accessible to the world without authentication.

*   `gs://chromeos-mirror/` (automatic mirrors): This bucket is where we keep
    mirrors of various projects we care about.  They are updated automatically
    so developers do not have to update files themselves.
    *   `gentoo/distfiles/` (Gentoo [distfiles mirrors]): See the
        [Gentoo mirror] section below for more details.
*   `gs://chromeos-localmirror/` (localmirror): This bucket is where developers
    manually copy files for mirroring purposes.
    *   `distfiles/`: Developers manually mirror files here for ebuild usage.
        This is usually for projects or prebuilts not in Gentoo.

### Private mirrors

These mirrors are entirely accessible to Googlers and on a per-file basis to
partners (working on specific projects).

*   `gs://chromeos-localmirror-private/` (localmirror-private): This is the
    internal corollary to `gs://chromeos-localmirror/`.  It has the same
    structure, but holds files that we don't want to share with the world.
    Googlers should not upload files here that come from partners; instead they
    should have the partner use [CPFE] to upload it.
*   `gs://chromeos-binaries/` (BCS/[CPFE]): This holds artifacts that are
    uploaded via [CPFE] (ChromeOS Partner Front End).  It replaces the old BCS
    (Binary Component Server), but is still sometimes colloquially referred to
    as BCS since it achieves the same goal.

### Gentoo mirror

Most ebuilds, especially open source ones, will use this mirror since most
ebuilds come directly from Gentoo.

The `gs://chromeos-mirror/gentoo/` mirror is automatically kept in sync with
Gentoo's [distfiles mirrors].
Developers do not need to upload files here themselves.

If there are files missing from this directory, it implies there's a bug in
our infrastructure pipeline that needs to be fixed (the DupIt builder), so
please [file a bug][bugs] for the Build team to look into.
People should not try to upload files to the localmirror bucket instead to
workaround bugs, or as a replacement for `chromeos-mirror`.

## Other buckets

Only the buckets documented above may be used by ebuilds to fetch archives.
The CrOS project has many other GS buckets, but they're not for ebuilds.

See the [gsutil] documentation for more details.

## Ebuild integration

Our build environment enforces fetching only from CrOS's GS mirrors rather than
pulling from the URI's listed in `SRC_URI`.
The reason for this is covered in the intro of this document.

We force these Portage settings in the make.conf & make.defaults files:

*   `GENTOO_MIRRORS`: Explicitly set to our `chromeos-mirror` and
    `chromeos-localmirror` buckets.
*   `FEATURES=force-mirror`: Ebuilds only fetch from `$GENTOO_MIRRORS`.

### SRC_URI setting

Ebuilds should continue to use normal `SRC_URI` settings and point to the
upstream site rather than change to refer to our mirrors directly.
The only time ebuilds point to our mirrors is when the archives are available
on one of our localmirrors (because we created them).

Each element in `SRC_URI` has two basic forms:

*   `SRC_URI="https://some-site/foo-1.tar.bz2"`: The artifact name is implicitly
    defined by the final component of the path (e.g. `foo-1.tar.bz2`).
*   `SRC_URI="https://some-site/foo -> foo-1.tar.bz2"`: The artifact name is
    explicitly defined by the value after the `->` string.

In both cases, `foo-1.tar.bz2` is the name used on the mirrors and when the file
is saved locally in the distfiles cache.
The fact that the second form uses `https://some-site/foo` and the upstream
remote names it `foo` doesn't matter.

### Updating Manifest files

Every package needs a [Manifest] file which contains metadata (hashes/sizes)
about every file downloaded from the network.
This makes sure that files aren't corrupted (on purpose or by accident).
Portage downloads files to a local directory, and then checks the metadata
every time it unpacks things.

When updating packages from Gentoo (e.g. using `cros_portage_upgrade`), the
[Manifest] file is updated for you automatically.

When updating packages manually, or creating new ones, you're on the hook for
creating/updating the file.
The `ebuild` command is used to manage the file for you.

```sh
$ ebuild <path/to/the/package.ebuild> manifest
>>> Downloading ...many URIs...
...curl/wget/gsutil fetch output...
>>> Creating Manifest for /path/to/the/package
```

The number of `Downloading` lines will vary based on how many files are listed
in the `SRC_URI`, and how many files have already been cached locally.
Don't be surprised if running the `ebuild` command multiple times only shows the
final `Creating Manifest` line.

If the file doesn't yet exist on our mirrors, and it hasn't been download
locally, you'll probably see errors like `!!! Couldn't download 'xxx'.`.
There's a few different ways to address this (any one of the following works).

*   Upload the file to the [mirrors] first, then run the `ebuild` command.
*   Download the file by hand to the local cache, then run the `ebuild` command.
    *   Use `portageq envvar DISTDIR` to find the path to the local cache.
*   Run `FEATURES=-force-mirror ebuild ...` to temporarily disable the mirror
    restriction and fetch the file directly from the upstream project.

Regardless of how the initial [Manifest] is generated, you're responsible for
double checking the integrity of the archive before uploading it to one of the
localmirror buckets (assuming you need to do so).

### Unpacking artifacts

You should never run `tar` or `unzip` yourself in ebuilds to unpack artifacts.
Instead, use Portage's `unpack` helper.
This makes sure the unpacked files have correct ownership and permissions.

By default, all artifacts listed in `SRC_URI` are unpacked during `src_unpack`.
But that's not a hard requirement -- you can `unpack` archives during any source
phase like `src_install`.
This way you could unpack things directly into the `$D` install path.
However, it's pretty rare to use `unpack` outside of the `src_unpack` or
`src_install` steps, so you'll want to double check if you are.

The `unpack` tool will operate on basenames of files or relative paths.
You never want to pass it full paths (like `${DISTDIR}/xxx`).

Here's some common examples.

```sh
# This is the default ebuild behavior.  You don't normally write this unless
# you have extra stuff to do in the unpack phase.
src_unpack() {
    unpack

    ...do more things...
}

# In newer EAPI's, the `default` function will call `unpack` for you.
src_unpack() {
    default

    ...do more things...
}
```

```sh
# If you want to unpack downloaded files by name into different places.
src_unpack() {
    unpack "${P}.tar.gz"
    cd "${S}"
    unpack "${P}-extra.tar.gz"
}
```

```sh
# If you want to unpack nested files.
src_unpack() {
    # This first archive has a "data.tar" inside of it.
    unpack "${P}.tar.gz"
    # Using the ./ prefix is required.
    unpack ./data.tar
}
```

```sh
# If you want to unpack files with different suffixes.
src_unpack() {
    ln -s "${DISTDIR}/${P}.bad.extension" data.tar
    unpack ./data.tar
    rm -f data.tar
}
```

```sh
# If you want to unpack files directly during src_install.
src_unpack() {
    # Disable unpacking since we don't have anything to compile.
    :
}

src_install() {
    # Unpack the files directly into the install path.
    dodir /opt/foo
    cd "${D}/opt/foo" || die
    unpack "${P}.tar.gz"
}
```

### Overlays vs mirrors

If an artifact is only available on an internal mirror (e.g. it's stored on
`chromeos-localmirror-private`), then the ebuild that uses that artifact must
be in a non-public overlay, even if we believe the ebuild itself contains no
secrets.
For example, no ebuild in `chromiumos-overlay` or `src/overlays/` may refer to
`chromeos-localmirror-private`.
This makes it easier to reason about the overall system, and helps prevent
breaking public builds -- adding dependencies on internal packages is much
harder to do, and our public builders can catch that early on.

However, ebuilds in internal overlays may freely refer to both internal and
public mirrors.
This comes up when creating internal board-specific forks of public packages.

### Internal mirrors

You might have noticed that our settings thus far only search public mirrors.
Since portage doesn't have a way of tweaking the mirror lists on a per-overlay
basis, we have to default only to the public mirrors.
That means for internal mirrors, ebuilds have to explicitly list them in their
`SRC_URI` setting and disable the mirror overrides.

For example, if you wanted to use `chromeos-localmirror-private`, you'd write:

```sh
SRC_URI="gs://chromeos-localmirror-private/distfiles/${P}.tar.xz"
...
RESTRICT="mirror"
```

This tells portage to only fetch from the URI's listed in `SRC_URI` and ignore
the default list of mirrors.
This setting is valid *only* when using an internal mirror.
Ebuilds must never use `mirror` while pointing to external sites.

#### CPFE & gs://chromeos-binaries/

Usually ebuilds that use [CPFE] will use the [`cros-cpfe.eclass`][cros-cpfe]
in order to populate `SRC_URI` with the correct settings.
Please see that eclass directly for more details.

## Custom artifact guidelines

If you do have to create your own tarball for ebuilds, please follow these
guidelines to make sure everything works correctly.

### Naming

When uploading a new artifact to the mirror, make sure it has a unique name.
This is usually accomplished using the basename form `<project>-<version>` or
`<project>-<some unique word>-<version>`.

Remember that `<version>` is `0.0.1` and not `0.0.1-r12` (i.e. it must not
include the revision number).
The revision field is reserved for changes in the ebuild itself and not for
changes in the artifacts it uses from `SRC_URI`.
That means you want to stick to `${P}` and `${PV}` in the `SRC_URI`, and never
use `${PF}` or `${PVR}`.
If you want to make a change to the tarballs, then increase `${PV}` in some way
like `0.0.1` -> `0.0.2` or adding a suffix like `0.0.1_p1`.

This name must be unique across all ebuilds and projects, so even if the name
is unique within the scope of one project, it might not be unique enough!
Mirrors merge all the archives from all ebuilds into a single flat directory,
both on remote mirrors, and in local directory caches.

### Archive format and compression

You should create tarballs using xz compression.

Everyone understands tarballs, how to work with them (and is the standard
format in the open source world), and everyone has `tar` installed.
Do not use `zip` or `rar` or `cpio` any other random format unless you have a
very explicit (and good) reason to do so.

We use xz compression because it provides the best compression ratios, and that
matters way more when we store & decompress the archives way more frequently.
Again, do not use any other compression format unless you have a very explicit
(and good) reason to do so.

The default `xz` utility is single threaded and can be quite slow, so make sure
you install a parallel implementation to speed everything up.
We recommend `pixz` as it's in the CrOS SDK and can easily be installed in most
distros (e.g. `sudo apt-get install pixz`).

### Source layout

The input files should always be under a unique directory name and not just in
`./` or similarly generic name.
Otherwise if you unpack multiple archives in a single ebuild, things can easily
collide and require manual code in the ebuild to workaround it.

The directory name itself should match the tarball name.
So if the filename is `some-project-0.0.1.tar.xz`, then all the files in the
tarball should be under the `some-project-0.0.1/` directory.

If you are using an existing tarball from an external website, do not modify the
source layout. If the files are not under a directory named
`some-project-0.0.1/` you can optionally set the [ebuild variable `S`] to
retrieve them from their actual location during the build.

### Creating the tarball

Here is some simple boiler plate commands to do all of this:

```sh
$ rm -rf some-project-0.0.1/
$ mkdir some-project-0.0.1/
$ cp <all the files you want> some-project-0.0.1/
# If you don't have pixz installed, then you can safely drop the -Ipixz option.
$ tar -Ipixz -cf some-project-0.0.1.tar.xz some-project-0.0.1/
```

Now the `some-project-0.0.1.tar.xz` file is reading for upload.

## Updating localmirror & localmirror-private

*** note
As a reminder, once a file has been uploaded to either of these GS mirrors and
an ebuild starts referring to it, you must never change it.
If you do change the file in place, you will break the builders immediately.
***

*** note
The `chromeos-localmirror-private` bucket is only for internal binaries.
*Never* mark a binary here as "Share publicly" or using "public-read" ACLs.
***

*** note
The localmirror buckets are not a replacement for `chromeos-mirror`.
If a package is in upstream Gentoo, but the file isn't automatically mirrored
on the `chromeos-mirror`, then please [file a bug][bugs] for the Build team.
***

You should have the artifact ready to upload at this point.
If you still need to create the artifact yourself, see the [Custom Artifacts]
section for more details first.

### Getting files onto localmirror

You can upload files using GS's web interface, or via the command line using
the [gsutil] tool.

In both cases, you'll need to authenticate using your `@google.com` account.

#### Web interface

To upload a file and make it immediately available, go to:
https://pantheon.corp.google.com/storage/browser/chromeos-localmirror/distfiles/

You should be looking at the main mirror directory at this point.
You can use the `Upload files` button, or drag & drop it from a local folder.

Finally, make it publicly visible by doing:

*   Click the 3 dots menu on the file.
*   Select `Edit access`.
*   Click `Add item`.
*   Use the settings:
    *   Entity: `Public`
    *   Name: `allUsers`
    *   Access: `Reader`

#### Command line interface

If you're comfortable using [gsutil], this step is a bit easier.
However, it can be easy to typo the command, so double check things before
actually uploading files.

```sh
$ gsutil cp -n -a public-read <local filename> gs://chromeos-localmirror/distfiles/<remote filename>
```

If you've already uploaded the file and forgot to set the ACLs, you can recover
with the `acl` subcommand.

```sh
$ gsutil acl set public-read gs://chromeos-localmirror/distfiles/<remote filename>
```

#### Verify usability

Once the file is uploaded, you can try downloading it anonymously:

```sh
$ wget http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/<filename>
```

### Getting files onto localmirror-private

The flow for `localmirror-private` is the same as `localmirror`, except we don't
use public ACLs.

#### Web interface

To upload a file and make it immediately available, go to:
https://pantheon.corp.google.com/storage/browser/chromeos-localmirror-private/distfiles/

You should be looking at the main mirror directory at this point.
You can use the `Upload files` button, or drag & drop it from a local folder.

The default ACLs are fine, so don't try to change them.

#### Command line interface

If you're comfortable using [gsutil], this step is a bit easier.
However, it can be easy to typo the command, so double check things before
actually uploading files.

Remember: only use `project-private` ACLs here and never `public-read`.

```sh
$ gsutil cp -n -a project-private <local filename> gs://chromeos-localmirror-private/distfiles/<remote filename>
```

If you've already uploaded the file and forgot to set the ACLs, you can recover
with the `acl` subcommand.

```sh
$ gsutil acl set project-private gs://chromeos-localmirror-private/distfiles/<remote filename>
```

#### Verify usability

Once the file is uploaded, you'll need to use [gsutil] to try downloading it.

```sh
$ gsutil cp gs://chromeos-localmirror-private/distfiles/<filename> ./
```

### Subdirectories

It's extremely rare for people to want to use subdirectories for files,
especially under the `distfiles/` path.

If you're worried about file name conflicts, using a subdirectory won't help.
See the [Custom Naming] section for details.

If you really feel like using a subdirectory makes sense, and you aren't going
to use the files in ebuilds, then post your query on one of the
[development mailing lists][Contact].

### Security for archive mirror GS buckets

Googlers have write permissions to the GS buckets that act as our archive
mirrors and can write files to paths that do not already exist, but cannot
delete or overwrite files. This is implemented by granting the `Storage Object
Creator` and `Storage Object Viewer` roles, which allow
`storage.objects.create` but not `storage.objects.delete` privileges.

Even if a file were to be overwritten with different (possibly malicious)
content despite these permissions, ChromiumOS builds would not use the new
content. Every package has a [Manifest] file which is committed (after review)
to git, and those files keep track of hashes and file sizes.

If mirrored content is replaced with new content, our builders would
automatically detect and reject those attempts because the hashes and file sizes
would not match the manifest. This is true whether the change is inadvertent or
malicious.

We also enable [Object Versioning] in these buckets in case someone accidentally
overwrites files, so we can recover the old version easily.

## FAQ

### As a non-Googler (e.g. partner), how do I get files onto the mirrors?

If you're working on a package from Gentoo, then the file should already be on
our [Gentoo mirror] already.
See that section for more details.

If you're working on a custom package (e.g. a new package on GitHub), then
you'll need to coordinate with a Googler to get all necessary files uploaded.
Just point them to this document :).

Usually the Googler reviewing your CL on Gerrit would be the best contact.

### Can I upload files to chromeos-localmirror-private partners sent me?

Please do not upload files directly a partner has e-mailed or shared privately
with you (e.g. via Google Drive), especially for firmware blobs.
The only time you may do this is if it's extremely clear what license it uses,
and that license freely allows for redistribution.

Instead, ask the partner to use [CPFE] to share the file with us.
This takes care of a lot of the messy licensing issues.

Even better would be to have the partner share the file publicly using a good
open source license (e.g. a [BSD license]).
If the file is a firmware blob that has corresponding Linux device drivers, then
the upstream [linux-firmware] repository is a great place to host things.

If you're still unsure, feel free to reach out to one of the
[development lists][Contact] for advice.

### Isn't it a security risk for Googlers to have write access to GS buckets?

In short, no.

These buckets used to be writable by all Googlers, so it was previously possible
for an insider to attempt to replace content in these mirrors with malicious
code, though security features documented in the [Security for archive mirror GS
buckets] section would have mitigated any impact. We've now eliminated the
ability of Googlers to delete or overwrite content in the mirror buckets, so
this remains a non-issue.


[BSD license]: https://en.wikipedia.org/wiki/BSD_licenses
[bugs]: reporting_bugs.md
[Contact]: contact.md
[CPFE]: https://goto.google.com/cpfe
[cros-cpfe]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/eclass/cros-cpfe.eclass
[Custom Artifacts]: #custom-artifacts
[Custom Naming]: #custom-naming
[distfiles mirrors]: https://gentoo.org/downloads/mirrors/
[ebuild variable `S`]: https://devmanual.gentoo.org/ebuild-writing/variables/index.html#ebuild-defined-variables
[Gentoo mirror]: #gentoo-mirror
[gsutil]: gsutil.md
[linux-firmware]: https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/
[Manifest]: https://devmanual.gentoo.org/general-concepts/manifest/index.html
[mirrors]: #mirrors
[Object Versioning]: https://cloud.google.com/storage/docs/object-versioning
[Security for archive mirror GS buckets]: #Security-for-archive-mirror-GS-buckets
