---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: hardening-prioritizing-guidelines
title: Prioritizing security hardening bugs
---

There is already great guidance for prioritization of vulnerability fixes in
the [Security Severity Guidelines], but there is not clear guidance in that
document for more preventative hardening tasks. The general guidance for
hardening tasks is that work that will remove an entire class of bugs or
sandboxing work is of higher priority than one-off fixes like those for memory
leaks or fuzzer finds. More granular guidance past that is as follows:

Order of priority:

1.  Exiting the forbidden intersection P1

    *   Not run as root P1

    *   Move out of init namespace P1

    *   Remove capabilities P1

    *   Seccomp for leaf nodes P1

2.  Move into a network namespace P2

3.  “Friday afternoon tasks” - Small bugs P2

4.  Seccomp for a parent process P2

5.  Adding fuzzer(s) P2


## Security-team-specific guidance

If you are not on the security team please use the above guidance or ask for
help from the security team as the following guidance requires security-specific
knowledge. The above list is a good starting point, but depending on other
factors, some issues can be upgraded or downgraded in priority. It is also
possible to skip the list altogether (as it is very general) and only use the
below formula for prioritizing issues. A good heuristic provided by mnissler@

*   severity - how bad would the consequences of exploitation be, and how much
does the hardening task raise the bar
*   risk - how likely is something to get exploited, how reachable is it, what
is the delta by the hardening task
*   generality - how many instances of the issue can we address with the
hardening task
*   cost - how much does it cost to implement the hardening measure and then
maintain it

Using these factors you can approximate: priority = severity \* risk \*
generality \* 1/cost (weighed with appropriate factors).

[Security Severity Guidelines]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/security_severity_guidelines.md
