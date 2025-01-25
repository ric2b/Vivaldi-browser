---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: security-whitepaper
title: Security in ChromeOS
---

[TOC]

## Introduction

This document describes the high-level security architecture of ChromeOS. It
enumerates the principles the ChromeOS team uses to think about security,
explains how these principles apply to different ChromeOS use cases, and
outlines how these principles translate to security features in ChromeOS
devices.

After going through this document the reader should be able to understand what
threats ChromeOS aims to defend against, and how the system implements these
defenses.

## Principles of ChromeOS security

ChromeOS' main security goal is to protect user data and prevent devices from
being compromised, while enabling users to operate their devices as they see
fit. The team strives for **usable security**: providing users with a computing
experience that achieves good security while being pleasant to use. To guide
this work, the ChromeOS security team adheres to the following set of
principles:

1.  **Deploy defenses in depth**: Because no security solution is ever perfect,
    ChromeOS believes that we must deploy a variety of defenses to act as a
    series of bulwarks against an attacker. We make it difficult for an attacker
    to get into the system, but assume that an attacker may gain entry. We place
    additional layers of defense to minimize the amount of damage the attacker
    can cause after compromising the initial layer.
1.  **Make the system secure by default**: A safe system is not an advanced or
    optional feature; it's the default for ChromeOS. All ChromeOS features are
    designed to minimize security risks so the user is generally not required to
    fully understand the security implications of using their device: the OS
    will just pick secure defaults. In particular, users should not need to make
    configuration changes to get a secure experience and we generally avoid
    providing settings that would allow users to inadvertently turn off security
    defenses.
1.  **Don't scapegoat our users**: In real life, people assess their risk all
    the time. The web and Android app ecosystems are really a huge set of
    intertwined, semi-compatible implementations of overlapping standards. It
    is therefore difficult to properly assess risk in the face of such
    complexity, but this is _not_ the users' fault. We work to design our user
    experience so that:
    *   We can send the right signals to users, and keep them informed.
    *   Users only get asked to make security decisions when there is no better
        alternative.
    *   We ask for decisions in a way users comprehend.
    *   We limit the impact of decisions, and make them revocable.
1.  **The perfect is the enemy of the good**: No security solution is perfect,
    and the security landscape is ever-changing. Mistakes will be made,
    unforeseen interactions between complex systems may create security holes,
    and some vulnerabilities won't be caught by pre-release testing. ChromeOS
    does not allow the search for elusive “perfect security” to prevent
    developing and shipping meaningful security improvements now.

### Common use-cases and security requirements

We believe these principles must apply to ChromeOS devices in all common
use-cases, such as:

*   Computing on the couch
*   Computing at work or school
*   Borrowing a device at a library or coffee shop
*   Sharing a device among family members of all ages

These use-cases inform several security requirements, among them:

*   The owner of the device should be able to share their device with users of
    their choice.
    *   This means it's straightforward for a user to allow a co-worker, friend,
        or family member to use their device without compromising the user's
        security.
*   A user should be able to manage their risk of data loss, even in the case of
    device loss or theft. This means two things for users:
    *   First, it should be easy for users to utilize cloud storage and other
        services to have access to their data with high availability.
    *   Second, even if the device is lost or stolen, it should be very
        difficult for another user to steal your data from the device without your
        login credentials.
*   It should be trivial for the user to establish whether the operating system
    is trustworthy.
    *   This means that ensuring a good security posture should be as simple as
        rebooting the device.
*   Users should not have to worry about "patching" their systems for security
    bugs. Updates (including security updates) should be seamless.
    *   This means that maintaining your security posture is simple: reboot and
        you're up-to-date. ChromeOS is designed to boot quickly, which helps
        ensure staying up-to-date isn't a burden for the user.
* TODO: Expand these.
    * Web pages and Android apps only have access to the resources the user
      allows.
    * Attackers can't access ChromeOS devices remotely.
    * Services users access from the device are isolated from each other.

### How our principles inform the threat model

Building secure computing experiences relies on defining a _threat model_, a
description of the malicious actors we are trying to defend against, and their
capabilities. We can describe attackers along different dimensions:

*   Presence: is the attacker remote, local, or do they have access to the
    inside of the device?
    *   A remote attacker can try to compromise the device by setting up a
        malicious webpage, publishing a malicious Android app, or manipulating
        data over the network.
        *   Remote attackers cannot directly interact with the device but a
            subset of them will be in close enough proximity to it (e.g. in the
            same room as the ChromeOS user) to allow exploiting bugs in RF
            device firmware (like WiFi or Bluetooth chips).
    *   A local attacker can interact with the device: connect a malicious USB
        stick, attempt to log in, reboot the device, or just steal the device
        altogether.
    *   A local attacker with unbounded, unsupervised access to the device and
        enough technical skill can disassemble the device, and modify or replace
        software or hardware. At the limit this attacker is hard to defend
        against because a modified device with entirely new electronics but the
        same case would leave nothing for ChromeOS to enforce, but might still
        be undetected by the user.

