---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: git-and-gerrit-intro
title: Introduction to Git & Gerrit for CrOS contributors
---

This guide provides a brief guide to creating Git commits and getting them
reviewed on Gerrit, for those who have not used either system before. It assumes
that you’ve already got a local copy of the repository you want to modify, and
have [set up your Gerrit credentials][Gerrit guide].  For more details on the
topics that this guide skims over (such as the commit queue and the review
process) see the [Contributing Guide](/chromium-os/developer-library/guides/development/contributing/).

## Terms

We’ll be talking about two related but separate systems here:

**[Git](https://git-scm.com/)** is a version control system, in many ways
similar to SVN, except that Git repositories do not need to be hosted on one
central server; instead, you have a local copy which can pull from many servers,
called **remotes**. It's strongly recommended to read
[this introductory presentation][Git slides] for more in-depth information
including an explanation of the concepts and workflows.

**[Gerrit](https://www.gerritcodereview.com/)** is a Git remote which hosts the
code and makes it easier to manage code reviews. Many different projects host
instances for their code, but mostly I’ll be referring to
[the Chromium Gerrit instance](https://chromium-review.googlesource.com/) when I
mention it here. For more details, see
[this introductory presentation][Gerrit slides] that goes into more detail about
the concepts and workflows.

## Make and commit some changes

In your local copy of your repository, make some changes, then run `git status`.
Here’s some sample output (in this example we'll be committing some files in the
`chip/max32660` directory):

```bash
$ git status
On branch main
Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)

        modified:   foo.txt

Untracked files:
  (use "git add <file>..." to include in what will be committed)

        chip/max32660/

no changes added to commit (use "git add" and/or "git commit -a")
```

This output tells us that a file that Git already tracks (`foo.txt`) has been
modified, and a directory has been created that Git hasn’t seen before
(`chip/max32660/`). We need to create a commit that contains these changes. To
do that, we first tell Git what we want to include in this commit:

```bash
$ git add chip/max32660/build.mk
$ git add chip/max32660/max32660.h
…
```

Now, when we run `git status`, we see that some changes have been added. (In Git
terms, we say that they are **staged**.)

```bash
$ git status
On branch main
Changes to be committed:
  (use "git restore --staged <file>..." to unstage)

        modified:   chip/max32660/build.mk
        modified:   chip/max32660/max32660.h

Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)

        modified:   foo.txt

Untracked files:
  (use "git add <file>..." to include in what will be committed)

        chip/max32660/foo.h
        chip/max32660/foo.c
        ...

no changes added to commit (use "git add" and/or "git commit -a")
```

We’re almost ready to make a commit, but notice that we’re currently on the
branch called **`main`**. (Older versions of Git may call this **`master`**
instead.) Most changes end up on `main` eventually, but for now we want to put
them on a different local branch (often called a **feature branch**) while
they’re in review. So, let’s create one called `max32660-basics` and switch to
it:

```bash
$ git checkout -b max32660-basics
$ git status
On branch max32660-basics
...
```

Now we can make the commit. When you run this command, a text editor will
appear:

```bash
$ git commit
```

In the editor, add a message for your commit. See
[the Commit messages section of the Contributing Guide](/chromium-os/developer-library/guides/development/contributing/#Commit-messages)
for suggestions on what to put in it. (No need to add the `Change-Id` line shown
there, Gerrit will do that for you.)

When you’re done, save the file and close your editor. The commit has now been
made. (You can see it in `git log`.)

## Uploading your branch as a changelist

Now you’ve got a branch with a commit in it, and you want to upload it to Gerrit
for review. Gerrit’s unit of code review is called a **changelist** (often
abbreviated to **CL**), and you can create one from your branch. First, let’s
make sure that you have Gerrit set up as a remote.

### Check your repository’s remotes

If you downloaded the repository by using `git clone` or `repo`, you probably
already have Gerrit set up as a remote. Check with the `git remote -v` command,
and note the name of the remote (`cros` in this case):

```bash
$ git remote -v
cros    https://chromium.googlesource.com/chromiumos/platform/ec (fetch)
cros    https://chromium.googlesource.com/chromiumos/platform/ec (push)
```

If you don’t see a suitable remote, you can add one (replacing `platform/ec`
with the appropriate path for the repository you're working in):

```bash
$ git remote add cros https://chromium.googlesource.com/chromiumos/platform/ec
```

### Uploading the code

First, check that you’re on your feature branch by running `git status` and
checking the first line. (If you’re on the wrong branch, use `git checkout
<branch name>` to get back to the right one.)

You'll need the `repo` command for this step. `repo` is a tool that makes it
easier to manage multiple related Git repositories, but we'll just be using it
to upload our change and run presubmit checks.  If you don't have it, you can
get it by [installing `depot_tools`][depot_tools].

First, set your branch’s upstream. This tells `repo` what version of the code it
should compare your current state with. You’ll probably want the following
command (assuming that your remote is called `cros`):

```bash
$ git branch --set-upstream-to=cros/main
```

Now you can tell `repo` to upload your change:

```bash
$ repo upload . --br=max32660-basics
```

When asked whether to run the checks, enter `yes`. If the checks find any
problems, they will tell you about them and the change will not be uploaded.
Once you've fixed them you can run the command again. Sometimes it will warn
about things that can't be fixed given the circumstances, in which case add
`--ignore-hooks` to the command (though you should probably amend your commit
message to explain why).

#### Adding reviewers

Whichever method you use, you should see some output like this if all goes well:

```
Upload project src/platform/ec/ to remote branch refs/heads/main:
  branch max32660-basics ( 1 commit, Wed Jun 5 12:14:47 2019 -0700):
         7d244dd1 Add basic support for MAX32660
to https://chrome-internal-review.googlesource.com (y/N)? y

remote: Processing changes: refs: 1, new: 1, done
remote:
remote: SUCCESS
remote:
remote:   https://chrome-internal-review.googlesource.com/c/chromeos/platform/ec/+/1234567 Add basic support for MAX32660 [NEW]
remote:
To https://chrome-internal-review.googlesource.com/chromeos/platform/ec
 * [new branch]      max32660-basics -> refs/for/main
```

Here, the URL
`https://chrome-internal-review.googlesource.com/c/chromeos/platform/ec/+/1234567`
is the one for your change. Open it in your browser to see the change.

Now, if you’re signed in to Gerrit, you can request that one or more other
Gerrit users review the CL. Click “Reply” and add the reviewers you want to the
“Reviewers” line.

## Updating your changelist

Once you’ve sent your CL for review, your reviewers will probably leave comments
on it, suggesting improvements. Once you’ve made the changes they asked for, you
need to update the CL in Gerrit.

### Modifying your commit

To do this, we need to modify the commit you created the CL from. First, let’s
check that the commit has the Change ID line that Gerrit uses to identify it.
Run `git log` and look for a line beginning `Change-Id:` in the message. With
that verified, let’s go ahead and modify the commit.

#### Amending

If you haven’t committed your recent changes yet, this is really simple. Use
`git add` as before to stage the changes, but then instead of creating a new
commit, change the existing one:

```bash
$ git commit --amend
```

The commit message editor will appear again, prefilled with your existing
message. If you want to change the commit message, do so now (but make sure to
leave the `Change-Id:` line intact). Then save and close the editor as before.

#### Rebasing

If you’ve already made additional commits on the branch, all is not lost. We can
combine them together after the fact, using a **rebase**:

```bash
$ git rebase -i main
```

(Yes, that should be `main`, even though you’re on a branch, as we’re telling
the rebase command to look at all the commits between this branch head and the
main branch.)

A text editor should appear, prepopulated with a list of commits, something like
the following:

```
pick 7d244dd12 Add basic support for MAX32660
pick 2e0a4ea43 Fix some review comments
pick 46d8c0a28 Fix some more comments

# Rebase 29e7e6eec..46d8c0a28 onto 29e7e6eec (3 commands)
#
# Commands:
# p, pick <commit> = use commit
# r, reword <commit> = use commit, but edit the commit message
# e, edit <commit> = use commit, but stop for amending
# s, squash <commit> = use commit, but meld into previous commit
# f, fixup <commit> = like "squash", but discard this commit's log message
# x, exec <command> = run command (the rest of the line) using shell
# b, break = stop here (continue rebase later with 'git rebase --continue')
# d, drop <commit> = remove commit
# l, label <label> = label current HEAD with a name
# t, reset <label> = reset HEAD to a label
# m, merge [-C <commit> | -c <commit>] <label> [# <oneline>]
# .       create a merge commit using the original merge commit's
# .       message (or the oneline, if no original merge commit was
# .       specified). Use -c <commit> to reword the commit message.
#
# These lines can be re-ordered; they are executed from top to bottom.
#
# If you remove a line here THAT COMMIT WILL BE LOST.
#
# However, if you remove everything, the rebase will be aborted.
#
# Note that empty commits are commented out
```

The first line will be your original commit, which we want to combine everything
in to. To do that, simply replace `pick` with `fixup` on all the lines _below_
it. Then, save and close the editor. Git will go through and do the combining.

(If you want to change the commit message too, just run `git commit --amend` at
this point.)

### Uploading again

Now that you’ve modified the commit, re-upload it using the same `repo upload`
command you used before.

Once the upload is done, open your CL in Gerrit again. You should see that it
now has another patch set. A **patch set** is Gerrit’s name for a version of a
changelist, and Gerrit allows you (and your reviewers) to see what you’ve
changed between patch sets.

### Respond to reviewer comments

To make it easier to see what’s going on, **reply to your reviewers’ comments**.
This is very important to make it easier for both you and your reviewers to keep
track of what's happening. If you’ve done what they asked for, click “Done” and
Gerrit will mark the comment as resolved. Otherwise you can reply explaining why
you think their suggestion won’t work or isn’t a good idea. Once you’re done,
click the blue “Reply” button at the top of the page to send your responses.

## Submitting the CL

To be submitted, your CL must have a Code-Review +2 from at least one reviewer,
and Verified +1 from anybody (including yourself). Once it has those, reply to
the CL adding Commit-Queue +2 to start the submission process. Assuming that the
tests pass, you will get an email some time later telling you that the CL has
been submitted.

## Getting ready to start again

You probably want to build on the code that you just submitted in your next CL.
To do this, you’ll need to update your local copy of the main branch to include
the changes that have just been merged. Checkout main, then tell `repo` to
synchronise your repositories:

```bash
$ git checkout main
$ repo sync
```

(If you’ve got other stuff going on in your ChromiumOS tree, you might find it
helpful to [split repo sync into two stages][repo sync tip].)

### Rinse and repeat

If you run `git log` now, your CL should appear somewhere in the list. (It might
be quite far down depending on how busy the repository is and how long it’s been
since it was submitted.) Now you can create another feature branch and start
again, building on the changes you’ve already submitted.

[Gerrit guide]: https://dev.chromium.org/chromium-os/developer-guide/gerrit-guide
[Gerrit slides]: https://docs.google.com/presentation/d/1C73UgQdzZDw0gzpaEqIC6SPujZJhqamyqO1XOHjH-uk/edit?usp=sharing
[Git slides]: https://docs.google.com/presentation/d/1IQCRPHEIX-qKo7QFxsD3V62yhyGA9_5YsYXFOiBpgkk/edit?usp=sharing
[depot_tools]: https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up
[repo sync tip]: /chromium-os/developer-library/guides/recipes/tips-and-tricks/#TOC-How-to-make-repo-sync-less-disruptive-by-running-the-stages-separately
