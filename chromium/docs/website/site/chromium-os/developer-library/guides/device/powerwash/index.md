---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: how-to-powerwash
title: How to powerwash
---

You can [powerwash](https://support.google.com/chromebook/answer/183084#:~:text=and%20info%20(important)-,Factory%20reset%20your%20Chromebook,-Sign%20out%20of) if you do `Ctrl + Alt + Shift + R` on
the sign-in page. The Chromebook will ask you to reboot, and then once it
reboots the dialog for powerwashing will come up.

It is not recommended to powerwash a device that is running a test image, as
this will remove rsync and other tools needed for deploying.

Note: Powerwashing is not available on managed devices.
