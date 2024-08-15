---
breadcrumbs:
- - /for-testers
  - For Testers
- - /for-testers/bug-reporting-guidelines
  - Bug Life Cycle and Reporting Guidelines
page_name: triage-best-practices
title: Triage Best Practices
---

[TOC]

## **What is Triage?**

*   A process (often taking the form of a regular meeting) for *making
            decisions about the need, priority, timing, and assignment of work
            requests*.

*   ### Triage boils down to Three Questions:

    1.  Do we want to do this work?
        *   **Pro Tip:** Be definitive. If the work isn't something that
                    we intend on doing (or isn't something we're able to confirm
                    is a real issue), then close the issue out and provide
                    honest and ***courteous*** feedback. It's always best to
                    keep your backlog reflective of the work that you intend on
                    doing.
    2.  How important is it to us (Priority) and when do we want it done
                (Schedule/Milestone)?
        *   **Note:** In the standard Chromium.org workflow Priority and
                    Schedule are very closely related concepts (please see the
                    background on priority below).
    3.  Who should own it?
        *   **Pro Tip:** People can only do a finite amount of work,
                    adding more work to someone's pile won't get it done faster.
        *   **Pro Tip:** It's perfectly fine to mark something as
                    "New" (i.e. no Owner), if the work doesn't need
                    to be scheduled immediately, but please make sure that the
                    work is in the proper component, so that it's clear which
                    team(s) are accountable for it's priority.
        *   **Pro Tip:** It's also advisable to add it to the
                    [Available hotlist](https://issues.chromium.org/hotlists/5438642)
                    to let people know it should be could be worked on.

## How to Think about Priorities

*   P0 - **Emergency**
    *   Requires immediate resolution
*   P1 - **Needed** for current milestone
    *   Teams should use best discretion here, but this bucket should
                generally represent high user impact issues and quality issues
                (e.g. regressions, crashers, security issues, feature
                completeness, etc...).
    *   The test: "Would someone notice (in a bad way) if this issue
                were present in the release?"
*   P2 - **Wanted** for current milestone
    *   Low/ No user impact, can safely be punted from a release.
    *   Most in-development work should have this priority.
*   P3 - **Not time sensitive**
    *   Can be completed at any time, no release targeting required.

## More Pro Tips

*   Set aside a regular time (and place) to Triage.
*   Give yourself enough time to handle your incoming stream of issues
            and make progress on (or at least review) your backlog.
*   Setup a rotation w/ your team members, to share the load. Though
            they are a little more burdensome to administer, they tend to be the
            most sustainable triage efforts, help prevent people from burning
            out, and ensure that the process doesn't break when people take a
            vacation.
*   Wade into and clean-up your backlog. New team members will often
            start from this pool of issues, please be kind and make sure that
            they start by working on valid/ relevant issues.
*   Make sure that things are properly categorized, especially as
            Available issues, so that they are discoverable.
*   **Spend most of your time on issues opened in the last year.**
    *   **Most issues have a half-life.** Issue reports are a static
                reflection of what was... not necessarily what is. That is...
                due to a variety of factors (e.g. code churn, environment
                changes, etc...) as issues age it becomes increasingly likely
                that they will become less reflective of the current state of
                the product.
    *   **People will re-file (and often duplicate) important issues.**
                Many of the old issues that linger often have not been properly
                duped, because the person who fixed the issue wasn't aware of
                the other issue's existence.
