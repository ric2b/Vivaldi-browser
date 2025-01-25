---
breadcrumbs:
- - /chromium-os/developer-library/getting-started
  - ChromiumOS > Getting Started
page_name: setup-chromebook
title: Setup your development Chromebook
---

[TOC]

Don’t have a development Chromebook yet? Skim this section and then move on to
[Checkout Chromium](../checkout-chromium). From there you can learn about
development flows that do not require a Chromebook, e.g., building and running
ChromiumOS on Linux.

## Download and flash a ChromiumOS test image onto the USB drive

### Download the test image

The
[cros flash](https://www.chromium.org/chromium-os/developer-library/reference/tools/cros-flash)
tool supports downloading ChromiumOS images built regularly and stored in Google
Storage. The
[xbuddy](https://www.chromium.org/chromium-os/developer-library/reference/tools/xbuddy/)
URL format allows specifying images by board, version, and image type. The
default xbuddy format downloads the latest test image for the specified board.
You can determine your board by navigating to chrome://version on your
development Chromebook; it’s the last word of “Platform”. Run the following
command in your Chromium or ChromiumOS source checkout, replacing `<board>`
appropriately, to download the latest test image for your board in the current
directory.

```shell
$ cros flash file://`pwd`/chromium_test_image.bin xbuddy://remote/<board>/latest-dev
```

If you downloaded the image on a remote machine (e.g., a virtualized
workstation) or in Crostini, you will need to move the image file to the local
machine with which you will connect the USB drive (an SSH connection in
Secure Shell is one option).

Alternatively, if you are working on your development workstation, you can
directly flash the USB drive in one command (see below for more information
about using `cros flash` to flash a USB drive).

```
$ cros flash usb:// xbuddy://remote/<board>/latest-dev
```

### Flash the test image to your USB drive

#### On a workstation

Plug the USB drive into your development workstation then run:

```shell
$ cros flash usb:// ./chromiumos_test_image.bin
```

Note: If cros flash can’t automagically find your USB drive, you may need to use
a different USB drive. If you must use that given USB drive, troubleshoot using
the
[`cros flash` documentation](https://www.chromium.org/chromium-os/developer-library/reference/tools/cros-flash/#known-problems-and-fixes).

Note: If you need to flash a different test image to the same USB drive later,
simply run cros flash ... as before; no extra steps are necessary.

The USB drive now contains the test image and is ready to be flashed to the
Chromebook. The next section goes over how to prepare the Chromebook for
flashing and the steps to flash the device.

#### On a Chromebook or Mac

To flash the test image to a USB drive if you're working on a Chromebook:

1.  Plug the USB drive into your Chromebook.
2.  Install and launch the
    [“Chromebook Recovery Utility”](https://chrome.google.com/webstore/detail/chromebook-recovery-utili/pocpnlppkickgojjlmhdmidojbmbodfm/related)
    extension
    ([documentation](https://support.google.com/chromebook/answer/1080595)).
3.  Click the gear icon and select "Use local image".
4.  Select the .bin image you downloaded previously.
5.  Proceed through the prompts.

If the prompts get stuck at erasing or writing to your recovery media, then you
are likely restricted to read-only on USB storage. See
[Removable media storage restrictions](https://support.google.com/techstop/answer/11065200?hl=en)
for more information and how to request an exception to get past this.

## Put your chromebook into developer mode

In order to flash your chromebook from the USB drive, the Chromebook needs to be
in
[developer mode](https://www.chromium.org/chromium-os/developer-library/guides/device/developer-mode/).

![Developer Mode](developer-mode.png)

To put the chromebook in developer mode, press (you will likely need to hold
down this combo for ~4 seconds):

`esc + Refresh (f3) + power`

Note: Some devices, such as Eve/Atlas, have the Refresh button bound to F2. When
in doubt, use the refresh button for this step.

Note: For some devices, like newer brya devices, tap the `power` button while
holding down `esc + Refresh (f2)`.

Note: For tablets, such as Nocturne, press both the increase and decrease volume
buttons and the power button to enter developer mode. You must use the buttons
on the actual tablet and not the ones on any attached keyboard. If the recovery
menu shows up, you can use the same volume buttons to navigate and the power
button to make selections; press both volume buttons simultaneously to open the
hidden developer mode menu (see go/tablet-dev for more information).

Note: For some devices, such as Teemo, keep the recovery button pressed (press
pin or paper clip inside small hole located on the right side of the box) while
pressing power button to go into recovery mode.

Once the device boots up and the developer screen shows up, press `ctrl-d` to
boot from disk.

If the device boots to the recovery screen, press `ctrl-d` to enter developer
mode. You should see "OS Verification turned OFF". From this point on, never hit
Spacebar to disable Developer Mode as the screen suggests you do.

After booting from disk, open the VT2 terminal by pressing:

`ctrl + alt + Forward (f2)`

Note: Some devices, such as Eve/Atlas, are missing a Forward button, with
another button (refresh) in the F2 place. When in doubt, still use the F2 button
for this step. \
\
You can usually locate the correct button by counting the function keys
left-to-right (disregard the esc key).

The VT2 terminal can be escaped by pressing

`ctrl + alt + backward (f1)`

Login as `root`. There should be no password.

(If there is a password, it may be `test0000`, as you may already be on a test
image, in which case, you probably don’t need to follow these steps, unless you
are trying to update to a newer test image.)

Note: The VT2 prompt suggests that you change the chronos password, but do not
do this.

Note: When development mode is enabled, you can also access a prettier-terminal
in ChromeOS by logging in (or pressing “Browse as Guest”), pressing

`ctrl + alt + T`, and then typing `shell`.

This provides you the same access that the VT2 terminal does, but is less
convenient because it requires sign in first. You will also need to run
`sudo su` to get a root shell before proceeding.

Note: If you are using a tablet without an attached keyboard, you will need to
use crosh (see above) instead of VT2.

Allow booting from a flashdrive in the VT2 terminal, then reboot:

```
$ crossystem dev_boot_usb=1
$ reboot
```

## Flash the test image to your Chromebook

Your device should now be in developer mode and be bootable from external media.
Plug the USB drive with the test image on it (see section
["Download and flash a ChromiumOS test image onto the USB drive"](#download-and-flash-a-chromium-os-test-image-onto-the-usb-drive))
into the Chromebook, and at the developer screen on boot, press

`ctrl + u`

(Instead of `ctrl + d`)

This boots the Chromebook from the USB drive. Once you’ve booted from the USB
drive, open the VT2 terminal again, login as `root` (the password will be
`test0000` this time), and run:

```shell
$ chromeos-install
```

This copies the contents of the USB drive onto the Chromebook’s drive, i.e.
installs the test image onto it. This can take a few minutes to complete. Once
finished, reboot:

```shell
$ reboot
```

Boot from disk (`ctrl + d`) and then remove the USB drive (you won’t need it
again until you need to repeat this process). You’re now running the test image!

IMPORTANT: once you have flashed a test image to your chromebook, avoid logging
into the device with an important account (especially a corp account). The root
password is public knowledge, so consider the device easily compromised. It
is okay to occasionally log in with important accounts, but do so sparingly, and
immediately wipe the account off the machine once finished. You should generally
only be using test accounts (see
["Owned Test Accounts"](http://go/owned-test-accounts-doc)).
If you need to restore the machine to be corp safe, follow steps 2 and 3 of
[“Restore your Chromebook”](https://support.google.com/chromebook/answer/1080595)
and steps 2 and 3 of
[“Reset or reimage a ChromeOS device”](https://support.google.com/techstop/answer/3017140).

### Help! chromeos-install is complaining it cannot determine a destination device

Some boards, including `morphius` boards, might have an issue where
`chromeos-install` cannot find the correct destination drive when booting from
USB. The error appears like so:

```shell
localhost$ chromeos-install
cros-disks stop/waiting
Error: can not determine destination device. Specify --dst yourself
```

This happens when the USB is being assigned as the dst (destination)
incorrectly. To resolve this, specify the correct destination drive, like so:

- Find the name of the current default (incorrect) destination drive.

```shell
localhost$ rootdev -s
/dev/sda3
```

- List all drives and media attached to this device as shown below. The next
bullet will tell you how to tell which one is the correct dst drive.

```shell
localhost$ lsblk
NAME          MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
loop0           7:0    0   552K  1 loop /run/chromeos-config/v1
                                        /run/chromeos-config/private
loop1           7:1    0   1.2G  0 loop
`-encstateful 254:1    0   1.2G  0 dm   /home/chronos
                                        /var
                                        /mnt/stateful_partition/encrypted
loop2           7:2    0  94.2M  1 loop /usr/share/chromeos-assets/speech_synthesis/patts
loop3           7:3    0   9.2M  1 loop /usr/share/chromeos-assets/quickoffice/_platform_specific
loop4           7:4    0   6.5M  1 loop /usr/share/cros-camera/libfs
loop5           7:5    0  16.9M  1 loop
`-DF7B400998A5472D01EB3888EBF7DAD7F96D243D09A8086B6EB3BE63FF666CDD
              254:2    0   6.5M  1 dm   /run/imageloader/nc-ap-dlc/package
sda             8:0    1  14.9G  0 disk
|-sda1          8:1    1     4G  0 part /var/cache/dlc-images
|                                       /var/lib/portage
|                                       /var/db/pkg
|                                       /usr/local
|                                       /home
|                                       /mnt/stateful_partition
|-sda2          8:2    1    32M  0 part
|-sda3          8:3    1   2.7G  0 part
|-sda4          8:4    1    32M  0 part
|-sda5          8:5    1     2M  0 part
|-sda6          8:6    1   512B  0 part
|-sda7          8:7    1   512B  0 part
|-sda8          8:8    1    16M  0 part /usr/share/oem
|-sda9          8:9    1   512B  0 part
|-sda10         8:10   1   512B  0 part
|-sda11         8:11   1   512B  0 part
`-sda12         8:12   1    64M  0 part
zram0         253:0    0  15.1G  0 disk [SWAP]
nvme0n1       259:0    0 238.5G  0 disk
|-nvme0n1p1   259:1    0 230.3G  0 part
|-nvme0n1p2   259:2    0    32M  0 part
|-nvme0n1p3   259:3    0     4G  0 part
|-nvme0n1p4   259:4    0    32M  0 part
|-nvme0n1p5   259:5    0     4G  0 part
|-nvme0n1p6   259:6    0   512B  0 part
|-nvme0n1p7   259:7    0   512B  0 part
|-nvme0n1p8   259:8    0    16M  0 part
|-nvme0n1p9   259:9    0   512B  0 part
|-nvme0n1p10  259:10   0   512B  0 part
|-nvme0n1p11  259:11   0   512B  0 part
`-nvme0n1p12  259:12   0    64M  0 part
```

- Find the name of the correct dst drive. Select the `nvme0n1` disk because
the Morphius board is a `nvme` device, and therefore the corresponding `nvme`
disk means that it is the correct dst drive for the chromeos installation.

- Set the destination based on the step above. At this point, the contents of
the USB should be installing correctly. Continue the instructions after the
`chromeos-install` step.

```shell
localhost$ chromeos-install --dst /dev/nvme0n1
```

## Connect your Chromebook to the internet

Your Chromebook must be connected to the internet so that your workstation (or
Cloudtop) can remotely access it via `ssh` and change its binaries. How this is
done depends on whether you are working from the office or from home.

### Working from the office

#### Lab network is not available

> Note: The lab network is not available in at least the following office:
>
> * TW-TPE-101 (as of August 2022)
>
> The Lab network condition is varied (may still be under construction and only
work in some areas), so please check with your neighbors. If in one of these
offices, you should use these steps.

Googlers without a lab network available in their office can use 1 of 2 options
to connect their test Chromebook to the internet / their workstation/cloudtop.

|               | Use a hotspot on a phone    | Establish direct connection   |
| ------------- | --------------------------- | ----------------------------- |
| Instructions  | go/hotspot-dut-deploy       | go/cros-direct-dut            |
| Prerequisites | Phone that supports hotspot | Ethernet cable, two USB-to-ethernet port adapters |
| Pros          | Less hardware required      | More reliable connection      |
| Cons          | Wi-Fi deploying can be less reliable | More hardware required        |

#### Lab network is available

In US-LAX-BIN1, you need to plug your Chromebook into the purple port at your
desk. If you don't have an available one, you can share a coworker's by using a
switch.

Note: If you work from an office beside US-LAX, you'll need to ask around to
understand if this guidance applies. Please update this page if this guidance
needs adjusting for other offices.

This purple port at your desk should already be on the lab network. If `ssh`'ing
from your workstation to your Chromebook as described above fails for you,
ensure that your workstation is on SNAX by clicking the icon in the system tray
of your workstation. If you're on SNAX and still can't SSH to your chromebook,
your purple port is probably not on the lab network.

In order to get your purple port on the lab network, file a ticket (go/guts) and
follow the format of this request
[here](https://gutsv3.corp.google.com/#ticket/36031296). You need to note down
the number written above the purple jack and ask the techs to patch that port to
the lax_cros_lab lab (as of December 2022, the switch name is
US-LAX-BIN1-LABSW1-1-2).

##### Additional step for Cloudtop users

Cloudtop users can only access the lab network through a proxy. Run `gcert` to
refresh your credentials, then use the `ProxyCommand` option when using SSH:

```shell
ssh -o "ProxyCommand=/usr/bin/corp-ssh-helper %h %p" root@<DUT IP address>
```

To avoid having to add this option whenever you use SSH, you can add the lab
network subnet to your SSH config file `~/.ssh/config` (the IP address is only
an example!):

```
Match host 100.127.92.* exec "/usr/bin/corp-ssh-helper -check"
 ProxyCommand /usr/bin/corp-ssh-helper -direct_capable %h %p
```

### Working from home

Connect your Chromebook to the same network that your corp laptop is connected
to. It's recommended that you connect to this network via ethernet for
deployment stability, but connecting via Wi-Fi works fine.

If you do connect via Wi-Fi, ensure that the network is available to all users
on the device, so that when your primary user signs out (e.g., when Chrome
is restarted during a deploy), the device remains connected to the network.
You can ensure this by 1 of 2 ways:

* Select "Allow other users of this device to use this network" if you connect
to the network while signed into a user account.
* Connect to the Wi-Fi network right after rebooting the Chromebook, at the
"login screen", *not* after signing into a user account.

## SSH into your Chromebook

Google's [Network access policy](https://g3doc.corp.google.com/company/teams/eip-access/policy/netpolicy.md)
requires you to use one of two tools to connect to devices using SSH:

The [corp SSH helper](https://g3doc.corp.google.com/corp/access/ssh/helper/g3doc/index.md)
This tool works like the standard ssh command, and is used by the
[Secure Shell extension](http://tshc/answer/2675787) on ChromeOS.

The [sshwatcher](https://chromium.googlesource.com/chromiumos/platform/dev-util/+/HEAD/contrib/sshwatcher/README.md)
This tool will automatically reconnect to the remote device if the connection
is dropped, e.g. the remote device reboots.

### Step 0: Find your Chromebook's IP address

With your Chromebook now connected to the internet, it will be more convenient
to remotely configure your Chromebook DUT via `ssh`. To do so, first find the
Chromebook's IP address via:

```shell
$ ifconfig
```

The device's IP address is `inet<address>` value of the ethernet (or Wi-Fi if
you connected wirelessly).

Note: An alternate way to view the device’s IP address is to log in, and click
into the network setting of the Ethernet/Wi-Fi connection.

### Working from the office: direct SSH

You can easily save this IP address for later use on your workstation/Cloudtop
by saving it to an environment variable:

```shell
$ MYDEVICE=<address>
```

You can now `ssh` into the device. The root password for all test images is
`test0000`:

```shell
$ ssh root@${MYDEVICE}
```

### Working from home: SSH port forwarding

Connect your Chromebook to your home network (preferably over Ethernet for
stability) and find the local IP address (for example,
DUT_IP_ADDRESS=192.168.1.12). Make sure you connect to the network from the
Chromebook login screen so that the network is not only configured for an
individual account. From your ChromeOS corp machine, in the Secure Shell App,
SSH to your workstation/cloudtop with this extra parameter in the "SSH
Arguments" field on the configuration page:

`-R 2233:<DUT_IP_ADDRESS>:22`

See figure:

![Port forwarding](ssh-reverse.png)

This forwards the local IP address of your Chromebook, making it accessible on
your workstation/cloudtop via `localhost:2233`.

You should now be able to `ssh` into the device. The root password for all test
images is `test0000`:

```shell
[workstation]$ ssh root@localhost -p 2233
```

#### Troubleshooting

##### Port forwarding failed

If you see a `Warning: remote port forwarding failed for listen port
<PORT>` message in the shell connection setup logs you may have a previous
process still using the port. Try killing the process using `sudo kill -9 $(sudo
lsof -t -i:<PORT>)` and restarting the shell.

You can optionally add the following helper function to your `bash` config to
handle this in the future:

```
kill-pid-on-port() {
  if [ $# -eq 0 ]; then
    echo "Error: port number argument not provided"
    exit 0
  fi
  sudo kill -9 $(sudo lsof -t -i:$1)
}
```

##### Chromebook IP address changes frequently

If there is no possible way to connect directly with an Ethernet cable, you can
attempt to configure the IP address manually in the event that the router keeps
assigning a new one to the Chromebook. Go to the Chromebook's Network Settings,
select the router you are connected to, and open the Network drop down panel.
Toggle off the "Configure IP address automatically" and manually change the IP
address to `192.168.1.X`. The Chromebook will attempt to keep this IP address
when restarting. Make sure to update the `<DUT_IP_ADDRESS>` SSH argument above
to reflect your manually set IP address.

See figure where `X` is set to `111`:

![Manually set IP](manual-network.png)

#### A historical note on Wireguard VPN

Wireguard was previously recommended as an alternative to SSH port forwarding,
but this type of setup is against Google policy. Wireguard should not be used.

## Optional final touches

### Optional: Make the drive writable

Note: This step is automatically performed for you when you
[deploy Chrome to your test Chromebook](developing.md#deploying-chrome).
However, you may need to explicitly use this step if, for example, you're
debugging a crash on Chrome binary directly from Goldeneye
([link](https://docs.google.com/document/d/1QRrh3ipnW9PWI9bJzqK_y_A_pOfA7otAoKM5rV1mYFo/edit#heading=h.c0uts5ftkk58)),
want to edit `/etc/chrome_dev.conf` (next bullet point) before deploying custom
Chrome.

Once `ssh`’d in, run:

```shell
$ /usr/libexec/debugd/helpers/dev_features_rootfs_verification
```

```shell
$ reboot ; exit
```

This removes rootfs verification and makes the drive writeable.
master
**You only need to follow this section if you wish to enable verbose logging or
pass a runtime flag to Chrome. The drive must be writeable to follow this
section.**

`/etc/chrome_dev.conf` contains the runtime arguments for every time Chrome (the
ui executable) is started. Whenever you change `/etc/chrome_dev.conf`, you’ll
need to restart Chrome (in order for the arguments to take effect) via:

```shell
$ restart ui
```

The most common reason you’ll want to edit `/etc/chrome_dev.conf` is to pass the
flags that enable logs. See the
[Logging]([logging.md](https://www.chromium.org/chromium-os/developer-library/guides/logging/logging/))
documentation.

### Optional: Ensure your firmware is up-to-date

If your device’s firmware is out of date, you may run into random reboots and
other weird behavior while using it. To prevent that, ensure that your device’s
firmware is updated (you need to be connected to the Internet when running this
command) by running the following on the Chromebook:

```shell
$ chromeos-firmwareupdate --mode=autoupdate
```

```shell
$ reboot
```

#### Optional: Configure a SSH config file

To make logging into your Chromebook over `ssh` easier, you can specify the
connection details in `~/.ssh/config` (create the directory and file if it
doesn't exist). For example, if you add:

```
Host eve
  HostName 192.168.0.1
  Port 2233
  User root
```

Then you can use `$ ssh eve` instead of `$ ssh -p 2233 root@192.168.0.1`.

#### Optional: Setup SSH key authentication

All ChromeOS test images have the same SSH public key set up for authentication,
so if you download the
[private key](https://chromium.googlesource.com/chromiumos/chromite/+/main/ssh_keys),
save it as `~/.ssh/testing_rsa`, and update the permissions (`chmod 0600
~/.ssh/testing_rsa`), then you can add the following to your SSH config file to
allow for passwordless authentication:

```
Host eve
  HostName 192.168.0.1
  Port 2233
  User root
  IdentityFile %d/.ssh/testing_rsa
```
History
References
Warnings

# Up next

You're now ready to [download the code](../checkout-chromium) necessary to build
and deploy ChromiumOS.

<div style="text-align: center; margin: 3rem 0 1rem 0;">
  <div style="margin: 0 1rem; display: inline-block;">
    <span style="margin-right: 0.5rem;"><</span>
    <a href="/chromium-os/developer-library/getting-started/development-environment">Development environment</a>
  </div>
  <div style="margin: 0 1rem; display: inline-block;">
    <a href="/chromium-os/developer-library/getting-started/checkout-chromium">Checkout Chromium</a>
    <span style="margin-left: 0.5rem;">></span>
  </div>
</div>
