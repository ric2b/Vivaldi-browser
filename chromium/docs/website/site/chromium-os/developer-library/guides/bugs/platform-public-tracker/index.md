---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: reporting-chromeos-bugs-public-tracker
title: Reporting ChromeOS bugs in the buganizer public tracker
---

[TOC]

Thank you for your interest in **ChromeOS!**

You can help improve ChromeOS by reporting issues and feature requests in the
[ChromeOS Public Tracker](#bug-queues). The ChromeOS Public Tracker
contains a list of pending technical tasks across a variety of topics,
information relevant to those tasks, and information about progress on those
tasks, including which ones might get worked on in the short term.
> **Note:** To learn how to use Issue Tracker, see the
[Issue Tracker developer documentation]. Also, there are no guarantees that
any particular bug can be fixed in any particular release.

The ChromeOS Public Tracker isn't a customer support forum. For support
information, see the [Chromebook](https://support.google.com/chromebook)
help center.

To see what happens to your bug after you report it, read [Life of a Bug].

## Should you report a bug?

The bug tracker is used by the people who develop ChromeOS to track our work
and to collaborate with each other. Bug reports and the conversations within
them are highly technical.

> Bug reports are NOT a support forum. If you want user-focused ways to let
us know about a problem or get an answer to a question, a bug report is
probably not the best choice. There are other things you can do that will
probably work better.

*   The [Chromebook Central Help Forum](https://productforums.google.com/forum/#!forum/chromebook-central)
    is an active, searchable community for user discussion of bugs and feature
    requests. It is a great place to ask for help, and there’s a good chance
    that your question has already been asked (and answered, hopefully).
*   **Feedback Reports** are the primary mechanism by which ChromeOS users can
    send feedback about the project. Reports are clustered into categories that
    are used to identify larger issues, but they are typically not individually
    reviewed. Submitted reports are not publicly accessible.

    To submit a feedback report, type Alt+Shift+i or click Report an issue
    within the Help submenu while logged in. Feedback reports include selected
    system logs if the Send system and app information, and metrics checkbox is
    checked. You can click the links within the checkbox text to see the
    information that will be attached to the report. See the
    [official feedback documentation](https://support.google.com/chromebook/answer/2982029)
    for more information.

    Before reporting a bug
*   [Search for your bug](https://issuetracker.google.com/issues?q=componentid:931982%2B)
    to see if anyone has already reported it. Don't forget to search for all
    issues, not just open ones, as your issue might already have been reported
    and closed. To help you find the most popular results, sort the result by
    the number of stars.
*   If you find your issue and it's important to you, [star it]
    (https://developers.google.com/issue-tracker/guides/subscribe
    #starring_an_issue)! The number of stars on a bug helps us know which bugs
    are most important to fix.
*   Proceed to filing a bug only if no one else has reported your bug.

## Filing a bug

1. Use the right tracker. ChromeOS bugs should be reported in the Chrome
   OS Public Tracker. If you file issues elsewhere (e.g. crbug.com), there is
   a good chance we won’t see them.
2. [Browse for the correct component](https://issuetracker.google.com/components),
   such as
   [Cellular](https://issuetracker.google.com/issues?q=componentid:960640)
   or [Graphics](https://issuetracker.google.com/issues?q=componentid:960692),
   and fill out the provided template. Or select the desired bug queue from
   the tables below.
>**Tip:** Some components contain subcomponents, like **Gaming &gt; Steam**
and **Gaming &gt; Stadia**.

3.  Include as much information in the bug as you can, following the
    instructions for the bug queue that you're targeting. A bug that simply
    says something isn't working doesn't help much, and will probably be
    closed without any action. The amount of detail that you provide, such
    as log files, repro steps, and even a patch set, helps us address your
    issue.

***promo
In particular, a good bug report would be one that contains:

    * A concise 1-2 sentence summary.

    Good: “Video goes black every 5 seconds on some Youtube videos.”
    Bad: “YouTube is glitching! Fix it!”.

    * What you were doing when the problem happened. Be as detailed as
    possible.

    Good: “I was in class in Google Meet, and the teacher sent us a link to
    the video at &lt;url&gt;”
    Bad: “The thing the teacher sent us didn’t work.”

    * What did you see? Again, be as detailed as possible. Were there any
    error messages?
    * Did it just happen once, always or only sometimes? Reproduction steps
    are the single most valuable thing we need.
    * Screenshots, videos, or any other info you can think of.
    Just remember that bugs are public, so don’t include anything you’re not
    comfortable with being visible to the public.

***

4. Consider also filing a feedback report. Feedback reports contain diagnostic
   info we can use to investigate. If you file a feedback report as well,
   please mention it in the bug so we know to look for it and can link the two
   together.

## Bug queues

The ChromeOS Public Tracker is used by developers to track ChromeOS bugs
and feature requests. New issues submitted here are periodically triaged and
examined by component owners. It has a variety of subcomponents in a number of
categories related to ChromeOS. There are subcomponents for Apps, I/O,
Connectivity, Services, and System.

### Security

If you want to report a security bug, please follow [Reporting Security Bugs](https://dev.chromium.org/Home/chromium-security/reporting-security-bugs).

The Security component in the ChromeOS Public Tracker is for issues in Chrome
OS userland daemons like cryptohome, chaps, attestation, etc. These are also
issues related to hardware-backed security features like user data encryption,
keystore and attestation. All other security issues have to be privately and
securely opened using the link above, please do not file them here.

### Connectivity

If you find an issue that impacts an aspect of ChromeOS connectivity, file
your bug in one of these components.

[Browse all Connectivity issues](https://issuetracker.google.com/issues?q=componentid:953285+)

| Browse | File |
|-------------|------------|
| [Bluetooth](https://issuetracker.google.com/issues?q=componentid:960690)|[Report Bug](https://issuetracker.google.com/issues/new?component=960690)|
| [Cellular](https://issuetracker.google.com/issues?q=componentid:960640)|[Report Bug](https://issuetracker.google.com/issues/new?component=960640)|
| [Network](https://issuetracker.google.com/issues?q=componentid:960667)|[Report Bug](https://issuetracker.google.com/issues/new?component=960667)|
| [WiFi](https://issuetracker.google.com/issues?q=componentid:960641)|[Report Bug](https://issuetracker.google.com/issues/new?component=960641)|

### Services

If you find an issue that impacts ChromeOS core services, file a bug in one of
these components.

[Browse all Services issues](http://issuetracker.google.com/issues?q=componentid:953286+)

| Browse | File |
|-------------|------------|
| [Install](https://issuetracker.google.com/issues?q=componentid:960642)|[Report Bug](https://issuetracker.google.com/issues/new?component=960642)|
| [Update](https://issuetracker.google.com/issues?q=componentid:960669)|[Report Bug](https://issuetracker.google.com/issues/new?component=960669)|
| [Fingerprint](https://issuetracker.google.com/issues?q=componentid:960643)|[Report Bug](https://issuetracker.google.com/issues/new?component=960643)|
| [Provisioning](https://issuetracker.google.com/issues?q=componentid:960539)|[Report Bug](https://issuetracker.google.com/issues/new?component=960539)|
| [Printing](https://issuetracker.google.com/issues?q=componentid:960591)|[Report Bug](https://issuetracker.google.com/issues/new?component=960591)|

### Multimedia

If you find an issue that impacts any ChromeOS media, file a
bug in one of these components.

[Browse all Multimedia issues](http://issuetracker.google.com/issues?q=componentid:960592+)

| Browse | File |
|-------------|------------|
| [Audio](https://issuetracker.google.com/issues?q=componentid:960644)|[Report Bug](https://issuetracker.google.com/issues/new?component=960644)|
| [Camera](https://issuetracker.google.com/issues?q=componentid:960645)|[Report Bug](https://issuetracker.google.com/issues/new?component=960645)|
| [Display](https://issuetracker.google.com/issues?q=componentid:960617)|[Report Bug](https://issuetracker.google.com/issues/new?component=960617)|
| [Other Media](https://issuetracker.google.com/issues?q=componentid:960542)|[Report Bug](https://issuetracker.google.com/issues/new?component=960542)|
| [Video](https://issuetracker.google.com/issues?q=componentid:960541)|[Report Bug](https://issuetracker.google.com/issues/new?component=960541)|

### Peripherals

If you find an issue that impacts ChromeOS peripherals, file a bug in one of
these components.

[Browse all Peripherals issues](http://issuetracker.google.com/issues?q=componentid:960593+)

| Browse | File |
|-------------|------------|
| [Input](https://issuetracker.google.com/issues?q=componentid:960435)|[Report Bug](https://issuetracker.google.com/issues/new?component=960435)|
| [Sensors](https://issuetracker.google.com/issues?q=componentid:960545)|[Report Bug](https://issuetracker.google.com/issues/new?component=960545)|
| [Storage](https://issuetracker.google.com/issues?q=componentid:960436)|[Report Bug](https://issuetracker.google.com/issues/new?component=960436)|
| [USB](https://issuetracker.google.com/issues?q=componentid:960647)|[Report Bug](https://issuetracker.google.com/issues/new?component=960647)|

### Core Systems

If you find a core ChromeOS System issue, please file it in one of the
following components.

[Browse all Core Systems issues](http://issuetracker.google.com/issues?q=componentid:192633%2B)

| Browse | File |
|-------------|------------|
| [ARC++](https://issuetracker.google.com/issues?q=componentid:1011095)|[Report Bug](https://issuetracker.google.com/issues/new?component=1011095)|
| [Factory](https://issuetracker.google.com/issues?q=componentid:960438)|[Report Bug](https://issuetracker.google.com/issues/new?component=960438)|
| [Firmware](https://issuetracker.google.com/issues?q=componentid:960439)|[Report Bug](https://issuetracker.google.com/issues/new?component=960439)|
| [Graphics](https://issuetracker.google.com/issues?q=componentid:960692)|[Report Bug](https://issuetracker.google.com/issues/new?component=960692)|
| [Hardware](https://issuetracker.google.com/issues?q=componentid:960596)|[Report Bug](https://issuetracker.google.com/issues/new?component=960596)|
| [Hardware-backed Security](https://issuetracker.google.com/issues?q=componentid:960649)|[Report Bug](https://issuetracker.google.com/issues/new?component=960649)|
| [Kernel](https://issuetracker.google.com/issues?q=componentid:960547)|[Report Bug](https://issuetracker.google.com/issues/new?component=960547)|
| [Memory](https://issuetracker.google.com/issues?q=componentid:960595)|[Report Bug](https://issuetracker.google.com/issues/new?component=960595)|
| [Performance](https://issuetracker.google.com/issues?q=componentid:960546)|[Report Bug](https://issuetracker.google.com/issues/new?component=960546)|
| [Power](https://issuetracker.google.com/issues?q=componentid:960673)|[Report Bug](https://issuetracker.google.com/issues/new?component=960673)|
| [Virtualization](https://issuetracker.google.com/issues?q=componentid:960622)|[Report Bug](https://issuetracker.google.com/issues/new?component=960622)|

[Issue Tracker developer documentation]:
https://developers.google.com/issue-tracker/
[Life of a Bug]: /chromium-os/developer-library/reference/bugs/life-of-a-bug/
