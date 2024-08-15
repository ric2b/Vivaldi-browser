---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: crosh
title: "Crosh -- The ChromiumOS shell"
---

[TOC]

The ChromiumOS shell ("crosh") provides a small set of command line tools for
developers to troubleshoot their system.  It is not intended as a polished
user-facing product feature.

## Launching

You can open a new crosh instance with the Ctrl+Alt+T key sequence.

Since crosh is not intended for frequent use, it does not have an associated
icon in the app launcher.  The only way to get to it is via the Ctrl+Alt+T
sequence.

You do not need to install any additional applications to access crosh.  This
is critical for debugging network connectivity issues.  It also means we can
avoid a "go install this tool" step from diagnostic procedures.

## Tools

> **Note**: The exact set of tools changes between releases, channels, admin
> settings, device mode developer mode enabled), and more.

Run `help` to get info about available commands and discover more.  That will
show the most common commands.  `help_advanced` command will show everything,
so be prepared to scroll up to see everything.

You can also use tab completion to quickly find existing commands.

Help for specific commands can be displayed with `help <command>` or with
`<command> --help`.

It's an adventure!

## Developing crosh

Developers that are working on the crosh tool itself should see the
[documentation in the crosh project itself](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/crosh/).
