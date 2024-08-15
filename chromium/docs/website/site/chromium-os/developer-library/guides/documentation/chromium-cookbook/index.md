---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: chromium-cookbook
title: Chromium cookbook
---

[TOC]

## Vision

Provide a concrete solution for commonly encountered problems on Chromium to
mitigate the steep learning curve and to improve productivity.

*   Use your own judgement about what recipes to add.
*   Generally speaking, anything that is not obvious and may be done again can
    be a good recipe. Examples:
    *   Common Task: How to add a metric.
    *   Common Task: How to add a feature flag.
    *   Common Task: How to test a System Web App is launched successfully.
    *   Common Task: How to mock a JavaScript function.
    *   Common Task: How to add a localized string.
    *   Common Task: How to add a skeleton of new WebUI application.
    *   Best practice: how to pass a file name from JS to C++ safely.
    *   Best practice: how to send big file content from JS to C++ efficiently.
    *   Design Pattern: how to use a fake mojo service provider in JS.
    *   Tricky Task: How to populate a BigBuffer object in JS.

## Background

The chromium project is large and complicated. The learning curve is steep.
Nooglers are joining our teams constantly. We often found that while it does not
take much effort to write the code, it does take lots of effort figuring out how
to do it.

*   If you have spent hours, even days trying to figure out how to solve a
    problem, you can help other team members to save time by writing the
    solution down.

*   If a team member has reached out to you for help, you can be even more
    helpful by documenting the answer.

*   If you have designed/done something cool, you can make it known and be
    adopted quicker by providing a working example.

NOTE: this is not a replacement for good documentation.

## Writing Guidance

*   Keep it concise to save you and your readers’ time.
*   List the prerequisites with links to docs and other cookbook entries.
*   Provide a concrete example. A reference CL saves a thousand words.
*   Highlight the keep points.

## Suggested contents for a recipe

**Title**

Tip: provide a short, descriptive title for the recipe.

**Prerequisites**

Tip: list background knowledge that may require to solve the problem.

**Problem**

Tip: state the problem facing.

**Solution**

Tip: list the key concepts, steps to solve the problem.

**Example CL**

Tip: provide a good reference CL. It does not have to be yours.

**References**

Tip: provide links to docs, other recipes if any.

**Comment/Discussion**

> Tip: share thoughts, comments if any, examples:
>
> *   Other possible use cases
> *   Comparison with other solutions

## FAQs

1.  How to organize the recipes?

    *   Keep closely related recipes in one file.
    *   Organize by tech areas, topics, etc.
    *   Re-categorize as needed.

2.  Who is responsible for maintenance?

    *   Everyone is welcome to contribute.
