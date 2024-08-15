---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: coreboot-upstream
title: Working with Coreboot upstream and Chromium
---

[TOC]

## Introduction

ChromeOS uses and actively contributes to [coreboot.org]. Development happens
in the chromiumos copy of the coreboot repo, and the resulting patch must be
pushed to upstream coreboot. While this can be done in one local git checkout,
many developers find it easier to use two coreboot git checkouts:

1. regular chromiumos checkout
2. coreboot.org checkout

This document describes how to set up a second coreboot.org upstream checkout
and how to synchronize patches between the two checkouts.

## Setting up a second coreboot checkout

The most familiar local coreboot checkout is the one from chromiumos. It lives
under `src/third_party/coreboot` in the chromiumos workspace. If you followed
the [ChromeOS Developer guide], it lives at the full path of
`~/chromiumos/src/third_party/coreboot`. The following steps will add an
additional coreboot.org upstream checkout at a secondary location on your
machine: `~/devel/coreboot`.

1. Create an account on [review.coreboot.org][Gerrit account page]. Sign in with
Google and fill in a username in the settings.
1. [Generate an SSH key][Gerrit's documentation] and copy the _public_ key into
   the Gerrit settings form:
   ```
   ssh-keygen -t rsa
   cat ~/.ssh/id_rsa.pub
   ```
1. Add the following entry to your `.ssh/config` for SSH access to Gerrit:

   ```
   Host review.coreboot.org
       Port 29418
       User <username you configured on Gerrit>
   ```
1. Check out upstream Coreboot from coreboot.org:

   ```bash
   $ mkdir -p ~/devel
   $ cd ~/devel
   $ git clone ssh://review.coreboot.org/coreboot.git
   ```

   Now the local checkout is tracking `origin/main` on the local `main`
   branch.

1. Assuming there is a chromiumos coreboot checkout at
   `~/chromiumos/src/third_party/coreboot`, you can link the two repositories
   using git remotes that are local to the system.

   ```bash
   $ cd ~/devel/coreboot
   $ git remote add cros-coreboot ~/chromiumos/src/third_party/coreboot
   $ git fetch cros-coreboot

   $ cd ~/chromiumos/src/third_party/coreboot
   $ git remote add upstream_local ~/devel/coreboot
   $ git fetch upstream_local
   ```

   The two repositories now have remotes that track one another. This serves as
   the basis for cherry-picking patches back and forth or rebasing commits from
   one repository to the other.

## Developing with two coreboot checkouts

Developers should initially work in the chromiumos tree since ChromeOS can
build images to test by flashing and booting on a machine.  When a commit is
ready in the chromiumos tree, it's time to push to review.coreboot.org.  Suppose
that the developer committed changes in the chromiumos Coreboot repository after
running `repo start feature1`. A fetch will bring these changes into
`~/devel/coreboot` in the `cros-coreboot/feature1` branch:

```bash
$ cd ~/devel/coreboot
$ git fetch cros-coreboot
remote: Counting objects: 4204, done.
remote: Compressing objects: 100% (1229/1229), done.
remote: Total 2495 (delta 2069), reused 1576 (delta 1245)
Receiving objects: 100% (2495/2495), 456.72 KiB | 0 bytes/s, done.
Resolving deltas: 100% (2069/2069), completed with 519 local objects.
From ${HOME}/chromiumos/src/third_party/coreboot
 * [new branch]      feature1   -> cros-coreboot/feature1
```

Now, the developer can cherry-pick or rebase the chromiumos patches into a
branch that is tracking coreboot upstream.

```bash
# To cherry-pick 1 commit
$ git cherry-pick cros-coreboot/feature1
# To cherry-pick a branch with N commits
$ git cherry-pick cros-coreboot/feature1~N..cros-coreboot/feature1
```

The commit message should follow the [ChromiumOS Contributing Guide],
including a Signed-Off-By line which can be easily added using `git commit -s`.
The prefix for the title should be an abbreviated path to the edited file. Use
`git log` to check for an example.

The developer can then push the patch to review.coreboot.org.

```bash
$ git push origin HEAD:refs/for/main
```
[coreboot.org]: https://coreboot.org
[ChromeOS Developer guide]: ./../developer_guide.md
[Gerrit account page]: https://review.coreboot.org/#/settings/ssh-keys
[Gerrit's documentation]: https://gerrit-review.googlesource.com/Documentation/user-upload.html#ssh
[ChromiumOS Contributing Guide]: ./../contributing.md#commit-messages
