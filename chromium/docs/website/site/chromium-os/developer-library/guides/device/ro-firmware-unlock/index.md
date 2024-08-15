---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: ro-firmware-unlock
title: Read-only firmware unlock on 2023+ devices
---

<!--
Note: This document is intended as a guide for Chromebook customers that want
to unlock their personal device. Please keep it on-point for that goal and do
not add confusion by mentioning tools and processes that are only relevant for
development within Google and its partners (like Servo or node-locked images).
If there is a need for a guide that explains RO verification to internal users,
it should be a separate document.
-->

[TOC]

## Purpose of this guide

Chromebooks launched in or after 2023 were built with a security feature that
detects changes to the write-protected (read-only) portion of the Chromebook's
firmware. This feature helps keep your Chromebook secure from attackers that
come into physical possession of it for a limited amount of time, or in
scenarios where you share a communal Chromebook with others. The feature was
not yet enabled on the first devices built with it, but will be turned on via
remote update in ChromeOS version M122.

If you're an enthusiast developer and want to replace the entire firmware of
your Chromebook (including the read-only section) with your own code, this
feature would prevent your Chromebook from booting afterwards. Users sometimes
do this when they want to install a different operating system (e.g. Chrubuntu,
GalliumOS), or when they want to set some of the safety-compromising firmware
boot policy options intended for developers that are known as "GBB flags". You
have always needed to disable the firmware write-protection to do this, usually
by disconnecting the battery. On these newer devices you now need to run an
additional command to disable firmware write-protection and also need to
disable this read-only firmware verification feature beforehand. This guide
explains how to do that.

> **NOTE:** Removing firmware write-protection and replacing read-only firmware
> is a dangerous operation that can leave your Chromebook unable to boot, break
> security guaratees and may void your warranty. Neither Google nor the device
> vendor are responsible for any damage caused by doing this. Proceed at your
> own risk.

Note that it is also possible to run most other operating systems by using the
[alternative bootloader] feature (also called "legacy BIOS" or the `RW_LEGACY`
method). This does not require replacing the read-only firmware and you do not
need to follow the instructions in this guide to use it. Unless you are really
sure that you want/need to replace the read-only firmware, we recommend using
that method instead to reduce the risk to your Chromebook.

## Does my device need this?

The new read-only firmware verification feature is used on all devices that use
the newer "Ti50" security chip (as opposed to the older "Cr50" chip). Those are
generally all device families that were first sold in or after 2023. To
determine exactly what kind of security chip your device uses, you can navigate
to `chrome://system` and look at the `tpm_version` entry: it will either say
`gsc_version: GSC_VERSION_TI50` to indicate your device supports the feature and
you need to follow this guide if you want to disable it, or `gsc_version:
GSC_VERSION_CR50` to indicate it doesn't and you can ignore this guide.

## Step-by-step

1. Make sure your device is in [developer mode]

1. Go to the [command prompt on VT2] and log in as `root`.

1. (optional) If you're willing to open your device chassis, you can disconnect
the battery at this point to make the following step less cumbersome. Make sure
your device is connected to an AC charger while doing so. (Note that opening the
chassis may void your warranty.)

1. Run `gsctool -a -o`

1. If you disconnected the battery, the command should succeed immediately. If
not, you will be prompted to press the power button multiple times. Whenever you
see the message "Press PP button now!" repeated on the screen, press the power
button. When you see "Another press will be required!", do **not** press the
button and wait until the text changes again. This may take up to 5 minutes. If
you don't follow the sequence exactly, the command may fail and you may have to
try again.

1. If the command succeeds, your Chromebook will immediately reboot and will no
longer be in developer mode. Re-enter [developer mode] the usual way, and go
back to the VT2 command prompt.

1. Run `gsctool -a -I AllowUnverifiedRo:always`. You will be prompted to press
the power button again to confirm. This permanently disables the read-only
firmware verification.

1. Run `gsctool -a -w disable`. You will be prompted to press the power button
again to confirm. This disables the firmware write-protection until the next
reboot. (If you want, you can run `flashrom --wp-disable` after this to disable
the [software write-protect], which remains permanently disabled even if the
`gsctool` setting reverts on reboot.)

1. If you disconnected the battery earlier, you can reconnect it now.

## Recovering a device that doesn't boot

If you somehow overwrite your read-only firmware without following the above
instructions to set the `AllowUnverifiedRo` flag first, your device will no
longer boot. You can use the following key sequence to recover from this
state:

---

#### Sequence for Chromebooks

1. Press and hold the power button

2. Press the Refresh (&#x27F3;/F2) button twice

3. Release the power button

4. Repeat the steps 1 through 3 a second time

---

#### Sequence for Chromeboxes

1. Press and hold the power button

2. Press the recovery pinhole button twice

3. Release the power button

4. Repeat the steps 1 through 3 a second time

---

#### Sequence for detachables and tablets

1. Press and hold the power button

2. Press down the Volume Up button, hold it for 10+ seconds, then release it

3. Press down the Volume Up button again, hold it for another 10+ seconds, then
release it again

4. Release the power button

5. Repeat the steps 1 through 4 a second time

---

This will temporarily disable the read-only firmware verification feature for
15 minutes. You can use this time to either follow the above instructions to
disable it permanently, or restore the official firmware to the device.

> **NOTE:** This key combination will only disable the read-only firmware
> verification security feature. It can not restore the read-only firmware to a
> good state. If you overwrote the read-only firmware with a bad or corrupted
> image that doesn't boot, you will still need an external flash programmer to
> recover your device.

## Further reading

For more information on how firmware write-protection works, see the
[write protection reference document].

For details on how you can use `gsctool` to configure low-level access controls
on the security chip or set a password to restrict future access, see the
[case-closed debugging documentation].

<!-- Links -->

[alternative bootloader]: ../developer-mode#running-an-alternative-bootloader-legacy-bios
[case-closed debugging documentation]: https://chromium.googlesource.com/chromiumos/platform/ec/+/cr50_stab/docs/case_closed_debugging_gsc.md
[command prompt on VT2]: ../developer-mode#get-the-command-prompt-through-vt-2
[developer mode]: ../developer-mode#enable-developer-mode
[software write-protect]: ../../../reference/security/write-protection#software-write-protect
[write protection reference document]: ../../../reference/security/write-protection
