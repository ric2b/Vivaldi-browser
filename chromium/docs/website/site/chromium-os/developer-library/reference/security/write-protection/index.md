---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: write-protection
title: Write protection
---

[TOC]

## Background

The core of the ChromeOS security model is rooted in a firmware image that we
fully control and whose integrity we can guarantee. All existing designs have
accomplished this through a dedicated flash part which is guaranteed to be
read-only once the device is shipped to customers. This is colloquially
referred to as the "read-only (RO) firmware".

The RO firmware is the first thing executed at power on/boot and is
responsible for verifying & loading the next piece of code in the system which
is usually the Linux kernel. There might be other components that are
loaded/chained (read-write (RW) firmware, etc...) before loading the Linux
kernel (see [Verified Boot] for more information), but those details are
immaterial here. The point is that the entire system security hinges upon the
integrity of the RO firmware.

## Hardware Write protect

We guarantee the RO firmware integrity via the Write Protect (WP) signal.
 This is a physical line to the flash (where the RO firmware is stored) that
tells the flash chip to mark some parts as read-only and to reject any
modification requests. So even if ChromeOS was full of bugs and was exploited
to gain all the permissions for direct write access to all pieces of hardware
in the system, any RO firmware write attempts from code running on the CPU
would be stopped by the flash chip itself. Then when the system reboots, the
verified boot process would detect any modifications or corruption to the hard
drive (e.g. kernel/root filesystem) and interrupt the normal boot flow
initiating the ChromeOS recovery process. Thus we can confidently tell
customers: if you can reboot a Chromebook into the login screen, you know it's
secure.

This is a somewhat tricky topic since write protection implementations can
differ between chips and the hardware write protection has changed over time.

Historically, the WP signal has been controlled by a physical screw
(colloquially referred to as the "WP screw") inside of the Chromebook.  Our
security goals have been that, in order to disable flash WP, someone needs
extended physical access to the device, and it would take a non-trivial amount
of effort and time to open up the device in order to remove the WP screw (and
thus disable the WP signal making the RO firmware writable from software).

In newer devices, we've moved away from the WP signal being controlled by a
physical screw and to a separate chip controlling the WP signal.  That way we
have more flexible control over the WP signal. It still takes long time and
physical presence to disable write protection, and can be configured such that
only the device owners are authorized to disable write protection.

That separate chip is often referred to as a secure element (SE), the firmware
controlling the SE is called Cr50. This secure element firmware is fully
authored and controlled by Google.

Note that even in case of the devices protected by the SE, opening up the
device and disconnecting the battery would still disable write protection.

## Software write protect

