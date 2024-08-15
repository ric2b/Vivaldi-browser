---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: contributing
title: ChromiumOS Contributing Guide
---

For new developers & contributors, the [Gerrit] system might be unfamiliar.
Even if you're already familiar with [Gerrit], how the Chromium project uses it
might be different due to the [CQ]/trybot integration and different labels.

For a general [Gerrit] overview, check out Android's [Life of a Patch].
This helpfully applies to everyone using [Gerrit]. If you're not familiar with
Git at all, check out the [Git & Gerrit Intro].

If you haven't checked out the source code yet, please start with the
[Developer Guide].
Once you've got a local checkout, come back here.

[TOC]

## Account setup

Please follow the [Gerrit Guide] for getting access to the [Gerrit] instances.
Once that is all set up, you can come back here.

### Sign a Contributor License Agreement

Before uploading [CL]s, you'll need to submit a [Contributor License Agreement].

**SIDE NOTE**: Googlers do not need to sign a CLA.

## Policies

CrOS uses code review systems to try and improve changes and catch bugs/problems
before they're ever merged, but we also have some strict policies that underlie
the system, and for good reason.
Some of these polices are not enforced by the system, so we rely on developers
to be aware of these and make sure they abide by them until we can get more
automation in place.

Keep the following high level principles in mind.
They are meant to guide developers to the spirit of the review/approval system,
not necessarily provide the full rule set.
That means we expect everyone to try align (and even do better!) than what we
outline here, not try and find loopholes that haven't been explicitly banned.

