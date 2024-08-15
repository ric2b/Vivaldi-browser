---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: firmware-updating
title: Peripheral firmware security
---

This document discusses security risks with peripheral firmware and defines
ChromeOS' approach and requirements to address that risk.

## Context

Several on- and off-device peripherals on ChromeOS require firmware to
function. This firmware typically gets executed by a microcontroller or other
execution core within the peripheral and can interacts both with the hardware as
well as with the OS, often via a kernel device driver.

The peripheral firmware typically has extensive control over behavior of the
devices seen by the CPU. With USB peripherals, it is not unusual for the
firmware to be responsible for providing the device descriptor, including vendor
and product IDs as well as device class etc. For PCIe devices (such as WiFi
controllers), the firmware will also generally be able to trigger system memory
accesses via DMA.

As a result, compromised or malicious firmware poses significant security risk.
Some examples:
*   Malicious USB device firmware can reconfigure the device to enumerate as a
    keyboard input device, then send malicious key sequences to the browser to
    exfiltrate sensitive data from the device to a web server. See [badusb] for
    a classic piece of research that demonstrated how USB storage device
    firmware can be reprogrammed to make the device pretend to be an input
    device and more.
*   There have been several instances of over-the-air remote code execution
    vulnerabilities in WiFi controller firmware. Malicious WiFi firmware has
    ample opportunity for privilege escalation to full system compromise. If the
    WiFi controller has unlimited access to system RAM, kernel exploitation is
    trivial. Even when WiFi memory is isolated, the WiFi kernel driver typically
    exposes large attack surface. See this [broadcom vulnerability] for an
    example.

The following measures help to manage this risk:
*   **Integrity** ensures that only firmware binaries which are
    confirmed to be from a legitimate source get executed in the peripheral.
*   **Updateability** of the firmware is necessary so we can fix security
    vulnerabilities in the firmware via automatic updates. Note that this often
    requires cooperation from the hardware vendor.

## Firmware Integrity Strategies

Depending on hardware characteristics, plausible firmware integrity strategies
may vary. For example, some hardware stores firmware in flash and executes it
from there, while other devices require the OS to upload firmware into the
peripheral after each boot. The former case is rarer, good examples are TPM/GSC
and EC. Every peripheral that isn't trivial and also not vital to work before
boot or during early boot usually falls in the latter category.

To verify integrity of firmware stored in flash, the firmware must be verified
against some trust anchor. This is typically achieved by cryptographically
signing the firmware image and then having the hardware do a signature
verification against a public key indicated by a trustworthy origin (typically
ROM or fuses). As per [Cryptographic Right Answers], NaCl/[libsodium] or Ed25519
are decent choices.

Firmware integrity verification via signatures hinges on protecting the private
keys that are used to sign firmware images. If the signing key leaks or can
otherwise be accessed by malicious parties, they could sign malicious firmware
which would pass verification when loaded. The private keys must thus be held in
a shielded location and suitable access control measures must be in place
(including physical access), e.g. by using a Hardware Security Module (HSM).
Setting up a suitable system from scratch is a nontrivial exercise, so please
reach out to the security team when in doubt.

Non-persistent firmware ideally still gets integrity-checked by the peripheral
after upload before the firmware gets executed. That said, since the OS needs to
load the firmware after each boot, it can perform integrity checking then.
Verified boot helps here - assuming that the OS is in good state after reboot,
making sure that the firmware file that gets uploaded originates from the
rootfs (which is protected by Verified Boot) goes a long way for firmware
integrity.

Note that firmware originating from a location covered by Verified Boot is
generally still weaker than verification by the peripheral:
*   Even if the OS loaded the correct firmware at boot, an attacker might
    later install malicious firmware and use the device as an escalation
    vehicle. This is a risk in particular with USB devices exposed via WebUSB.
*   Verified Boot is only meaningful if a malicious devices can't subvert the
    boot process. If the device gets reliably reset by the hardware at reboot
    and that stops running firmware, Verified Boot provides satisfactory
    protection. However, reboot power sequencing is a complex topic, so it's
    hard to make sure this is correct across all SoCs and kinds of peripherals.

## Security Requirements

Motivated by the considerations above this section lists the security
requirements for the update mechanism.

For the firmware **image**:
*   The firmware image *must* be covered by ChromeOS' Verified Boot. This
    can be met by including the firmware image in the kernel or root partitions,
    as well as loading it via [DLC].
*   The peripheral *should* implement a firmware verification mechanism to
    validate the integrity of the firmware update file.
*   If the peripheral does not verify the integrity of the firmware image before
    executing it, the peripheral *must* support a way to block updates. I.e.
    allow updates during boot, but expose a mechanism to block updates
    immediately after boot and until the peripheral is power-cycled during
    reboot/power off.

For the firmware update **tool**:
*   The update tool *must* be open source and compiled as part of the ChromeOS
    build system.
*   The update tool *must* be sandboxed following the [sandboxing guide].
    Examples of this can be seen e.g. for [fwupdtool].

Speaking of `fwupd`, this project has become the standard for firmware updates
on Linux. If the peripheral can be updated using `fwupd` this will cover the
open-source requirement, and sandboxing can be reused straightforwardly.

## Further Risk Management

There are further strategies that can be employed to reduce the risk due to
compromised or malicious peripheral firmware.

First, hardware capabilities should be limited to restrict the level of access a
peripheral has into the system. This is a consideration when choosing
components. Other technical isolation mechanisms should also be considered, for
example we have IOMMU requirements in place so we can limit the memory regions a
DMA-capable peripheral can access.

It's sometimes useful to audit peripheral firmware to assess its robustness
against attacks. Ideally, the firmware source code is open, so we can examine
it. For cases where it's not, we can contract 3rd-party code audits.

[badusb]: https://en.wikipedia.org/wiki/BadUSB
[broadcom vulnerability]: https://googleprojectzero.blogspot.com/2017/04/over-air-exploiting-broadcoms-wi-fi_4.html
[DLC]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/dlcservice/docs/developer.md
[sandboxing guide]: /sandboxing.md
[fwupdtool]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/sys-apps/fwupd/files/init/fwupdtool-update.conf
[Cryptographic Right Answers]: https://latacora.micro.blog/2018/04/03/cryptographic-right-answers.html
[libsodium]: https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures
