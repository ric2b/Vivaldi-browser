---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: tools-used
title: Tools used
---

[TOC]

To help acquaint you with ChromeOS development, here are a list of the tools
used by the System Services team.

## Text editors / IDEs

### Chromium

Unfortunately, as of Jan 2023, there is no IDE that supports Chromium C++.
However, the text editors described below have some support for niceties like
auto-complete and references. Pick whichever editor you prefer.

#### VSCode

- go/vscode
- go/vscode/remote_development_via_web (Nicer than using Chrome Remote Desktop
when working remotely)
- [External site](https://code.visualstudio.com/)

#### Sublime Text

Briefly look over go/sublime for instructions on how to install a licensed
Sublime, but do not read further in its setup instructions. Instead, read
[Using Sublime Text as your IDE](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/sublime_ide.md).
This document is critical to follow if you wish to use Sublime, because without
it, Sublime will unnecessarily search over thousands of Chrome build artifacts
and run painfully slow.

#### vim

- go/vim
- [External site](https://www.vim.org)

#### Emacs

- go/emacs
- [External site](https://www.gnu.org/software/emacs)

### ChromiumOS

See go/cros-ide.

### Google3: Cider

You can use [Cider-V](https://cider-v.corp.google.com) for editing G3 code such
as G3Docs and [Finch](http://go/finch) configs. It cannot be used for
Chromium/ChromiumOS development.

## Useful Chrome extensions

Most of your work outside of developing will be done in the Chrome browser.
go/useful-internal-chrome-extensions contains a curated list of chrome
extensions to improve your productivity. Here are a few that are recommended:

-   go/cs-rainbow-blame color-codes the blame layer in Chromium CodeSearch.
-   [Gerrit Monitor](https://chrome.google.com/webstore/detail/gerrit-monitor/leakcdjcdifiihdgalplgkghidmfafoh)
    notifies you when your action is required on a
    [Gerrit CL](https://chromium-review.googlesource.com/).
-   go/get-gvc-labs Adds helpful tools to Meet, including Meetmoji reactions and
    speaking time.
-   [G3 Notifications](https://chrome.google.com/webstore/detail/g3-notifications/mdkmkbpoljeogkhbjlehikpbpdmfaopg)
    notifies you when your action is required on a [Buganizer issue](http://b/)
    or a [Critique CL](http://cl/).
-   go/kodify allows you to beautifully format code blocks in your design docs.
-   go/snipit allows you to easily share screenshots for debugging or other
    purposes.
-   [Chromium CL Finder](https://chromewebstore.google.com/detail/egncfhncpaakcfegigpnijpdlffhljcc?pli=1)
    shows the first landed version on Gerrit for Chromium CLs. See
    [Understanding ChromeOS Releases](/chromium-os/developer-library/reference/release/understanding-chromeos-releases)
    for why you'd want to use this.
-   [Linkable](https://chromewebstore.google.com/detail/gfgjcjnidbkemmgcgpjfnplblfdkgcfk)
    provides alternate, short-form URLs to easily use in emails, bugs, or
    documents.
-   [Don't Be Late](https://chromewebstore.google.com/detail/dpnkhajhejhnmadcjpnbnnmjnjnkbhol?pli=1)
    automatically opens Google Meet rooms in a new tab when it starts.

Also check out go/lifehacks for 250 life hacks for your work and personal life!

Tip: You can shorten links to Gerrit CLs with crrev/c/{id}, to Critique CLs with
cl/{id}, and Buganizer issues with b/{id}!

## Working from home

Check out the go/newtochrome-wfh-tips doc for tons of WFH tips and a bushel of
useful go/ links!

Most developers on the team connect to their build machines over go/crd and run
their editors on their build machines.

Another option to CRD is to connect to your build machine using the "Secure
Shell App." Then you can use terminal multiplexer and a terminal-based editor,
you can get 99% of the same functionality with lower bandwidth requirements.

2 developers run VSCode on their local laptops and remotely connecting to their
workstations from VSCode itself (this approach does not work from a Chromebook
as the system permissions don't allow it).

## Terminal multiplexers

These tools allow you to switch between multiple programs in one terminal.

-   [tmux](https://github.com/tmux/tmux/wiki/Getting-Started)
-   [GNU Screen](https://www.gnu.org/software/screen/manual/screen.html)