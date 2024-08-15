---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: cros-deploy
title: cros deploy
---

## Overview

[`cros deploy`][script] is a script to deploy (install) a package to your
ChromiumOS device. It copies the binary packages built in your chroot to the
target device and installs (emerges) on the device.

**Note:** `cros deploy` does **not** build packages for you. Please make sure
you have built all the requested packages using `emerge` before invoking `cros
deploy`.

**Note:** To be able to write to your rootfs partition, `cros deploy` may
remount your rootfs partition as read-write or disable the rootfs verification
on your device.

**Note:** `cros deploy` does **not** currently support deploying kernel
packages.  See the [Kernel Development guide][deploying-kernel] for supported
methods.

[script]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/cli/cros/cros_deploy.py
[deploying-kernel]: /chromium-os/developer-library/guides/kernel/kernel-development/#Build-and-deploy

[TOC]

## Requirements

1.  Chroot: `cros deploy` needs to run in chroot.
2.  A SSH-able ChromiumOS device: Any ChromiumOS **test** image will work
    (base or dev images won't).

If your device is currently running a non-test image, you may want to use a USB
stick to image the device.

## Example usage

```bash
$ cros deploy <device> <package>...
```

*   `<device>`: The IP or hostname of the device, in the form
    `ssh://hostname[:port]` or `hostname[:port]`, where `port` is the port
    number used for SSH. (It can be omitted and defaults to 22.)
*   `<package>`: One or more packages to install.

For example, to install the `cherrypy` package to your device (with IP address
`${DUT_IP}`):

```bash
$ cros deploy ${DUT_IP} cherrypy
```

### How to specify package names

`cros deploy` does its best to figure out what packages you want to install,
even when you do not enter a complete or unique package name. When there is
ambiguity, it may prompt you to choose.

A unique package name should include three parts:
`${category}/${package}-${version}`. For example, `dev-python/cherrypy-3.1.2`.

`cros deploy` accepts any of the following formats:

```none
${category}/${package}-${version}
${category}/${package}
${package}-${version}
${package}
```

When the given input such as `${package}` (e.g. `cherrypy`) is unique, which is
the common case, `cros deploy` automatically selects the right package for you.
If, however, `${package}` exists in multiple categories, `cros deploy` prompts
you to choose. For example,

```none
Multiple matches found for cherrypy:
  [0]: dev-python/cherrypy
  [1]: foo/cherrypy
Enter your choice to continue [0-1]: 1
```

When there are multiple versions available, `cros deploy` chooses the best visible
one. For example, if you `cros workon` a package, `cros deploy` will try to use
the 9999 version instead.

## Known problems and fixes

### Failed to emerge package

Errors like the one below are likely caused by `emerge` missing on the target
system. (This can happen if the stateful partition is wiped after the OS image
has been deployed. Also remember that the target needs to run a **test** image.)

```none
13:14:55: ERROR: Failed to emerge package chromeos-chrome-46.0.2472.0_rc-r1.tbz2
13:14:56: ERROR: Oops. Something went wrong.
13:14:56: ERROR: cros deploy failed before completing.
13:14:56: ERROR: <class 'chromite.lib.cros_build_lib.RunCommandError'>: return code: 127
Failed command "ssh -p 22 '-oConnectionAttempts=4' '-oUserKnownHostsFile=/dev/null' '-oProtocol=2' '-oConnectTimeout=30' '-oServerAliveCountMax=3' '-oStrictHostKeyChecking=no' '-oServerAliveInterval=10' '-oNumberOfPasswordPrompts=0' -i /tmp/ssh-tmpb5S6kc/testing_rsa root@172.26.58.201 -- 'FEATURES=-sandbox' 'PORTAGE_CONFIGROOT=/usr/local' "CONFIG_PROTECT='-*'" 'PKGDIR=/usr/local/tmp/cros-deploy/tmp.6xIVEajmyJ/packages' 'PORTDIR=/usr/local/tmp/cros-deploy/tmp.6xIVEajmyJ' 'PORTAGE_TMPDIR=/usr/local/tmp/cros-deploy/tmp.6xIVEajmyJ/portage-tmp' emerge --usepkg /usr/local/tmp/cros-deploy/tmp.6xIVEajmyJ/packages/chromeos-base/chromeos-chrome-46.0.2472.0_rc-r1.tbz2 '--root=/'", cwd=None, extra env={'LC_MESSAGES': 'C'}
```

### No space left on device

The following mostly occurs when deploying large packages such as Chrome:

```none
rsync: write failed on "/usr/local/tmp/cros-deploy/tmp.Ts2Ayh1nKT/packages/chromeos-base/chromeos-chrome-46.0.2472.0_rc-r1.tbz2": No space left on device (28)
rsync error: error in file IO (code 11) at receiver.c(393) [receiver=3.1.1]
rsync: [sender] write error: Broken pipe (32)
13:21:46: ERROR: Oops. Something went wrong.
13:21:46: ERROR: cros deploy failed before completing.
13:21:46: ERROR: <class 'chromite.lib.cros_build_lib.RunCommandError'>: return code: 11
```

This happens because the target's `/usr/local/tmp` has insufficient space to
download the package. A workaround is pointing `/usr/local/tmp` to `/tmp`:

```bash
$ rm -r /usr/local/tmp
$ ln -s /tmp /usr/local/tmp
```