In addition to the hardware WP signal, there is a software WP setting that
allows us some more flexibility in managing write protection.  The Status
Register Write Disable (SRWD), a non-volatile bit, is operated together with
Write Protection (WP#) pin for providing hardware protection mode.  The hardware
protection mode requires SRWD set to 1 and WP# pin signal asserted low. In the
hardware protection mode, the Write Status Register (WRSR) instruction is no
longer accepted for execution and the SRWD bit and Block Protect bits (BP2, BP1,
BP0) are read only. The Block Protect, BP, bits mask the regions within the SPI
flash data address space such that access that results in mutations can be
controlled.

This allows us to ship systems with hardware WP on, but leaving software WP off
until a point in time where software decided to engage it. One case where we're
using this is devices that originally get flashed with development signing
keys, but eventually get upgraded to production keys, after which software WP
would get enabled.

## Write-protect for developers

Some people ask why we allow the RO firmware to be made writable in the first
place if it's so critical to the security of ChromeOS.  It's not an
unreasonable question.

Since the start of the project, the ChromeOS team strongly believes that when
someone buys a device, they own it fully.  The device does not belong to OEMs,
to Google, to ChromeOS, or to anyone else. If the user wants to run Windows,
or Linux, or some other random software on their device that isn’t ChromeOS,
they should have that freedom.

To that end, we strongly believe that users must be free to fully program their
device in any way they want.  Developer mode and support for developers and
free software is extremely important to us as a project.  So if someone wanted
to write their own firmware (which they can as the firmware on devices is open
source that we release), they should have that ability.  We’ve seen some
projects do just that, as well as users who have leveraged that.

The ability to reprogram firmware is also a core requirement for
Google-internal development needs.  It would be much more complicated during
the early phases of a new hardware project to iron out kinks without the
ability to fix firmware on the chips in the devices themselves.

## Application Processor (AP) Firmware

AP firmware (also known as "SOC firmware", "host firmware", "main firmware" or
even "BIOS") typically resides on a SPI ROM. Protection registers on the SPI
ROM are programmed to protect the read-only region, and these registers cannot
be modified while the SPI ROM WP (write protect) pin is asserted. This pin is
asserted through various physical means, but with effort, users can unprotect
devices they own.

## Embedded Controller (EC) Firmware

The ChromeOS Embedded Controller (EC) typically has a WP input pin driven by
the same hardware that generates SOC firmware write protect. While this pin is
asserted, certain debug features (eg. arbitrary I2C access through host
commands) are locked out. Some ECs load code from external storage, and for
these ECs, RO protection works similar to SOC firmware RO protection (WP pin is
asserted to EC SPI ROM). Other ECs use internal flash, and these ECs emulate
SPI ROM protection registers, disabling write access to certain regions while
the WP pin is asserted.

## Reading Write Protection Status

All commands shown in this section are ran on the ChromeOS device terminal,
which is available when the device is booted in developer mode.

### Hardware write protection Status

To check hardware write protection status, run the following command:
`crossystem | grep wpsw`

```
localhost ~ # crossystem | grep wpsw
wpsw_cur                = 1                              # [RO/int] Firmware write protect hardware switch current position
```

A value of `1` indicates that write protection is enabled.

### Software Write Protection and Write Protection Range Status

To check software write protection and write protection range status, run the
following commands. Note that the write protection range is independent from
the software write protection status (if sw write protection is disabled, it
means you can manipulate the protection range).

*   AP: `futility flash --wp-status`
*   EC: `ectool flashprotect`

## Disabling write protect

The typical sequence for disabling write protection is to disable hardware write
protect, disable software write protect, and clear the write protection ranges.

### Disabling hardware write protect

#### Write Protect Screw

*   Power down the device and open the case
    *   Locate and remove the write protect screw on the motherboard.
*   Restart the device

#### Cr50
*   Use [Servo] to connect to the Cr50 console
    *   Enter the chroot with `cros_sdk --no-ns-pid`
    *   Ensure servod is running with `sudo servod &` if necessary.
    *   Run `dut-control cr50_uart_pty` to discover the pty file for the console.
    *   Connect to that console with minicom, cu, or screen.
*   Perform the ["CCD open"] process to enable functionality.
    *    Run `wp disable` on the [Cr50 console].

If you don't want to go through the CCD open process or don't have a [suzyQ], you
can open the case and remove the battery to disable hardware write protect.
*   Disassemble the device, locate the battery connector, remove the battery connector from the
    PCB to disconnect the battery.
*   Reassemble the device, insert the original OEM charger (necessary since the
    battery is no longer providing power to the system), then boot to developer
    mode.

#### Servo Header

Servo when connected can override the native write protection (either using
the write protect screw or the security element). For systems with a servo
header:
*   Connect servo board to the ChromeOS device
*   Enter the chroot with `cros_sdk --no-ns-pid`
*   Start servod inside the chroot: `sudo servod &`
*   Force write-protect on via the servo header: `dut-control fw_wp_state:force_off`.

In all cases, after the device is back up, verify that write protection is
disabled by running `crossystem wpsw_cur`, it should now output `0`.

### Disabling software write protect

*** note
*NOTE*: You cannot disable software write protect if hardware write protect is
enabled.
***

*   AP: run `futility flash --wp-disable`.
*   EC: run `ectool flashprotect disable`. If `RO_AT_ROOT is not clear` message
    is displayed, check if hardware WP has been disabled and do an
    [EC Reset](#reset-ec).

## Enabling write protect

### Enabling hardware write protect

Hardware write protect can be controlled by 3 different mechanisms:
*   A write-protect screw, when present,
*   Cr50 firmware, when present, and
*   A pin on the servo header, when servo is connected

#### Write Protect Screw

For systems with a write-protect screw:
*   Power down the device and open the case
*   Insert the write protect screw on the motherboard.
*   Restart the device
*   `crossystem wpsw_cur` should now output `1`

#### Cr50

For systems that use Cr50, you can control it on the device itself:
*   Run `gsctool -a --wp` to check the current system (look for "Flash WP").
*   If CCD factory mode was enabled, run `gsctool -a -F disable` to enable WP
    protect.
    *    This command will fail if factory mode is not enabled. Newer Chrome
    OS versions allow to enable flash write protection by running `gsctool -a
    --wp enable`
*   See also the internal
    [crosops docs](https://support.google.com/crosops/answer/9790616).

Or you can control it with a suzyQ:
*   Connect to [Cr50 console] with a serial terminal program (minicom, cu,
    screen, etc.)
*   Perform the ["CCD open"] process to enable functionality.
    *    Run `wp enable` or `wp follow_batt_pres` on the [Cr50 console].

#### Servo Header

*   Connect servo board to the ChromeOS device
*   Enter the chroot with `cros_sdk --no-ns-pid`
*   Start servod inside the chroot: `sudo servod &`
*   Force write-protect on via the servo header: `dut-control fw_wp_state:force_on`.

### Enabling software write protect

For AP firmware,
*   Run: `futility flash --wp-enable`.
Note that this will enable SW WP and automatically set the WP correct range for
the device.

For EC firmware,
*   Run `ectool flashprotect enable now`.

["CCD open"]: https://chromium.googlesource.com/chromiumos/platform/ec/+/cr50_stab/docs/case_closed_debugging_cr50.md#ccd-open
[Cr50 console]: https://chromium.googlesource.com/chromiumos/platform/ec/+/cr50_stab/docs/case_closed_debugging_cr50.md#consoles
[Servo]: https://chromium.googlesource.com/chromiumos/third_party/hdctools/+/HEAD/README.md
[suzyQ]: https://chromium.googlesource.com/chromiumos/third_party/hdctools/+/HEAD/docs/ccd.md#suzyq-suzyqable
[Verified Boot]: https://www.chromium.org/chromium-os/chromiumos-design-docs/verified-boot
