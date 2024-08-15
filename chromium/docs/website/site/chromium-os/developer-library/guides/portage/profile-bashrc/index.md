---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: profile-bashrc
title: CrOS bashrc Override Framework
---

CrOS takes a portage-specific feature (profile.bashrc files) and extends it to
distribute into per-package fragments. This can make for easier updates,
delegation of maintenance (i.e. OWNERS), and cleaner separation of code.

[TOC]

## Portage Profile Settings

Portage supports global `profile.bashrc` files that stack across
[profiles](/chromium-os/developer-library/guides/portage/ebuild-faq/#profiles). These are bash scripts that are sourced
before & after every ebuild phase, and are automatically loaded from the
current board's set of profiles. The script can do pretty much anything: run
arbitrary commands (patch or modify files, delete or create files, etc...),
change environment variables, override shell functions, and more.

In practice, CrOS has only one of these files active:
[chromiumos-overlay/profiles/base/profile.bashrc](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/profiles/base/profile.bashrc)

From a quick scan, one can see that it's already quite large. However, most
people need not ever look at this file.

## CrOS Per-package Overrides

### Layout

Developers may create per-package bashrc overrides in the directory that
corresponds to the package. For example, the sed package has a
[`sys-apps/sed/sed.bashrc`](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/sys-apps/sed/sed.bashrc)
file that is loaded when emerging it.

NB: The file currently must live in chromiumos-overlay using the
`${CATEGORY}/${PN}/${PN}.bashrc` naming convention regardless of where the
ebuild itself lives. This includes packages that live in portage-stable or in
board overlays.

### Coding Conventions

Code should be placed into functions to help keep organized, and to avoid
running too much logic in global scope (which can slow things down).

The framework provides a naming policy for you to follow:
`cros_<pre|post>_${EBUILD_PHASE}_<unique name>`
The unique name should be fairly descriptive at a glance, both for humans, and
to help minimize collisions.

For example, to run code after sed's `src_prepare` function, the `sed.bashrc`
file defines `cros_post_src_prepare_force_sandbox`.

### APIs

The bashrc file has access to all the same variables & functions that the ebuild
it is overriding has access to. This makes sense as the code is literally
sourced and executed inside the active shell environment.

This means standard ebuild variables are available, as are normal shell
commands, and in `src_install`, ebuild install commands.

However, some global aspects cannot be overridden or extended. Most notably,
the `inherit` statement (which imports eclasses) is unavailable. So if you need
specific eclasses, see if the ebuild in question inherits it. If it doesn't,
then you cannot access it, and it might be easier to open-code the logic.

### Logging

The common framework will logic the name of each hook as it's run, so you don't
have to emit any extra log details yourself if you don't want to.

## Style

Since these files are effectively Gentoo ebuilds, use Gentoo shell style. The
biggest difference from the CrOS shell style guide is tabs instead of 2 spaces
for indentation.

## Linting

Although these use Gentoo ebuild code, `cros lint`'s shellcheck integration
doesn't currently detect them as such, so you might get a lot of warnings when
using standard ebuild variables & APIs. You can safely ignore those for now.

## FAQ

### Why does this exist at all?

Having bashrc fragments disconnected from the ebuild itself can significantly
improve maintenance of upstream ebuilds. The portage-stable tree has a good
flow where developers can blindly upgrade without reviewing the package's
history to look for CrOS customizations. By keeping that logic in a separate
file, the CrOS-specific overrides are quite clear.
