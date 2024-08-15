---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: overlay-faq
title: Overlay FAQ
---

This document covers common issues or questions about ChromiumOS's use of
[Portage] overlays. See also the [Ebuild FAQ] for information about ebuilds.

Gentoo has its own documentation on [ebuild repositories] (a.k.a., "overlays")
and their [repository format].

[TOC]

## How do I write a metadata/layout.conf file?

The metadata/layout.conf file describes some properties of an overlay
repository. While Gentoo provides documentation for [metadata/layout.conf],
it's important to note that ChromeOS extends the usage of the _masters_ field
with some unique behavior.

Particularly, ChromeOS uses the _masters_ field to construct a sysroot's
`PORTDIR_OVERLAY` variable, which determines the set of visible overlays. This
ChromeOS-specific logic transitively (unlike Gentoo's use, which does not
inherit from parents) walks the _masters_ from each entry, starting with any
existing `overlay-${BOARD}-private` and `overlay-${BOARD}`. Some useful tips:

*   Include private repositories after public ones. The _masters_ list is in
    reverse priority order: later entries take precedence.
*   Always include your parent repositories; even if things work for you today,
    you might be leaving a confusing problem for future developers who might
    inherit from your overlays, if the transitive inclusion or implied
    `${BOARD}`/`${BOARD}-private` handling lines up differently.

Note that this walk is recursive, which means we will walk every item's private
and regular overlay, until we have walked the whole tree.

## Profiles

Whereas the `metadata/layout.conf` has the masters list where the list of
overlays populated, the `profiles/{profile}/parent` file contains profiles from
which configs are pulled.

## Overlays

### portage-stable

The `src/third_party/portage-stable/` tree is an overlay containing the majority
of the unmodified packages from upstream Gentoo.  Developers can utilize the
[`cros_portage_upgrade`](/chromium-os/developer-library/guides/portage/package-upgrade-process/) tool to automatically
import packages.

"Unmodified" isn't exactly true.  A number of modifications are made to ebuilds
when importing from upstream.  The spirit of this overlay is to simplify the
upgrade process: updates can be made without having to review any changes to
previous versions.  So along those lines, changes that are made, or allowed:

*   Change `KEYWORDS="*"`.
*   Downgrade `EAPI` versions in case Gentoo uses a newer version our current
    portage version does not yet support.
*   Fixes that have been sent upstream Gentoo
    (via [PR's to Gentoo's GH](https://github.com/gentoo/gentoo)).
*   In limited situations, cherry-picks from newer versions.  We have a much
    stronger preference to upgrade to newer versions, so this situation has to
    be justified (e.g. an important security fix).

### chromiumos-overlay

The `src/third_party/chromiumos-overlay/` tree contains public first-party
packages, as well as forked upstream packages.

For example, if we need to apply custom patches to Gentoo ebuilds, they should
be moved to this overlay.  That way when the package gets upgraded, developers
closely review the existing ebuild & patches to forward port those changes.

Try to include a `README.md` in the package directory providing some details as
to what/why the package has been forked.

### eclass-overlay

The `src/third_party/eclass-overlay/` tree contains custom & forked eclasses.
It is generally not expected for modifications to be done to this tree.

## Board overlays

The `src/overlays/` tree contains all the boards & projects.  This is where
the software meets the hardware.


[Portage]: https://wiki.gentoo.org/wiki/Portage
[Ebuild FAQ]: /chromium-os/developer-library/guides/portage/ebuild-faq/
[ebuild repositories]: https://wiki.gentoo.org/wiki/Ebuild_repository
[repository format]: https://wiki.gentoo.org/wiki/Repository_format
[metadata/layout.conf]: https://wiki.gentoo.org/wiki/Repository_format/metadata/layout.conf