*   Every change should have a reason, which is appropriately documented.
    ([Write good commit messages](#Commit-messages)!)
*   All changes should be agreed upon by two trusted persons (i.e. Googlers).
    *   Authors, if they are trusted, count as one.
    *   Upstream developers, even if you personally know them, do not count.
    *   Automatic processes may count towards this requirement if the
        infrastructure is maintained by the infra team, and the projects being
        imported automatically have policies adhering to these principles.
        *   If the projects are themselves Google managed and follow this same
            two party approval system, we can allow automatic merging into our
            project.  For example, Chromium commits.
        *   If the projects are external 3rd party projects, then the creation
            of automatic merge CLs are allowed, but will require one trusted
            person to review+approve them.  For example, most GitHub projects.
    *   NB: We currently allow 1 trusted person to approve, as long as that
        person is not the author.
*   Google & CrOS process a lot of user data that we must keep secure, and as
    such, makes us targets by a wide range of potential attackers.
    We need to protect our systems not only from obvious "bad actors", but
    also from [insider threats].
    You shouldn't assume your peers are malicious, obviously, but we need to
    make sure we design things such that we are resilient even if they were.

Any code that ends up in a build signed with production keys, or code in
services that facilitate the creation of that build, must abide by these rules.

See the [FAQ] below for some concrete questions.

Googlers should also see https://goto.google.com/change-management-policy for a
lot more in-depth detail & company wide policies that CrOS is subject to.

## Commit messages

For general documentation for how to write git commit messages, check out
[How to Write a Git Commit Message](https://chris.beams.io/posts/git-commit/).

As a quick overview, here's what a sample description should look like.
It will show up in its entirety in [Gerrit], and the first line will be used as
the subject line for the review (e.g. in e-mail notifications):

```
power: Use fusion power for the torque propounder

At present the torque propounder is powered by a hamster. The hamster
is quite tired. Also the hamster has started demanding a wide variety
of exotic food, which is difficult to provide in a remote, rural
environment.

Power the torque propounder with the fusion reactor instead. With this
we can remove all limits for the foreseeable future.

Provide a small retirement hut for the hamster while we are here.

BUG=b:99999, chromium:88888
TEST=Ran all the power tests

Change-Id: I8d7f86d716f1da76f4c85259f401c3ccc9a031ff
```

Notice that the commit message (below the subject) starts 'At present', rather
than 'Prior to this commit'. The commit is written based on the current state of
the world (present tense), and takes some action based on that. Discussions
about the state before and after this commit just confuse things.

The subject should be no more than 50 characters, including any tags. The tags
indicate the subsystem the commit relates to. Capitalize the first word after
the final tag but do not add a period at the end. You can include identifiers
or filenames from the code in your subject, although this is somewhat rare,
since often you can just reference the topic or feature. When you do add
identifiers / filenames, use the same case in the subject.

Examples of bad subjects and how to improve them:

*  'Dynamic charging current control.' - use a tag and don't add a period
   ```
   charger: Add dynamic current control
   ```

*  'zephyr: initial it8xxx2_evb bringup'  - indicate action, use capital letter
   ```
   zephyr: Add initial it8xxx2_evb support
   ```

*  'charger: support 300V batteries'  - use capital letter
   ```
   charger: Support 300V batteries
   ```

The body should consist of a motivation followed by an impact/action. Without
the motivation no one will understand why your patch is needed. Without the
impact/action, they won't know what the commit does. Put the motivation and
impact in separate paragraphs if they are longer than a line. In some cases the
motivation is implicit, e.g. if you are enabling a feature which everyone
understands is needed.

Examples of bad messages and how to improve them:

*  'Update the time zone to GMT'  - OK, but why? Missing motivation
   ```
   At present the time is shown incorrectly.

   Update the time zone to Greenwich Mean Time.
   ```

*  'Fix the problem'  - what problem? Why?
   ```
   This driver is using the wrong I2C Address. Fix it.
   ```

*  'Add a new wibble binary'  - missing motivation
   ```
   We need to support the Wibble 2.0 protocol so we can communicate with
   a wider variety of clients.

   Add an updated wibble binary (version 2.0.1) to allow this.
   ```

*  'The battery does not work. Fix it.'  - vague action... what is the fix?
   ```
   The battery does not charge when it gets above 80%.

   Fix this by updating the charge curve for the 'nearly full' state.
   ```

Wrap the body text to 72 characters so that `git log` looks nice. Don't say
'this patch' or 'this commit' or 'this CL adds'. We know it is a
patch / commit / CL. For example, 'Switch to light speed' is better than
'This patch will change things to use light speed'. If your CL adds something,
say 'Add something', not 'Adding something'.

The commit message should stand alone from the attached bug so don't just point
to the bug and leave the commit message empty. Provide enough to understand
what is going on. The bug provides extra context and history that would be too
much for a commit message. For private bugs in a public repo you may need to be
circumspect about certain details but bear in mind that the only information
visible in public is what you put in your commit message, so it should be
sufficient to understand/judge the commit.

Longer examples [kernel](https://crrev.com/c/2722434),
[EC](https://crrev.com/c/2733454),
[U-Boot](http://patchwork.ozlabs.org/project/uboot/patch/20210214123358.22724-1-andre.przywara@arm.com/).

If you're unsure of the form to use in a particular repo, look at the recent
commit log via `git log` to get a sense for local customs.

### Link to issue trackers

Issue trackers are critical to the smooth running of the project, both for
tracking bugs/regressions as well as new features.
When making changes that are related to an issue (open or closed), the commit
message should link to the relevant issue via `BUG=` lines.

The general form is `BUG=bug-tracker:number`.
The ChromiumOS project has supported various bug trackers over the years, but
currently there are 2 supported trackers: one at [crbug.com], for which you
should use the prefix `chromium:`, and one at [issuetracker.google.com] (see
[issue tracker]; internally known as Buganizer), for which you should use the
prefix `b:`. The `b:` prefix can also be used with the [partner issue tracker].
If your changes are related to more than one issue, you can list all the
issues separated with commas, or include multiple `BUG=` lines.

*** promo
If your commit would close an issue, you can append `FIXED=bug-tracker:number`
(using the same format as `BUG=`) to your CL description as well. When the
change lands, the `FIXED` issue will close automatically.
***

The `BUG=` lines should be separated by the rest of the commit message by a
blank line, and should come before the `TEST=` lines (see below).

### Describe testing performed

When evaluating [CL]s, other developers want to know what kind of tests were
performed to make sure the code behaves as expected.
Reviewers who are familiar with these code paths can often suggest alternative
tests to run in case the ones run were not adequate.

These are described in the commit message with `TEST=` lines.
These come directly after the `BUG=` lines and are generally free-form.
There should be a blank line between them and the [Change-Id] tag at the end.

Some common examples:

*   `TEST=cros_run_unit_tests --board ${BOARD} --packages metrics`: Implies the
    [unittests] in the package are sufficient to prove functionality.
*   `TEST=precq passes`: Implies all [unittests]/vmtests/hwtests passed, and
    those tests are sufficient to validate the code.
*   `TEST=None`: Used if the [CL] in question doesn't need testing (e.g. fixing
    typos in documentation files).

Avoid:

*   `TEST=manual`: Does not specify which test(s) you actually ran.
*   `TEST=ran unit tests`: Does not specify which test(s) you actually ran. Did
    you run "all" the unit tests? A subset?

Prefer instead:

*   `TEST=cros deploy ${DUT} power_manager && restart powerd &&
powerd_dbus_suspend`: Describes exactly which tests you ran and allows reviewers
    to verify that the test coverage is sufficient or suggest alternative tests.

### Change-Id

It is important to note that Gerrit uses the [Change-Id] in your git commit
message to track code reviews.
So if you want to make a change to an existing CL, you'll want to use `git
commit --amend` rather than making an entirely new commit.

This allows you to follow the standard git flow by making multiple changes in
a single branch and uploading them together.

For more details, see [Gerrit]'s [Change-Id] documentation.

### CL dependencies

Sometimes work will span multiple [CL]s across different repos. The `Cq-Depend:`
lines are used to make sure [CL]s are merged in a specific order and [CQ]
attempt to merge [CL]s altogether.

*   Specify dependencies in the last paragraph of your change, just before
    `Change-Id:`, using `Cq-Depend: chromium:12345`.
*   Each dependency should start with a Gerrit instance prefix followed by a
    number (the [Gerrit][CL] number on the server).
    *   Specifically it should be noted that there is no way currently to
        express `Cq-Depend` by referencing another patch's [Change-Id].  If
        you are trying to express circular dependencies that means the only
        way to do it is to upload at least one of your patches twice because
        you need gerrit to assign a CL number before you can reference it.
*   You may specify multiple dependencies. Each dependency should be separated
    with a comma and a space (e.g. `Cq-Depend: chromium:12345,
    chrome-internal:4321`).
*   Use `chrome-internal` prefix to denote internal dependencies (e.g.
    `chrome-internal:4321`).
*   You may specify `Cq-Depend` loops where "CL A" depends on "CL B", and "CL B"
    depends on "CL A" (there's no limit to the number of [CL]s). This makes sure
    the [CQ] will pick up and test them together.
*   Atomic submission/merge is not guaranteed but best-effort supported by [CQ]
    due to [Gerrit] limitations.
    *   There is a small window where syncs may pull a partial set of changes.

Here's an example:

```
Add file to install to 9999 ebuild file

BUG=chromium:99999
TEST=Tested with dependent CL's in trybot.

Cq-Depend: chromium:12345, chrome-internal:4321
Change-Id: I8d7f86d716f1da76f4c85259f401c3ccc9a031ff
```

### Signed-off-by

The majority of, but not all, projects in CrOS do not use `Signed-off-by` tags.
They do not serve any function in general since we require everyone to sign a
[Contributor License Agreement] before uploading CLs.

A limited number of projects, such as Linux Kernel, EC, and Coreboot, require
a `Signed-off-by: Random J Developer <random@developer.example.org>` line in
the commit message.
Please follow the rules defined by that project when adding this line.
This line can be added automatically by specifying the `--signoff`
option when committing.

When using `repo upload`, the tool will automatically check the project's
configuration and then require or reject the tag accordingly.

## Upload changes

*** note
Please do not use `git push` or `git cl upload` when developing in CrOS.
That bypasses our preupload hooks that run automatically via `repo upload`
which makes it easy to accidentally introduce breakage to the tree.
You might not notice the problem when you always ignore the hooks, but that
breakage affects every other developer using the proper tooling.

Similarly, never use the `--no-verify` option.  If you find preupload checks
hooks are causing problems or are broken, you should file a bug so that they
can be fixed (or disabled) as makes sense.
You can use `--ignore-hooks` to still upload even if the hooks fail, but you
must review the output to make sure your CLs are not adding more breakage.

Mistakes have a network effect on your fellow developers and can significantly
slow them down.
***

Once your changes are committed locally, you upload them using `repo upload`.
This command takes all of the changes that are unmerged, runs preupload checks
on them, asks if you want to upload them, and then publishes them.

By default, `repo upload` looks across all branches & projects, so most of the
time you want to restrict this to the local repo instead:

```sh
# Command most people will use most of the time.
$ repo upload --cbr .

# General format.  See `repo upload --help` for more.
$ repo upload [.|${PROJECT-NAME}] [--current-branch] [--reviewers=REVIEWERS]
```

You'll often want to specify reviewers using the `--re` option, but don't worry
if you didn't specify it here as they can be added later.
See the [Adding Reviewers] section for more details.

Once you run `repo upload`, this uploads the changes and prints out a URL for
the code review for easy access.

For more in-depth details, check out [Gerrit]'s [Uploading Changes] document.

## Going through review

<!-- mdformat off(b/139308852) -->
*** note
See [Google's Engineering practices documentation] for additional tips.
***
<!-- mdformat on -->

### Start the review

When [CL]s start in [Work-in-Progress (WIP)] mode, people are not notified.
You'll need to go to the web interface and click the **Start Review** button.
There you can add reviewers and comments before clicking the next **Start
Review** button.

If the [CL] is not in WIP mode, as soon as the [CL] is uploaded, notifications
are sent out to any reviewers (if `--re` is used).
You can still go to the web interface and click the **Reply** button to add
reviewers and comments before clicking the **Send** button.

### Add reviewers

You should pick reviewers that know the code you're working on well and that
will do the best reviews.
Picking reviewers who will just rubber-stamp your changes is a bad idea.
The point of submitting changes is to submit good code, not to submit as much
code as you can.

If you don't know who should review your changes, use the `Suggest Owners`
button in Gerrit after you've uploaded the CL, which can be found in the
`reply` dialog.  See the [Gerrit OWNERS documentation] for detailed
screenshots. Owners enforcement is mandatory, so make sure that at least
one OWNER is included in your reviewers.  Prefer adding reviewers like
`foo-reviews`, it's best to start there as those tend to be automatic
rotations of developers.

Some OWNERS files include inline comments to help direct to the right groups or
people, but the Gerrit UI doesn't show those currently.
If a lot of people are shown in the list, you can consult the file directly to
see if there are any rules to help.

If there are no OWNERS files, you can use `git log` to find people.
You can use it on the specific files you're touching, or on the entire project.
Simply type the commands below in a directory related to your project:

```bash
$ git log <file>
$ git log <directory>
```

### Address feedback

Your reviewers will likely provide comments about changes that you should make
before submitting your code.
You should make such changes, commit them locally, and then re-upload your
changes for code review.
Make sure to reply to all open comments and resolve them when possible.
This is a signal to reviewers that they'll want to re-review the changes.
If there are no open comments, and you want another review pass, you'll want to
post an explicit comment asking for it to send out a notification.

You can amend your changes to your git commit and re-run `repo upload`.

```bash
# <make some changes>
$ git add -u .
$ git commit --amend
```

If you have a chain of commits (which `repo upload .` converts to a chain of
[CL]s), and you need to modify any commits that are not at the top of the chain,
use interactive rebase:

```bash
$ git rebase -i
# This shows a list of cherry-picks into a temporary branch.
# Change some of the "pick" keywords to "edit".  Then exit the editor.

# Look at the first "edit"ed commit.  All earlier commits are cherry-picked.
$ git log

# Make some modifications.
$ git add -u .
$ git commit --amend
# Move on to the next "edit"ed commit.
$ git rebase --continue

# Finally upload when all modifications are ready.
$ repo upload . --current-branch
```

### Getting Code-Review

Ultimately, the point of review is to obtain both looks good to me (LGTM) and
approved statuses in your [CL]s.
Those are tracked by the Code-Review+1 (LGTM) and Code-Review+2 (LGTM &
approved) [Gerrit] labels.

Reviewers use the Code-Review+2 label to say the [CL] looks good (LGTM), and
the reviewer is also approving it as they're fully OK with it.

Some reviewers might be OK with the [CL], but they aren't comfortable approving
it (e.g. they aren't that familiar with the particular piece of code).
They'll add a Code-Review+1 label and wait for someone else to approve it.

Only once you've obtained a Code-Review+2 label can you move on.
Note that two Code-Review+1 labels does not equal a Code-Review+2.
It simply means more than one person said the code looks good, but they all want
someone else to approve the [CL].

#### Code-Review+2 access

ChromiumOS repos allow anyone to give Code-Review+1 on [CL]s, but restrict
access to Code-Review+2 to developers (and partners in specific repos).

For Googlers, if you don't have Code-Review+2 but think you should,
your manager or TL should nominate you for committer access via
[go/new-cros-committer-nomination](http://go/new-cros-committer-nomination).

For partners, raise an issue through your partner contact channels.

If you're looking for someone to give Code-Review+2 to your [CL], then see the
[Adding Reviewers] section.

### Setting Verified

Some reviewers, depending on how they reviewed things, might add the Verified+1
label to indicate that they also tested/verified the [CL].
This is not requirement for them and is entirely their own preference.

Ultimately it's your responsibility to mark the [CL] as Verified+1 to indicate
that all your testing has passed.
It's recommended you do this even if other reviewers set Verified+1 themselves.

## Send your changes to the Commit Queue

Once you've got Code-Review+2 (LGTM & approved) and Verified+1 labels, it's time
to try to merge it into the tree.
You'll add the Commit-Queue+2 label so the [CQ] will pick it up.
If the [CL] doesn't have Code-Review+2, Verified+1, and Commit-Queue+2 labels,
then the [CL] will never be picked up by the [CQ].
Further, if someone adds Code-Review-2 or Verified-1, the [CQ] will ignore it.

For a stack of dependent [CL]s, the gerrit command line tool offers a method
for setting the labels for the entire set:

```bash
$ gerrit review -l V+1 -l CQ+1 $(gerrit --raw deps <top CL number>)
```

More details on the Commit Queue can be found in the [Commit Queue
Overview][CQ].

### Merge conflicts

It is possible that your change will be rejected because of a merge conflict.
If it is, rebase against any new changes and re-upload your [CL].

For example, this could be done via these commands:

```bash
$ repo sync
$ repo rebase
```

This will stop on the first conflict; you can use usual `git` techniques for
resolving the conflict and continuing the rebase (`git rebase --continue`).
After resolving all conflicts, upload the changes via `repo upload` as usual.

### Updating CLs after Code-Review+2

Whenever non-trivial changes are made to a [CL], all labels are cleared.
This means a Code-Review+2 label must be attained again.
For developers with access, they'll often apply Code-Review+2 to their own
[CL] with a comment like "inheriting CR+2 from previous patch".
The expectation here is that the developer hasn't made significant changes that
the reviewers would have objected to.

Adding Code-Review+1 to your own [CL]s doesn't make sense.
It's like saying "my code LGTM" which we already know because you uploaded it.

If the developer doesn't have access, they'll have to get approvals from the
reviewers again.

If only trivial changes are made, then the Code-Review+2 labels will be sticky.
The kind of trivial changes are:

* Rebases onto newer commits without conflicts.
* Changes to the commit message.

### Make sure your changes didn't break things

If all testing passes, the [CQ] will merge your [CL] directly.
If something did go wrong, the [CQ] will post details of the run.
This will often include a lot of logs that you're expected to go through and
make sure the failure wasn't due to your [CL].

If you're confident your [CL] was not at fault, simply add Commit-Queue+2 again.
It can sometimes take several tries to get it to pass.

If you're still unsure, feel free to reach out to the reviewers.

(Googlers only) You may also reach out to sheriffs using this link:
[go/cros-oncall](https://goto.google.com/cros-oncall)

*** note
**Also see**: [Breakage and Flake Policy]
***

## Bypass the commit queue (chumping)

Rarely should you bypass the [CQ] (aka "chumping a [CL]").
Doing so puts the system at risk by including a [CL] that hasn't been properly
tested on devices.
This should be reserved for people trying to fix existing breakage, and should
be coordinated with the sheriffs.

Note: chump requests for other reasons (e.g., trying to get a CL in
before a branch cut) are very likely to be rejected by the sheriff.

You can do this by hitting the **Submit** button when a [CL] has Code-Review+2
and Verified+1.

## Clean up

After you're done with your changes, you're ready to clean up.
You'll want to delete the branch that repo created.
There are a number of ways to do so; here is one way:

```bash
# Command most people will use most of the time; run it in the project.
$ repo abandon ${BRANCH_NAME} .

# General format.  See `repo abandon --help` for more.
$ repo abandon ${BRANCH_NAME} ${PROJECT-NAME}
```

*** note
**Warning**: If you don't specify a project name, the `repo abandon` command
will throw out any local changes across **all projects**.
You might also want to look at `git branch -D` or `repo prune`.
***

## Advanced topics

### Share your changes using the Gerrit sandbox

It is possible to upload changes to a personal sandbox on [Gerrit].
This lets developers share changes with others before they're ready for review.

*** note
The sandbox spaces are **not** private.
Anyone can find & access commits posted here.
Do not use this to hold secret work.

You're free to create as many branches as you want under your own namespace
`refs/sandbox/${USER}/`.
We leave it up to your discretion to properly manage these branches.
Please don't abuse it by uploading large binary files that don't belong in git.

When we say `${USER}`, we mean your username, **not** your e-mail address.
The `@` in e-mail addresses do not work smoothly in all scenarios.
e.g. Use `vapier` and not `vapier@chromium.org`.

Further, the sandbox spaces are a bit loose with access.
You can push to any path under `refs/sandbox/`, but we've all agreed to restrict
ourselves to the `${USER}` subdir.
***

#### Pushing directly with git and bypassing Gerrit/code review

```bash
$ project_url="https://chromium.googlesource.com/$(git config remote.cros.projectname)"
$ git push ${project_url} HEAD:refs/sandbox/${USER}/${BRANCH_NAME}
```

Other developers can then fetch your changes using the following commands:

```bash
$ project_url="https://chromium.googlesource.com/$(git config remote.cros.projectname)"
$ git fetch ${project_url} refs/sandbox/${USER}/${BRANCH_NAME}
$ git checkout FETCH_HEAD
```

In a given repository, you can explore sandboxes using the `ls-remote` command:

```bash
$ git ls-remote cros "refs/sandbox/${USER}/*"
$ git ls-remote cros "refs/sandbox/*"
```

Once uploaded, you can browse commits via [Gitiles].
The URL will look like:
`https://chromium.googlesource.com/${projectname}/+/sandbox/${USER}/${BRANCH_NAME}`.
Note that the `refs/` part is omitted.

If you want to preview markdown changes (e.g. `README.md`), check out
[Previewing changes](./README.md#previewing-changes).

Once you're finished with a sandbox, you can delete it:

```bash
$ project_url="https://chromium.googlesource.com/$(git config remote.cros.projectname)"
$ git push $project_url :refs/sandbox/${USER}/${BRANCH_NAME}
```

#### Uploading to Gerrit for early code review

If you really want to have CLs accessible in Gerrit for code review, you may use
the sandbox namespace for this.
Keep in mind that, if you never intend on submitting/merging the CL, you can
always upload to the default ToT branch, and set the status to WIP so it's clear
to others you don't intend on merging.
CQ dry-run is not available with sandbox branches either.

Before you can upload a CL to a sandbox branch, you have to manually create it.

```bash
# Assuming the project is public ("cros"), and default ToT manifest ("m/main").
$ git push cros m/main:refs/sandbox/${USER}/${BRANCH_NAME}
```

Then you can push CLs to it for review.

```bash
$ repo upload --cbr . -D refs/sandbox/${USER}/${BRANCH_NAME}
```

### Gerrit sandbox access

Write access to the sandbox namespace is restricted to contributors & partners.
We don't allow any registered user to push to prevent spam.

If you don't have access but you're working on the CrOS project and it would
help to easily share in-progress changes, please contact whatever Googlers you
are currently working with.
Any of them should have access to add your specific account.

Note that we already add:

*   All Googlers
*   All "ChromeOS Approvers" (people who have CR+2 access)
*   All "ChromeOS Committers" which is a large umbrella group of CrOS friends
*   (chrome-internal only) All ChromeOS partners

You can double check your access by visiting your
[settings page](https://chromium-review.googlesource.com/settings/#Groups).
Look for `chromeos-gerrit-sandbox-access` there.

If you still need to be added, here are the groups on the servers to update:

*   https://chromium-review.googlesource.com/admin/groups/chromeos-gerrit-sandbox-access
*   https://chrome-internal-review.googlesource.com/admin/groups/chromeos-gerrit-sandbox-access

### Switch back to ToT

While you're working on your changes, you might want to go back to the mainline
for a little while (maybe you want to see if some bug you are seeing is related
to your changes, or if the bug was always there).
If you want to go back to the mainline without abandoning your changes, you can
run the following commands from within a directory associated with your project.

*** note
The `m/main` is a ref managed by `repo` to point to the right remote and
branch for the particular repo.
The remote name (e.g. `cros` or `cros-internal`) depends on where the repo is
hosted, and the remote branch name (e.g. `main`) depends on what the specific
project is using for its current development branch.
Thus `m/main` should point to the right branch regardless.
***

```bash
$ git checkout m/main
```

When you're done, you can get back to your changes by running:

```bash
$ git checkout ${BRANCH_NAME}
```

Take care when running `repo sync` in case it switches branches on you again.

### Work on something else while waiting for reviews

If you want to start on another (unrelated) change while waiting for your code
review, you can `repo start` to create another branch.
When you want to get back to your first branch, run the following command from
within a directory associated with your project:

```bash
$ git checkout ${BRANCH_NAME}
```

### Updating CL without rebasing

When going through the review process, it's common to receive feedback that
requires making changes in your CL and uploading a new patchset (PS).
If other CLs land in the repo in the meantime, your local checkout has probably
been updated and your changes been rebased onto the latest main branch.
To be clear, this isn't a problem: when the CL lands, it will be rebased as
part of the CQ merge process.

However, when viewing inter-PS differences (e.g. the diff between PS1 & PS2
rather than the default base & PS2), changes made to the file by other CLs will
show up too.
Those diffs might add significant noise for the reviewer who is focusing only on
the bits that have been changed by you.
When you keep the parent commit the same between patchsets, even if it isn't the
latest available commit in the repo, the inter-PS diffs remain stable.

This trick can be applied to another common scenario: when you have a patch
series and you only want to refresh one CL in the middle without updating all of
them at the same time (which can generate noise in the CL with rebase).
This assumes, of course, the change doesn't run into conflicts with later CLs.

*** note
This flow assumes that you were the author of the CL and that it was uploaded
from the same checkout that you will be working in here, and that the git object
directory hasn't been garbage collected.
Or that the parent commit is one that's been merged into the tree already.
This is the most common flow, so it's normally OK.
***

Here's the process:

1.  Go to your CL in Gerrit.
1.  Create a temporary branch in the exact state as the CL.
    1.  Click the ⋮ menu.
    1.  Select "Download patch" section.
    1.  Click the "REPO" tab next to the "HTTP" tab.
    1.  Copy the "Branch" code snippet.
    1.  Run the `repo download ...` command in your local git checkout.
1.  Make any last changes you want before uploading it like normal.
    *   e.g. `repo upload --cbr .`
    *   Any changes that haven't yet been merged will be run through the set of
        pre-upload hooks even if you didn't author them.
        If you've verified that your CL passes the hooks, you can use the
        `--ignore-hooks` flag to bypass the checks.
        *Use with care.*
1.  Once you're all done, you can delete the temporary branch.
    1.   Run `repo abandon change-1234 .` to delete & detach.

The key to this process is that the commits you're building on top of have not
changed since they were uploaded to Gerrit.
Gerrit defines "changed" as "has new commit id", not "the diff & commit message
are the same".
That is why we used `repo download` above to pull down the exact CL and its
commit state to make sure the local tree state matched it exactly.
If you have multiple unmerged commits in this branch (e.g. a patch series), and
they get rebased (e.g. you ran `git rebase` or `repo sync` rebased for you),
then uploading changes from that branch will update all the CLs in Gerrit (which
is what you were trying to avoid in the first place).

Since this does take effort, and many times CLs landed don't touch the files
you're also working on, developers are not required or generally expected to
go through this.
We leave it to your discretion as to when it makes sense.

### Basing your CL on another uploaded CL

If someone else has posted a CL that you want to build on top of, but you don't
want to take over their CL or have it rebased when you upload your new CL, then
this is the flow for you.

The flow is largely the same as the [Updating CL without
rebasing](#updating-cl-without-rebasing) process.

Be extra aware of the caveat for how this works as noted in the section above:
the commits you downloaded and are basing things on must not change (i.e. their
git commit id must be exactly the same).

## FAQ

### Do I have to upload every single upstream commit for individual code review?

Certainly not!

If you create a merge commit, that single CL is the only thing that needs to be
uploaded & approved during code review. See the [merge CL] session below for
instructions.
It is the reviewers responsibility to validate the changes pulled in via that
merge commit using whatever means is appropriate, and the approval of the merge
commit applies transitively to all of its children.

This would also apply to requesting a new branch be created from a specific
starting point -- getting it into the overall build would require code review
at some point (e.g. updating ebuilds or repo manifests), and those approvals
would implicitly apply to the new source history being pulled in.

### How do I upload a merge CL for review?

Here's the process:

1.  (Googlers only) Join the corresponding ACL group (e.g.
    chromeos-kernel-mergers).
2.  (Googlers only) Push a branch into the `merge/` namespace.
    ```bash
    $ git push cros HEAD:merge/${BRANCH_NAME}
    ```
    If the individual commits were ever uploaded one by one into Gerrit, you
    need to remove all the Change-Id tag. Any commits with associated Change-Id
    can not be involved in a merge due to Gerrit limitations.
    ```bash
    $ git config --local gerrit.createChangeId false
    $ EDITOR="sed -i -e 's/Change-Id: .*//'" git rebase -i m/main -x 'git commit --amend'
    $ git config --local gerrit.createChangeId true
    ```
3.  Create a merge commit.
    ```bash
    $ git fetch cros
    $ git checkout m/main
    $ git merge --no-ff --log=${NUMBER_OF_CLS} cros/merge/${BRANCH_NAME}
    ```
    The commit description should include a shortlog of the commits being merged
    (e.g. the output from `git request-pull`).
4.  Upload the merge commit by `repo upload` and go through the normal code
    review process.

    You'll have to ignore all the warnings about "too many CLs". Everything but
    the merge CL should already be present on `cros/merge/${BRANCH_NAME}` and
    will not create new CLs.

Also see `src/platform/dev/contrib/merge-kernel`, the script to merge upstream
kernel tags into chromeos.

### Do I have to review every commit going into upstream 3rd party releases?

While that would be great, it is definitely not required by our [Policies].
Like merge commits or branch creation, the approval of the version bump of a
package (e.g. ebuild) is sufficient to cover the entire codebase in one go.

### What about binary firmwares from partners?

While we might request 3rd party audits/reviews of firmware source, we do not
need to approve the code directly ourselves.
Just like the questions above, the CL that integrates that firmware into our
build will undergo review+approval, and that satisfies our [Policies].

Reviewers are still responsible for verifying the integrity & provenance of the
binary, as well as the trust of the organization/partner providing it.

### What about artifacts uploaded to our GS mirrors?

The act of uploading files to our [archive mirrors] does not require any review
(any CrOS dev can update these files directly), but simply uploading them there
does not get them into production.
A commit to an ebuild/Manifest file is still required to go through review, and
it is the responsibility of reviewers at that time to check the source files.

If you're worried about people modifying files on the mirrors and bypassing all
of our reviews, worry not!
The [archive mirrors] FAQ has a section specifically documenting security risks
and how we prevent them.

### Can I push directly to git repositories?

The answer is almost always no.
Consistent with our [Policies], all code that ends up in a signed build must
undergo code review & approval before being merged.
If you were able to push directly to a git repository, you would be able to
bypass those requirements, and thus be able to push whatever changes you want
without anyone noticing.

If you have a repository that is not used in any way to produce a signed build,
then you might be granted direct push access.
This is exceedingly uncommon though; see the next question.

### What about git repositories marked 'notdefault' in the repo manifest?

If your `<project>` is marked with `groups=notdefault`, then we might allow
direct access to the git repository.
Keep in mind that any source that is used in signed builds/production must
be kept secure even if it isn't directly part of a device build.

For example, projects that have a CI pipeline independent of the main CrOS CQ
and produce release artifacts that go into signed components -- they might not
be in our manifest, but they still must follow our [Policies].

### Can I rewrite repository history?

The answer is largely the same as "can I push directly to git repositories":
if the source code is used in signed builds, then history must be kept forever.

If your development process plans on rewriting history regularly, then it might
not be a good design, and you should strongly rethink your approach.

### What about images not automatically released to users?

Our [Policies] do not differentiate between intended target audiences.
That is to say, we protect the artifacts that are signed by production keys.
Trying to do "internal-only" production signed builds does not work as we have
no way of enforcing those builds are never used or leaked externally, and this
wouldn't protect from [insider threats].


[Adding Reviewers]: #reviewers
[archive mirrors]: archive_mirrors.md
[Breakage and Flake Policy]: breakages_and_flakes.md
[Change-Id]: https://gerrit-review.googlesource.com/Documentation/user-changeid.html
[ChromeOS sheriff rotation]: https://www.chromium.org/developers/tree-sheriffs/sheriff-details-chromium-os#TOC-How-do-I-join-or-leave-the-rotation-
[CL]: glossary.md
[Contact]: contact.md
[Contributor License Agreement]: https://cla.developers.google.com/
[CQ]: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/infra/cq.md
[crbug.com]: https://crbug.com/
[Developer Guide]: developer_guide.md
[FAQ]: #faq
[Gerrit]: https://gerrit-review.googlesource.com/Documentation/
[Gerrit Guide]: https://dev.chromium.org/chromium-os/developer-guide/gerrit-guide
[Gerrit OWNERS documentation]: https://gerrit.googlesource.com/plugins/code-owners/+/HEAD/resources/Documentation/how-to-use.md#add-code-owners-to-your-change
[Git & Gerrit Intro]: git_and_gerrit_intro.md
[Gitiles]: https://gerrit.googlesource.com/gitiles/
[insider threats]: https://en.wikipedia.org/wiki/Insider_threat
[issue tracker]: https://developers.google.com/issue-tracker/
[issuetracker.google.com]: https://issuetracker.google.com/
[partner issue tracker]: https://partnerissuetracker.corp.google.com
[Life of a Patch]: https://source.android.com/setup/contribute/life-of-a-patch
[merge CL]: #How-do-I-upload-a-merge-CL-for-review
[Policies]: #policies
[unittests]: testing/unit_tests.md
[Uploading Changes]: https://gerrit-review.googlesource.com/Documentation/user-upload.html
[Work-in-Progress (WIP)]: https://gerrit-review.googlesource.com/Documentation/intro-user.html#wip
[Google's Engineering practices documentation]: https://google.github.io/eng-practices/review/developer/
