---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: eol-aue-process
title: End of Life (EOL) / Auto Update Expiration (AUE) Process
---

*** note
The official Google policy can be found at
https://support.google.com/chrome/a/answer/6220366.
This document is purely for developers making code changes to the project.
It is not a reference for end users and support status for their devices.
Any statements made in this document that appear to conflict with the official
aforementioned policy document are accidental and do not supersede it.
***

All good things must come to an end and ChromeOS devices are no different.
At some point, devices will be marked EOL and dropped from the ChromiumOS
project and all of the source trees.
This document is a guide for developers to determine when they can safely
move forward in that cleanup process.

You might see the terms EOL and AUE used interchangeably in documents.
The official term is Auto Update Expiration (AUE) as it refers to the last
release version that is made available through Omaha (our update servers).
Practically speaking, they mean the same thing.

[TOC]

## AUE Process Overview

First, a milestone version is picked based on the AUE policy and other factors
by The Deciders (the group of people high up the food chain who make Decisions)
for a hardware platform and the boards that use it.
Typically this is done on an SoC basis (e.g. all SandyBridge devices) as support
and feature sets are done at that level.

That version is set in [GoldenEye] for each board under the "Auto Update
Expiration Target Milestone".
Omaha uses that data to send AUE notifications to the respective boards which
is then presented to the user so they're aware their device needs replacement.

This version makes its way from ToT (canary) to the dev/beta/stable channels
like normal.

If something comes up that causes us to push back the AUE version a release or
more (e.g. significant stability regressions) we can do it easily in
[GoldenEye].
This means the actual AUE is fluid and not set in stone once decided earlier.

Only after newer versions (AUE + 1) make it to the stable channel for 100% of
users do we finally consider the AUE final and people can safely start removing
support for it.

Finally the [public device page] is updated with the AUE version for the board.

## Example Flow

Since the AUE version is not guaranteed to "stick" when the branch is first
cut for the milestone, we cannot safely delete code right away.
Instead, we basically have to wait for 4 to 5 more branches to pass.
This also helps ease backporting of security fixes.

For example, stumpy was originally slated to AUE with R64.
When R63 was branched, ToT updated to R64, and the AUE version was set in
[GoldenEye] which in turn made it public to users via Omaha.

Then R64 was branched, and started to make its way through the channels.
The AUE ended up being delayed to R65, so had we deleted code as soon as
R64 branched, we would have had to revert a lot of things.

Eventually R65 made it to the stable channel for everyone including stumpy.
We still did not clean things up just yet -- only once R66 was promoted to
stable (except for stumpy since it was AUE with R65) did we start cleaning.

Generally we don't go through the codebase and scrub references right away.
We let things get cleaned up as people notice them, or as the cruft gets in
the way of other things (like code refactoring).

## FAQ

### When can we drop support for an AUE board?

Once the version in the stable channel is newer than the board's AUE version.

You cannot delete support as soon the branch is initially cut in case the AUE
version is pushed back.

You can find the AUE version in [GoldenEye] for the board (or the
[public device page]).
You can find the current versions being served by Omaha at:
https://chromiumdash.appspot.com/serving-builds

### When can we remove devices from the hardware lab?

Follow the same guidance in the
[previous question about dropping support](#faq-drop-board).


[GoldenEye]: http://go/goldeneye
[public device page]: https://dev.chromium.org/chromium-os/developer-information-for-chrome-os-devices
