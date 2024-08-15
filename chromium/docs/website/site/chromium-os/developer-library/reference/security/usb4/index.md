---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: usb4
title: USB4 security
---

This document discusses:

*   Security risks associated with USB4 peripherals
*   ChromeOS' approach to mitigating the risks

<!-- mdformat off(b/139308852) -->
***  note
USB4 specification includes interoperability with USB Type-C Alternate Mode
and more significantly, *Thunderbolt*. This document also applies to Thunderbolt
peripherals connected to a Chromebook USB4 host since they require very similar
security considerations.
***
<!-- mdformat on -->

## Threat Model and Security Risks

ChromeOS uses [USB Guard] and [USB Bouncer] to mitigate threats associated with
legacy USB (USB3 and earlier). These mitigations are equally effective for
legacy USB devices attached to a USB4 Chromebook host. We treat USB3 as the
baseline and limit our discussion to incremental risks introduced by USB4.

*PCI Express (PCIe) Tunneling* is a prominent feature of USB4 (and Thunderbolt)
peripherals. This feature provides an external peripheral with *Direct Memory
Access (DMA)* to Chromebook's System Memory (DRAM).

Additionally, PCIe Tunneling entails external Peripherals interacting directly
with corresponding device drivers running in the highly privileged kernel mode.
In other words, PCIe device drivers now directly contribute to the kernel attack
surface available to a malicious USB4 (or Thunderbolt) peripheral.

The security measures discussed below aim to mitigate a
[short-term, physically present attacker] from using a malicious USB4 (or
Thunderbolt) peripheral to steal user data directly from System Memory.

## Security Mitigations

### DMA protection with IOMMU

ChromeOS utilizes *I/O Memory Management Units (IOMMUs)* to control and limit
System Memory locations that are directly accessible to external peripherals. By
default, the peripheral can access nothing. Only the kernel or its device
drivers are able to open up explicit System Memory locations for direct access
by the peripheral.

Additionally, ChromeOS also configures the kernel to be stricter in regards to
DMA access from external peripherals. These extra mitigations close certain
spatial and temporal gaps that allow limited DMA access to unintended data.
Specifically, ChromeOS employs the following Linux kernel options for external
peripherals:

*   Bounce buffers when requested allocation or mapping is smaller than IOMMU
    granularity
*   Synchronous IOTLB flushing

One de-facto industry name for this technique is *Kernel DMA Protection*.
Therefore it's worth mentioning that ChromeOS also provides DMA protection
during early boot i.e during the firmware or *Coreboot* phase. This protection
is implicit because PCIe Tunneling for external peripherals is disabled in the
Coreboot environment.

### Device driver vetting

ChromeOS currently allows a limited, vetted subset of device drivers to
interact with external PCIe peripherals. This helps reduce the kernel attack
surface exposed to a malicious device. In absence of this allowlist, a malicious
device could easily convince the kernel to bind it to a particularly buggy
device driver. Our vetting process includes fuzzing and auditing the
device-facing interface of the driver.

### Peripheral authentication (or the lack thereof)

Academic research (such as [Thunderspy]) has shown that trivial UUID based
device authentication is inadequate to prevent spoofing. We also do not want to
*click fatigue* ChromeOS users by asking them to authenticate devices. ChromeOS
therefore does not rely on UUID based peripheral authentication.

### PCIe Tunnel authorization policy

#### Data access protection for peripherals opt-out

<!-- mdformat off(b/139308852) -->
*** note
Despite the name, **DMA protection and driver allowlist are always active**
even if Data access protection is *disabled*.
***
<!-- mdformat on -->

For maximum security against residual risk, PCIe Tunnels are forbidden by
default. ChromeOS requires the Chromebook owner or an equivalent Enterprise IT
admin to explicitly enable them by opting out of *Data access protection for
peripherals*. More information on this can be found in the help article on
[Connecting your Chromebook to Thunderbolt & USB4 accessories]

#### Login/Lock screens and Guest sessions

ChromeOS prohibits authorization of new PCIe tunnels on login or lock screens.
ChromeOS also prohibits PCIe tunnels in a *Guest session*. Existing PCIe
tunnels are deauthorized on user logout. For sake of user experience, already
authorized PCIe tunnels remain authorized if a user locks their session without
logging out completely.

#### Policy

Putting the above rules together, ChromeOS will automatically **authorize** any
PCIe tunnel if:

*   *Data access protection for peripherals* is disabled for the Chromebook
    **and**
*   A non-guest user session is active and unlocked

ChromeOS will automatically **deauthorize** PCIe tunnels if:

*   The user signs out **or**
*   The peripheral is disconnected **or**
*   *Data access protection for peripherals* is enabled

### Legacy alternate modes

ChromeOS falls back to safer legacy *alternate* modes whenever all of the
following conditions are met:

*   PCIe tunneling is prohibited by policy
*   The peripheral does not support USB4
*   The peripheral supports such a legacy alternate mode.

One such fallback mode is a USB Type-C Alternate mode which is supported by some
newer Thunderbolt 3 peripherals -- *DisplayPort Alternate mode*. This mode
carries DP and also USB sans PCIe tunnels. Some user experience examples that
this helps without sacrificing on security:

*   Allow the user to login or unlock their docked Chromebook via a display and
    keyboard attached to the dock.
*   Provide limited dock functionality without requiring the Data access
    protection opt-out.

## Software Connection Managers

USB4 devices are managed by two ChromeOS daemons. Their main responsibilities
in this context are as follows:

### [pciguard]

*   Source of truth for the device driver allowlist
*   Performs PCIe tunnel authorization and deauthorization based on user session
    state

### [typecd]

*   Manages Type-C modes. Enters USB4, Thunderbolt alt mode, Type-C alt mode,
    etc based on *Data access protection for peripherals* and user session
    states.

[USB Guard]: https://usbguard.github.io/
[USB Bouncer]: https://chromium.googlesource.com/chromiumos/platform2/+/main/usb_bouncer/README.md
[short-term, physically present attacker]: /security/chromeos_security_whitepaper.md#how-our-principles-inform-the-threat-model
[Thunderspy]: https://thunderspy.io/
[Connecting your Chromebook to Thunderbolt & USB4 accessories]: https://support.google.com/chromebook/answer/10483408
[pciguard]: https://chromium.googlesource.com/chromiumos/platform2/+/main/pciguard/README.md
[typecd]: https://chromium.googlesource.com/chromiumos/platform2/+/main/typecd/README.md
