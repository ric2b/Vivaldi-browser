---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: cpp-security-best-practices
title: Security - Best Practices for C++
---

[TOC]

## The Rule of Two

(see https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md)

The Rule of Two is a simple heuristic for avoiding the worst kinds of
exploitable bugs. Code should never do more than two of the following at the
same time:

1.  **Written in an unsafe language** \
    Memory-unsafe languages like C++ make it far easier to introduce exploitable
    bugs like use-after-free.
2.  **Processes untrustworthy inputs** \
    Code that is exposed to potentially malicious inputs provides an attack
    vector for any bugs that do exist to be exploited.
3.  **Runs without a sandbox** \
    If a bug is successfully exploited, then a sandbox (i.e. an unprivileged
    environment) helps to minimize any damage done by severely limiting what an
    attacker can do after gaining control of a process.

![](images/rule-of-two.png)

<mark><strong>Since Chromium code is usually written in C++, this means that all
code that handles untrustworthy inputs must be sandboxed.</strong></mark>

In practice, any code that parses data from an unreliable source on the internet
or from a potentially malicious data channel like Bluetooth has to be isolated
on an unprivileged process using Mojo. This is the reason why security reviewers
are required on code reviews with `.mojom` files: they want to ensure that the
attack surface of your Mojo interface is small to mitigate the risk of further
privilege escalation.
