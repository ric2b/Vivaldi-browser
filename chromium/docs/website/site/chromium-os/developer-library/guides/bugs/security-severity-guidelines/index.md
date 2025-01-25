---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: security-severity-guidelines
title: ChromeOS Security Severity Guidelines
---

This document provides the guidelines we use to assign a
[severity level](?tab=t.0#heading=h.ql1quskh0n)
to security issues and the [Service Level Objectives](?tab=t.0#heading=h.a111dazd7v71)
for fixing vulnerabilities.

## ChromeOS Security Model

ChromeOS is a hardened operating system intended to run as part of a
complete and verticallyintegrated hardware/software stack such as
Chromebooks,Chromeboxes, etc. When ChromeOS isbooted in verified mode
on a supported production device, ChromeOS is not a general purpose
operating system.

Specifically:

* No untrusted native code execution on ChromeOS.
    * The ChromeOS image booted in verified mode is considered trusted
code, i.e., code we build and release.
    * We rely on a number of different sandboxing technologies including
seccomp, virtual machines, containers, etc. as well as other hardening
approaches to isolate untrusted code from the trusted compute base.
    * In a default configuration, any third party applications must
originate from a trusted source such as the Playstore (and are run in
one of the native VM-based run-times).
* Single trusted user active on the device.
    * Only one person can be active on a ChromeOS device (the user
may have multiple profiles)
    * The active user is considered trusted in terms of their own
data. However a ChromeOS device can be potentially shared by adversarial
users, including the device user and the device owner. Additionally
a device may be used to access DRM content not owned by the device user.
    * We guarantee user isolation and protection of all user data even
when a device user is not active on the device.
* No persistent compromise
    * ChromeOS is designed to be resilient to attacks and we guarantee
that users can get back to a known good state via reboot or a recovery.
    * Device recovery ensures the device is restored to factory default
state.

### Vulnerabilities v. Security Bugs

A security vulnerability is a bug that can be exploited by an attacker
to impact the confidentiality, integrity, and availability of a system.

A security bug is a bug that crosses a security boundary. The following
are examples of boundaries we consider critical:

* Verified boot (code execution that persists across reboot)
* User isolation (privileged code execution that survives sign out but
not necessarily reboot). One user being able to exploit another user or
access the encrypted data at rest of another user
(e.g. [crbug.com/764540](https://crbug.com/764540))
* Native code execution via a remote vector
* Kernel code execution via a remote vector
* A lock screen bypass (or anything an attacker can do short of opening
the case) from the lock screen

If an attacker cannot directly exploit the bug on ChromeOS or if
there’s no direct security impact to triggering the bug, we would
not consider the bug to be a vulnerability.

However security bugs may be used as part of an exploit chain. We
will consider any security bug that is reported as part of an
exploit chain to be a vulnerability, though the severity of each
bug in the chain may differ.

Security bugs are in scope for our Vulnerability Rewards Program
but will be rewarded at the Panel’s discretion. Security bugs
will have type _bug_ rather than _vulnerability_ in our Issue
Tracker.


### Social Engineering and User Interaction

Security vulnerabilities may be triggered by user interaction.
However, an attacker has to be able to control the user interaction.
If the attacker must rely on social engineering to get a user to
perform a certain action that the user would not otherwise perform,
we don’t consider that a vulnerability.

Social engineering attacks are out of scope for our Vulnerability
Rewards Program.


### Developer Mode

ChromeOS can be run as a general purpose OS when Developer mode
is configured. Anyvulnerability that can only be exploited in Developer
mode would be considered a security bug rather than a vulnerability.


### Abuse Related Methodologies

Abuse related methodologies are not security vulnerabilities. Examples
of abuse related methodologies might involve bypass of enterprise
policies on managed devices, unauthorized access to special offers, etc.

Abuse related methodologies can be reported via the ChromeOS
Vulnerability Rewards Program but reward will be at the discretion of
the VRP Panel.


### Managed Devices

Although we consider device users as trusted, we acknowledge managed
device users may try to unenroll their devices or potentially exfiltrate
provisioned enterprise secrets such as user or device certificates.

If a user is able to unenroll a managed device or exfiltrate secrets
or other confidential device data, we consider that a vulnerability.


### ChromeOS Flex

ChromeOS Flex is a version of ChromeOS intended to run on
[select supported third party hardware
platforms](https://support.google.com/chromeosflex/answer/11513094?hl=en).
Since we do not control the security of the hardware/firmware
layers on these devices, we cannot provide the same security
guarantees that we do on Chromebooks.

ChromeOS Flex devices rely on the standard Linux UEFI Secure Boot
flow and do not have a Google Security Chip, the lynchpin of coreboot
verified boot on Chromebooks.

Third party hardware/firmware vulnerabilities impacting ChromeOS
Flex can be reported through our VRP and will be treated like any
other vulnerability in a third party component.


### Third Party/Dependent Software Components

Since we are not a general purpose OS, many vulnerabilities in
software components we ship (including the Chrome browser and
Android OS) cannot be exploited on ChromeOS, or may not have the
same severity level.

Unless a vulnerability in a third party component or dependency
can be exploited on ChromeOS in a default verified boot configuration,
we consider these issues to be security bugs rather than
vulnerabilities if they are present in code we ship.

We routinely update third party packages to ensure defense-in-depth.


### Toolchain and Code Hardening

We compile with clang and take advantage of compiler hardening
options such as FORTIFY_SOURCE. We also use MiraclePtr and other
techniques to harden code. Many of these techniques will result
in a crash that prevents an attacker from exploiting a bug.
We do not consider bugs that are mitigated via these techniques
to be security bugs.


### Crashes and denial of service bugs

We don’t consider bugs that result in a process crash, system
crash or more broadly denial of service attacks that can be restored
with a reboot to be vulnerabilities. ChromeOS was designed with this
in mind and offers extremely fast reboots and restores state
such as browser tabs.

See the [Chromium Security FAQ](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/faq.md#TOC-Are-denial-of-service-issues-considered-security-bugs-)
for more information on denial of service bugs in Chrome.


### Fuzzers

Fuzzer found bugs are not considered security bugs unless they
are demonstrated to be vulnerabilities e.g. attacker reachable
and controllable.


### Graphics Stack

The GPU is not directly exposed to untrusted code on ChromeOS
in verified boot mode. All code that runs on the GPU is
validated/sanitized or originates from a native process. A bug
that allows untrusted code to run natively on the GPU would be
considered a vulnerability.

In the case of graphics virtualization, the Vulkan or Venus
renderer process is a sandboxed process designed to be an extension
of the guest process it serves. If an attacker is able to compromise
the guestOS and trigger a security bug (e.g. UAF, buffer
overrun, etc) in the renderer (e.g. via UAF, buffer overrun, etc),
the attacker may be able to leak data from their own process. However,
since we consider the process within the guest boundary we would not
consider this a vulnerability. These issues may still be considered
security bugs since they may represent a step in an exploit
chain that results in code execution.


### Exploit Chains and Device Takeover

An exploit chain is composed of one or more vulnerabilities and
security bugs that allow an attacker to achieve a security impact.
Device takeover refers to persistent root access on a machine,
even in guest mode ([crbug.com/766253](https://crbug.com/766253)).

An exploit chain is considered mitigated, when one of the issues
in the chain is mitigated such that it _breaks the chain_.


## Security Severity Levels

ChromeOS severity guidelines are aligned with Google wide
severity guidelines.

* [Critical Severity](?tab=t.0#heading=h.369tyvc07ye6)
* [High Severity](?tab=t.0#heading=h.k6pwverqzms)
* [Medium Severity](?tab=t.0#heading=h.snhrhvgk9uzq)
* [Low Severity](?tab=t.0#heading=h.me1tqhvvhaff)
* [Service Level Objectives](?tab=t.0#bookmark=id.q2b3g8ykveiu)


### [Critical Severity (S0)](?tab=t.0#heading=h.369tyvc07ye6)

If the vulnerability were exploited, damage could be substantial.

Vulnerabilities that allow an attacker to achieve persistent
arbitrary code execution on the device or exfiltrate unencrypted
user data in verified boot is rated as Critical severity.


### [High Severity (S1)](?tab=t.0#heading=h.k6pwverqzms)

If the vulnerability were exploited, damage could be considerable.

Vulnerabilities that allow an attacker to take control over a
sandboxed process are rated as High severity.


### [Medium Severity (S2)](?tab=t.0#heading=h.snhrhvgk9uzq)

If the vulnerability were exploited, damage would be limited.

Medium severity vulnerabilities allow attackers to read or
modify limited amounts of information, or security bugs that
are potentially more harmful as part of an exploit chain.


### [Low Severity (S3)](?tab=t.0#heading=h.me1tqhvvhaff)

We track these issues as bugs rather than vulnerabilities.


### [No Security Impact (NSI)](?tab=t.0#heading=h.fz1n9xtwvi67)

Some bugs are commonly reported as security bugs but are not
actually considered security relevant on ChromeOS. These may
be closed as “Won’t Fix”.


###
Service Level Objectives

A _Service Level Objective_ (SLO) is a definition of the desired
performance of a service or process for a single metric. In this
case, the process is the fixing of security issues in ChromeOS.

| Severity | SLO |
| -------- | --- |
| S0 | **Fix backported to current milestone/s**, emergency push may be required, may bypass or have accelerated dev/beta soak, CVE assigned if one doesn’t already exist. |
| S1 | **Fix backported to current milestone/s**, fix released as part of a planned weekly respin, accelerated dev/beta soak, CVE assigned if one doesn’t already exist. |
| S2 | **Fix in ToT with no backport to current milestone/s**, fix released via regular monthly release process, full dev/beta soak, CVE assigned if one doesn’t already exist. |
| S3 | May be disclosed before the fix is released, should get fixed or closed within the year, no CVE assigned. |


### Disclosure Timelines

All issues reported to the ChromeOS Vulnerability Rewards Program
will be made public per specified timelines.