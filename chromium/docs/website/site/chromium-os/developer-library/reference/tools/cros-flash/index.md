---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: cros-flash
title: cros flash
---

`cros flash` is a script to update a ChromiumOS device with an image, or to
copy image onto a removable device (e.g. a USB drive) or save it as a file.

[TOC]

## Overview

`cros flash` utilizes the [devserver] to download images and/or generate
payloads.

When updating a ChromiumOS device, `cros flash` relies on an SSH connection
to talk to the device (which is enabled in test images). `cros flash` assumes
that the device is NOT capable of initiating an SSH connection to your
workstation, allowing it to be used in a more restricted/secured network
environment.

## Requirements

### Updating a Chromium device

1.  A ChromiumOS device with a test image.
2.  SSH test keys so that the script can SSH into the test device without a
    password. See [Setting up SSH Access] to your test device.
3.  Chroot: `cros flash` downloads artifacts and generates payloads.
4.  Full ChromiumOS checkout.

If your device is currently running a non-test image, you will need to use a
USB stick to image the device with a test image first (see [developer guide]).

### Download images/payloads from Google Storage

1.  Chroot: `cros flash` [xBuddy]/[devserver] to download the files.
    Devserver currently only runs in the chroot.
2.  Credentials to download from Google Storage. External developers can only
    download images for publicly available boards such as amd64-generic and
    kevin. See the [gsutil] docs to configure your credentials.

## Example Usage

```bash
cros flash <device> <image>
```

*   `<device>`: Required. `ssh://IP[:port]` of your ChromiumOS device,
    `usb://path/to/removable/device`, or `file://path/to/a/file`
*   `<image>`: Optional.  Can be a path to an image, an xBuddy path, a payload
    directory, or simply `latest` for latest locally-built image. Defaults to
    `latest`.

***note
For IPv6 addresses, the brackets in the address are non-optional.  For
example, you must write `[::1]:2222` instead of `::1:2222`, as the
port number is optional, and this prevents syntax ambiguity.
***

For example, to update the device with the latest locally-built image, you can
use the shortcut `latest`:
```bash
cros flash ssh://${DUT_IP} latest
```

or simply:
```bash
cros flash ssh://${DUT_IP}
```

To use a locally-built test image:
```bash
cros flash ssh://${DUT_IP} xbuddy://local/amd64-generic/R78-12450.0.0/test
```

To specify a local image by path:
```bash
cros flash ssh://${DUT_IP} path/to/image
```

To download the latest canary image:
```bash
cros flash ssh://${DUT_IP} xbuddy://remote/amd64-generic/latest-canary
```

> Note: external developers will not be able to download non-public remote
> images.
> If you are prompted to confirm due to board mismatch, simply type 'yes' to
> proceed.

You can replace `ssh://${DUT_IP}` in the above examples with
`usb://device/path` or `usb://` to copy the image onto a removable device.
However, you have to specify the board to use.
```bash
cros flash usb:// ${BOARD}/latest

cros flash usb:///dev/sdc path/to/image
```

Finally, if you just want to look at the image you are installing (using
`mount_gpt_image.sh`) or save the file for later, you can use `file://` to save
it to a file of your choice e.g.
```bash
cros flash file://path/to/save/image.bin path/to/image
```

## Device

### ChromiumOS Device

Device can be `ssh://hostname[:port]` or `hostname[:port]`. Port number is the
SSH port to use to connect to your device (defaults to `22`).

> Note: If you use `cros flash` to install a non-test image, `cros flash` will
> not be able to confirm that the device has rebooted successfully after the
> update. Also, you will not be able to use `cros flash` to reimage again
> because `cros flash` relies on being able to ssh into the device. You will
> need to use a USB stick to image your device.

### Removable Device

Device can be `usb://path/to/removable/device` or `usb://`. If a device path is
specified, `cros flash` will check if the device is indeed removable. If no path
is given, the user will be prompted to choose from a list of removable devices.

> Note that auto-mounting of USB devices should be turned off as it may corrupt
> the disk image while it's being written.

## xBuddy paths

Please see the [xBuddy] documentation for more information on the format of
xBuddy paths.

## Pre-generated update payloads

`cros flash` your pre-generated update payloads directly if you pass a
payload directory as `<image>`:
```bash
cros flash ${DUT_IP} path/to/payload/directory
```

`cros flash` looks for `update.gz` and/or `stateful.tgz` in the payload
directory and uses them to update the device.

## Testing OS updates without reboot

`cros flash` can be used to test ChromiumOS update process without rebooting
using option `--no-reboot`. This is useful when testing the update UI
repeatedly.

```bash
cros flash --no-reboot ssh://${DUT_IP} path/to/image
```

## Known problems and fixes

### Where is Cros Flash?

`cros flash` is in the chromite repo. The script is
`chromite/cli/cros/cros_flash.py`. The source is [available here].

### Failures related to update engine

Make sure that update-engine service on device is running. If not:

```bash
# On the device
start update-engine
```

[available here]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/cli/cros/cros_flash.py
[devserver]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/docs/devserver.md
[Setting up SSH Access]: /chromium-os/developer-library/guides/development/developer-guide/#Set-up-SSH-connection-between-chroot-and-DUT
[Simple Chrome]: /chromium-os/developer-library/guides/development/simple-chrome-workflow/
[developer guide]: /chromium-os/developer-library/guides/development/developer-guide/
[xBuddy]: /chromium-os/developer-library/reference/tools/xbuddy/
[gsutil]: /chromium-os/developer-library/reference/tools/gsutil/
