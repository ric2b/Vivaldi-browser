---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: performance
title: ChromeOS performance tests
---

## Generating a Bootchart

1.  Enable the bootchart feature when building the image by setting the
    following variable in the ChromeOS chroot prior to building an image.
    ```bash
    export USE="bootchart"
    ```
    You may want to add in any other features separated by spaces such as
    `chrome-internal`.
1.  Build the image and flash it onto a device.
1.  Setup the device state the way you desire it to be tested. This involves
    creating any user accounts and enabling any features necessary to see the
    particular boot sequence desired. For example, enabling ARC++ for a user.
1.  Reboot the device and wait for it to finish booting.
1.  Copy `/var/log/bootchart/boot-YYYYMMDD-hhmmss.tgz` to your host machine
    (`scp` works for this purpose).
1.  Install `pybootchartgui` on your host system outside the change root and run
    the following command to generate a `bootchart.png` file:
    ```bash
    pybootchartgui boot-YYYYMMDD-hhmmss.tar.gz
    ```
