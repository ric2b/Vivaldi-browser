---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: releases
title: ChromeOS Releases
---

This document covers a variety of topics related to CrOS releases.
The primary audience is other CrOS developers, but it can be interesting to
users as well.

[TOC]

## Cycle / Schedule

ChromeOS operates on a 4 week cycle.
We largely follow the [Chrome Release
Cycle](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/process/release_cycle.md),
so check out that document for a lot more information.

NB: Before M96 (Dec 2021), CrOS followed a 6 week cycle.
See the [announcement](https://blog.chromium.org/2021/06/changes-to-chrome-oss-release-cycle.html)
for more details.

## Channels

CrOS, [like Chrome](https://www.chromium.org/getting-involved/dev-channel/),
operates 5 channels:

*   Stable: Most users should be on this.  Only the most stable is here!
*   Beta: A good balance between stability & new features for users.
*   Dev: Passed automated tests, but can still be a bit unstable.
    [See below](#dev-channel).  Most users should not use this unless there's
    a specific feature they want to test out.
*   Canary: Might not have passed all tests.  Meant for developers only to
    verify the latest code.  Not meant to be used as a primary or day-to-day
    system by anyone.
*   Long-term support (LTS): A release with a longer support cycle where
    only security issues and critical bugs are fixed.  Only available to
    [enrolled devices](https://support.google.com/chrome/a/answer/1360534).
*   Long-term support candidate (LTC): Used as a stabilization period before
    the long-term support release starts. Regression fixes are accepted.

### Dev Channel

Unfortunately "dev" (and "developer") is a heavily overloaded term, and can be
used colloquially for very different things.

The dev channel only refers to the channel and its corresponding release track
(which tends to be newer than the beta channel, but older than the canary
channel).  It does not change any other aspect of the system, or enable features
that otherwise are unavailable (*).

Notably, this should not be confused with things like CrOS dev-mode, or
[Chrome extensions developer mode](https://developer.chrome.com/docs/extensions/mv3/faq/#faq-dev-01),
or [Chrome DevTools](https://developer.chrome.com/docs/devtools/).
These are all completely independent settings unrelated to the dev channel.

(*): Some [Chrome APIs](https://developer.chrome.com/docs/extensions/reference/)
are rolled out according to the channel, but this isn't specific to the dev
channel per se.

## Version Numbers

ChromeOS uses a `[Milestone.]<TIP_BUILD>.<BRANCH_BUILD>.<BRANCH_BRANCH_BUILD>`
style.  The meaning of these are documented in the
[Version Numbers page](https://www.chromium.org/developers/version-numbers/#chromium-os).

## Stepping Stones

*** note
Googlers can view the currently defined stepping stone versions via
[GoldenEye](http://cros-goldeneye/chromeos/console/listSteppingStone).
***

Stepping stone versions refer to the auto-update process, and what new version
a device may upgrade to based on the version it is currently running.

Normally, every device can upgrade from any version to any other version.
Sometimes though, we need to restrict or force a device to upgrade to a specific
version before it can upgrade beyond that.
For example, if a device is running R40, and it were to upgrade to R90, things
wouldn't work correctly, so we force it to upgrade to an intermediate version
(a.k.a. a stepping stone version) like R50.

Since the bug exists in released versions of the OS as it exists on a device,
it's not possible to backport a fix to that version -- even if we did, the
device will still try and update to the latest OS version completely skipping
the new fixed version from the release branch.

Since this can be a really bad UX (e.g. user buys a device, uses it for the
first time, gets an update, reboots, gets an update, reboots, gets an update,
etc...), we strongly try to minimize how often we use this functionality.
Hence this is usually in response to a known bug that has been discovered.
We try to narrow down the version range (e.g. if R40->R90 is broken, what is
the oldest version we can force that doesn't have the bug, so we minimize the
number of users who go through this flow), and we try to narrow the devices
(e.g. if only device X is affected due to some device-specific firmware, we
can apply the stepping stone to that device).

Unfortunately, the biggest reason for such bugs is the lack of comprehensive
automated testing for this situation.
Part of the problem is that the amount of testing increases exponentially with
every release and every device we make.
While we verify that every OS release can update once to the "next" version, the
scenarios described here requires verifying every version that has been released
is able to update to the latest version, and do it for every device.
We also have to factor in that some bugs manifest themselves only when certain
features are used (e.g. enrolled devices with specific policies), and sometimes
only after having updated multiple times (e.g. R40->R90 works, but R40->R41->R42
->R90 fails due to incremental state changes).

Considering how often such major upgrade bugs occur (less than once a year, and
usually most tend to be device/firmware specific), we're OK with this risk.

Ideally stepping stones should be aligned with LTS milestones. These are
currently planned as M96+6*X (See [Chromium Dash] for current data). Having
stepping stones on LTS and LTC milestones allows devices on the respective
channels to move from one release to the next one without having to go through
a non-LTS milestone.

[Chromium Dash]: https://chromiumdash.appspot.com/schedule

### Code Cleanups

A side benefit for developers is that stepping stones sometimes allow us to
clean up migration code we have in the system.
For example, if R60 made a major filesystem change, we have to check at every
boot to see if the previous boot was from an older version (pre-R60).
If it was, we migrate the data; if not, we don't.
We have to maintain this code until we're confident that every device that
shipped with <R60 has gone EOL, or until we have a stepping stone version.
So if a stepping stone version is applied to R65 for *all devices*, we can
safely assume that any device on R66 already upgraded to R65, and thus had
the migration code run.
That means the migration code can be removed from ToT to keep things tidy.

Keep in mind that if the stepping stone was applied for specific devices,
we still have to keep the migration code around for everyone.

## End of Life (EOL) / Auto Update Expiration Devices

This is covered in detail in a [dedicated document](./eol_aue_process.md).
