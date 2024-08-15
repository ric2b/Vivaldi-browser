---
breadcrumbs:
- - /for-testers
  - For Testers
page_name: bug-reporting-guidelines
title: Bug Life Cycle and Reporting Guidelines
---

[TOC]

## Important links

### Chromium (the web browser)

*   Report bugs at <https://issues.chromium.org/new>
*   Specifically:
    *   [Bug Reporting Guidelines for the Mac & Linux
                builds](/for-testers/bug-reporting-guidlines-for-the-mac-linux-builds)
                (with links to known issues pages)
    *   [Instructions for reporting
                crashes](/for-testers/bug-reporting-guidelines/reporting-crash-bug).
    *  View existing bugs at <https://issues.chromium.org/issues?q=status:open>

### Chromium OS (the operating system)

*   Report bugs at [Chromium Issue Tracker](https://issues.chromium.org/new)
*   View existing bugs at [Chromium OS
            issues](https://issues.chromium.org/issues?q=status:open%20customfield1223084:%22ChromeOS%22)

You need a [Google Account](https://www.google.com/accounts/NewAccount)
associated with your email address in order to use the bug system.

## Bug reporting guidelines

*   If you're a web developer, see [How to file a good
            bug](https://developers.google.com/web/feedback/file-a-bug)
*   Make sure the bug is verified with the latest Chromium (or Chrome
            canary) build.
*   If it's one of the following bug types, please provide some further
            information:
    *   **Web site compatibility problem:** Please provide a URL to
                replicate the issue.
    *   **Hanging tab: See [Reporting hanging tab
                bugs](/for-testers/bug-reporting-guidelines/hanging-tabs).**
    *   **Crash: See [Reporting crash
                bugs](/for-testers/bug-reporting-guidelines/reporting-crash-bug).**
*   Provide a high-level problem description.
*   Mention detailed steps to replicate the issue.
*   Include the expected behavior.
*   Verify the bug in other browsers and provide the information.
*   Include screenshots, if they might help.
*   If a bug can be [reduced to a simplified
            test](/system/errors/NodeNotFound), then create a simplified test
            and attach it to the bug.
*   Additional [Bug Reporting Guidlines for the Mac & Linux
            builds](/for-testers/bug-reporting-guidlines-for-the-mac-linux-builds).
*   Additional Guidelines for [Reporting Security
            Bugs](/Home/chromium-security/reporting-security-bugs).

## Release block guidelines

*   [Release Block Guidelines](/issue-tracking/release-block-guidelines)

## Triage guidelines

*   [Triage Best
            Practices](/for-testers/bug-reporting-guidelines/triage-best-practices)

## Hotlists

Hotlists are used to help the engineering team categorize and prioritize the bug
reports that are coming in. Each report can (and should) have multiple hotlists.

For details on hotlists used by the Chromium project, see [Chromium Bug
Hotlists](/for-testers/bug-reporting-guidelines/chromium-bug-hotlists).

## Status

To better understand the various fields and statuses, please visit:
[Issues Overview](https://developers.google.com/issue-tracker/concepts/issues)

## Bug life cycle

*   When a bug is first logged, it is given **New** status.
*   The `Unconfirmed` hotlist is removed when the bug has been verified as a Chromium bug.
*   Once a bug has been picked up by a developer, it is marked as
            **Assigned**.
*   A status of **In Progress (Accepted)** means a fix is being worked on.
*   A status of **Fixed** means that the bug has been fixed, and
            **Verified** means that the fix has been tested and confirmed.
            Please note that it will take some time for the "fix" to make it
            into the various channels (canary, beta, release) - pay attention to
            the milestone attached to the bug, and compare it to
            chrome://version.
*   A bug on the [Unconfirmed hotlist](https://issues.chromium.org/savedsearches/6680973)
            is a newly logged bug that hasn't been confirmed
*   A bug on the [Available hotlist](https://issues.chromium.org/savedsearches/6681397)
            has been triaged and is waiting for a fix.

## Helping with bug triage

Read <http://www.chromium.org/getting-involved/bug-triage> if you're interested
in helping with bug triage.

Infrastructure and build tools

If you find an issue with our infrastructure or build tools, please file the
ticket using the Build Infrastructure template:

*   <https://code.google.com/p/chromium/issues/entry?template=Build%20Infrastructure>
