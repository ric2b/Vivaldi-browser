---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: repo-tool
title: repo
---

ChromiumOS uses the [repo tool] to manage its [source checkout].

This document is for CrOS developers & users only.

[TOC]

## Background

Some random concepts as names might be overloaded leading to confusion.

* Launcher: The `repo` program that people run directly.
* Manifest: The XML file that defines the source layout and git repositories.
* Repo client checkout: The actual checkout of CrOS, e.g. `~/chromiumos/`.
* Project: A single git repository listed in the manifest.

## Launcher

The repo launcher is the program that developers run directly.
It isn't the full repo source, it is just a wrapper to set up an initial repo
client checkout by checking out the manifest & the repo sources.
Once that's been initialized, the launcher serves as a trampoline to reexec
itself using the local copy of the repo source checkout under `.repo/repo/`.

NB: You should never mess around with `.repo/repo/` as repo will reset any
changes you make in there, or it will wedge/break it requiring manual fixing.
See the [updating repo](#updating-users) section for more details.

For CrOS, developers get `repo` by putting [depot_tools] into their `$PATH`.
The copy in [depot_tools] has a single change to point it to our fork of the
repo source, and it saves developers from having to install it themselves.

Many distros include packages to install it at `/usr/bin/repo`, and the official
[Android documentation] suggests downloading & installing it under e.g. `~/bin`.
Those launchers will point to the upstream [repo tool].
While not strictly required, CrOS users should stick to the [depot_tools] copy.

[Android documentation]: https://source.android.com/setup/build/downloading

## Source Repositories

There are two source repositories to be aware of:
*   https://chromium.googlesource.com/external/repo: CrOS fork.
    *   This is used when running `repo` from [depot_tools].
*   https://gerrit.googlesource.com/git-repo: The upstream repo project.
    *   This is used when running `repo` from standard locations.

The CrOS fork used to contain a lot of customizations, but nowadays is almost
exactly the same as upstream.
We are actively working to reduce the differences
(we're down to [1 patch](https://crbug.com/825366)),
and we do not want to add anymore custom code.
That means any changes/requests should be sent to the upstream [repo tool]
project for implementation, and once they're merged+released there, we can
update our fork to the new release.

## Updating Repo Locally

> NB: For CrOS bots/builders, see the [updating bots] section.

The repo tool will keep itself up-to-date automatically.
There is usually no need for users to ever worry about this.
Whenever you run `repo`, it will check once/24hrs to see if there's an update.
If there isn't, then it won't try again for another 24hrs.

If you know a new release was made and you're itching to try it out immediately,
you can run `repo selfupdate` to force it.

This guidance applies equally to [rollbacks] -- repo will automatically roll
itself back as needed; see that section for more details.

## Updating The Launcher

If the repo launcher version is increased, we need to update our copy of the
launcher to avoid confusing users.
This does not happen with every repo release as the repo launcher is updated
much less frequently.

Ignoring unintended bugs, it is completely safe to have a new repo launcher with
an old release of repo sources.
It is also safe to have an old repo launcher with a new release of repo, but
this combination will trigger user-visible warnings.

So if you're aware that a new repo launcher version has been released, you
should try to update it in the various places well in advance of pushing the new
release.
Developers often don't update their checkouts every day, but the repo release is
self-updating, so when a new release is pushed, they'll get it automatically.

If a repo release needs to be rolled back for some reason, you usually do not
have to worry about rolling back the repo launcher version.
This scenario has yet to happen in the history of the CrOS project :).

### Comparing Versions

You can easily check to see if this is needed by running & comparing the version
output of the launchers.

```sh
repo$ ./repo --version
<repo not installed>
repo launcher version 2.4       <-- compare this line to ...
       (from ./repo)
...

depot_tools$  ./repo --version
<repo not installed>
repo launcher version 2.4       <-- ... this line!
       (from ./repo)
...
```

### Places To Update

Here are all the places you'll want to update.

*   [depot_tools]: Copy `repo` from external/repo to `repo_launcher` in
    depot_tools.
    NB: Update the `repo_launcher` file instead of `repo` in here.
*   Pinned depot_tools checkouts: Once [depot_tools] is updated with the new
    launcher, update the pinned commits to the new version.
    *   [private manifest](https://chrome-internal.googlesource.com/chromeos/manifest-internal/):
        See [source checkout] for updating `~/chromiumos/manifest-internal/`.
        Our infrastructure will propagate changes to the public manifest
        automatically.

## Managing Our Fork

> NB: Only CrOS build/infra developers have access to push to this repo.
> Do not attempt to do so if you aren't on the team.
> Well, you can try, but it'll fail, so maybe don't be surprised when it does?

You will need to get a checkout of the two repo source repositories.
The snippet below shows how to initialize things, but all other examples will
simply assume your working directory is a repo source checkout.

```sh
# Our copy of repo.
~$ git clone https://chromium.googlesource.com/external/repo
~$ cd repo

# Upstream repo.
~/repo$ git remote add upstream https://gerrit.googlesource.com/git-repo
~/repo$ git fetch upstream
```

### Code Review

Since all changes happen in the upstream [repo tool] project, we don't utilize
Gerrit for updates to our fork anymore.
The only exception is if we actually need to make a CrOS-specific change to our
code base, but that hasn't happened in years.
All work should go through upstream first.

### Branch Management

> While good git practices say to always fast-forward branches, non-fast-forward
> pushes to any of these branches will not break users or bots.
> The repo tool will safely checkout whatever commit a branch is at.
>
> This is not a recommendation to be cavalier in branch management, but a protip
> for extraordinary cases like rollbacks.

These are the branches in our repository that we care about.

*   `stable`: This is [production].  Push here carefully.  The branch **must
    always** have a signed tag in order for repo to use it.
*   `main`: This is [staging].
    *    Used to test new repo release before being sent to [production].
    *    Although we try to use signed tags in here, it technically does not
         require them, although devs have to opt-in to unsigned tags.
    *    The branch isn't used unless explicitly opted-in to.
    *    Also used for review of CrOS-specific changes.
*   `upstream/*`: These refs are kept exactly in sync with upstream.  They are
    not used by any tooling.  They exist as a convenience to compare the state
    of our fork to upstream at any time.  Thus they may safely point at the
    exact same refs as upstream even if our own branches are behind.

Upstream branches are automatically mirrored to our fork via GoB copy
configuration rules, so the `upstream/*` namespace is always up-to-date.

### Tag Management

Upstream tags use standard names like `v2.4`.
CrOS specific tags append a `-cr#` suffix to keep them distinguished.
If we make local changes, we increment the suffix.

So if our branch is based on/has merged upstream `v2.4`, we'll use `v2.4-cr1`.
Any changes before merging the next upstream release would be e.g. `v2.4-cr2`.

Tags may be safely pushed/mirrored at any time.
They will never have a direct effect -- repo watches branches, not tags.
So if upstream releases `v2.5`, but our branches are still on `v2.4-cr1`, the
new `v2.5` tag may be safely pushed to our fork.

To mirror upstream tags to our fork:

```sh
# This will fetch the tags from upstream.
$ git fetch --tags upstream
# Check which tags will be pushed to our fork.
$ git push origin --tags -n
# Do the actual push!
$ git push origin --tags -o push-justification='b/<id> - repo release'
```

### Merging Upstream Releases

Once upstream has made a new release, we need to merge it into our fork.
See the previous sections for syncing the upstream branches & tags first.

The example below will assume the state:
*   Our `main` branch is sitting at `v2.4-cr1`.
*   `~/repo` has a local `main` branch checked out at the same state as the
    remote `origin/main` branch (and is tracking it).
*   Upstream `stable` branch is sitting at `v2.5`.
*   We want to merge `v2.5` into our `main` branch and create `v2.5-cr1`.

> NB: Signing & pushing tags is required to get changes into [production], but
> it is not required to test changes in [staging].

```sh
# Merge in the new release to our local main branch.
$ git merge --log=1000 v2.5

# If there are conflicts, git merge will tell you that they need to be resolved.
# You should resolve them at this point :).  If there are no conflicts, skip to
# the next step.  Once you resolve the conflicts, commit them.
$ git commit

# Your editor should have opened at this point to generate a commit message for
# the merge commit.  You can look at previous releases (git log -1 v2.3-cr1) for
# example formats.  If you resolved conflicts, you should note them here.
<save the commit message & exit your editor>

# Sign the new release.  The script defaults to signing HEAD.
# NB: This requires access to official repo keys.  See go/repo-release.
# NB: --force is required since ChromeOS tag naming convention is different than
# upstream.
$ ./release/sign-tag.py --force v2.5-cr1

# Push the new tag.  This is safe to do as you're only pushing the tag, not
# updating any branches.  See the next sections for those steps.
$ git push origin --tags -n
# If the -n output looked correct, push for real.
$ git push origin --tags -o push-justification='b/<id> - repo release'
```

### Sidenote On Tagging Syntax

In the sections below, we use a `^0` syntax that you might not be familiar with.
This is to distinguish between "lightweight" (a.k.a. "normal" or "plain") and
"annotated" (a.k.a. signed) tags.
The [Git manual](https://git-scm.com/book/en/v2/Git-Basics-Tagging) has a
section explaining things, but it is **vitally important** that you always use
this syntax when pushing to branches.

If an annotated tag is pushed to a branch, both git & repo get extremely upset,
and this will break users & bots.
The `^0` syntax refers to the commit that the signed tag is signing and that is
what branches must always track.

### Sidenote On Bot Updates

> NB: For end users/developers, see the [updating users] section.

> NB: This documentation applies to builders using LUCI recipes.  Legacy
> builders, most notably release & branch builders, do not run `repo selfupdate`
> explicitly, so rollout to them will be gradual.  There are no plans to fix
> this since we're working to migrate all legacy builders to recipes.

CrOS builders will explicitly run `repo selfupdate` at the start of all of
their builds.
This makes sure they're always running the latest published version, otherwise
the random 24hr rollout can be quite confusing for everyone.
For example, if a new release causes problems, only the bots that pick it up
will exhibit that problem, and CQ/release builders will fail inconsistently.
This will in turn cause developers/oncall to think specific bots are flaking
which will in turn cause delays in attributing problems to repo itself.

After the builder runs `repo selfupdate`, it will not automatically update
itself again during the build (since it resets the 24hr check).
So if bots are in the middle of a build and a new repo release is made, they
won't see it until the next build starts.
Thus you don't have to worry about waiting for all bots to finish their builds
just so a new release can be pushed out.

### Staging Repo Releases

Before pushing any upgrades to [production], they need to be run through our
[staging] instances first (ignoring "normal" emergency pushes of course).

CrOS bots are configured to watch the `main` branch in our repo fork, and do so
without enforcing signed tags (see https://crbug.com/1055913).
This allows us more flexibility in testing out new repo changes before they go
into a release.

So before pushing a new release to production, push it to the `main` branch, and
watch the staging bots to see how they handle things.
Pay particular attention to the [staging CQ] as they will cycle the fastest.

> **Warning**: The `^0` syntax is required; see the [tagging sidenotes] section.

```sh
# Push the new release to the main branch.  The staging bots will pick this up
# immediately, so start monitoring the bots.
$ git push origin v2.5-cr1^0:main -n
# If that looked good, push it for real.
$ git push origin v2.5-cr1^0:main -o push-justification='b/<id> - repo release'

# If you don't have a tag but just want to test some new changes on staging
# bots, you can push a commit to the main branch.
$ git push origin <commit>:main -n
# If that looked good, push it for real.
$ git push origin <commit>:main -o push-justification='b/<id> - repo release'

# If things go wrong, it's perfectly safe to do non-fast-forward pushes.
# Bots will rollback to the right version immediately.
$ git push origin <old commit>:main -f -o push-justification='b/<id> - repo release'
```

[staging CQ]: https://ci.chromium.org/p/chromeos/g/chromeos.staging-cq/builders

### Pushing Repo Releases To Production

> NB: If the new repo release contains an updated launcher, make sure to
> [update the launcher](#launcher-update) before pushing the release to the CrOS
> fork, otherwise all CrOS developers will see warnings whenever they run `repo`
> that their launcher is out of date.  That will just confuse them and cause
> them to raise support issues with the build team.

Before pushing any upgrades to [production], they need to be run through our
[staging] instances first (ignoring "normal" emergency pushes of course).

Once a release has been vetted in [staging], it's time to roll it out to prod.
Pushing to the `stable` branch will make the release available to
[users](#updating-users) and [bots](#updating-bots) immediately.
So make sure you want to do this :).

Signed tags are required in order to push to production.
Any commits pushed to `stable` that are not signed will be ignored and repo
will fallback to whatever closest signed tag it can find in the branch.

> NB: Releases to production must be treated like any other production service.
> That means:
> *   Avoid new releases during off-hours.  If something goes wrong,
>     staff/oncall need to be active in order to respond quickly & effectively.
> *   Stick to normal working hours: Mon - Thu, 9:00 - 14:00 PT (i.e. MTV time).
> *   Avoid US holidays (and large international ones if possible).
> *   Follow the normal Chops/Google production freeze schedule.

Before you push the release, make sure you let people know!
This helps users find out about new features, but it also helps devs/oncall
triage failures faster that might be related to the new release.

* [go/cros-infra-chat]: The CrOS Infra team chat.
* [go/cros-oncall]: The current oncall rotation.

> **Warning**: The `^0` syntax is required; see the [tagging sidenotes] section.

```sh
# Push the new release to the main branch.  This is to make sure things are
# in sync with the stable branch, and allow for any last minute checks via
# e.g. gitiles.  NB: This will not affect any bots or users.
$ git push origin v2.5-cr1^0:main -n
# If that looked good, push it for real.
$ git push origin v2.5-cr1^0:main -o push-justification='b/<id> - repo release'

# Push the new release to the stable branch.  This makes it live in production!
$ git push origin v2.5-cr1^0:stable -n
# If that looked good, push it for real.
$ git push origin v2.5-cr1^0:stable -o push-justification='b/<id> - repo release'
```

### Rolling Back A Release

If a new release is causing a problem, the best course of action is most likely
to rollback to the previous working version.

Non-fast-forward pushes to the `stable` branch is perfectly safe.
Repo will always sync to the latest branch state exactly.
As noted in the [production] section, only signed commits will be respected!

Make sure all rollbacks are coordinated with [go/cros-oncall].

```sh
# Push the an old release to the stable branch.  This makes it live in production!
$ git push origin v2.4-cr1^0:stable -f -n
# If that looked good, push it for real.
$ git push origin v2.4-cr1^0:stable -f -o push-justification='b/<id> - repo release'
```

## FAQs

### Are signed tags required in production?

Yes!
If the `stable` branch is not pointing to a signed commit, then repo will search
backwards through the branch's history for the first signed commit.
It will warn about this state before falling back.

### Are signed tags required in staging?

Nope!
Our staging bots are configured to use whatever commits are in the `main` branch
even if they aren't yet signed.
This allows more CrOS infra developers to help in testing new changes.

### Who can push to the CrOS repo fork?

Contact the [CrOS infra group](https://goto.google.com/cros-infra-help)
for assistance.

### Who can sign new tags?

See [go/repo-release](https://goto.google.com/repo-release) for details.

### Do we rebase our branches?

We don't need to rebase our branches.
Merging commits is fine.

Of course, rebasing our local branches isn't problematic for users/bots as
they'll gladly pull down any rewritten refs.
If it helps to clean up history or something, it's permissible.

### I want a new feature in repo!

Please send requests to the
[upstream issue tracker](https://issues.gerritcodereview.com/issues/new?component=1370071)
so they can be triaged & implemented.

### I found a bug in repo!

Nice work!
Please report issues to the
[upstream issue tracker](https://issues.gerritcodereview.com/issues/new?component=1370071)
so they can be triaged & addressed.

### Who is responsible for supporting repo?

THe upstream repo project is a cross-collaboration between CrOS, Android, and
other teams & customers who utilize repo in their project.
Each project tends to have a subteam who is responsible for their own users.

In CrOS, issues that affect CrOS developers & systems are supported by folks
across the CrOS Infra team.
It's a group effort.

### I wrote a new feature for repo!  Please merge it.

Awesome, we love when people take initiative.
Please
[send your CL upstream](https://gerrit.googlesource.com/git-repo/+/HEAD/SUBMITTING_PATCHES.md)
and have it merged there.

Once upstream makes a new release, we'll roll it back into our fork.

### I want to merge my CL into CrOS's repo fork.

Sorry, but we don't do that anymore.
It is expensive to deviate from upstream which is why we want everyone to merge
there first.
Plus, they can do reviews for general appropriateness of the request.

### How many repos could repo reap if repo reaped repos?

Zero.
When you run `repo sync`, it takes care to not delete modified checkouts.

### Where can I find more documentation on repo?

Check out the upstream [repo tool] project.
It has user & developer & internal documentation.

### Where can I find more information on our source checkouts?

Check out our [source checkout] documentation.


[cros_portage_upgrade]: /chromium-os/developer-library/guides/portage/package-upgrade-process/
[depot_tools]: https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html
[dev-vcs/repo]: https://chromium.googlesource.com/chromiumos/overlays/portage-stable/+/HEAD/dev-vcs/repo
[go/cros-infra-chat]: https://goto.google.com/cros-infra-chat
[go/cros-oncall]: https://goto.google.com/cros-oncall
[production]: #prod
[repo tool]: https://gerrit.googlesource.com/git-repo
[rollbacks]: #rollback
[source checkout]: /chromium-os/developer-library/reference/development/source-layout/
[staging]: #staging
[tagging sidenotes]: #sidenote-tags
[updating bots]: #updating-bots
[updating users]: #updating-users
