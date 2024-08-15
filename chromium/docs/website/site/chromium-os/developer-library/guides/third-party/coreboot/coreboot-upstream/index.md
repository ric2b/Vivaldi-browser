--------------------------------------------------------------------------------

breadcrumbs: - - /chromium-os/developer-library/guides - ChromiumOS > Developer
Library > Guides page_name: coreboot-upstream

## title: Working with coreboot upstream and Chromium

[TOC]

## Introduction

coreboot development happens in the chromiumos copy of the coreboot repo, and
the resulting patch must be pushed to upstream coreboot. While this can be done
in one local git checkout, many developers find it easier to use two coreboot
git checkouts:

1.  Regular chromiumos checkout
2.  coreboot.org checkout

This document describes how to set up a coreboot.org upstream checkout both in
tree and as a separate checkout, and how to synchronize patches between the two
checkouts.

## Working with coreboot and Chromium

The most familiar local coreboot checkout is the one from chromiumos. It lives
under `src/third_party/coreboot` in the chromiumos workspace. If you followed
the [ChromeOS Developer guide], it lives at the full path of
`~/chromiumos/src/third_party/coreboot`.

### Developing with coreboot

#### Creating an account with coreboot

1.  Create an account on [review.coreboot.org][Gerrit account page]. Sign in
    with Google and fill in a username in the settings.
