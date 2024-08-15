---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: sensitive-chromeos-packages
title: Security-sensitive ChromeOS packages
---

## Objective

This is a list of security-sensitive Portage packages that the ChromeOS team
should strive to update **quickly** in the event of a security bug in them.

On an incoming security bug report, the ChromeOS security sheriff should
ideally update the package themselves, or find an owner that can commit to
updating the package quickly, namely **before the end of their sheriff shift**.
Bear in mind that sometimes there will be no upstream fix for the bug, in which
case the package should be updated as soon as a fix is available.

## Rules

*   The package has to be a third-party package (i.e. nothing from
    `chromeos-base`).
*   The kernel doesn't count.

## Packages

### `dev`

*   `dev-db/sqlite`
*   `dev-libs/expat`
*   `dev-libs/glib`
*   `dev-libs/jsoncpp`
*   `dev-libs/libpcre`
*   `dev-libs/libpcre2`
*   `dev-libs/libxml2`
*   `dev-libs/openssl`
*   `dev-libs/protobuf`

### `media`

*   `media-libs/freetype`
*   `media-libs/libpng`
*   `media-libs/tiff`

### `net`

*   `net-dns/avahi`
*   `net-fs/samba`
*   `net-libs/libmicrohttpd`
*   `net-libs/libpcap`
*   `net-misc/curl`
*   `net-misc/dhcpcd`
*   `net-misc/modemmanager-next`
*   `net-misc/openssh`
*   `net-misc/tlsdate`
*   `net-print/cups`
*   `net-print/hplip`
*   `net-vpn/openvpn`
*   `net-vpn/strongswan`
*   `net-wireless/bluez` (hopefully this is going away soon)
*   `net-wireless/wpa_supplicant`

### `sys`

*   `app-arch/unrar`
*   `sys-apps/dbus`
*   `sys-apps/restorecon`
*   `sys-apps/usbguard`
*   `sys-fs/fuse`
*   `sys-fs/fuse-archive`
*   `sys-fs/mount-zip`
*   `sys-fs/rar2fs`
*   `sys-fs/udev`
*   `sys-libs/glibc`
*   `sys-libs/libselinux-2.7`
*   `sys-libs/zlib`
