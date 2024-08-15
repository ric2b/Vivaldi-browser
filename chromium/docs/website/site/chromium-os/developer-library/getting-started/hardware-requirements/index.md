---
breadcrumbs:
- - /chromium-os/developer-library/getting-started
  - ChromiumOS > Getting Started
page_name: hardware-requirements
title: Hardware Requirements
---

Developing an operating system requires a hardware-rich environment. The
ChromiumOS development infrastructure supports a range of configurations from
the traditional local workstation and device-under-test (DUT) to a fully remote
setup where both the build machine and DUT are in the cloud.

Note: ChromiumOS hardware comes in many form factors including laptops,
clamshells, tablets, and Chromeboxes. The rest of the Getting Started guide may
refer to the test Chromebook or DUT, but the instructions apply to any
ChromiumOS form factor.

## Overview

ChomiumOS development requires the following hardware:
* A Linux workstation to build the OS. Ubuntu and Debian releases are well
  supported.
* A ChromiumOS device to flash and test your changes, also known as the DUT.
* A USB thumb drive to flash images onto the DUT.

It is recommended that you connect the DUT to the network that has access to the
build workstation over Ethernet. Deploying builds from the workstation to the
DUT often involves transferring files that can be several hundred megabytes to
gigabytes in size. This configuration requires the following:
* An Ethernet cable.
* A USB-to-Ethernet adapter if the DUT does not have an Ethernet port.

### Workstation

Whether you are building ChromiumOS locally on a workstation or relying on a
distributed compilation system, building and testing the platform is resource
intensive and benefits from a powerful machine. The following workstation specs
are recommended:
* Multi-core x86-64 CPU
* 32 GB RAM
* 200 GB free disk space (NVMe SSD recommended)

> Googlers: Please visit <a
> href="https://go.corp.google.com/cros-hardware-acquisition"
> target="_blank">go/cros-hardware-acquisition</a> for details on acquiring the
> hardware you need to develop ChromiumOS.

#### Flexible development options

The development workstation may be sitting at your desk, or you may access a
remote, cloud-based workstation (e.g., [Google Cloud
Workstation](https://cloud.google.com/workstations/pricing)).

The ChromiumOS build system requires a Linux distribution. The OS may be
installed on your local workstation, on a cloud-based compute instance, or in a
container on your development device.

For both the cloud-based workstation and the Linux container options, your main
development machine can be any system which supports connecting to the
workstations over the network through SSH or running a virtual machine. You can
even build ChromiumOS on ChromiumOS using the [Linux terminal
feature](https://support.google.com/chromebook/answer/9145439?hl=en)!

### USB thumb drive

Putting the DUT into developer mode requires booting from a USB thumb drive
formatted with a ChromiumOS image containing test and debug tools. Once in
developer mode, the device can be flashed over the network from the workstation.
The minimum required size of the thumb drive is 8 GB.

Up next you will configure your workstation [development
environment](/chromium-os/developer-library/getting-started/development-environment)
to meet the requirements of developing ChromiumOS.

<div style="text-align: center; margin: 3rem 0 1rem 0;">
  <div style="margin: 0 1rem; display: inline-block;">
    <span style="margin-right: 0.5rem;"><</span>
    <a href="/chromium-os/developer-library/getting-started">Getting Started</a>
  </div>
  <div style="margin: 0 1rem; display: inline-block;">
    <a href="/chromium-os/developer-library/getting-started/development-environment">Development Environment</a>
    <span style="margin-left: 0.5rem;">></span>
  </div>
</div>
