---
breadcrumbs:
- - /chromium-os/developer-library/getting-started
  - ChromiumOS > Getting Started
page_name: development-environment
title: Development Environment
---

Once you've acquired your
[development hardware](/chromium-os/developer-library/getting-started/hardware-requirements),
the next step is to ensure your development environment is capable of
downloading and building the code. This includes ensuring runtime hardware
needs, package management, and Git configuration.

## Checking resource requirements

You may have 128GB of RAM and a 2TB harddrive, but if all of those resources are
being consumed, you will have a hard time developing ChromiumOS.

Check for at least 64GB of free memory:

```
$ free -gh
```

The `chromium` and `chromiumos` repositories each require roughly 200GB of disk
space including source files and build artifacts. Ideally you should aim for at
least 500GB of free space on the mount which hosts your development directory:

```
$ df -BG
```

## Update package management system

To make sure you have the most up-to-date packages installed (or a similar
command if using a different package manager):

```
$ sudo apt update
```

> Googlers only: If you receive the error `Could not resolve
> 'external-repositories-are-not-allowed.google.com'`, follow
> [these instructions](https://support.google.com/techstop/answer/3272365?hl=en).
> You may need to run `sudo glinux-updater` to resolve the issue.

## Install and configure Git

Install Git if necessary:

```
$ sudo apt-get install git
```

Determine the name and e-mail address you intend to use for development, and
configure those values with Git. This will likely be an @chromium.org account if
you have one. Use your corporate email address if advised by your company (e.g.,
@google.com address for Googlers).

```
$ git config --global user.email "yourname@company.com"
$ git config --global user.name "Your Name"
```

## Install depot_tools

<a
href="https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools.html"
target="_blank">depot_tools</a> is a collection of tools that simplify
development of Chromium, some of which are essential to the development flow.
Download `depot_tools` and add the location of the tools to your environment's
PATH.

```
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
$ echo 'PATH="$PWD/depot_tools:$PATH' >> ~/.bashrc
$ source ~/.bashrc
```

Note that it is important to put the path to `depot_tools` at the beginning of the
PATH to ensure that only the binaries in this location are used, avoiding
conflicts with other binaries of the same name in another location.

## Up next

Now that your workstation is ready for development, you'll
[setup your development Chromebook](../setup-chromebook) such that you will be
able to deploy your custom builds to the device. If you don't have a development
Chromebook, skip ahead to [Checkout Chromium](../checkout-chromium) to continue
with the workstation-only development flow.

<div style="text-align: center; margin: 3rem 0 1rem 0;">
  <div style="margin: 0 1rem; display: inline-block;">
    <span style="margin-right: 0.5rem;"><</span>
    <a href="/chromium-os/developer-library/getting-started/hardware-requirements">Hardware Requirements</a>
  </div>
  <div style="margin: 0 1rem; display: inline-block;">
    <a href="/chromium-os/developer-library/getting-started/setup-chromebook">Setup your development Chromebook</a>
    <span style="margin-left: 0.5rem;">></span>
  </div>
</div>