*   Targeting: is the attacker interested in a specific user or does the
    attacker not care about which users they're trying to compromise?
    *   An _opportunistic adversary_ is an attacker who is trying to compromise
        any user's device or data, without care for which user or organization
        controls the device. This attacker's motivation is usually economic, so
        an opportunistic adversary is likely to deploy attacks which attempt to
        lure users to websites or apps which:
        *   Compromise the user's device (e.g. to mine cryptocurrency, or deploy
            a ransomware attack).
        *   Steal user data (to obtain credit card details, or extort the user
            with personal information).
    *   A _dedicated adversary_ is an attacker that _may_ target a specific user
    or organization. A dedicated adversary may steal a device to recover account
    credentials or data, as opposed to simply selling a stolen device for money.
    The intention is generally to harm the targeted user or organization.
    A dedicated adversary may deploy DNS or other network-level attacks to
    subvert a user's attempt to login (i.e. deploy a *phishing* attack) or
    update their device. A dedicated adversary is assumed to _do anything an
    opportunistic adversary may do_. Defense against an opportunistic adversary
    can aid in defense from a dedicated adversary, but cannot be considered
    complete.

Attackers can also vary in their sophistication and resource availability: from
a malware campaign that attempts to mine cryptocurrency on users' devices to
targeted attacks from an oppressive government.

These dimensions combine into different personifications of attackers:

*   A nosy roommate is a local/physically present, but likely opportunistic and
    not very sophisticated attacker. They might try a few common passwords on
    the user's device.
*   An abusive partner is a local/physically present but *targeted* attacker.
    They might still not be very sophisticated but would be highly motivated.
*   A thief is a physically present attacker who's more likely than not to be
    opportunistic (but could be targeted as part of an operation from an
    intelligence agency or oppresive government).
*   An online malware or ransomware campaign is a remote attacker, likely
    opportunistic and with varying levels of sophistication. Their motivation
    would usually be economic.
*   An intelligence agency or oppressive government can be a remote or local,
    but targeted and likely sophisticated attacker.

Given the use-cases ChromeOS enables, ChromeOS' main threat model is remote:
ChromeOS protects against malicious webpages, and malicious or compromised
Android apps, that attempt to take over the user's device or steal user data.
These attacks can be opportunistic or targeted.

Notwithstanding our main remote threat model, user data protection is paramount
in ChromeOS so the team also considers a subset of local/physically present
threat models: device theft, and _short-term_ opportunistic or dedicated
physically-present attackers.

We specifically use the term "short-term" because a dedicated, resourceful
adversary with unlimited physical access to a ChromeOS device is hard to defend
against. ChromeOS can provide reasonable guarantees against user data
extraction, but the attacker could completely replace the internals of the
device with ones that are backdoored and there would be little ChromeOS could
do against that.

The remainder of this whitepaper describes the defenses ChromeOS deploys
against these attackers, what these defenses mean for users, and how we seek to
fortify our defenses over time.

## Layers of ChromeOS security

![Layers of ChromeOS security](/chromium-os/developer-library/reference/security/security-whitepaper/chromeos_security_layers.png)

**Figure 1**: High-level view of the ChromeOS software stack.

As depicted in Figure 1, we will examine each of the layers of defenses ChromeOS
deploys from the user's primary point of interaction (a browser accessing the
web, an Android app, or a VM) to the device hardware. The principle of
_deploying defenses in depth_ is paramount here, as the layers of defense
combine to provide a system more robust in its protection against threats.

### Security and the Web

The web presents a unique security scenario: accessing a web page via a browser
amounts to executing untrusted code on a user's device. However, this code is
being executed by a system (the web browser) which can take many steps to limit
the privileges that untrusted code may hold. This limitation of privilege is an
important security concept and key to ChromeOS' approach to security.

#### The principle of least privilege

The more privilege code executing on a device has, the greater potential for
harm. _Privilege_, in this context, is the ability to access information or
resources. The _principle of least privilege_ calls for giving code only the
privileges and permissions it needs to do its job, and nothing else. This is a
general security principle and not specific to ChromeOS.

### Security in the Chrome browser

The Chrome browser approaches security the same way that ChromeOS does: by
enforcing the principle of least privilege, deploying several layers of defense,
and having fast, automatic updates.

The Chrome browser implements a _multi-process_ architecture. This allows
separating, in different operating system processes, the parts of the browser
processing untrusted input, like a web page, and the parts of the browser
accessing system resources or user data. This enforces the principle of least
privilege by only giving web pages access to a very limited set of resources
(drawing on a window, executing Javascript). A web page cannot directly access
the user's files.

