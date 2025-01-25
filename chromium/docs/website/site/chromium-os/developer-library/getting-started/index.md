---
breadcrumbs:
- - /chromium-os/developer-library
  - ChromiumOS
page_name: getting-started
title: Getting Started
---

Welcome to ChromiumOS development! This guide will help you bootstrap your
workflow by making sure you have the right hardware, development environment,
and source checkout. The guide will also step you through building, testing,
uploading and committing code.

If you are familiar with ChromiumOS development and have a workstation, jump
ahead to TODO(Checking out the source code). Otherwise continue reading to
understand the high level overview of the Chromium projects and repositories.

## The Chromium projects

The Chromium projects include Chromium and ChromiumOS, the open-source projects
behind the Google Chrome browser and Google ChromeOS. Read more about the
Chromium projects at the <a href="https://chromium.org/chromium-projects"
target="_blank">chromium.org home page</a>. Chromium (the browser) and Chromium
OS (the platform) share code due to historical development decisions.

ChromeOS and ChromiumOS are two separate products which share the same code
base yet the available features and look-and-feel of each product vary. Chromium
OS is supported by the open source community and ChromeOS is supported by
Google and its partners. Read more about the differences between ChromiumOS and
ChromeOS at the [ChromiumOS FAQ](/chromium-os/developer-library/reference/development/chromium-os-faq/).

## Repositories

ChromiumOS is developed across several repositories, the most prominent being
`chromium` and `chromiumos`. The `chromium` repository contains implementations
of the user-facing surfaces of ChromiumOS, the Chromium browser, and a
communication interface with lower layers of the OS. These lower layers in the
`chromiumos` repository include components such as the kernel, system daemons,
firmware, and implementations of low-level technologies such as Bluetooth and
WiFi.

Links to Code Search for these repositories:

* `chromium`: <a href="https://source.chromium.org/chromium" target="_blank">
https://source.chromium.org/chromium</a>
* `chromiumos`: <a href="https://source.chromium.org/chromiumos"
target="_blank">https://source.chromium.org/chromiumos</a>

## Hardware requirements

Developing an operating system requires significant resources such as a powerful
workstation to build and debug the system and a ChromiumOS device to flash and
test changes.

Head over to the [hardware
requirements](/chromium-os/developer-library/getting-started/hardware-requirements)
page to understand what hardware you need in order to develop ChromiumOS.

<div style="text-align: center; margin: 3rem 0 1rem 0;">
  <div style="margin: 0 1rem; display: inline-block;">
    <a href="/chromium-os/developer-library/getting-started/hardware-requirements">Hardware Requirements</a>
    <span style="margin-left: 0.5rem;">></span>
  </div>
</div>
