---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: breakages-and-flakes
title: Breakage and Flake Policy
---

While in an ideal world, the [commit queue] should help us keep the
tree green, occasionally, it's possible that breakages can get
introduced. This is no good, as it blocks everyone from being able to
get their work done.

[TOC]

## How can bad CLs land in the tree?

Bad CLs can land a number of ways:

1. The [CL] was [chumped].
1. Due to a CI configuration, a builder was marked as either
   informational or [post-submit] only, and was skipped during the
   commit queue run of the bad [CL]. Note that we intentionally don't
   run certain builders on certain files/repositories to save on
   testing resources and developer time, as our CI configuration
   considers those files/repositories low risk to the builders.
1. Two CLs land which introduce conflicting functionalities separately
   via the Parallel CQ. Note that this is actually rather rare, but
   has happened.
1. A CL introduces a build or test [flake](#flakes) to the tree. Any
   flake can land through the CQ with enough tries.

## Reverting Bad CLs

**ChromiumOS is a revert-first project.**

This means, whenever it's possible to solve a breakage simply by
reverting a [CL] or two, this is the preferred way to solve the
breakage.

*Why revert first instead of fixing forward?* Breakages impact
everyone's productivity. Generally, reverting a recently-landed [CL]
which is known to cause a failure is so safe that the revert can be
[chumped], giving us many valuable hours during the time a commit
queue run would have taken for a forward fix. Of course, please
coordinate with the sheriff before chumping.

If you are sent a revert for a [CL] you wrote or reviewed, don't feel
ashamed, it happens to the best of us. Instead, help the sheriff
verify that reverting your change will fix the issue, and you can
reland it later with the fix applied.

## Making Builders/Tests Informational

If the cause of the failing builder or test cannot be narrowed down to
a [CL] that can be reverted, the sheriffs and on-callers may have no
other choice but to disable it, or mark the builder/test as
informational, experimental, or non-critical.

Generally, this is done as a last-resort step, as it allows new
breakages to slip in. A reverted CL would be preferred much more to
this.

In most cases, we do not allow making builders or tests informational
in order to land a change which introduces a breakage to that builder
or test, unless it is coordinated by or closely with the owners of the
build/test.

## Flakes

There's a special variety of breakages called "flakes," breakages
that only happen occasionally. Even flakes can cost us many developer
hours.

For example, consider a hypothetical build flake which happens 1 out
of every 1000 builds. This may seem harmless at first, but lets run
some numbers:

* Assume that the commit queue spawns 100 builds (which is close to
  the actual number of builders spawed by the CQ as of this time of
  writing).

* Next, assume that we have 300 developers committing to the tree
  every day, each of whom are going to be sending an average of 3 CLs
  thru the CQ that day (these numbers were totally made up but not
  unreasonable).

* From a flake which happens 1/1000 builds, 999/1000 builds will pass,
  assuming that there's no other issues with the tree.

* 999/1000 builds to the power of 100 builders per CQ run means that
  only **90.47% of CQ runs will pass**.

* For a developer landing 3 CLs in a day, the probability that they
  see your flake cause issues in at least one of their CQ runs is
  **25.93%**.

* This means that approximately 78 of 300 developers will have to put
  up with your flake delaying at least one of their CLs during their
  day.

Hopefully this goes to show that flakes should be treated as high
priority issues, just like total breakages. Given this, similar
guidelines should apply:

* If a flake can be cured with a revert, that's the best way to put
  out the fire.

* Otherwise, the sheriff and on-call staff may need to do whatever is
  possible to stop the flake, including disabling tests or making them
  informational, or creating ugly workarounds.

If the sheriff or on-call staff reaches out to you to help with
solving a flake, please **treat this as a high priority issue**, and
don't forget that a revert is always preferred over a forward fix.

[chumped]: contributing.md#chumping
[CL]: glossary.md#acronyms
[commit queue]: cros_commit_pipeline.md
[Parallel CQ]: cros_commit_pipeline.md#parallel-cq
[post-submit]: cros_commit_pipeline.md#post-submit-builders
