---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: work-on-branch
title: ChromiumOS Guide to Working on Branches
---

This guide covers tips to work with branches. You may need to work on release
branches for ChromeOS versions e.g. `release-R76-12239.B`, or factory and
stabilize branches that serve a project-specific purpose
e.g. `stabilize-nocturne-10986.B`.

*** note
This applies to commits in ChromiumOS repositories. For changes to Chromium
repository branches, see the information about [Drover][drover];
for Blink, see [experimental branches][experimental-branches].
***

[TOC]

## Process: When to merge, How to get approval

Check out the Chromium [merge request] overview for details on branch
life-cycle, when to request merges and how to get approval for merging to
release branches. For project-specific factory and stabilize branches, a similar
process applies, except the approval comes from the kernel/firmware lead or
SIE on the project, rather than the ChromeOS TPM managing the release branch.

## Merge via Gerrit UI

The Gerrit UI contains a 'Cherry Pick' feature, you may need to click on the
&vellip; menu in the top-right to find it. This is currently the easiest way of
merging a change that does not have conflicts with the target branch. If the
change you want to merge to a branch is already on Gerrit, you need only a
browser.

1.   Open up the CL in question on Gerrit.
1.   Select "Cherry Pick" from the &vellip; menu in the top right.
1.   Enter the branch name, for instance **release-R58-9334.B**. The UI has
     an autocomplete feature for branch names to help you with this.
1.   Optionally you can edit the commit message, or specify a target git SHA
     to rebase your cherry-pick on top of (if you want to apply the change on
     top of a commit that is not the head of your target branch). Reasons for
     editing the commit message could be changing `Cq-Depend` directives as
     appropriate, explaining why the change is needed in the target branch
     or explaining any modifications or conflict-resolutions made while
     cherry-picking.
1.   Click "Cherry Pick" at the bottom of the pop up.
1.   This should create a new CL against that branch in Gerrit. To land the CL,
     mark it ready as usual. See the [contributing] guide for what votes you
     need to mark your CL as ready.

Note this only works if the patch applies cleanly on the target branch.
Otherwise, if there is a conflict then the merge must be done manually using one
of the methods below, or resolved via editing files directly in Gerrit.

To resolve simple conflicts manually in Gerrit:
1.   Cherry-picking via the UI will be interrupted before actually creating the
     cherry-picked CL.
1.   The UI will offer to create the cherry-picked CL (with conflicts) anyway.
     Create it.
1.   In the cherry-picked CL, click the "Edit" button to directly edit files
     that have conflict resolution markers in them (i.e. <<<<<<<), resolving all
     conflicts.

*** promo
Note: When to update ebuilds while merging changes

This works the same way on other branches as it does on the master branch:
If you're merging a change to code that is built as part of a `cros_workon`
package, or to a `.ebuild` file for a `cros_workon` package, the package will be
uprev-ed automatically.

If you're changing a non-`cros_workon` package, you must uprev the corresponding
`.ebuild` file on the branch manually, just as you do when making changes to
non-`cros_workon` packages on the master (or main) branch.
***

## Use cros\_merge\_to\_branch tool from your chroot

The second easiest way to create a change from a change you already committed on
top-of-tree (ToT) in ChromeOS is using `cros_merge_to_branch`.

Example usage:

```
cros_merge_to_branch 1376991 release-R76-12239.B
```

This creates Gerrit changes for R76 from CL 1376991 in less than 10 seconds.
After running, you can check Gerrit to actually commit the changes (search your
open CLs for R76-\* branch). You can also run these changes through a
[try-job][using-remote-tryjobs] (make sure to specify the branch with -b with
the remote tryjob).
For more advanced usage information, use `--help`, or ping
[chromeos-chatty-eng@][contact].

You should run with `--dry-run` the first time around to not actually upload
your change until you are sure about how to use the tool. Note this tool accepts
either gerrit change numbers or Change-Id's. However, since the former is
guaranteed to be unique, it is advised you use those instead.

## Check out the whole tree (with repo)

You must have different checkouts (yes, new chroots in a completely new
directory) for every branch you are working on. This is to ensure all the
prebuilts work automatically for you. You have to pass the -b <branch\_name>
option to repo during init and you will follow exactly the same workflow
described in the [ChromiumOS Developer Guide][developer-guide] i.e.
`cros_workon` and `repo start` etc.