Web pages are loaded and executing inside a heavily-restricted runtime
environment we call the _Chrome sandbox_. ChromeOS is Linux-based. Chrome on
ChromeOS implements a Linux-based sandboxing environment inherited from Linux
Chrome. Web pages loaded on ChromeOS Chrome have no access to the device's
filesystem, nor to user files. Linux [kernel namespaces] ensure pages have no
access to other processes on the system besides Chrome. Linux secure computing
mode ([seccomp]) ensures pages have limited access to the operating system
kernel.

ChromeOS follows Chrome's six-week release cycle, which allows for quickly
delivering security fixes to users. Critical security bugs result in out-of-band
releases that happen faster than every six weeks.

### Going beyond the browser

#### Protecting user data

ChromeOS devices are intended to be both portable and safely shared. As a
result, protection for user data stored on the local disk is a requirement for
ChromeOS. This is accomplished via system-level encryption of all of the user's
data. This also includes the entire Chrome browser profile so there is no risk
of browser data leaking outside of encrypted storage.

ChromeOS uses ext4 encryption (using [fscrypt]) on newer devices, or the
eCryptfs stacked filesystem (on older devices). In a nutshell, each user gets a
unique "vault" directory and keyset that is created at their first login. The
vault is used as the underlying encrypted storage for their data. The keyset is
tied to their login credentials and is required to allow the system to both
retrieve and store information in the vault. The vault is opened transparently
to the user at login. On logout or reboot, the user's data is locked away again.

ChromeOS devices use a Trusted Platform Module (TPM) chip or an H1 security
chip to protect against brute-force attempts to recover a user's keyset (and
therefore the data it protects), and against attempts to directly extract the
keys from the hardware.

#### Android apps and ChromeOS

Many ChromeOS devices, including tablets, support running Android applications.
Android applications are executed inside a Linux container which restricts
interactions with the rest of the system and access to user data. Moreover,
Android applications are confined to the currently logged-in user session, which
means that Android applications cannot be used to access information belonging
to other users, nor to attack other user accounts. For devices managed by an
organization, the ability to run Android applications can be disabled. ChromeOS
does not allow Android application sideloading (except in a restricted developer
case), so all Android applications installable on ChromeOS come from the Google
Play Store and are covered by Google Play Protect.

The Android runtime in ChromeOS updates with the rest of ChromeOS, at least
once every six weeks. Android has settled on a monthly cadence for releasing
security patches. These patches are normally applied to ChromeOS when they're
released publicly by the Android team. For critical issues the patches will be
included in the next ChromeOS update released on the stable channel.

#### Linux applications and ChromeOS

ChromeOS devices with sufficient storage and computing power may support
running user-installed Linux applications. Linux applications are executed in a
container running inside a hardware-based virtual machine. ChromeOS leverages
the [KVM Linux virtual machine solution] with a ChromeOS specific
[virtual machine monitor] written in the Rust language, which is memory and
thread safe. This provides an effective security barrier between Linux
applications and the ChromeOS system, without sharing the host ChromeOS kernel
with the virtual machine guest.

This functionality provides a Linux environment, in effect a Linux "sandbox",
that is well isolated from the rest of the system and should allow executing
untrusted workloads without exposing the main ChromeOS environment to these
workloads.

#### Hardware root-of-trust and Verified boot

ChromeOS enforces a _hardware_ root-of-trust for the software running on the
device. This means that the integrity and provenance of the software on the
device are ensured by Google. This assurance is tied to the hardware on the
device and cannot be subverted by purely software means. We call this ChromeOS'
_Verified boot_.

What this means in terms of our threat model is that it should be impossible for
a purely remote attacker to persistently modify ChromeOS system software in a
way that's undetectable by our Verified boot stack. That is, any modifications
to system software would be detected and corrected when the device is rebooted,
or the device would refuse to boot.

An attacker needs to be physically present and have control of the device, and
moreover needs to perform a hardware change to the device to be able to run
untrusted code or code not coming from Google. And even in this case, user data
remains safe because it is stored in an [encrypted partition], specific to the
user's account.

