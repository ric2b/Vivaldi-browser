---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: logging
title: Logging
---

[TOC]

## Enable verbose logging

You can view verbose logs (`VLOG`s and `LOG(VERBOSE)`) by passing runtime
arguments to enable logging. You must first enable passing runtime arguments;
see the [Getting Started](/chromium-os/developer-library/getting-started).

Once you’ve done that, specific flags need to be added to the
`/etc/chrome_dev.conf` file.

Note: vim as-is on ChromeOS devices behaves like classic `vi`. If you haven't
done so, you can create a `.vimrc` file with the command below:

```shell
$ echo "set nocompatible backspace=2" >> ~/.vimrc
```

You can now use vim as expected. Add the following lines to the
`/etc/chrome_dev.conf` file:

```
--enable-logging
--v=1
```

You can enable verbose logging (printed by the VLOG() macros) of a given log
level and for particular modules (in this case, I chose bluetooth, dbus, bluez)
like so:

```
--vmodule=*bluetooth*=1,*dbus*=1,*bluez*=2
```

The flags in the `/etc/chrome_dev.conf` file are added as arguments when the
chrome binary is run during startup. In the same way, you can enable logging
when running tests by adding args
like so:

```shell
$ ./out/Default/extensions_browsertests --gtest_filter=*BluetoothSocket* --vmodule=*bluetooth*=1,*dbus*=1,*bluez*=2
```

## Viewing logs

There are 4 separate log files kept by ChromeOS. Those log files are:

1.  `/var/log/messages`

    1.  Logged to by the actual operating system or kernel.
    2.  Past files are kept as `/var/log/messages.1`, `/var/log/messages.2`,
        etc.

2.  `/var/log/chrome/chrome`

    1.  Logged to by chrome, but only before the user logs in. **However, in
        dev-mode, it’s also logged to during a user session. Therefore, this
        will be the primary log file you use when developing.**
    2.  In non-dev-mode devices, you will only need to inspect this file if you
        are curious about operations that occur before user login.
    3.  `/var/log/chrome/chrome` is a symbolic link to the current log file. The
        current log file and all past files are formatted like
        `/var/log/chrome/chrome_YYYYMMDD-HHDDMM`. There is also a symbolic link
        to the log file for the previous session, called
        `/var/log/chrome/chrome.PREVIOUS`.

3.  `/var/log/ui/ui.LATEST`

    1.  **You will generally never use this file in dev-mode devices.**
    2.  Logged to by chrome after user login, but before it creates the
        `/home/chronos/user/log/chrome` log file.
    3.  `/var/log/ui/ui.LATEST` is a symbolic link the current log file. The
        current log file and all past files are formatted like
        `/var/log/ui/ui.20170331-101012`.

4.  `/home/chronos/user/log/chrome`

    1.  **Only exists in non dev-mode devices. You will generally never use this
        file.**
    2.  Logged to by chrome after user login.
    3.  You will primarily be inspecting logs from this file.
    4.  `/home/chronos/user/log/chrome` is a symbolic link the current log file.
        The current log file and all past files are formatted like
        `/home/chronos/user/log/chrome_20170330-182904.`
    5.  (For context, chronos is the name of the user which launches the chrome
        executable)

You can simply read through the logs with `vim` or `cat`. The files can be
fairly spammy, so I recommend using `cat` or `tail` with `grep` like so:

```shell
$ cat /var/log/chrome/chrome | grep -e “keyword 1” -e keyword2
```

Files in chrome can also be accessed directly with `file://` urls. For example,
to view `/var/log/chrome/chrome`, you can open up the browser and enter:

```
file:///var/log/chrome/chrome
```

If you wish to move log files from your test device onto your workstation and
your workstation is a ChromeOS device, you can use
[Nearby Share](https://www.howtogeek.com/719022/how-to-use-nearby-share-on-a-chromebook/)
to transfer files over. In order to use Nearby Share, the log files must be
accessible from the Files app. To move a file, such as `/var/log/chrome/chrome`,
into the Downloads folder, ssh into your test device, then run:

```
mv /var/log/chrome/chrome /home/chronos/user/MyFiles/Downloads
```
