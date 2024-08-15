---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: review-process
title: The ChromeOS security review process
---

This document describes the ChromeOS security review process from the security
team's perspective. It focuses on the process itself, and how to ensure that all
features are properly reviewed before they launch.

This document does not contain actual security review guidance. For that consult
the main [security review HOWTO] document.

## The role of the security review lead

This section is mostly only relevant for security team members and can be safely
skipped by feature owners.

The security review lead represents ChromeOS security at ChromeOS Launch
review meetings. They are responsible for getting features *that are ready to be
reviewed* actually reviewed in time. Their objective should be to support the
ChromeOS team shipping features that improve the experience of ChromeOS users
without compromising the security posture of the product. And remember, security
is a feature too!

If you're fulfilling the role of security review lead, ensure first and
foremost that you're invited to ChromeOS Launch review meetings. Ask the
ChromeOS security leads, or the Launch process lead (currently ovanieva@) to add
you to the meeting.

Make sure to review outstanding launches before each meeting, by querying
[crbug.com] for features aimed for the
current milestone. Identify features that are in *ReviewRequested* state, and
make sure that they are actually ready to be reviewed, with the launch bug
listing a design document that covers security considerations.

Identify as well features that are in *NotReviewed* state. Ping the feature
owner to confirm whether those features are still aimed at the current
milestone, and remind them that until the features are changed to
*ReviewRequested*, they will not be reviewed by the ChromeOS security team.

For features in *NeedInfo* state, consider pinging the feature owner if there
has been no reply on the launch bug for a while (say a week). Remind the feature
owner that launches in *NeedInfo* state cannot be reviewed, or approved, by the
ChromeOS security team until the extra information is provided, or the extra
mitigations are implemented.

Some tooling can make the role of the security review lead easier:

*   Configure *Saved queries* in [crbug.com] to search for launch bugs in each
    of the three above states: *ReviewRequested*, *NotReviewed*, *NeedInfo*.
    This can easily be done in the main [crbug.com] page with the *Saved
    queries* link to the right of the search box. Create saved queries that
    **don't** include a milestone since you'll likely be using these queries for
    several milestones.
*   Use the [crbug.com] *Bulk edit* functionality to quickly update, for
    example, all *NotReviewed* bugs still aimed for the current milestone.

In general, strive to communicate early and often. The days after the first
launch review meeting for a given milestone (check [chromiumdash] for milestone
schedules) are a good time to follow up on the *NotReviewed* features. *Feature
freeze* happens **two weeks** before branch point: the days leading to feature
freeze are a good time to follow up on the *NeedInfo* features to make sure
there's enough time to improve documents or implementation.

### Launch review meetings

The objective of Launch review meetings is for ChromeOS leads to understand the
readiness of features aimed for the current milestone. Some features are hard
requirements for a specific milestone (for example, because they enable key
hardware support for a new device), but many features can be punted to a later
milestone if they are not ready in time for the current one.

Your role as the security review lead attending the launch review meeting is to
provide visibility into the *security* readiness of features aimed for the
current milestone. If your or a team member's security review of a feature
suggests it will not be ready on time, communicate that in the launch review
meeting.

Bear in mind that launch review meetings are not the place for in-depth
discussions about feature security or concerns discovered during the review.
Launch review meetings are status tracking meetings. There are a lot of features
to get through and therefore not a lot of time can be dedicated to individual
features.

You can discuss security concerns on the launch bug, on the design doc, or by
scheduling a meeting specifically to that effect.

### Assigning reviews to other team members

Each feature review is assigned to a specific security team member. We track
assignment using *SECURITY-${username}* labels in [crbug.com]. This bookkeeping
information helps the security review lead to keep the big picture and is useful
for future reference in case the feature review becomes relevant for future
reviews, bugs, etc. You can add the *SECURITY* field as a column to show for the
search list page to get a quick overview of what feature is assigned to which
team member for review, and more importantly which features are pending
assignment.

Consider team members' expertise in the different aspects of ChromeOS when
assigning security reviews. Work with ChromeOS security leads to validate and
confirm assignments, and make sure that folks know which features are assigned
to them. Also, ensure that folks provide feedback to the feature owner before
launch review meetings.

[crbug.com]: https://crbug.com
[security review HOWTO]: ../security_review_howto.md
