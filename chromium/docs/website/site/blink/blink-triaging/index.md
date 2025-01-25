---
breadcrumbs:
- - /blink
  - Blink (Rendering Engine)
page_name: blink-triaging
title: Component:Blink bug labeling rotation
---

[Document with tips for bug
labeling](https://docs.google.com/document/d/1l9XehKEHAJu3-LnWDdXl8-t-8rz9dk8dy1bEI4zzUOU/edit)

## [Mapping of labels to owners and teams](https://docs.google.com/spreadsheets/d/19JEFMvsxD3eThyGiJRqAjcpx362LHUDdVzICAg7TYZA/edit#gid=0)

## Goal

Deliver Blink bugs in crbug.com to engineers who are responsible for the bug
area by adding Blink&gt;*Foo* components.

## Example Instructions

We don't have a common instruction yet. The below is an example, and you don't
need to follow it. Important points are:

*   Reduce the number of [Component=Blink
            bugs](https://issues.chromium.org/issues?q=customfield1222907:Blink%20status:open)
*   Newer bugs are important.

### Task 1: Handling Component=Blink issues (mandatory, daily)

1) Search for ["Component=Blink -Hotlist=CodeHealth -Needs=Feedback
-Needs=Reduction"](https://issues.chromium.org/issues?q=customfield1222907:Blink%20status:open%20-hotlistid:5433459%20-customfield1223031:Needs-Reduction%20-customfield1223031:Hotlist-CodeHealth)

2) Read the issue description and comments and add Blink&gt;*Foo* component and
remove Blink component, if the area/ownership is clear. Otherwise:

*   If the issue has not enough information, ask for additional
            information, add Needs-Feedback (hotlist 5433459), and add your email address to
            Cc field.
    Add a Next-Action value set to 14 days from the current date.
    You're responsible for this bug. You should handle the bug until you
    identify Blink areas or feedback timeout.
*   If the issue doesn't seem to be reproducible, but plausible, add
            Needs-TestConfirmation (hotlist 5432926).
*   If the reproduction is too complex to understand the culprit area,
            add Needs-Reduction.
*   If you understand the culprit, but can't find an appropriate
            Blink&gt;*Foo* component (eg. by looking at similar bugs resolved in
            the not-too-distant past), email
            [crblink-rotation@](https://groups.google.com/a/chromium.org/forum/#!forum/crblink-rotation)
            (and/or add Hotlist-BlinkNoLabel, this is TBD). You should find an
            owner if the bug looks serious.

**Task 2: Handling Component=UI issues (mandatory, daily)**

1) Search for untriaged
[Component=UI](https://issues.chromium.org/issues?q=customfield1222907:UI%20status:open%20-status:Assigned%20-hotlistid:5433459%20-type:feature_request%20-type:task)
bugs

2) Read the issue description and add comments or move to sub-components of UI
or other components (including Blink sub-components as appropriate). Set
priorities as needed.

### Task 3: Handling issues without Component: field (optional)

Do the same things as task 1 and task 2 for issues without Component: field. If
an issue isn't related to Blink, add appropriate non-Blink components such as
Component:`UI`, Component:`Internals`.

*   [All bugs without
            components](https://issues.chromium.org/issues?q=customfield1222907:None%20status:open%20-status:Assigned%20-hotlistid:5433459%20-hotlist:%205562294)
*   [All bugs without
            owners](https://issues.chromium.org/issues?q=status:open%20-status:Assigned%20-hotlistid:5433459%20-hotlist:%205562294)

**Task 4: monitor stale P0/P1 security bugs (optional)**

*   Review all result from [this
            search](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Type%3DBug-Security+component%3Ablink+pri%3D0%2C1+modified%3Ctoday-30&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids).
*   Check that the bug has an owner, the owner is actively working on
            the issue, and is fixing. Re-assign, re-categorize or ping the bug
            as appropriate.

## Contact

Public mailing list:
[crblink-rotation@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/crblink-rotation)
(<https://groups.google.com/a/chromium.org/forum/#!forum/crblink-rotation>)
