---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: fuzzer-bugs
title: Handling bugs found by fuzzers
---

Thanks to the work of the Chrome Security Bugs-- team, the ChromeOS Toolchain
team, and the ChromeOS security team, there is an increasing number of fuzzers
written for ChromeOS targets. This document describes the expectations for
handling bugs found by these fuzzers.

## Ownership

The expectation is that bugs, including security bugs, found by a fuzzer will be
owned and fixed by the team that owns the code being fuzzed. While the ChromeOS
security team handles triaging duties for security bugs, *the responsibility
for fixing those bugs rests on the team that owns the code in question*. Bugs
found by fuzzers in third-party packages should similarly be owned by the team
pulling in that third-party package. The ChromeOS security team maintains a
[list of security-sensitive ChromeOS packages]. Bugs in these packages should
be prioritized by the teams owning or pulling in the package.

This is consistent with the ChromeOS position that teams are ultimately
responsible for all aspects of their features, including security; while the
ChromeOS security team maintains security-critical features and services,
develops tools, and provides advice and support.

## Timeline

Security bugs found by fuzzers are handled just like any other
internally-reported security bug, and should be fixed according to the
[security severity guidelines].

[security severity guidelines]: /security_severity_guidelines.md

[list of security-sensitive ChromeOS packages]: sensitive_chromeos_packages.md