1.  [Generate an SSH key][Gerrit's documentation] and copy the *public* key into
    the Gerrit settings form: `ssh-keygen -t ed25519 cat ~/.ssh/id_ed25519.pub`
1.  Add the following entry to your `.ssh/config` for SSH access to Gerrit:

    ```
    Host review.coreboot.org
        Port 29418
        User <username you configured on Gerrit>
    ```

#### Commit messages

The commit message should follow the [ChromiumOS Contributing Guide], including
a Signed-Off-By line which can be easily added using `git commit -s`. The prefix
for the title should be an abbreviated path to the edited file. Use `git log` to
check for an example.

##### Dependencies

CQ-Depend entries can be added to a commit message upstream. The CQ-Depend
statement will be ignored upstream, but will take effect in the chromium gerrit.
See [CL dependencies].

*   If you add a CQ-Depend to a CL, please notify the current coreboot
    downstreamer and/or request their attention by adding
    coreboot-downstreamer@google.com as a reviewer to related CL's.

##### Bugs

coreboot has its own [issue tracking system] that can be referenced. However,
development intended for ChromeOS should still utilize the same BUG field used
in the Chromium Gerrit. Just like the dependencies, the bug field will be
ignored by the coreboot Gerrit, but will apply downstream in the Chromium repos.

*   Don't use Chromium issues to hide more information than is strictly
    necessary (e.g. NDA'd details). The purpose of a change must be
    understandable without access to Google's issue tracker.

#### Pushing code for review upstream

Once the patch has been tested up and downstream and the appropriate commit
message has been filled out, the developer can push the patch to
review.coreboot.org.

```bash
$ git push origin HEAD:refs/for/main
```

### Working with coreboot out of tree(Preferred)

#### Creating a separate upstream coreboot checkout

Check out upstream coreboot from coreboot.org:

```bash
   $ mkdir -p ~/devel
   $ cd ~/devel
   $ git clone ssh://review.coreboot.org/coreboot.git
```

Now the local checkout is tracking `origin/main` on the local `main` branch.

#### Building coreboot in an upstream checkout

1.  Navigate to the top level directory of the upstream checkout `bash $ cd
    ~/devel/coreboot`
1.  Run the abuild command for the desired board (util/abuild/abuild -x -t
    {VENDOR}_{BOARD} --clean) `bash $ util/abuild/abuild -x -t GOOGLE_SKYRIM
    --clean`
    1.  Omit `--clean` to preserve build artifacts/logs
    1.  Output can be found at
        ~/devel/coreboot/coreboot-builds/{VENDOR}_{BOARD} #### Setting up a
        second coreboot checkout to work with chromium Assuming:
1.  there is a chromiumos coreboot checkout at
    `~/chromiumos/src/third_party/coreboot`
1.  there is a coreboot upstream checkout at `~/devel/coreboot`

you can link the two repositories using git remotes that are local to the
system.

```bash
$ cd ~/devel/coreboot
$ git remote add cros-coreboot ~/chromiumos/src/third_party/coreboot
$ git fetch cros-coreboot

$ cd ~/chromiumos/src/third_party/coreboot
$ git remote add upstream_local ~/devel/coreboot
$ git fetch upstream_local
```

The two repositories now have remotes that track one another. This serves as the
basis for cherry-picking patches back and forth or rebasing commits from one
repository to the other.

#### Developing with two coreboot checkouts

Developers should initially work in the chromiumos tree since ChromeOS can build
images to test by flashing and booting on a machine. When a commit is ready in
the chromiumos tree, it's time to push to review.coreboot.org. Suppose that the
developer committed changes in the chromiumos coreboot repository after running
`repo start feature1`. A fetch will bring these changes into `~/devel/coreboot`
in the `cros-coreboot/feature1` branch: 1. Ensure you are in the working
directory: `bash $ cd ~/devel/coreboot` 1. Fetch the repo 1. If the repositories
are linked: `bash $ git fetch cros-coreboot` 1. If the repositories are not
linked: `bash $ git fetch ~/devel/coreboot` 1. Expected output `bash remote:
Counting objects: 4204, done. remote: Compressing objects: 100% (1229/1229),
done. remote: Total 2495 (delta 2069), reused 1576 (delta 1245) Receiving
objects: 100% (2495/2495), 456.72 KiB | 0 bytes/s, done. Resolving deltas: 100%
(2069/2069), completed with 519 local objects. From
${HOME}/chromiumos/src/third_party/coreboot * [new branch] feature1 ->
cros-coreboot/feature1`

Now, the developer can cherry-pick or rebase the chromiumos patches into a
branch that is tracking coreboot upstream.

```bash
# To cherry-pick 1 commit
$ git cherry-pick cros-coreboot/feature1
# To cherry-pick a branch with N commits
$ git cherry-pick cros-coreboot/feature1~N..cros-coreboot/feature1
```

### Working with coreboot in tree

#### Checking out coreboot in tree

```bash
$ cd ~/trunk/src/third_party/coreboot/
$ git remote add origin https://review.coreboot.org/coreboot.git
$ git remote update
$ git checkout origin/main
```

#### Building coreboot in tree

1.  Setup the build system for the board you want to use: `bash $ ./setup_board
    --board=$board`

1.  Tell the build system to use the most recent coreboot tree: `bash $
    cros_workon-$board start coreboot coreboot-utils libpayload`

1.  Build the firmware: `bash $ emerge-$board coreboot coreboot-utils libpayload
    chromeos-bootimage`

    1.  Prepend `FEATURES="keepwork"` to the emerge command to preserve build
        artifacts
    1.  Output can be found at /build/{BOARD}/firmware/
    1.  Output images have the format image-{BOARD}.bin. Images are:
        1.  image-{BOARD}.bin - Normal bootable image
        1.  image-{BOARD}.serial.bin - Normal bootable image with serial console
        1.  image-{BOARD}.net.bin - Debug build. Only applicable on Intel
            platforms

[ChromeOS Developer guide]: https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide
[ChromiumOS Contributing Guide]: https://www.chromium.org/chromium-os/developer-library/guides/development/contributing
[CL dependencies]: https://www.chromium.org/chromium-os/developer-library/guides/development/contributing/#cl-dependencies
[coreboot.org]: https://coreboot.org
[Gerrit account page]: https://review.coreboot.org/#/settings/ssh-keys
[Gerrit's documentation]: https://gerrit-review.googlesource.com/Documentation/user-upload.html#ssh
[issue tracking system]: https://ticket.coreboot.org/
