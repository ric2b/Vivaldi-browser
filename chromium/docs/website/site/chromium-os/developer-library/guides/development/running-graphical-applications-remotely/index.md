---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: running-graphical-applications-remotely
title: Running graphical applications remotely
---

[TOC]

Remote development often requires executing binaries that open graphical
windows. Some of the most common examples are running ChromeOS on Linux and
running any number of graphics-related tests. These binaries will immediately
crash unless they are run against an
[X display](https://en.wikipedia.org/wiki/X_Window_System), which is not the
default case over SSH.

## Using the remote desktop display

When [Chrome Remote Desktop](https://remotedesktop.corp.google.com/) is
installed it will create an X display, `:20`, which is what you see when you
connect to your workstation or cloudtop. This display can be targeted by
binaries run from the command line, even over SSH. For example:

```shell
DISPLAY=:20 ./out/Default/chrome
```

This will set the `DISPLAY` environment variable to `:20` for the duration of
the command, causing the `chrome` binary to run using `:20` as the targeted
display. This results in the ChromeOS window opening on the remote desktop
display despite being run over SSH, or elsewhere.

## Forwarding the display using xpra

Using the remote desktop display results in the ChromeOS window running inside
the remote desktop window which itself is running within the browser. To avoid
these nested windows and potentially reduce overall latency, you can allow your
binaries to target a display ***on your MacBook***. This is accomplished using
the program `xpra`.

Install xpra on your MacBook via go/softwarecenter.

Install xpra on your workstation or cloudtop.

```shell
sudo apt-get install xpra
```

SSH to your workstation or cloudtop with port forwarding to allow xpra to
forward the X11 window without hitting issues with gnubby.

```shell
ssh -L 5950:localhost:5950 <HOSTNAME>
```

Run xpra on your workstation or cloudtop and create a display xpra can forward
to.

```shell
xpra start :50 --bind-tcp=127.0.0.1:5950
```

Attach xpra, on your MacBook, to the loopback address at the forwarded port.

```shell
xpra attach tcp://127.0.0.1:5950/
```

Run the graphical binary using the display created by xpra.

```shell
DISPLAY=:50 ./out/Default/chrome
```

Stop xpra when you are finished.

```shell
xpra stop :50
```

Note: Port-forwarding can introduce noticeable keystroke latency so it is
recommended that you open a second connection solely for development.
