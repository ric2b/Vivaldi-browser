---
breadcrumbs:
- - /chromium-os
  - Chromium OS
- - /chromium-os/build
  - Chromium OS Build
page_name: circular-deps
title: Dealing with circular dependencies
---

### Dealing with Circular Dependencies

Circular dependencies (A->B->A) result in portage errors like this:

```
  * Error: circular dependencies:
 (dev-go/luci-go-0.0.1-r4:0/0::chromiumos, ebuild scheduled for merge) depends on
  (dev-util/android-provision-0.0.1-r111:0/0.0.1-r111::chromiumos, binary scheduled for merge) (buildtime)
   (dev-go/luci-go-0.0.1-r4:0/0::chromiumos, ebuild scheduled for merge) (runtime)
  * Note that circular dependencies can often be avoided by temporarily
  * disabling USE flags that trigger optional dependencies.
```

#### Refactoring

In case of `A`->`B`->`A` dependencies, breaking up the `A` package into
`A1`->`B`->`A2` is always the preferred solution.

#### PDEPEND

In cases where breaking up packages would require an excessive time investment
AND if the circular dependency is only a runtime dependency,
rather than compile-time dependency, `PDEPEND` can be used.

The `PDEPEND` variable specifies runtime dependencies that do not require being
satisfied during build time, and can be merged after the depending package.
