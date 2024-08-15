---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: crosfleet
title: Crosfleet
---

[TOC]

You can check out lab-based test devices using a tool called `crosfleet`. This
can be useful for testing, especially when debugging device-specific issues or
when you need a device with specific config for WiFi/Cellular.

## Setup

1.  Follow the setup instructions at
[go/crosfleet-cli](http://goto.google.com/crosfleet-cli)
2.  Complete the SSH setup described in go/chromeos-lab-duts-ssh.

## Lease

You can lease a device by model, board, or hostname. The lease will last 60
minutes by default, but this can be configured. For example:

```shell
crosfleet dut lease -model atlas -minutes 120
crosfleet dut lease -board octopus
crosfleet dut lease -host chromeos2-row7-rack4-host33
crosfleet dut lease -dims label-pool:cellular -minutes 480
crosfleet dut lease -board guybrush -dims label-pool:wificell -minutes 480
```

Because the hostname describes the physical location of the DUT, you can request
multiple devices in physical proximity by incrementing the host address:

```shell
crosfleet dut lease -host chromeos2-row7-rack4-host33
crosfleet dut lease -host chromeos2-row7-rack4-host32
```

With this method you can lease devices that are within Bluetooth range of each
other.

## Connect

Connecting over SSH is as straightforward as `ssh <host>`.

To connect over CRD, follow the instructions at
[go/arc-wfh#remote-desktop](http://goto.google.com/arc-wfh#remote-desktop).
There are a few gotchas:

*   You will need to run a Tast test on the DUT to generate an access code. This
    requires setting up the "chroot" for ChromiumOS, and following the setup
    instructions in the dev guide up to the [Build the packages for your board]
    step.
*   You need to use an owned test account (Rhea account) to connect as standard
    Gmail accounts are not permitted to connect over CRD.
*   As part of the connection flow you will run a Tast test on the DUT to
    generate an access code for CRD. Be prompt when verifying the login and when
    connecting after the access code is generated as the test will timeout.
*   If you need to enable feature flags, do so via `/etc/chrome_dev.conf` rather
    than in chrome://flags. I ran into issues getting logged back in after
    enabling features via the UI.

## Flash image and run tests

-   Flashing an image on lab DUTs **outside the chroot** is similar to
    [Flashing ChromiumOS] on a locally connected device as long as
    [SSH private keys] are set up.
-   Lab DUT access requires CorpSSH certificate (go/corp-ssh-helper) which is
    not supported **inside the chroot**, but you can use
    [corp-ssh-helper-helper](http://go/corp-ssh-helper-helper) instead.

    See instructions at [SSH to a lab DUT inside chroot environment].

Example of flashing a locally built image and running a tast test on the leased
DUT:

```shell
# Outside the chroot.
~/chromiumos % src/platform/dev/contrib/corp-ssh-helper-helper/corp-ssh-helper-helper-server.py
```

```shell
# In chroot in a different terminal.
cros flash chromeos2-row7-rack4-host33 octopus/latest
tast run chromeos2-row7-rack4-host33 login.Chrome
```

[Build the packages for your board]: https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide/#build-the-packages-for-your-board
[Flashing ChromiumOS]: /chromium-os/developer-library/guides/device/flashing-chromiumos/#flash-a-custom-built-of-chromium-os-image
[SSH private keys]: http://go/chromeos-lab-duts-ssh#setup-private-key-and-ssh-config
[SSH to a lab DUT inside chroot environment]: http://go/chromeos-lab-duts-ssh#optional-ssh-to-a-lab-dut-inside-chroot-environment
