---
breadcrumbs:
- - /getting-involved
  - Getting Involved
page_name: become-a-committer
title: Become a Committer
---

[TOC]

## What is a committer?

Technically, a committer is someone who can submit their own patches or
patches from others. A committer can also review patches from others, though
all patches need to either be authored by or reviewed by an
[OWNER](https://chromium.googlesource.com/chromium/src/+/main/docs/code_reviews.md#owners-files)
as well.

This privilege is granted with some expectation of responsibility: committers
are people who care about the Chromium projects and want to help them meet their
goals. A committer is not just someone who can make changes, but someone who has
demonstrated their ability to collaborate with the team, get the most
knowledgeable people to review code, contribute high-quality code, and follow
through to fix issues (in code or tests).

A committer is a contributor to the Chromium projects' success and a citizen
helping the projects succeed. See [Committer's
responsibility](/developers/committers-responsibility).

What is written below applies to the main Chromium source repos; see the note at
the bottom for ChromiumOS, which has different policies. For some other Chromium
repos (e.g., the infra repos), we follow the same policies as the main Chromium
repos, but have different lists of actual committers. Certain other repos may
have different policies altogether. When in doubt, ask one of the
[OWNERS](https://chromium.googlesource.com/chromium/src/+/main/docs/code_reviews.md#owners-files)
of the repo in question.

## Becoming a committer

To become a committer, you must get at least ten non-trivial patches merged to
the [Chromium src](https://chromium.googlesource.com/chromium/src/) Git
repository, and get an existing committer to nominate you. You will need at
least two other committers to support the nomination, so getting at least three
different people to review your patches is a good idea.

We want to see sufficient evidence that you can follow Chromium best practices,
and in situations where you're uncertain, you ask for additional guidance
effectively. Perhaps the most important aspect of being a committer is that you
will be able to review and approve other people's changes, so we're looking for
whether we think you'll do a good job at that.

So, in addition to actually making the code changes, you're basically
demonstrating your

*   commitment to the project (10+ good patches requires a lot of your
    valuable time),
*   ability to collaborate with the team and communicate well,
*   understanding of how the team works (policies, processes for testing
    and code review
    [OWNERS](https://chromium.googlesource.com/chromium/src/+/main/docs/code_reviews.md#owners-files)
    files, etc),
*   understanding of the projects' code base and coding style,
*   ability to judge when a patch might be ready for review and to submit
    (your work should not generally have glaring flaws unless you're
    explicitly requesting feedback on an incomplete patch), and
*   ability to write good code (last but certainly not least)

### Non-trivial patches

It is unfortunately not easy to define what a non-trivial patch is, because a
one-line change might be subtle, and changes that touch lots of files might
still be trivial. For example, changes that are more-or-less mechanical (e.g.,
renaming functions) will probably be considered trivial. Here are some
guidelines:

* "Non-trivial" means literally "anything other than trivial"; it explicitly is
  not the same thing as "major" or "significant". If it took you more than a
  minute of thought, it's probably non-trivial.
* Even a small change is non-trivial if the rationale or benefit was
  non-trivial to arrive at.
* Almost any change that isn't a pure comment change, symbol rename, or
  clearly-obvious-on-its-face rewording whose effects are completely local (e.g.
  a transforms like "a += 1" -> "++a") is a non-trivial change.
* Don't worry that the bar is impossibly high; you won't normally need a hundred
  CLs to find ten that qualify.
* If you aren't certain whether your work meets the bar, ask an existing
  committer.

### Nomination process

If you think you might be ready to be a committer, ask one of the reviewers
of your CLs or another committer familiar with your work to see if they will
nominate you.

If they are, they nominate you by sending email to committers@chromium.org
containing the following information.
**Committers: Please do not CC the nominee on the nomination email.**

*   your first and last name
*   your email address. You can also ask to get an @chromium.org email
            address at this time, if you don't already have one (see below).
*   an explanation of why you should be a committer,
*   a list of representative landed patches

Two other committers need to second your nomination. We will wait five working
days (U.S.) after the nomination for votes and discussion. If there is
discussion, we'll wait an additional two working days (U.S.) after the
last message in the discussion, to ensure people have time to review
the nomination. If you get the votes and no one objects, at that point
you become a committer. If anyone objects or wants more information,
the committers discuss and usually come to a consensus. If
issues can't be resolved, there's a vote among current committers.

In the rare cases where a nomination fails, the person who nominated you
will let you know. The objection is usually something easy to address
like "more patches" or "not enough people are familiar with this person's work."

If the person you ask to nominate you thinks you're not ready, they should
be able to tell you why not and what you need to do to meet the criteria.

If you haven't done so already **you'll need to set up a security key on your
account before you're added to the committer list**.

Mechanically, being a committer means that you are a member of
committers@chromium.org. It may take a few days longer after your nomination
to actually be added to the list, but the whole process usually won't take
longer than two weeks. Keep writing patches!

Historically, most committers have worked at least partially on the Chromium
core product and thus demonstrated C++ coding ability in their CLs, but this is
not required. It is possible to be a committer if you only work on other parts
of the code base (e.g., build and test scripts in Python), but you still have to
demonstrate that you understand the processes of the project with a list of CLs
that you've landed. Committership is primarily a mark of trust, and we expect
committers to only submit or approve changes that they are qualified to review.
Failure to do so may result in your committership being revoked (also see below
for other reasons that you might get your committership revoked).

Being a committer is something that a person is, not something an email
address or account is. This means that you can be a committer under multiple
email addresses, and you can change your address, without needing each
address to be re-nominated. To do so, send an email to accounts@chromium.org.

If you have questions about this process, you can ask on community@chromium.org
and people there will be happy to help you.

## Other statuses

If you just want to edit bugs, see: [Get Bug-Editing
Privileges](/getting-involved/get-bug-editing-privileges).

### Getting a @chromium.org email address

Many contributors to chromium have @chromium.org email addresses. These days, we
tend to discourage people from getting them, as creating them creates some
additional administrative overhead for the project and have some minor security
implications, but they are available for folks who want or need them.

At this time there are only a few fairly rare cases where you really need one:

*   If you are an employee of a company that will not let you create
            public Google Docs, and you wish to do so (e.g., to share public
            design specs). Often you can work around this by having someone with
            an account create the document for you and give you editing
            privileges on it.
*   If you need to do Chromium/ChromiumOS-related work on mailing lists
            that for some reason tend to mark email addresses from your normal
            email address as spam and you can't fix that (some mail servers are
            known to have problems with DMARC, for example).

It is also fine to get a @chromium.org address if you simply don't want your
primary email addresses to be public. Be aware, however, that if you're being
paid to contribute to Chromium your employer may wish you to use a specific
email address to reflect that.

You can get a @chromium.org email address by getting an existing contributor to
ask for one for you; normally it's a good idea to do this as part of being
nominated to be a committer. Include in your request what account name you'd
like and what secondary email we can use to associate it with (and what company
you are affiliated with, if you wish to make that clear; we track this
affiliating internally but it isn't publicly visible). People tend to match
usernames (for example, someone who usually uses email@example.com would ask for
email@chromium.org) to minimize confusion, but you are not required to do so and
some people do not.

### Try job access

If you are contributing patches but not (yet) a committer, you may wish to be
able to run jobs on the [try servers](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/infra/trybot_usage.md)
directly rather than asking a committer or reviewer to do so for you. There are
two potential scenarios:

You have an @chromium.org email address and wish to use it for your account:

*   If you have an @chromium.org email address, you most likely already
            have try job access. If for some reason you do not, please send an
            email to accounts@chromium.org with a brief explanation of why you'd
            like access.

You do not have an @chromium.org email address, or wish to use a different email
address. If this is your situation, the process to obtain try job access is the
following:

*   Ask someone you're working with (a frequent reviewer, for example)
            to send email to accounts@chromium.org nominating you for try job
            access.
*   You must provide an email address and at least a brief explanation
            of why you'd like access.
*   It is helpful to provide a name and company affiliation (if any) as
            well.
*   It is very helpful to have already had some patches landed, but is
            not absolutely necessary.

If no one objects within two (U.S.) working days, you will be approved for
access, and someone should get back to you to let you know.

### Gardening

Committers who are Google employees on the Chrome team are also expected to
help keep the tree open and the waterfall green, a process we call
[gardening](https://chromium.googlesource.com/chromium/src/+/main/docs/gardener.md).
Non-Google committers are neither expected nor able to be gardeners;
this is primarily because we don't want to require this of people who aren't
being paid to do it, and secondarily because it can require tools, processes,
and builder access that are not public.

## Maintaining committer status

A community of committers working together to move the Chromium projects forward
is essential to creating successful projects that are rewarding to work on. If
there are problems or disagreements within the community, they can usually be
solved through open discussion and debate.

In the unhappy event that a committer continues to disregard good citizenship
(or actively disrupts the project), we may need to revoke that person's status.
The process is the same as for nominating a new committer: someone suggests the
revocation with a good reason, two people second the motion, and a vote may be
called if consensus cannot be reached. I hope that's simple enough, and that we
never have to test it in practice.

In addition, as a security measure, if you are inactive on Gerrit for more than
a year, we may revoke your committer privileges and remove your email
address(es) from any OWNERS files. This is not meant as a punishment, so if you
wish to resume contributing after that, contact accounts@ to ask that it be
restored, and we will normally do so. This does not mean that we will shut off
your @chromium.org address, if you have one; that should continue to work.

If you have questions about your committer status, overall, please contact
accounts@chromium.org.

\[Props: Much of this was inspired by/copied from the committer policies of
[WebKit](http://www.google.com/url?q=http%3A%2F%2Fwebkit.org%2Fcoding%2Fcommit-review-policy.html&sa=D&sntz=1&usg=AFrqEze4W4Lvbhue4Bywqgbv-N5J66kQgA)
and
[Mozilla](http://www.google.com/url?q=http%3A%2F%2Fwww.mozilla.org%2Fhacking%2Fcommitter%2F&sa=D&sntz=1&usg=AFrqEzecK7iiXqV30jKibNmmMtzHwtYRTg).\]

## Chromium OS Commit Access (Code-Review +2)

Note that any registered user can do Code-Review +1. Only access to Code-Review
+2 in Chromium OS repos is restricted to Googlers and managed by team lead
[nominations](http://go/new-cros-committer-nomination).
