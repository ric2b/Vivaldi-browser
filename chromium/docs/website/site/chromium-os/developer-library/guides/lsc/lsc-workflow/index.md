---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: lsc-workflow
title: ChromeOS LSC Workflow
---

This document describes the workflow for getting a large-scale change approved
([ChromeOS LSC](large_scale_changes.md)).

[TOC]

## 1 Complete the LSC template

This [LSC Template] is a short document that will describe the large-scale
change you want to make: What the change is, why you want to make it, estimated
size (#CLs, #files), etc. Once you start sending out CLs, a link to that
document should also be included in the description of each CL (more details at
the ChromeOS [LSC Template]).

For Googlers, if the details are Google-private, you can do the same thing but
with your google.com account. Share comment access to google.com, in this case.

## 2 Request a domain review

Find [a domain reviewer](large_scale_changes.md#FAQ-for-Domain-Reviewers) – someone
who is knowledgeable in the area you are changing. Request a domain review for
the LSC document you created in step 1 creating a document comment thread and
assigning it to this person. The comment thread should include the instructions
for the domain reviewer (briefly, if the domain reviewer approves the proposed
LSC, they should add an "LGTM", their name, and the date to top of the doc).

## 3 Wait for approval by chromeos-lsc-review@

Once the domain review is complete, you should email the doc to
[chromeos-lsc-review@chromium.org](chromeos-lsc-review),
who will review your LSC request. You should expect an initial response within
two business days. How long it takes until the request is approved depends on
how complex the change is and how much needs to be discussed; for simple
changes, it should only take a few days. **Final approval will be indicated by a
member of the LSC committee granting a reviewer Owners-Override approval power
to land your LSC**. Put a link to the [chromeos-lsc-review] thread
in the table at the top of the doc.

## 4 Start sending your CLs!

Once your LSC is approved (Owners-Override is granted) you may begin sending
your CLs for review.

**NOTE**: your CLs should contain only the changes required for your LSC.
Unnecessary changes (e.g. formatting unrelated lines) must be avoided and
LSC reviewers should reject CLs if they contain such changes.

If unrelated changes are required to land the CL (e.g. lint or other
presubmit requirements), you must add OWNERs for those files as well as
the LSC reviewer in order to ensure there will not be unintended
consequences.

## Questions

If at any point you have questions about this workflow, please contact
[chromeos-lsc-policy@chromium.org](mailto:chromeos-lsc-policy@chromium.org) and
we'll be happy to help.

[LSC template]: https://docs.google.com/document/d/1iLGbamQyUEgfsYXX8a4p2oop9MDzxi3S6-NeXG6bJqE/preview
[chromeos-lsc-review]: https://groups.google.com/a/chromium.org/d/forum/chromeos-lsc-review
