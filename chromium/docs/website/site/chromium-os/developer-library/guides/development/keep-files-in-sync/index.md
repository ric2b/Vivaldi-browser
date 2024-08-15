---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: keep-files-in-sync
title: How to use LINT.IfChange to keep files in sync
---

## Problem

There are two files have a binding relationship. If I change one of them, then
I need to change the other one.

## Solution

Add `// LINT.IfChange` before the content you want to change.

Add `// LINT.ThenChange(path)` after the content you want to change.

Some other syntax:

```
// LINT.IfChange
... content
// LINT.ThenChange(path)

// LINT.IfChange
... content
// LINT.ThenChange(path:label)

// LINT.IfChange(my-own-label)
... content
// LINT.ThenChange(path)

// LINT.IfChange(my-own-label)
... content
// LINT.ThenChange(path:label)
```

## Example

*   https://crsrc.org/c/ash/public/cpp/accelerator_actions.h;l=26

## References

*   [Gerrit IfThisThenThat Lint](http://doc/1XAU_3RalhvbeK2eC1mzCCVeGJDj-tyksWEEsL2yZJL4){.external}
