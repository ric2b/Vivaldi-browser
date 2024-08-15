--------------------------------------------------------------------------------

breadcrumbs: - - /chromium-os/developer-library/guides - ChromiumOS > Developer
Library > Guides page_name: coreboot

## title: coreboot in Chromium

[TOC]

## Introduction

coreboot is an extended firmware platform that delivers a lightning fast and
secure boot experience on modern computers and embedded systems. As an Open
Source project it provides auditability and maximum control over technology. Its
design philosophy is to do the bare minimum necessary to ensure that hardware is
usable and then pass control to a different program called the payload. coreboot
reserves an area in memory to save some data after it’s finished booting, known
as CBMEM. In addition to CBMEM, on X86 systems, coreboot will set up SMM. This
will contain things such as the boot log and other information, tables that get
passed to the payload, and ACPI tables for the OS. ChromeOS uses and actively
contributes to [coreboot.org]. See also the [coreboot documentation].

## Comparison to UEFI

coreboot and UEFI both handle the initialization of the hardware, but are
otherwise not similar. coreboot’s goal is to JUST initialize the hardware and
then exit. UEFI is actually a firmware specification, not a specific software
implementation. Intel has released an open-source implementation of the overall
framework, TianoCore, but it does not come with hardware support. Most hardware
running UEFI uses a proprietary implementation. coreboot does not follow the
UEFI specification, but it can be used to launch a UEFI payload such as
Tianocore in order to provide UEFI boot services.

The UEFI specification also defines and allows for many things that are outside
of coreboot’s scope: * Boot device selection * Updating the firmware * A shell *
Network communication

## Use within ChromeOS

All [ChromeOS devices](Chromebooks, Chromeboxes, Chromebit, etc) released from
2012 onward use coreboot for their main system firmware. ChromeOS utilizes the
[depthcharge payload] which performs additional actions and ultimately loads the
Linux kernel. See [coreboot upstream] for information on how to work with
upstream coreboot in the chromium repo.

## Upstream philosophy

ChromeOS is an upstream first project. With rare exception, all changes should
be made in the upstream coreboot repository and subsequently downstreamed to the
Chromium repo. See [downstreaming coreboot] for more information on the
downstreaming process.

[ChromeOS devices]: https://www.chromium.org/chromium-os/developer-information-for-chrome-os-devices/
[coreboot.org]: https://coreboot.org
[coreboot documentation]: https://doc.coreboot.org/
[coreboot upstream]: ./coreboot-upstream
[depthcharge payload]: https://chromium.googlesource.com/chromiumos/platform/depthcharge/
[downstreaming coreboot]: ./downstreaming
