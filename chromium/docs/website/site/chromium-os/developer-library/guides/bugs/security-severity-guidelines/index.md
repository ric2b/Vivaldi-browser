---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: security-severity-guidelines
title: Chrome OS Security Severity Guidelines
---

These are the severity guidelines for ChromeOS Security Issues.
They are related to the [Severity Guidelines for Chrome Security Issues].
One key difference between the Chrome and ChromeOS security models is that
ChromeOS needs to protect against physically local attackers in certain cases,
such as at the lock screen. For guidance on prioritizing hardening (proactive)
security bugs see [Hardening Prioritizing Guidelines].

[TOC]

## Critical Severity

Critical severity issues breach vital security boundaries. The following
boundaries are considered critical:

*   Verified boot (any attack that persists across reboot)
*   User isolation. One user being able to exploit another user or
    access the encrypted data of another user (e.g. [crbug.com/764540])
*   Native code execution via a remote vector
*   Kernel code execution via a remote vector
*   A lock screen bypass from the lock screen

They are normally assigned priority **Pri-0** and assigned to the current
stable milestone (or earliest milestone affected). For critical severity
bugs, [SheriffBot] will automatically assign the milestone.

> For critical vulnerabilities in the Chrome browser, we aim to release a fix
> no later than seven days after the vulnerability is fixed in the Chrome
> desktop stable channel.
> For all other critical vulnerabilities, we aim release a fix in under 14 days.
> See the [Service Level Objectives] section below for details.

Critical vulnerability details may be made public in 60 days,
in accordance with Google's general [vulnerability disclosure recommendations],
or [faster (7 days)], if there is evidence of active exploitation.

A typical type of critical vulnerability on ChromeOS is
an exploit chain comprising individual bugs that allows
persistent root access on a machine, even in guest mode ([crbug.com/766253]).

For a critical severity exploit chain, releasing a fix that *breaks the chain*,
that is, a fix that resolves one of the links in the exploit chain, is enough
to consider the exploit chain fixed in the stable channel.

Note that the individual bugs that make up the chain will have lower
severity ratings.

## High Severity

A high severity bug allows code execution in a sandboxed process. Bugs
which would normally be critical severity may be rated as high severity
when mitigating factors exist. These include:

*   A specific user interaction is required to trigger the exploit.
    For example, a user must be tricked into typing something into an address
    bar, or the vulnerability requires a user to install an extension or
    Android app.
*   A small fraction of ChromeOS devices are affected due to hardware
    or kernel specific issues.

The above might be generalized to: bugs that allow bypassing of protection
domains are rated as High severity. A Chrome sandbox escapes allows
bypassing the Chrome sandbox. A bug that allows code execution as the
`chronos` user would also be rated High severity. The individual bug that
allows kernel code execution from root (or a regular user) would be rated
High severity. The bug that allows for exploit persistence given root
access would be rated High severity as well.

In general, these are the bypasses we consider High-severity bugs:

*   Javascript (or other web technology, like SVG) to native code execution in a
    renderer process
*   Renderer native code execution to browser process/`chronos` user code
    execution
*   `chronos`/unprivileged user native code execution to `root` (or other more
    privileged user, such as system service users) code execution
*   Code execution in the kernel that requires local privileges to exploit (i.e.
    local kernel privilege escalation)
*   Persistent code execution (turning a transient compromise into one that
    survives a reboot)

Full chain exploits don't always need to break all these layers. For example,
most persistent exploitation chains don't need a kernel bug.

They are normally assigned priority **Pri-1** and assigned to the current
stable milestone (or earliest milestone affected). For high severity bugs,
[SheriffBot] will automatically assign the milestone.

> For high severity vulnerabilities, we aim to release a fix in under 30 days.
> See the [Service Level Objectives] section below for details.

## Medium Severity

Medium severity bugs allow attackers to read or modify limited amounts of
information, or are not harmful on their own but potentially harmful when
combined with other bugs. This includes information leaks that could be
useful in potential memory corruption exploits, or exposure of sensitive
user information that an attacker can exfiltrate. Bugs that would normally
rated at a higher severity level with unusual mitigating factors may be
rated as medium severity.

Examples of medium severity bugs include:

*   Memory corruption in a system service that's not directly
    triggerable from an exposed interface