**If you have an existing repo checkout**: You can run `repo init` with
`--reference` to re-use the objects of your existing checkout, to reduce sync
time. Note that an absolute path is required (`../../foo` won't work) and that
it must be the topmost directory of the existing repo checkout, i.e. the one
that contains the `.repo` directory.

You can find the exact name of the branch by browsing the
[manifest repo](https://chromium.googlesource.com/chromiumos/manifest/+refs).

```bash
mkdir release-R76-12239.B
cd release-R76-12239.B/
repo init -u <URL> -b <branchname> [-g minilayout] [--reference /path/to/existing/checkout]
repo sync
```

Example (See [developer_guide.md] if you are doing an internal build and
replace the manifest.git link with the appropriate one).

```bash
repo init \
    -u https://chromium.googlesource.com/chromiumos/manifest.git \
    -b release-R76-12239.B \
    --repo-url https://chromium.googlesource.com/external/repo.git
```

Next, follow steps in the [developer guide][developer-guide] to
sync/edit/modify files i.e `repo sync`, `cros_workon start`, `repo start`, etc.
After you've cherry-picked or made the changes you want, upload the changes for
review.

## Check out a single repository (with repo)

If you don't have a full repo checked out already and want to do a quick one-off
merge, you can still check out the much smaller buildtools group:

```bash
repo init \
    -u https://chromium.googlesource.com/chromiumos/manifest.git \
    --repo-url https://chromium.googlesource.com/external/repo.git \
    -g buildtools
```

This will get you chromite and all the tools it includes i.e.
`cros_merge_to_branch`. Make sure you pass the `--nomirror` option so it will
fetch the single git repo needed to cherry-pick & upload the CL.

Finally, use `cros tryjob` to run remote tryjobs.
See the [page on testing with remote tryjobs][using-remote-tryjobs] for more
info.

## Checking out a single repository (with git)

If you want to push up a few changes without checking out the entire tree,
then you can use git to do just that.
You can re-use an existing repo checkout if you like (but make sure you clean
up when you're done). Let's assume you're going to make a new checkout to keep
things clean though.

### Cloning a new repository

Find the git url you care about. You can get it by going into your repo
checkout and look at `.git/config` (the url field). You'll need to use the
-review variant of the URL to push to the special `refs/for/*` refs. Let's
demonstrate with the chromite git tree.

```bash
git clone https://chromium-review.googlesource.com/chromiumos/chromite.git
```

If you want to speed things up, you can use the `--reference` option to re-use a
local tree.

```bash
git clone https://chromium-review.googlesource.com/chromiumos/chromite.git \
    --reference ~/chromiumos/chromite/.git
```

### Setting your author/committer settings

If your normal user information is not your chromium.org e-mail, you'll need to
set it in the new repo.

```bash
git config user.name "Awesome Developer"
git config user.email ${USER}@chromium.org
```

### Start a new branch

Let's assume you want to work on the R76 branch.  You need the full name of the
branch, and then create a new local branch to work on with that info.

```bash
git branch -a | grep R76
```

That shows us the full branch name is "remotes/cros/release-R76-12239.B", so we
can do:

```bash
git checkout -b R76 remotes/cros/release-R76-12239.B
```

### Make your changes

This part is where the real work happens. Use git's or repo's cherry-pick
feature, or make the changes by hand, or apply patches, or whatever you want.

```bash
# When the editor pops, try to change a few attributes to help tracking commit
# history. Change Reviewed-on to Previous-Reviewed-on, and
# add a line like
# `**(cherry picked from commit b9e382afa7e410745ac96b12b49d5a941070db1e).**`
# For changes in different branches, you can keep same Change-Id;
# otherwise remove that to get a new unique Change-Id.
git cherry-pick -x -e <SHA1>
```

### Publish your commits to gerrit

Now for the last step.  If you didn't create a new clone, you might have to
change "origin" to "cros", or replace it with the full git url.  The "R76" is
whatever you called the local branch, and the "release-R76-12239.B" is exactly
what the official branch name is called - make sure it is correct as gerrit will
allow you to push to anything.

```bash
git push ${REMOTE_NAME} ${LOCAL_BRANCH_NAME}:refs/for/${REMOTE_BRANCH_NAME}
# for example, if remote is 'origin', your local branch is called 'R76'
# and the branch to merge to is the R76 branch on the ChromeOS server:
git push origin R76:refs/for/release-R76-12239.B
```

## Testing with remote trybot

Before you commit the change, test it! Launch a tryjob to verify it actually
builds properly. See [Using Remote Trybots][using-remote-tryjobs] for more
information.

```bash
cd <repo_root>/chromiumos/chromite
git checkout cros/master
cros tryjob -g <review_id> -b <branchname> caroline-release-tryjob eve-release-tryjob
```

## Reusing a single repository in an existing repo checkout

While it is possible to manually checkout a different branch in an existing
repo checkout (e.g. checking out `release-R69-10895.B` in `chromite/` when the
rest of the manifest is tracking `ToT`), this is *strongly not recommended*.

Mixing different branches in git trees in a single repo checkout can easily
break existing tools and is not supported. Even if you want to do it as a one
off (e.g. checkout a branch, make a change, upload it, and then discard the
branch), it's still not recommended as sometimes people forget to clean up when
they're done. Depending on the git tree, this can manifest itself days, weeks,
or even months later as a weird error in a seemingly unrelated location.

This is why we only support the methods listed above.

[contact]: ./contact.md
[contributing]: ./contributing.md#Getting-Code_Review
[drover]: https://dev.chromium.org/developers/how-tos/drover
[developer-guide]: ./developer_guide.md
[experimental-branches]: https://dev.chromium.org/developers/experimental-branches
[developer_guide.md]: developer_guide.md
[merge request]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/process/merge_request.md
[using-remote-tryjobs]: ./remote_trybots.md