Bugs in this enforcement are rewarded as part of our [vulnerability rewards
program](#vulnerability-rewards-program).

#### Root versus the OS kernel

ChromeOS enforces a clear separation between the _root_ or supervisor user in
the underlying Linux system, and the operating system _kernel_. This is another
case of enforcing the principle of least privilege. While the root user has
wide-ranging powers in the system, there is no need for the root user to have as
much privilege as the operating system kernel: there are things that the
operating system kernel can do (like talking to hardware directly, or accessing
certain CPU registers) that the root user does not need to do, at least not in
an unrestricted way.

The Linux root user is also not exposed in the ChromeOS user interface, which
means that ChromeOS does not have all-powerful (human) user accounts that would
require this level of access to the system.

On ChromeOS, kernel module loading is restricted to modules loaded from the
root partition covered by Verified boot. This means that while the root user can
modify the kernel at runtime by loading kernel modules, these modules are still
trusted code. Module loading from outside the verified root partition is
disallowed. ChromeOS also restricts kernel functionality exposed to processes
running as root using [seccomp].

By enforcing the kernel/root barrier, we ensure that an attacker that attempts
to compromise the kernel will need to exploit a kernel privilege escalation bug
in addition to all the other bugs that they needed to exploit to obtain root
code execution, making a kernel compromise less likely.

## What ChromeOS security means for you

### What ChromeOS security means for users

The ChromeOS team strives to provide a computing experience that's _secure by
default_. That is, using a ChromeOS device to get things done, or browse the
web, or run Android apps should not require the user to be a security expert.
Importantly, ChromeOS deploys defenses against both persistent compromises
(i.e. those which could survive a reboot of the device) and non-persistent
compromises which exist only within an active session.

The Chrome browser in ChromeOS brings security features like renderer process
sandboxing, Safe Browsing, and Site Isolation, which provide protections against
non-persistent compromises: it's harder for malicious webpages to compromise the
OS. These features are enabled by default, and protect all Chrome users,
including those on ChromeOS. For ChromeOS users, Verified boot means increased
protection from persistent compromises: even in the unlikely case that a device
is successfully attacked by a malicious webpage or potentially harmful
application, all it takes to go back to a known-good state is a simple reboot:
no need to reinstall anything. This makes ChromeOS devices significantly less
suceptible to viruses and malware.

ChromeOS users can share the device with others without worrying that they will
have access to your data: once you log out, your data is encrypted with a key
tied to your password.

### What ChromeOS security means for organizations

The ability of ChromeOS to enforce a known-good state for the software stack on
a ChromeOS device simplifies the management of ChromeOS devices for
organizations. The same features that ensure code integrity on a ChromeOS
device for consumers ensure that a fleet of ChromeOS devices can be reliably
managed.

The computing environment provided by ChromeOS, where the base software stack
is verified on each boot, and software installation comes from curated sources
(the Chrome Web Store and the Google Play Store) prevents users from installing
software from untrusted sources. ChromeOS' approach is to be proactive rather
than reactive: be strict about what software can run on the device, and have the
operating system enforce this. With ChromeOS, endpoint integrity is provided by
default.

## Security boundaries

ChromeOS implements the principle of _defense in depth_, where we deploy more
than one layer of defense between untrusted content or code and sensitive data
or device access. These layers define security boundaries. Bypasses of these
boundaries are considered security bugs.

*   Chrome renderer process to Chrome browser process: this boundary is
    traversed only with Chrome IPC or Mojo. It should not be trivial for a
    Chrome renderer process to directly manipulate or control the Chrome browser
    process or the rest of the system. This is enforced by the Chrome sandbox.
*   Chrome browser process (running as user `chronos`) to system services: this
    boundary is traversed with D-Bus or Mojo. It should not be possible for the
    Chrome browser process to directly manipulate a system service or directly
    access system resources (e.g. firewall rules or USB devices). This is
    enforced by UID separation.
*   ARC++ container to Chrome browser or ChromeOS system: this boundary is
    traversed with Mojo IPC. It should not be possible for the container to
    directly manipulate resources outside of the container. Trust decisions
    should always be made outside of the container.
*   Userspace processes to kernel: this boundary is traversed by system calls.
    It should not be possible for userspace to gain untrusted kernel code
    execution (this includes loading untrusted kernel modules). [Seccomp] is
    used to secure this boundary.
*   Kernel to firmware: it should not be possible for a kernel compromise to
    result in persistent, undetectable firmware manipulation. This is enforced
    by Verified boot.

## Vulnerability rewards program

Because no defense is ever perfect, we invite the broader security community to
continually test our work. In particular, ChromeOS is part of the broader
[Google Vulnerability Rewards Program], which rewards external security
researchers for finding and notifying Google of vulnerabilities.

[kernel namespaces]: https://en.wikipedia.org/wiki/Linux_namespaces
[seccomp]: https://en.wikipedia.org/wiki/Seccomp
[Seccomp]: https://en.wikipedia.org/wiki/Seccomp
[KVM Linux virtual machine solution]: https://www.linux-kvm.org/page/Main_Page
[virtual machine monitor]: https://chromium.googlesource.com/chromiumos/platform/crosvm
[encrypted partition]: #protecting-user-data
[Google Vulnerability Rewards Program]: https://www.google.com/about/appsecurity/chrome-rewards/
[fscrypt]: https://www.kernel.org/doc/html/v6.1/filesystems/fscrypt.html