*   An out of bounds read in a system service

They are normally assigned priority **Pri-1** and assigned to the current
stable milestone (or earliest milestone affected). If the fix seems too
complicated to merge to the current stable milestone, they may be assigned
to the next stable milestone.

## Low Severity

Low severity vulnerabilities are usually bugs that would normally be a higher
severity, but which have extreme mitigating factors or highly limited scope.

Example bugs:

*   Someone with local access to the machine could disable security
    settings without authenticating (i.e. disable the lock screen).

They are normally assigned priority **Pri-2**. Milestones can be assigned
to low severity bugs on a case-by-case basis, but they are not normally
merged to stable or beta branches.

## Service Level Objectives

A *Service Level Objective* (SLO) is a definition of the desired performance of
a service or process for a single metric. In this case, the process is the
fixing of security issues in ChromeOS, and the metric is the time taken to
triage, fix, and release the fix for a security issue.

For ChromeOS security we specify the SLOs for:

*   Time to triage the issue (assign priority, owner, and milestone).
*   Time between updates to the issue.
*   Time to release the fix to users after the fix has landed on the tree.

Moreover, for critical and high severity bugs we also specify the target amount
of time for the entire process, as described in the sections above:

*   Critical severity issues: **14 days** from receiving the bug to releasing
    the fix.
    *   For critical vulnerabilities in the Chrome browser, the fix should be
        be released no later than **seven days** after the vulnerability is
        fixed in the Chrome desktop stable channel.
*   High severity issues: **30 days** from receiving the bug to releasing the
    fix.

### SLO matrices

| Priority | Owner assigned, milestone tagged | Issue updated |
| :--- | :--- | :--- |
| P0  | Within 1 business day | Every business day |
| P1  | Within 1 week | Every week  |
| P2  | Within 1 month | Every month |

| Priority | Fix released |
| :--- | :--- |
| P0  | Requires emergency push unless next stable release can include the fix and happen before the end of the workweek. |
| P1  | Fix must be included in next stable release, no later than two weeks. |
| P2  | Fix on tip-of-tree, considering merging if the fix is straightforward. |

## FoundIn Labels

FoundIn labels are explained in [Security Labels and Components] and designate
which milestones of ChromeOS are impacted by the bug. Multiple labels may be
set on a bug, but the most important one is the earliest affected milestone.

## Security Impact Labels

Security Impact labels are used to identify what release a particular
security bug is present in. This label is derived from FoundIn, which specifies
the earliest affected release channel. Security impact should not normally be
set by humans, except in the case of None.

`Security_Impact-None` is used in the following cases:

*   A bug is only present on ToT and has not made it to any channel
    releases (except possibly the canary channel).
*   A bug is present in a component that is disabled by default.
*   A bug is present in a package only used by a downstream consumer
    of ChromiumOS (like OnHub, WiFi, or Home devices).

## Not Security Bugs

Some bugs are commonly reported as security bugs but are not actually considered
security relevant. When triaging a bug that is determined to not be a security
issue, re-classify as Type=Bug, and assign it to a relevant component or owner.

These bugs are often:

*   Denial of service bugs. See the [Chromium Security FAQ] for more
    information.
*   Enterprise policy bypass bugs. For a good example, see [crbug.com/795434].
    These bugs should be assigned to the Enterprise component and labeled
    Restrict-View-Google.

[Severity Guidelines for Chrome Security Issues]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/severity-guidelines.md
[Hardening Prioritizing Guidelines]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/security/hardening_prioritizing_guidelines.md
[crbug.com/764540]: https://crbug.com/764540
[SheriffBot]: https://www.chromium.org/issue-tracking/autotriage
[vulnerability disclosure recommendations]: https://security.googleblog.com/2010/07/rebooting-responsible-disclosure-focus.html
[faster (7 days)]: https://security.googleblog.com/2013/05/disclosure-timeline-for-vulnerabilities.html
[crbug.com/766253]: https://crbug.com/766253
[Chromium Security FAQ]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/faq.md#TOC-Are-denial-of-service-issues-considered-security-bugs-
[crbug.com/795434]: https://crbug.com/795434
[Service Level Objectives]: #service-level-objectives
[Security Labels and Components]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/security-labels.md#labels-relevant-for-any-type_bug_security
