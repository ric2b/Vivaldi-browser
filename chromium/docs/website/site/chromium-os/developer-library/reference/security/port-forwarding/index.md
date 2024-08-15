---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: port-forwarding
title: Port forwarding and tunneling in ChromeOS
---

## localhost to Crostini

ChromeOS will forward ports from `localhost` into [Crostini]. This allows
developers to use Chrome to access their development environment inside
[Crostini].

[cicerone] will [ask](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/cicerone/service.cc#853)
[chunnel] to tunnel all ports listening in the [Crostini] container, except:

*   Privileged (<1024) since [chunnel] lacks `CAP_NET_BIND_SERVICE`.
*   2222 (SFTP for the ChromeOS Files app) and 5355 ([Link-Local Multicast
    Name Resolution]) which are
    [blocked](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/cicerone/service.cc#71).

Moreover, tunneled ports are [locked down](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/patchpanel/firewall.cc#347)
to reject traffic from non-`chronos` UIDs.

[Crostini]: /containers_and_vms.md
[cicerone]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/cicerone/
[chunnel]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/chunnel/
[Link-Local Multicast Name Resolution]: https://en.wikipedia.org/wiki/Link-Local_Multicast_Name_Resolution
