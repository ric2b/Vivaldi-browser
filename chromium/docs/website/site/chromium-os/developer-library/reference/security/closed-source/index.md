---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: closed-source
title: Security guidelines for closed-source components
---

Closed-source third-party components pose particular challenges to ensuring the
overall security and stability of ChromeOS since they:

*   are hard to inspect for security and robustness.
*   are compiled with a toolchain that we don't control.
    *   toolchain-based hardening varies with vendor
    *   availability & usage of tools like asan/msan/tsan is inconsistent
*   make vulnerability response more complex.
    *   dependent on responsiveness of vendor
    *   requires much more coordination even in the ideal case

For these reasons binary third-party components should be avoided as far as
possible.

If there are strong business reasons to include the component, then it **must**
be [strictly isolated](/chromium-os/developer-library/guides/development/sandboxing/). It should:

*   run with minimal privileges, and
*   be walled off from untrusted input.

In addition, we must **require** the vendor to use some means of ensuring
robustness. For example:

*   Fuzzing
*   Static analysis
*   Code audits

The vendor should be asked to document their development practices, with
emphasis on:

*   continuous testing, fuzzing
*   security practices during design and development
*   security validation of the finished product
*   incident response (SLO, notifications, etc.)
*   proactive hardening

The first party code that interacts with the binary component must strictly
verify its inputs and outputs, and fuzzing is strongly recommended.

For firmware binaries, the sandboxing should ideally take the form of hardware
mechanisms that make it impossible for compromised firmware to affect the rest
of the system (eg. IOMMU).
