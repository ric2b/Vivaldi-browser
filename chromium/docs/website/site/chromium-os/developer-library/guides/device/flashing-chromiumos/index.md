---
breadcrumbs:
- - /chromium-os/developer-library/guides/device
  - ChromiumOS > Guides > Device
page_name: flash-chromiumos
title: Flashing chromiumOS
---

[TOC]

If you need to update your platform version (i.e. to update to the latest
release, or to flash a specific version for debugging), you will need to be able
to flash ChromiumOS to your device. This guide will walk you through the
process of checking out the ChromiumOS code and setting up your environment to
flash a device over SSH. For more context, please refer to the full
[ChromiumOS developer guide](https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide).

## Prerequisites

This guide assumes you are working from home and have a gLinux workstation or
Cloudtop, a corp laptop, and a DUT (Device Under Test). It also assumes you are
familiar with the process of deploying ChromeOS to your DUT over SSH. If you
haven't yet, please follow the instructions in
[Building, deploying, and testing](https://g3doc.corp.google.com/chrome/chromeos/system_services_team/dev_instructions/g3doc/developing.md).

You will need to prepare the DUT by flashing a test image using a USB stick at
least once before you can flash ChromiumOS over SSH.

## Setup

Follow these steps to set up your environment and check out the ChromiumOS
code.

Add the following line to your .bashrc to ensure new files are global-read by
default:

```shell
~$ echo "umask 022" >> ~/.bashrc
```

Apply the change:

```shell
~$ source ~/.bashrc
```

Create the folder for the ChromiumOS code:

```shell
~$ mkdir ~/chromiumos
```

Navigate to the `chromiumos` directory and initialize the repo:

```shell
~$ cd ~/chromiumos
```

```shell
~/chromiumos$ repo init \
    https://chrome-internal.googlesource.com/chromeos/manifest-internal.git
```

```shell
~/chromiumos$ repo sync -j4
```

Note: This init + sync process will take quite a while, possibly a few hours.

## Enter the `chroot`

The ChromiumOS workflow uses a [chroot](https://en.wikipedia.org/wiki/Chroot),
a self-contained execution environment. To set up the chroot (and in the future,
to re-enter it):

```shell
~chromiumos$ cd src/scripts
```

```shell
~/chromiumos/src/scripts$ cros_sdk
```

Once you're inside the chroot, you will see `(cr)` prepended to your prompt.

## Flash a pre-built chromiumOS image

You can now flash ChromiumOS to your DUT over SSH using a tool called
[xBuddy](https://www.chromium.org/chromium-os/developer-library/reference/tools/xbuddy).
You can reference the examples below to get started. Be sure to customize the
address of your DUT in the ssh URL, as well as the board name and version in the
xBuddy URL.

Example 1: Flash the latest version from the dev channel to an eve device.

```shell
(cr) ~/chromiumos$ cros flash ssh://127.0.0.1:7777 \
    xbuddy://remote/eve/latest-dev \
    --no-ping --log-level=info
```

Example 2: Flash a specific version or board.

* Port:
<input type="text" class="g3doc-ext-placeholder" placeholder="$PORT" value="7777">

* Board:
<input type="text" class="g3doc-ext-placeholder" placeholder="$BOARD" value="trogdor">

* Milestone:
<input type="text" class="g3doc-ext-placeholder" placeholder="$MILESTONE" value="107">

* Platform version:
<input type="text" class="g3doc-ext-placeholder" placeholder="$PLATFORM_VERSION" value="15096.0.0">

```shell
(cr) ~/chromiumos$ cros flash ssh://127.0.0.1:$PORT  \
    xbuddy://remote/$BOARD/R$MILESTONE-$PLATFORM_VERSION \
    --no-ping --log-level=info --board=$BOARD
```

You can find version numbers, and the corresponding release number, by searching
on go/goldeneye. See
[Understanding chromeOS releases](understanding_releases.md) for more detail on
version numbers.

Each image is downloaded to your workstation, and each image is several GB in
size. You should periodically remove these downloaded images to prevent filling
up storage space on your workstation. You can find previously downloaded images
under `~/chromiumos/devserver/static/`.

## Flash a custom-built of chromiumOS image

Sometimes it may be required to work on a local build of ChromiumOS and to test
changes on your DUT. The following instructions are from the
[ChromiumOS developer guide](https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide)
but summarized here.

NOTE: If this your first time running these commands, it could take a long time.

Some of the steps require you to enter the chroot and some are run outside in
the `~/chromiumos` repository. Commands entered in the chroot will be
represented using (cr) and commands outside the chroot will be represented using
(outside). To enter the chroot:

```shell
cd ~/chromiumos
cros_sdk
```

You will then need to set an environment variable `BOARD` depending on which DUT
you would like to deploy to.

```shell
(cr) ~/chromiumos/src/scripts$ export BOARD=<your pick of board>
```

1.  **Initialize build for a board**

    ```shell
    (cr) ~/chromiumos/src/scripts$ setup_board --board=${BOARD}
    ```

2.  **Build packages for your board**

    If this is the first time running `cros build-packages`, then run this
    command:

    ```shell
    (outside) ~/chromiumos/src/scripts$ cros build-packages --board=${BOARD} --autosetgov --autosetgov_sticky
    ```

    If you would like to disable DCHECKS:

    ```shell
    (outside) ~/chromiumos/src/scripts$ cros build-packages --board=${BOARD} --no-withdebug
    ```

    Else you may use this default command and add the flags you need.

    ```shell
    (outside) ~/chromiumos/src/scripts$ cros build-packages --board=${BOARD}
    ```

3.  **Build a disk image for your board**

    ```shell
    (outside) ~/chromiumos/src/scripts$ cros build-image --board=${BOARD} --noenable_rootfs_verification test
    ```

4.  **Installing ChromiumOS on your device**

    There are two options, either by USB or by SSH.

    *   SSH:

        DUT:
        <input type="text" class="g3doc-ext-placeholder" placeholder="$DUT" value="localhost">

        Port:
        <input type="text" class="g3doc-ext-placeholder" placeholder="$PORT" value="7777">

        ```shell
        (cr) ~/chromiumos/src/scripts$ cros flash ssh://$DUT:$PORT ../build/images/${BOARD}/latest/chromiumos_test_image.bin
        ```

    *   USB stick:

        NOTE: This option doesn't seem to work when port forwarding through a
        cloudtop machine.

        Plug in your USB stick and give it about 10 seconds to register.

        ```shell
        (cr) ~/chromiumos/src/scripts$ cros flash usb:// ${BOARD}/latest
        ```


