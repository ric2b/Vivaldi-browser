---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: life-of-a-bug
title: Life of a ChromeOS Bug
---

[TOC]

ChromeOS maintains two public issue trackers where you can report bugs and
request features for core ChromeOS functionality. (For details on this issue
tracker, see [Reporting Bugs].

Reporting bugs is great (thank you!), but what happens to a bug report after
you file it? This page describes the life of a bug.

>**Note:** The ChromeOS Public Issue Tracker is intended only for bugs and
feature requests related to core ChromeOS functionality, and is a technical
tool for the Open Source community.

**This isn't a customer support forum.** For support information, see the
[Chromebook](https://support.google.com/chromebook) help center.

*** promo
Here are the key stages in the life of a bug:

1.  A bug is filed, and has the state **_New_**.
2.  The internal team periodically reviews and triages bugs. Bugs are triaged
into one of four **_buckets_**: New, Open, No-Action, or Resolved.
1.  Each bucket includes a number of states that provide more detail on the
fate of the issue.
1.  Bugs marked **_Resolved_** will be included in a future release of ChromeOS.
***

## Bucket details

We use the **Status** field in Issue Tracker to specify the status of an issue
in the resolution process. This is consistent with the definitions specified
in the [Issue Tracker documentation]
(https://developers.google.com/issue-tracker/concepts/issues#status).

### New issues

New issues include bug reports that haven't been acted upon. The two states
are:
*** promo
*   **New:** The bug report hasn't been triaged (that is, reviewed by the
ChromeOS team).
*   **Reassigned to reporter:** The bug report has insufficient information
to act upon. The person who reported the bug needs to provide additional
details before it can be triaged. If enough time passes and no new
information is provided, the bug may be closed by default, as one of the
No-Action states.
***
### Open issues

This bucket contains bugs that need action, but that are still unresolved,
pending a change to the source code.
*** promo
*   **Assigned:** The bug report has been recognized as an adequately detailed
report of a legitimate issue and the bug has been assigned to a specific
contributor to assess and analyze.
*   **Accepted:** The assignee has acknowledged the issue and has started to
work on it.
***
Typically, a bug starts in **Assigned**, and remains there until someone
intends to resolve it, at which point it enters **Accepted**. However, note
that this isn't a guarantee, and bugs can go directly from **Assigned** to
one of the Resolved states.

In general, if a bug is in one of the Open states, the ChromeOS team has
recognized it as a legitimate issue, and a high-quality contribution fixing
that bug is likely to get accepted. However, it is impossible to guarantee
the completion of a fix in time for any particular release.

### No-Action issues

This bucket contains bugs that are deemed to not require any action.
*** promo
*   **Won't Fix (Not reproducible):** The team attempted to reproduce the
behavior described, and was unable to do so. This sometimes means that the
bug is legitimate but simply rare or difficult to reproduce, or there wasn't
enough information to fix the issue.
*   **Won't Fix (Intended behavior):** The team has determined that the
behavior described isn't a bug, but is the intended behavior. This state is
also commonly referred to as _working as intended (WAI)_. For feature
requests, the team has determined that the request isn't going to be
implemented in ChromeOS.
*   **Won't Fix (Obsolete):** The issue is no longer relevant due to changes
in the product.
*   **Won't Fix (Infeasible):** The changes that are needed to address the
issue aren't reasonably possible. This status is also used for issues reported
that can't be handled, typically because it is related to a customized device
or to an external app, or the reporter mistook this tracker as a help forum.
*   **Duplicate:** There was already an identical report in the issue tracker.
Any actual action will be reported on that report.
***
### Resolved issues

This bucket contains bugs that have had action taken, and are now considered
resolved.
*** promo
*   **Fixed (verified):** This bug has been fixed, and has been confirmed by
testing on an official build, but may not necessarily be included in a formal
release. When this state is set, we try to also set a property indicating
which release it was fixed in.
*   **Fixed:** This bug has been fixed (or feature implemented) in a source
tree, but might not yet have been tested against an official build.
***
## Maintenance

The states and lifecycle above are how we generally try to track software.
However, ChromeOS contains a lot of software and gets a correspondingly large
number of bugs. As a result, sometimes bugs don't make it through all the
states in a formal progression. We try to keep the system up to date, but we
tend to do so in periodic _bug sweeps_ where we review the database and make
updates.

[Reporting Bugs]: reporting_bugs.md
